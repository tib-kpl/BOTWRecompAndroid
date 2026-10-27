// fsp-srv: RomFS as a raw storage image, save data and SD card backed by host directories.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../mods.h"
#include "../hle.h"
#include "../ipc.h"
#include "../paths.h"
#include "../romfs_build.h"

#define FS_PATH_NOT_FOUND MAKE_RESULT(2, 1)
#define FS_PATH_EXISTS MAKE_RESULT(2, 2)
#define FS_TARGET_LOCKED MAKE_RESULT(2, 7)
#define FS_NOT_EMPTY MAKE_RESULT(2, 8)

const char *g_save_dir = "save";
static int romfs_fd = -1;

// ------------------------------------------------------------------ RomFS file index (offset -> path)
typedef struct { uint64_t start, size; char *path; int fd; const uint8_t *mem; } Overlay;
// in-image patches (copies of RomFS tables with changes): header, dir meta, file hash, file meta
typedef struct { uint64_t off, size; uint8_t *data; } Patch;
static Patch patches[8];
static int npatches;
static Overlay *ov;
static int ov_n, ov_cap;
static uint8_t *ov_meta;
static uint64_t ov_meta_off, ov_meta_size, ov_end;
// Lets the runtime see which game files are being streamed (e.g. which level is loading).
typedef struct { uint64_t off, size; char *path; uint32_t meta; } RomFile;
static RomFile *rom_files;
static int rom_nfiles;
static uint64_t rom_data_off;

// Reboot: the next boot builds its own index / overlay when the game opens the RomFS again.
RT_ON_RESET(fs_reset) {
    for (int i = 0; i < ov_n; i++) {
        if (ov[i].fd >= 0) close(ov[i].fd);
        free(ov[i].path);
    }
    for (int i = 0; i < npatches; i++) free(patches[i].data);  // (ov_meta and the extended table are among these)
    for (int i = 0; i < rom_nfiles; i++) free(rom_files[i].path);
    free(ov), free(rom_files);
    ov = NULL, ov_n = ov_cap = 0;
    ov_meta = NULL, ov_meta_off = ov_meta_size = ov_end = 0;
    npatches = 0;
    rom_files = NULL, rom_nfiles = 0, rom_data_off = 0;
    if (romfs_fd >= 0) close(romfs_fd);
    romfs_fd = -1;
    g_save_dir = "save";
}

// RomFS mods: LARECOMP_MODS=<folder> with a romfs/ laid out like the game's; its files replace the game's files of the
// same path, and files the game doesn't have are added (in directories it has).
static const char *mods_dir(void) {
    const char *e = getenv("LARECOMP_MODS");
    return e && *e ? e : NULL;
}

static bool mods_override(const char *path, char *out, size_t cap) {
    const char *d = mods_dir();
    if (!d) return false;
    snprintf(out, cap, "%s/romfs/%s", d, path);
    struct stat sb;
    return stat(out, &sb) == 0 && S_ISREG(sb.st_mode);
}

static int rom_cmp(const void *a, const void *b) {
    const RomFile *x = a, *y = b;
    return x->off < y->off ? -1 : x->off > y->off;
}

static void rom_dir_path(const uint8_t *dmeta, uint32_t dsize, uint32_t dir, char *out, size_t cap) {
    char parts[16][256];
    int n = 0;
    while (dir != 0 && dir < dsize && n < 16) {  // root (offset 0) has an empty name
        uint32_t parent, namelen;
        memcpy(&parent, dmeta + dir, 4);
        memcpy(&namelen, dmeta + dir + 0x14, 4);
        if (namelen > 255) namelen = 255;
        memcpy(parts[n], dmeta + dir + 0x18, namelen);
        parts[n][namelen] = 0;
        n++;
        dir = parent;
    }
    out[0] = 0;
    for (int i = n - 1; i >= 0; i--) { strlcat(out, parts[i], cap); strlcat(out, "/", cap); }
}

// ---- files a mod ADDS (names the game's RomFS doesn't have). The game parses the
// RomFS tables itself, so they're extended: a copy of the file-meta table with the new entries appended (linked
// into their directory's file list and the name hash buckets) is placed past the end of the image, the header,
// directory table and hash table are patched to match. New files must go into existing directories.
static uint32_t romfs_hash(uint32_t parent, const char *name, uint32_t len) {
    uint32_t hash = parent ^ 123456789;
    for (uint32_t i = 0; i < len; i++) { hash = (hash >> 5) | (hash << 27); hash ^= (uint8_t)name[i]; }
    return hash;
}

static int path_cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

typedef struct { char rel[1024]; char host[1200]; uint64_t size; } NewFile;

static void walk_new(const char *root, const char *rel, char **known, int nknown, NewFile **out, int *n, int *cap) {
    char dirp[1200];
    snprintf(dirp, sizeof dirp, "%s/%s", root, rel);
    DIR *d = opendir(dirp);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char r2[1024], h2[1200];
        snprintf(r2, sizeof r2, "%s%s%s", rel, rel[0] ? "/" : "", e->d_name);
        snprintf(h2, sizeof h2, "%s/%s", root, r2);
        struct stat sb;
        if (stat(h2, &sb)) continue;
        if (S_ISDIR(sb.st_mode)) { walk_new(root, r2, known, nknown, out, n, cap); continue; }
        const char *key = r2;
        if (bsearch(&key, known, nknown, sizeof *known, path_cmp)) continue;  // replaces an existing file
        if (*n == *cap) { *cap = *cap ? *cap * 2 : 64; *out = realloc(*out, sizeof **out * *cap); }
        strlcpy((*out)[*n].rel, r2, sizeof (*out)[*n].rel);
        strlcpy((*out)[*n].host, h2, sizeof (*out)[*n].host);
        (*out)[*n].size = (uint64_t)sb.st_size;
        (*n)++;
    }
    closedir(d);
}

// directory entry offset for "a/b/c" (NULL-terminated walk from the root, offset 0), -1 if missing
static int64_t dir_lookup(const uint8_t *dm, uint64_t dsz, const char *path) {
    uint32_t cur = 0;
    char buf[1024];
    strlcpy(buf, path, sizeof buf);
    for (char *tok = strtok(buf, "/"); tok; tok = strtok(NULL, "/")) {
        uint32_t child;
        memcpy(&child, dm + cur + 8, 4);
        while (child != 0xFFFFFFFFu && child + 0x18 <= dsz) {
            uint32_t nl;
            memcpy(&nl, dm + child + 0x14, 4);
            if (nl == strlen(tok) && !memcmp(dm + child + 0x18, tok, nl)) break;
            memcpy(&child, dm + child + 4, 4);
        }
        if (child == 0xFFFFFFFFu || child + 0x18 > dsz) return -1;
        cur = child;
    }
    return cur;
}

static void add_new_files(int fd, const uint64_t *h, const uint8_t *dmeta, uint64_t dsz, uint64_t fsz) {
    const char *mods = mods_dir();
    if (!mods) return;
    char root[1100];
    snprintf(root, sizeof root, "%s/romfs", mods);
    char **known = malloc(sizeof *known * rom_nfiles);
    for (int i = 0; i < rom_nfiles; i++) known[i] = rom_files[i].path;
    qsort(known, rom_nfiles, sizeof *known, path_cmp);
    NewFile *nf = NULL;
    int n = 0, cap = 0;
    walk_new(root, "", known, rom_nfiles, &nf, &n, &cap);
    free(known);
    if (!n) return;
    rom_files = realloc(rom_files, sizeof(RomFile) * (rom_nfiles + n));
    uint64_t hoff = h[5], hsz = h[6];
    uint8_t *dcopy = malloc(dsz), *hcopy = malloc(hsz), *head = malloc(0x50);
    memcpy(dcopy, dmeta, dsz);
    if (pread(fd, hcopy, hsz, (off_t)hoff) != (ssize_t)hsz || pread(fd, head, 0x50, 0) != 0x50) return;
    // the file-meta table to extend: the override-patched copy if there is one
    const uint8_t *base_meta = ov_meta;
    uint8_t *tmp = NULL;
    if (!base_meta) { tmp = malloc(fsz); pread(fd, tmp, fsz, (off_t)h[7]); base_meta = tmp; }
    uint64_t extra = 0;
    for (int i = 0; i < n; i++) {
        const char *name = strrchr(nf[i].rel, '/');
        name = name ? name + 1 : nf[i].rel;
        extra += 0x20 + ((strlen(name) + 3) & ~3u);
    }
    uint64_t nsz = fsz + extra;
    uint8_t *meta = calloc(1, nsz);
    memcpy(meta, base_meta, fsz);
    free(tmp);
    if (!ov_end) {
        struct stat rs;
        fstat(fd, &rs);
        ov_end = (rs.st_size + 0xF) & ~0xFull;
    }
    uint64_t meta_at = ov_end;  // the new table first, then the new files' data
    uint64_t data_at = (meta_at + nsz + 0xF) & ~0xFull;
    uint32_t nb = (uint32_t)(hsz / 4);
    uint64_t o = fsz;
    int added = 0;
    for (int i = 0; i < n; i++) {
        char dirp[1024];
        strlcpy(dirp, nf[i].rel, sizeof dirp);
        char *slash = strrchr(dirp, '/');
        const char *name = slash ? slash + 1 : nf[i].rel;
        if (slash) *slash = 0; else dirp[0] = 0;
        int64_t dir = dir_lookup(dcopy, dsz, dirp);
        if (dir < 0) { LOG("[fs] mod file %s: directory not in the game's RomFS, skipped\n", nf[i].rel); continue; }
        uint32_t nl = (uint32_t)strlen(name), d32 = (uint32_t)dir, entry = (uint32_t)o, child;
        memcpy(&child, dcopy + dir + 0xC, 4);  // dir's first file -> becomes our sibling
        uint64_t rel = data_at - rom_data_off, size = nf[i].size;
        uint32_t bucket = romfs_hash(d32, name, nl) % nb, prev;
        memcpy(&prev, hcopy + 4 * bucket, 4);
        memcpy(meta + o, &d32, 4);
        memcpy(meta + o + 4, &child, 4);
        memcpy(meta + o + 8, &rel, 8);
        memcpy(meta + o + 0x10, &size, 8);
        memcpy(meta + o + 0x18, &prev, 4);
        memcpy(meta + o + 0x1C, &nl, 4);
        memcpy(meta + o + 0x20, name, nl);
        memcpy(dcopy + dir + 0xC, &entry, 4);
        memcpy(hcopy + 4 * bucket, &entry, 4);
        o += 0x20 + ((nl + 3) & ~3u);
        if (ov_n == ov_cap) { ov_cap = ov_cap ? ov_cap * 2 : 256; ov = realloc(ov, sizeof *ov * ov_cap); }
        ov[ov_n++] = (Overlay){data_at, size, strdup(nf[i].host), -1, NULL};
        rom_files[rom_nfiles++] = (RomFile){data_at, size, strdup(nf[i].rel), entry};
        data_at = (data_at + size + 0xF) & ~0xFull;
        added++;
    }
    // the new table goes before the new data (keep the overlay list sorted by start)
    if (ov_n == ov_cap) { ov_cap = ov_cap ? ov_cap * 2 : 256; ov = realloc(ov, sizeof *ov * ov_cap); }
    memmove(ov + (ov_n - added) + 1, ov + (ov_n - added), sizeof *ov * added);
    ov[ov_n - added] = (Overlay){meta_at, nsz, NULL, -1, meta};
    ov_n++;
    ov_end = data_at;
    uint64_t fmeta_off = meta_at, fmeta_size = o;
    memcpy(head + 7 * 8, &fmeta_off, 8);
    memcpy(head + 8 * 8, &fmeta_size, 8);
    patches[npatches++] = (Patch){0, 0x50, head};
    patches[npatches++] = (Patch){h[3], dsz, dcopy};
    patches[npatches++] = (Patch){hoff, hsz, hcopy};
    LOG("[fs] mods: %d new romfs files\n", added);
}

static void rom_index(int fd) {
    uint64_t h[10];
    if (pread(fd, h, sizeof h, 0) != (ssize_t)sizeof h) return;
    // header: size, dir hash (off,size), dir meta (off,size), file hash (off,size), file meta (off,size), data off
    uint64_t doff = h[3], dsz = h[4], foff = h[7], fsz = h[8];
    rom_data_off = h[9];
    uint8_t *dmeta = malloc(dsz), *fmeta = malloc(fsz);
    if (pread(fd, dmeta, dsz, (off_t)doff) != (ssize_t)dsz || pread(fd, fmeta, fsz, (off_t)foff) != (ssize_t)fsz) return;
    int cap = 4096;
    rom_files = malloc(sizeof(RomFile) * cap);
    for (uint64_t o = 0; o + 0x20 <= fsz;) {
        uint32_t parent, namelen;
        uint64_t data, size;
        memcpy(&parent, fmeta + o, 4);
        memcpy(&data, fmeta + o + 8, 8);
        memcpy(&size, fmeta + o + 0x10, 8);
        memcpy(&namelen, fmeta + o + 0x1C, 4);
        char dir[1024], name[256];
        rom_dir_path(dmeta, (uint32_t)dsz, parent, dir, sizeof dir);
        uint32_t nl = namelen > 255 ? 255 : namelen;
        memcpy(name, fmeta + o + 0x20, nl);
        name[nl] = 0;
        if (rom_nfiles == cap) { cap *= 2; rom_files = realloc(rom_files, sizeof(RomFile) * cap); }
        char *path = malloc(strlen(dir) + nl + 1);
        strcpy(path, dir);
        strcat(path, name);
        rom_files[rom_nfiles++] = (RomFile){rom_data_off + data, size, path, (uint32_t)o};
        o += 0x20 + ((namelen + 3) & ~3u);
    }
    // Mods: replacement files are served from the mods folder (LARECOMP_MODS), then from the game mods turned on in
    // the menu's Mods page (runtime/mods.c: mods/<mod>/romfs, Android). The file-meta table is patched (in a copy) to
    // point them at data appended past the end of the image; reads there go to host files.
    mods_scan();
    for (int i = 0; i < rom_nfiles; i++) {
        char host[1200];
        if (!mods_override(rom_files[i].path, host, sizeof host) && !mods_romfs_override(rom_files[i].path, host, sizeof host))
            continue;
        struct stat sb;
        if (stat(host, &sb)) continue;
        if (!ov_meta) {
            ov_meta = malloc(fsz);
            memcpy(ov_meta, fmeta, fsz);
            ov_meta_off = foff;
            ov_meta_size = fsz;
            struct stat rs;
            fstat(fd, &rs);
            ov_end = (rs.st_size + 0xF) & ~0xFull;
        }
        uint64_t rel = ov_end - rom_data_off, size = (uint64_t)sb.st_size;
        memcpy(ov_meta + rom_files[i].meta + 8, &rel, 8);
        memcpy(ov_meta + rom_files[i].meta + 0x10, &size, 8);
        if (ov_n == ov_cap) { ov_cap = ov_cap ? ov_cap * 2 : 256; ov = realloc(ov, sizeof *ov * ov_cap); }
        ov[ov_n].start = ov_end;
        ov[ov_n].size = size;
        ov[ov_n].path = strdup(host);
        ov[ov_n].fd = -1;
        ov[ov_n].mem = NULL;
        ov_n++;
        ov_end = (ov_end + size + 0xF) & ~0xFull;
    }
    if (ov_n) LOG("[fs] mods: %d romfs files replaced\n", ov_n);
    if (ov_meta) { patches[npatches++] = (Patch){ov_meta_off, ov_meta_size, ov_meta}; }
    add_new_files(fd, h, dmeta, dsz, fsz);
    qsort(rom_files, rom_nfiles, sizeof *rom_files, rom_cmp);
    free(dmeta);
    free(fmeta);
    LOG("[fs] romfs index: %d files\n", rom_nfiles);
}

// Virtual RomFS image read: original bytes, with the patched file table and appended replacement files.
static ssize_t romfs_read(int fd, uint8_t *dst, uint64_t size, uint64_t off) {
    if (!ov_n) return pread(fd, dst, size, (off_t)off);
    uint64_t done = 0;
    while (done < size) {
        uint64_t o = off + done, n = size - done;
        if (o >= ov[0].start) {  // appended region
            int i = 0;
            while (i < ov_n && !(o >= ov[i].start && o < ov[i].start + ((ov[i].size + 0xF) & ~0xFull))) i++;
            if (i == ov_n) { memset(dst + done, 0, n); done += n; break; }
            uint64_t in = o - ov[i].start, avail = ((ov[i].size + 0xF) & ~0xFull) - in;
            if (n > avail) n = avail;
            ssize_t r;
            if (ov[i].mem) {  // in-memory blob (the extended file table)
                r = in < ov[i].size ? (ssize_t)(n < ov[i].size - in ? n : ov[i].size - in) : 0;
                memcpy(dst + done, ov[i].mem + in, (size_t)r);
            } else {
                if (ov[i].fd < 0) ov[i].fd = open(ov[i].path, O_RDONLY);
                r = in < ov[i].size ? pread(ov[i].fd, dst + done, n < ov[i].size - in ? n : ov[i].size - in, (off_t)in) : 0;
            }
            if (r < 0) r = 0;
            if ((uint64_t)r < n) memset(dst + done + r, 0, n - r);
        } else {
            if (o + n > ov[0].start) n = ov[0].start - o;
            ssize_t r = pread(fd, dst + done, n, (off_t)o);
            if (r < 0) r = 0;
            if ((uint64_t)r < n) memset(dst + done + r, 0, n - r);
            // overlay the patched tables (file meta, and with added files: header, dir meta, file hash)
            for (int k = 0; k < npatches; k++) {
                uint64_t a = o > patches[k].off ? o : patches[k].off;
                uint64_t b = o + n < patches[k].off + patches[k].size ? o + n : patches[k].off + patches[k].size;
                if (a < b) memcpy(dst + done + (a - o), patches[k].data + (a - patches[k].off), b - a);
            }
        }
        done += n;
    }
    return (ssize_t)done;
}

// Path of the RomFS file containing image offset `off` (NULL if none).
const char *romfs_file_at(uint64_t off) {
    int lo = 0, hi = rom_nfiles - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (off < rom_files[mid].off) hi = mid - 1;
        else if (off >= rom_files[mid].off + rom_files[mid].size) lo = mid + 1;
        else return rom_files[mid].path;
    }
    return NULL;
}

// ------------------------------------------------------------------ IStorage (raw RomFS image)
typedef struct { int fd; uint64_t size; bool plain; } Storage;  // plain: another image than the game's (no mod overlay)

static void storage(Service *s, IpcReq *req, IpcResp *resp) {
    Storage *st = s->state;
    switch (req->cmd) {
    case 0: {  // Read(offset, size) -> B buffer
        uint64_t off = IN(uint64_t, 0), size = IN(uint64_t, 8);
        IpcBuf *b = out_buf(req, 0);
        if (!b) { resp->result = MAKE_RESULT(2, 6001); break; }
        if (size > b->size) size = b->size;
        plat_write_prepare(b->addr, size);  // (pread into pages write tracking made read-only: EFAULT)
        ssize_t n = st->plain ? pread(st->fd, (uint8_t *)(uintptr_t)b->addr, size, (off_t)off)
                              : romfs_read(st->fd, (uint8_t *)(uintptr_t)b->addr, size, off);
        if (n < 0) LOG("[fs] storage read failed at %llx: %s\n", off, strerror(errno));
        if (st->plain) break;
        {
            const char *path = romfs_file_at(off);
            static const char *last;
            if (path && path != last) {
                last = path;
                if (getenv("LARECOMP_LOG_FILES")) LOG("[fs] read %s\n", path);
            }
        }
        static uint64_t reads, bytes;
        reads++;
        bytes += size;
        if ((reads & (reads - 1)) == 0) LOG("[fs] romfs reads: %llu (%llu KB)\n", reads, bytes >> 10);
        break;
    }
    case 4: OUT(uint64_t, 0) = ov_n && !st->plain ? ov_end : st->size; resp_size(resp, 8); break;  // GetSize
    default: LOG("[fs] IStorage cmd %u (ok)\n", req->cmd);
    }
}

static Service *open_romfs_storage(void) {
    if (romfs_fd < 0) {
        char path[1024];
        snprintf(path, sizeof path, "%s.bin", g_romfs_dir);  // extracted/romfs.bin
        romfs_fd = open(path, O_RDONLY);
        if (romfs_fd < 0) FATAL("cannot open %s (set up the game files on the start screen)\n", path);
        char why[128];
        if (!romfs_image_ok(path, why, sizeof why))  // (else the game's SDK aborts on it with an svcBreak, nothing said)
            FATAL("The game data (romfs.bin) is incomplete or damaged: %s. Set up the game files again (Settings > Files > "
                  "Set up game files), or copy romfs.bin again whole.\n", why);
        rom_index(romfs_fd);
    }
    Storage *st = calloc(1, sizeof *st);
    st->fd = romfs_fd;
    struct stat sb;
    fstat(romfs_fd, &sb);
    st->size = sb.st_size;
    return service_new("IStorage(romfs)", storage, st);
}

// ------------------------------------------------------------------ the DLC (add-on content)
// The Expansion Pass's data: the RomFS of its first add-on content (01007EF00011F001, the Master Trials - the
// Champions' Ballad's own is only its unlock), one image for 1.6.0, 1.8.2 and 1.9.0 alike: <1.0.0's data root>/dlc/aoc.bin
// (LARECOMP_AOC_IMAGE=<path> for tests). The game mounts it as aoc: (nn::fs::MountAddOnContent ->
// OpenDataStorageByDataId) once aoc:u lists both packs (services/misc.c).
#define AOC_DATA_ID 0x01007EF00011F001ull
const char *fs_aoc_image(void) {
    static char path[1100];
    const char *e = getenv("LARECOMP_AOC_IMAGE");
    char root[1024];
    if (e && *e) snprintf(path, sizeof path, "%s", e);
    else if (rt_version_root(100, root, sizeof root)) snprintf(path, sizeof path, "%s/dlc/aoc.bin", root);
    else return NULL;
    struct stat sb;
    return !stat(path, &sb) && sb.st_size > 0 ? path : NULL;
}

static Service *open_aoc_storage(void) {
    static int fd = -1;
    const char *path = fs_aoc_image();
    if (fd < 0 && path) {
        char why[128];
        if (!romfs_image_ok(path, why, sizeof why)) {
            LOG("[fs] the DLC's data (%s) is incomplete or damaged: %s\n", path, why);
            return NULL;
        }
        fd = open(path, O_RDONLY);
        if (fd >= 0) LOG("[fs] the DLC's data: %s\n", path);
    }
    if (fd < 0) return NULL;
    Storage *st = calloc(1, sizeof *st);
    st->fd = fd;
    st->plain = true;
    struct stat sb;
    fstat(fd, &sb);
    st->size = sb.st_size;
    return service_new("IStorage(aoc)", storage, st);
}

// ------------------------------------------------------------------ host-directory filesystem
typedef struct { char root[1024]; } HostFs;
typedef struct { int fd; char path[1100]; } HostFile;
typedef struct { DIR *d; char path[1024]; int mode; } HostDir;

static const char *in_path(IpcReq *req, int i) {
    if (i < req->nx) return (const char *)(uintptr_t)req->x[i].addr;
    if (i < req->na) return (const char *)(uintptr_t)req->a[i].addr;
    return "";
}

static void host_path(HostFs *fs, const char *p, char *out, size_t n) {
    snprintf(out, n, "%s%s%s", fs->root, p[0] == '/' ? "" : "/", p);
}

static uint32_t errno_result(void) {
    if (errno == ENOENT || errno == ENOTDIR) return FS_PATH_NOT_FOUND;
    if (errno == EEXIST) return FS_PATH_EXISTS;
    if (errno == ENOTEMPTY) return FS_NOT_EMPTY;
    return MAKE_RESULT(2, 4000);
}

static void file(Service *s, IpcReq *req, IpcResp *resp) {
    HostFile *f = s->state;
    switch (req->cmd) {
    case 0: {  // Read(option, offset, size) -> B
        int64_t off = IN(int64_t, 8), size = IN(int64_t, 16);
        IpcBuf *b = out_buf(req, 0);
        if ((uint64_t)size > b->size) size = (int64_t)b->size;
        plat_write_prepare(b->addr, size);
        ssize_t n = pread(f->fd, (void *)(uintptr_t)b->addr, (size_t)size, off);
        if (strstr(f->path, "savedata")) {
            LOG("[save] read %s off %lld size %lld -> guest %llx\n", f->path, (long long)off, (long long)size, (unsigned long long)b->addr);
        }
        OUT(int64_t, 0) = n < 0 ? 0 : n;
        resp_size(resp, 8);
        break;
    }
    case 1: {  // Write(option, offset, size) <- A
        int64_t off = IN(int64_t, 8), size = IN(int64_t, 16);
        IpcBuf *a = in_buf(req, 0);
        if (strstr(f->path, "savedata"))
            LOG("[save] write %s off %lld size %lld <- guest %llx\n", f->path, (long long)off, (long long)size, (unsigned long long)a->addr);
        if (pwrite(f->fd, (void *)(uintptr_t)a->addr, (size_t)size, off) < 0) resp->result = errno_result();
        break;
    }
    case 2: fsync(f->fd); break;
    case 3: if (ftruncate(f->fd, IN(int64_t, 0))) resp->result = errno_result(); break;
    case 4: {
        struct stat sb;
        fstat(f->fd, &sb);
        OUT(int64_t, 0) = sb.st_size;
        resp_size(resp, 8);
        break;
    }
    default: LOG("[fs] IFile cmd %u (ok)\n", req->cmd);
    }
}

static void directory(Service *s, IpcReq *req, IpcResp *resp) {
    HostDir *d = s->state;
    switch (req->cmd) {
    case 0: {  // Read -> B buffer of 0x310-byte entries
        IpcBuf *b = out_buf(req, 0);
        uint8_t *out = (uint8_t *)(uintptr_t)b->addr;
        int64_t max = (int64_t)(b->size / 0x310), n = 0;
        struct dirent *e;
        while (n < max && (e = readdir(d->d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
#ifdef _WIN32  // no d_type
            char full[2048];
            struct stat es;
            snprintf(full, sizeof full, "%s/%s", d->path, e->d_name);
            bool isdir = !stat(full, &es) && S_ISDIR(es.st_mode);
#else
            bool isdir = e->d_type == DT_DIR;
#endif
            if ((isdir && !(d->mode & 1)) || (!isdir && !(d->mode & 2))) continue;
            uint8_t *ent = out + n * 0x310;
            memset(ent, 0, 0x310);
            strncpy((char *)ent, e->d_name, 0x300);
            ent[0x304] = isdir ? 0 : 1;
            if (!isdir) {
                char p[2048];
                struct stat sb;
                snprintf(p, sizeof p, "%s/%s", d->path, e->d_name);
                if (!stat(p, &sb)) *(int64_t *)(ent + 0x308) = sb.st_size;
            }
            n++;
        }
        OUT(int64_t, 0) = n;
        resp_size(resp, 8);
        break;
    }
    case 1: {  // GetEntryCount
        long pos = telldir(d->d);
        rewinddir(d->d);
        int64_t n = 0;
        struct dirent *e;
        while ((e = readdir(d->d)))
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
        seekdir(d->d, pos);
        OUT(int64_t, 0) = n;
        resp_size(resp, 8);
        break;
    }
    }
}

static int rm_rf(const char *p, bool keep_root) {
    DIR *d = opendir(p);
    if (!d) return unlink(p);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char c[2048];
        snprintf(c, sizeof c, "%s/%s", p, e->d_name);
        rm_rf(c, false);
    }
    closedir(d);
    return keep_root ? 0 : rmdir(p);
}

static void filesystem(Service *s, IpcReq *req, IpcResp *resp) {
    HostFs *fs = s->state;
    char p[2048], p2[2048];
    host_path(fs, in_path(req, 0), p, sizeof p);
    switch (req->cmd) {
    case 0: {  // CreateFile(option, size)
        int fd = open(p, O_CREAT | O_EXCL | O_RDWR, 0644);
        if (fd < 0) { resp->result = errno_result(); break; }
        if (ftruncate(fd, IN(int64_t, 8))) resp->result = errno_result();
        close(fd);
        break;
    }
    case 1: if (unlink(p)) resp->result = errno_result(); break;
    case 2: if (mkdir(p, 0755)) resp->result = errno_result(); break;
    case 3: if (rmdir(p)) resp->result = errno_result(); break;
    case 4: if (rm_rf(p, false)) resp->result = errno_result(); break;
    case 5: case 6:
        host_path(fs, in_path(req, 1), p2, sizeof p2);
        if (rename(p, p2)) resp->result = errno_result();
        break;
    case 7: {  // GetEntryType
        struct stat sb;
        if (stat(p, &sb)) { resp->result = FS_PATH_NOT_FOUND; break; }
        OUT(uint32_t, 0) = S_ISDIR(sb.st_mode) ? 0 : 1;
        resp_size(resp, 4);
        break;
    }
    case 8: {  // OpenFile(mode)
        uint32_t mode = IN(uint32_t, 0);
        int fd = open(p, (mode & 2) ? O_RDWR : O_RDONLY);
        if (fd < 0) { resp->result = errno_result(); break; }
        HostFile *f = calloc(1, sizeof *f);
        f->fd = fd;
        strlcpy(f->path, p, sizeof f->path);
        ipc_return_object(resp, service_new("IFile", file, f));
        break;
    }
    case 9: {  // OpenDirectory(mode)
        DIR *d = opendir(p);
        if (!d) { resp->result = errno_result(); break; }
        HostDir *hd = calloc(1, sizeof *hd);
        hd->d = d;
        hd->mode = (int)IN(uint32_t, 0);
        snprintf(hd->path, sizeof hd->path, "%s", p);
        ipc_return_object(resp, service_new("IDirectory", directory, hd));
        break;
    }
    case 10: break;  // Commit
    case 11: case 12: OUT(int64_t, 0) = 0x40000000; resp_size(resp, 8); break;
    case 13: rm_rf(p, true); break;
    case 14: memset(resp->data, 0, 0x20); resp_size(resp, 0x20); break;
    default: LOG("[fs] IFileSystem cmd %u (ok)\n", req->cmd);
    }
    // (failures logged: newer SDKs abort on a result they don't expect - v1.6.0's SDK 7.3.2 in its save thread)
    if (resp->result) LOG("[fs] IFileSystem cmd %u %s: result %#x\n", req->cmd, p, resp->result);
}

// A copy of folder src at dst (made by this process, so its files are this app's: on Android, files adb copied belong
// to the shell user, and the game couldn't open them for writing - result 2/4000)
static void copy_tree(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) return;
    mkdir(dst, 0755);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char a[2048], b[2048];
        snprintf(a, sizeof a, "%s/%s", src, e->d_name);
        snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
        struct stat sb;
        if (stat(a, &sb)) continue;
        if (S_ISDIR(sb.st_mode)) { copy_tree(a, b); continue; }
        int in = open(a, O_RDONLY), out = in < 0 ? -1 : open(b, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        char buf[65536];
        ssize_t n;
        while (out >= 0 && (n = read(in, buf, sizeof buf)) > 0) if (write(out, buf, (size_t)n) != n) break;
        if (in >= 0) close(in);
        if (out >= 0) close(out);
    }
    closedir(d);
}

static Service *open_host_fs(const char *sub) {
    HostFs *fs = calloc(1, sizeof *fs);
    if (getenv("LARECOMP_SAVE_DIR")) g_save_dir = getenv("LARECOMP_SAVE_DIR");  // test runs use a scratch copy
    snprintf(fs->root, sizeof fs->root, "%s/%s", g_save_dir, sub);
    {   // LARECOMP_SAVE_SEED=<save folder>: a save that doesn't exist yet starts as a copy of that one's (another game
        // version's data root starting from the player's saves - android/run_v160.sh)
        const char *seed = getenv("LARECOMP_SAVE_SEED");
        struct stat sb;
        if (seed && *seed && stat(fs->root, &sb)) {
            char from[1200];
            snprintf(from, sizeof from, "%s/%s", seed, sub);
            mkdir(g_save_dir, 0755);
            copy_tree(from, fs->root);
            LOG("[fs] save %s started as a copy of %s\n", fs->root, from);
        }
    }
    {   // LARECOMP_SAVE_REPLACE=<save folder>: this save becomes a copy of that one's, the one it was kept beside it as
        // <save>.before-replace[-n] (a test save made the player's own: adb can't write the app's folders on Android)
        // (once a run: the game opens its save again and again)
        static bool replaced;
        const char *from_dir = getenv("LARECOMP_SAVE_REPLACE");
        char from[1200], keep[1300];
        struct stat sb;
        snprintf(from, sizeof from, "%s/%s", from_dir ? from_dir : "", sub);
        if (from_dir && *from_dir && !replaced && !stat(from, &sb) && S_ISDIR(sb.st_mode)) {
            replaced = true;
            snprintf(keep, sizeof keep, "%s.before-replace", fs->root);
            for (int n = 2; !stat(keep, &sb) && n < 100; n++) snprintf(keep, sizeof keep, "%s.before-replace-%d", fs->root, n);
            if (stat(fs->root, &sb) || !rename(fs->root, keep)) {
                copy_tree(from, fs->root);
                LOG("[fs] save %s replaced by a copy of %s (the one before: %s)\n", fs->root, from, keep);
            } else {
                LOG("[fs] save %s not replaced: can't move it aside (errno %d)\n", fs->root, errno);
            }
        }
    }
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "%s", fs->root);
    for (char *q = cmd + 1; *q; q++)
        if (*q == '/') { *q = 0; mkdir(cmd, 0755); *q = '/'; }
    mkdir(cmd, 0755);
    LOG("[fs] mounted host directory %s\n", fs->root);
    return service_new("IFileSystem", filesystem, fs);
}

// ------------------------------------------------------------------ fsp-srv
SERVICE("fsp-srv", fsp_srv) {
    (void)s;
    switch (req->cmd) {
    case 1: break;  // SetCurrentProcess
    case 18: ipc_return_object(resp, open_host_fs("sdcard")); break;
    case 22: break;  // CreateSaveDataFileSystem
    case 51: case 53: {  // OpenSaveDataFileSystem(space, attribute)
        uint8_t type = IN(uint8_t, 0x8 + 0x18);  // attribute.type
        char sub[64];
        snprintf(sub, sizeof sub, "save_%u", type);
        ipc_return_object(resp, open_host_fs(sub));
        break;
    }
    case 200: ipc_return_object(resp, open_romfs_storage()); break;  // OpenDataStorageByCurrentProcess
    case 202: {  // OpenDataStorageByDataId(storage id, data id): the DLC's data (system data archives: none)
        // (the Champions' Ballad's own data, 01007EF00011F002, is System/AocVersion.txt alone - the same file the Master
        // Trials' image holds: that image answers for both)
        uint64_t id = IN(uint64_t, 8);
        Service *aoc = id == AOC_DATA_ID || id == AOC_DATA_ID + 1 ? open_aoc_storage() : NULL;
        if (aoc) { ipc_return_object(resp, aoc); break; }
        LOG("[fs] OpenDataStorageByDataId %llx (not available)\n", IN(uint64_t, 8));
        resp->result = MAKE_RESULT(2, 1001);
        break;
    }
    case 203:  // OpenPatchDataStorageByCurrentProcess: no update installed (BOTW asks at boot) - TargetNotFound
        resp->result = MAKE_RESULT(2, 1002);
        break;
    case 1005: OUT(uint32_t, 0) = 0; resp_size(resp, 4); break;  // GetGlobalAccessLogMode
    case 1003: case 1004: case 1006: break;
    case 1011: memset(resp->data, 0, 0x10); resp_size(resp, 0x10); break;
    default: LOG("[fs] fsp-srv cmd %u (ok)\n", req->cmd);
    }
}
