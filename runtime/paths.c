// Data root and shipped resources (runtime/paths.h).
#include <SDL3/SDL.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "gamelib.h"
#include "paths.h"
#include "platform.h"

static char g_root[1024], g_start[1024], g_exe_dir[1024];

#ifdef _WIN32
#define SEP "\\"
static bool is_link(const char *p) {  // symlink or junction
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
}
static bool resolve(const char *p, char *out, size_t n) { return _fullpath(out, p, n) != NULL; }
#define path_eq(a, b) (_stricmp(a, b) == 0)
#else
#define SEP "/"
static bool is_link(const char *p) {
    struct stat st;
    return lstat(p, &st) == 0 && S_ISLNK(st.st_mode);
}
static bool resolve(const char *p, char *out, size_t n) {
    char buf[PATH_MAX];
    if (!realpath(p, buf)) return false;
    snprintf(out, n, "%s", buf);
    return true;
}
#define path_eq(a, b) (strcmp(a, b) == 0)
#endif

static bool is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void mkdirs(const char *path) {
    char p[1024];
    snprintf(p, sizeof p, "%s", path);
    for (char *c = p + 1; *c; c++)
        if (*c == '/' || *c == '\\') { char s = *c; *c = 0; mkdir(p, 0755); *c = s; }
    mkdir(p, 0755);
}

static bool writable(const char *dir) {
    char t[1100];
    snprintf(t, sizeof t, "%s" SEP ".write_test", dir);
    FILE *f = fopen(t, "wb");
    if (!f) return false;
    fclose(f);
    remove(t);
    return true;
}

static void fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "BOTWRecompAndroid", msg, NULL);
    exit(1);
}

static bool g_dev;  // the data root is the starting folder because it has extracted/ (a source checkout)
bool rt_dev_mode(void) { return g_dev; }

// ---- game versions (paths.h): 1.0.0 in the data root, 1.6.0 / 1.8.2 / 1.9.0 in its v160 / v182 / v190
static char g_main[1024];   // 1.0.0's data root, which the others' folders are in
static int g_version = 100;  // whose folder the data root is

#define VERSION_FILE "game_version.txt"

// The one the start screen chose (game_version.txt in the main root; none = 1.0.0)
static int selected_version(const char *main_root) {
    char p[1100];
    snprintf(p, sizeof p, "%s" SEP VERSION_FILE, main_root);
    FILE *f = fopen(p, "rb");
    int v = 100;
    if (f) {
        if (fscanf(f, "%d", &v) != 1) v = 100;
        fclose(f);
    }
    return v == 160 || v == 182 || v == 190 ? v : 100;
}

static bool copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in) return false;
    FILE *out = fopen(to, "wb");
    bool ok = out != NULL;
    char buf[1 << 16];
    for (size_t n; ok && (n = fread(buf, 1, sizeof buf, in)) > 0;) ok = fwrite(buf, 1, n, out) == n;
    fclose(in);
    if (out) ok &= fclose(out) == 0;
    if (!ok) remove(to);
    return ok;
}

// The data root is a version's own folder (cwd): its settings start as 1.0.0's (once), its saves as 1.0.0's (fs.c copies
// them when the game first opens one: LARECOMP_SAVE_SEED)
static void version_seed(void) {
    char from[1100];
    snprintf(from, sizeof from, "%s" SEP "botwrecompandroid.ini", g_main);
    if (!exists("botwrecompandroid.ini") && copy_file(from, "botwrecompandroid.ini"))
        fprintf(stderr, "[paths] settings: a copy of 1.0.0's\n");
    if (getenv("LARECOMP_SAVE_SEED")) return;
    snprintf(from, sizeof from, "%s" SEP "save", g_main);
#ifdef _WIN32
    _putenv_s("LARECOMP_SAVE_SEED", from);
#else
    setenv("LARECOMP_SAVE_SEED", from, 1);
#endif
}

int rt_game_version(void) { return g_version; }

bool rt_version_root(int v, char *out, size_t n) {
    if (v == 100) return (size_t)snprintf(out, n, "%s", g_main) < n;
    if (v == 160 || v == 182 || v == 190) return (size_t)snprintf(out, n, "%s" SEP "v%d", g_main, v) < n;
    return false;
}

bool rt_select_game_version(int v) {
    char p[1100];
    snprintf(p, sizeof p, "%s" SEP VERSION_FILE, g_main);
    FILE *f = fopen(p, "wb");
    if (!f) return false;
    fprintf(f, "%d\n", v);
    return fclose(f) == 0;
}

int rt_game_dir_version(const char *game_dir) {
    char got[1100];
    if (!game_dir || !*game_dir) return 0;
    if (!resolve(game_dir, got, sizeof got)) {  // (not there yet: as given, made absolute)
        bool abs = game_dir[0] == '/' || game_dir[0] == '\\' || game_dir[1] == ':';
        if (abs) snprintf(got, sizeof got, "%s", game_dir);
        else snprintf(got, sizeof got, "%s" SEP "%s", g_root, game_dir);
    }
    static const int versions[] = {100, 160, 182, 190};
    for (int i = 0; i < (int)(sizeof versions / sizeof *versions); i++) {
        char r[1024], want[1100];
        if (!rt_version_root(versions[i], r, sizeof r)) continue;
        snprintf(want, sizeof want, "%s" SEP "extracted", r);
        if (path_eq(want, got)) return versions[i];
    }
    return 0;
}

bool rt_version_game_lib(int v, char *out, size_t n) {
    if (v != 160 && v != 182 && v != 190) return false;
#ifdef __ANDROID__
    const char *in = SDL_GetAndroidInternalStoragePath();
    return in && (size_t)snprintf(out, n, "%s/v%d/%s", in, v, GAMELIB_NAME) < n;
#else
    char r[1024];
    return rt_version_root(v, r, sizeof r) && (size_t)snprintf(out, n, "%s" SEP "extracted" SEP "%s", r, GAMELIB_NAME) < n;
#endif
}

void rt_paths_init(void) {
    if (!getcwd(g_start, sizeof g_start)) g_start[0] = 0;
    if (plat_exe_path(g_exe_dir, sizeof g_exe_dir)) {
        char *s = strrchr(g_exe_dir, '/'), *b = strrchr(g_exe_dir, '\\');
        if (b > s) s = b;
        if (s) *s = 0;
    }
    char root[1024];
    const char *env = getenv("LARECOMP_DATA_ROOT");
    if (env && *env) snprintf(root, sizeof root, "%s", env);
    else if (is_dir("extracted")) snprintf(root, sizeof root, "%s", g_start), g_dev = true;  // dev / portable: as before
    else {
#ifdef _WIN32
        snprintf(root, sizeof root, "%s", g_exe_dir);
#elif defined(__ANDROID__)
        // the app's external files folder (Android/data/<package>/files): reachable over adb and from file managers
        const char *ext = SDL_GetAndroidExternalStoragePath();
        snprintf(root, sizeof root, "%s", ext ? ext : SDL_GetAndroidInternalStoragePath());
#elif defined(__linux__)
        // (the AppImage is read-only): $XDG_DATA_HOME/botwrecompandroid, ~/.local/share/botwrecompandroid by default
        const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
        if (xdg && *xdg == '/') snprintf(root, sizeof root, "%s/botwrecompandroid", xdg);
        else snprintf(root, sizeof root, "%s/.local/share/botwrecompandroid", home && *home ? home : "/tmp");
#else
        const char *home = getenv("HOME");
        snprintf(root, sizeof root, "%s/Library/Application Support/" RT_BUNDLE_ID, home && *home ? home : "/tmp");
#endif
    }
    if (!(env && *env)) {  // the game version the start screen chose: 1.6.0 / 1.8.2 / 1.9.0 in their own folder
        int v = selected_version(root);
        size_t l = strlen(root);
        if (v != 100) snprintf(root + l, sizeof root - l, SEP "v%d", v);
    }
    mkdirs(root);
    char msg[1400];
    if (!is_dir(root) || !writable(root) || chdir(root) != 0) {
#ifdef _WIN32
        snprintf(msg, sizeof msg, "BOTWRecompAndroid can't write to its folder:\n%s\n\nIt keeps game files, saves and settings "
                 "next to the program. Move the \"BOTWRecompAndroid\" folder somewhere you can write to (e.g. Documents or "
                 "Desktop, not Program Files) and start it again.", root);
#else
        snprintf(msg, sizeof msg, "BOTWRecompAndroid can't write to its data folder:\n%s", root);
#endif
        fail(msg);
    }
    if (!getcwd(g_root, sizeof g_root)) snprintf(g_root, sizeof g_root, "%s", root);
    // a version's own folder by its name (v160 / v182 / v190, chosen or LARECOMP_DATA_ROOT's), in 1.0.0's root
    const char *base = strrchr(g_root, '/'), *bs = strrchr(g_root, '\\');
    if (bs > base) base = bs;
    snprintf(g_main, sizeof g_main, "%s", g_root);
    g_version = 100;
    if (base && (!strcmp(base + 1, "v160") || !strcmp(base + 1, "v182") || !strcmp(base + 1, "v190"))) {
        g_version = atoi(base + 2);
        snprintf(g_main, sizeof g_main, "%.*s", (int)(base - g_root), g_root);
        version_seed();
    }
    // (tests: screenshots and debug frames go to build/, which a new data root doesn't have yet)
    if (getenv("LARECOMP_DEBUG_FRAME") || getenv("LARECOMP_SHOT_AT") || getenv("LARECOMP_SCREENSHOT_EVERY") ||
        getenv("LARECOMP_SAVE_MSL") || getenv("LARECOMP_LAUNCHER_SHOT"))
        mkdirs("build" SEP "frames");
    fprintf(stderr, "[paths] data: %s (game version %d)\n", g_root, g_version);
}

const char *rt_data_root(void) { return g_root; }

bool rt_resource(const char *name, const char *dev_rel, char *out, size_t n) {
    char c[4][1100] = {{0}};
    if (name) {
        snprintf(c[0], sizeof c[0], "%s" SEP "%s", g_exe_dir, name);
        snprintf(c[1], sizeof c[1], "%s" SEP ".." SEP "Resources" SEP "%s", g_exe_dir, name);  // .app/Contents/Resources
    }
    if (dev_rel) {
        snprintf(c[2], sizeof c[2], "%s" SEP "%s", g_start, dev_rel);
        snprintf(c[3], sizeof c[3], "%s" SEP ".." SEP "%s", g_exe_dir, dev_rel);  // build/botwrecompandroid -> repo root
    }
    for (int i = 0; i < 4; i++)
        if (c[i][0] && exists(c[i])) return (size_t)snprintf(out, n, "%s", c[i]) < n;  // (as is: a venv python must stay a link)
    return false;
}

// ---- game files
// What the setup (runtime/ui/launcher.mm) creates in extracted/, plus what older versions' extraction and manual
// extractions left there (never read at run time).
static const char *const GAME_FILES[] = {"exefs", "romfs", "romfs.bin", GAMELIB_NAME, GAMELIB_NAME ".id", ".gamebuild",
                                         ".extract", "extract.log", "xci_secure", "nso_raw", "update_pfs", ".DS_Store"};

// game_dir must be <data root>/extracted or a game version's (set up from another version's start screen), a real
// folder (a dev worktree's symlinked extracted/ is refused)
static bool game_dir_ok(const char *game_dir) {
    char want[1100], got[1100];
    snprintf(want, sizeof want, "%s" SEP "extracted", g_root);
    if (!g_root[0] || is_link(game_dir) || !resolve(game_dir, got, sizeof got)) return false;
    return path_eq(want, got) || rt_game_dir_version(game_dir) != 0;
}

bool rt_remove_game_file(const char *game_dir, const char *name) {
    bool known = false;
    for (size_t i = 0; i < sizeof GAME_FILES / sizeof *GAME_FILES; i++) known |= !strcmp(name, GAME_FILES[i]);
    if (!known || !game_dir_ok(game_dir)) {
        fprintf(stderr, "[paths] refusing to delete %s/%s (not app-created game files in the data root)\n", game_dir, name);
        return false;
    }
    char p[1100];
    snprintf(p, sizeof p, "%s" SEP "%s", game_dir, name);
    if (!exists(p) && !is_link(p)) return true;
    if (is_link(p)) {
        fprintf(stderr, "[paths] refusing to delete %s (a link)\n", p);
        return false;
    }
    return is_dir(p) ? plat_remove_tree(p) : remove(p) == 0;  // (plat_remove_tree doesn't follow links inside)
}

bool rt_remove_game_files(const char *game_dir) {
    if (!game_dir_ok(game_dir)) {
        fprintf(stderr, "[paths] refusing to delete game files in %s (not <data root>/extracted)\n", game_dir);
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < sizeof GAME_FILES / sizeof *GAME_FILES; i++) ok &= rt_remove_game_file(game_dir, GAME_FILES[i]);
    rmdir(game_dir);  // only if empty: anything else in there stays
    return ok;
}

static unsigned long long tree_size(const char *p) {
    struct stat st;
    if (is_link(p) || stat(p, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return (unsigned long long)st.st_size;
    unsigned long long n = 0;
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char c[2048];
            snprintf(c, sizeof c, "%s" SEP "%s", p, e->d_name);
            n += tree_size(c);
        }
        closedir(d);
    }
    return n;
}

unsigned long long rt_game_files_size(const char *game_dir) {
    unsigned long long n = 0;
    char p[1100];
    for (size_t i = 0; i < sizeof GAME_FILES / sizeof *GAME_FILES; i++) {
        snprintf(p, sizeof p, "%s" SEP "%s", game_dir, GAME_FILES[i]);
        n += tree_size(p);
    }
    return n;
}

// Window / Dock icon: icon.png shipped next to the exe or in the .app (none for a plain source build).
void rt_window_icon(SDL_Window *w) {
    char p[1100];
    if (!rt_resource("icon.png", NULL, p, sizeof p)) return;
    SDL_Surface *s = SDL_LoadPNG(p);
    if (s) SDL_SetWindowIcon(w, s), SDL_DestroySurface(s);
}
