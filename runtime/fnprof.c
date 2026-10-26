// Function-entry profiler. LARECOMP_FNPROF=<from>:<to> counts guest function entries (every generated
// function calls FN_ENTER) between those presented frames, then writes build/fnprof.txt (addr count, most
// called first). Diffing two runs shows which game functions a change brings in; counts also show hot code, and
// play sessions' counts make the hot/cold game code build's lists (tools/hot_functions.py, runtime/gamebuild.c).
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "hle.h"
#include "platform.h"
#include "rt.h"
#include "kernel.h"

// A bounded diagnostic for 1.9.0's terrain foot adjustment. Enabled only inside an explicit FNPROF window;
// never changes the ray result, character position, or height tiles. Ordinary profiling leaves it off.
static int foot_trace;
static _Atomic unsigned foot_hits, height_hits;
static _Thread_local unsigned foot_queries_left;
static bool foot_pointer(uint64_t p) { return p >= GUEST_LO && p < GUEST_HI - 0x1000; }
static void foot_trace_hit(uint64_t a) {
    if (g_game_version != 190) return;
    bool foot = a == MAIN_BASE + 0x11d0e34, height = a == MAIN_BASE + 0x1448fec;
    if (!foot && !height) return;
    if (height && !foot_queries_left) return;
    Cpu *c = kernel_current_cpu();
    if (!c || !foot_pointer(c->x[0]) || !foot_pointer(c->x[1])) return;
    unsigned n = atomic_fetch_add_explicit(foot ? &foot_hits : &height_hits, 1, memory_order_relaxed);
    if (n >= 32) return;
    if (foot) {
        foot_queries_left = foot_pointer(c->x[2]) && (LD32(c->x[2] + 0x80) & 0x80000000u) ? 3 : 0;
        float ray, xyz[3], limit;
        memcpy(&ray, &c->v[0], 4);
        memcpy(xyz, (void *)(uintptr_t)c->x[1], sizeof xyz);
        memcpy(&limit, (void *)(uintptr_t)(c->x[0] + 0x280), 4);
        LOG("[foot190] actor %llx ray %.6f xyz %.6f %.6f %.6f limit %.6f\n",
            (unsigned long long)c->x[0], ray, xyz[0], xyz[1], xyz[2], limit);
    } else {
        foot_queries_left--;
        uint64_t root = LD64(c->x[0]);
        uint32_t bank = LD32(c->x[0] + 8), level = (uint32_t)c->x[2];
        if (!foot_pointer(root) || bank > 16 || level > 7) return;
        uint64_t tile = LD64(root + bank * 0x88 + level * 8 + 0xc0);
        unsigned pitch = LD16(root + level * 12 + 0x18), side = LD16(root + level * 12 + 0x1a);
        unsigned pixel = LD32(root + 0x944);
        if (!foot_pointer(tile) || !side || side > 512 || !pitch || pitch > 4096 || pixel != 2) return;
        if ((uint64_t)(side - 1) * pitch + (uint64_t)side * pixel > GUEST_HI - tile) return;
        uint32_t x = LD32(c->x[1]), y = LD32(c->x[1] + 4);
        unsigned x0 = x < side ? x : side - 1, y0 = y < side ? y : side - 1;
        unsigned x1 = x < side - 1 ? x + 1 : side - 1, y1 = y < side - 1 ? y + 1 : side - 1;
        uint16_t v[4] = {LD16(tile+y0*pitch+x0*pixel), LD16(tile+y0*pitch+x1*pixel),
                         LD16(tile+y1*pitch+x0*pixel), LD16(tile+y1*pitch+x1*pixel)};
        float f[2]; memcpy(f, (void *)(uintptr_t)(c->x[1]+8), sizeof f);
        float h0 = v[0] + ((float)v[1]-v[0])*f[0], h1 = v[2] + ((float)v[3]-v[2])*f[0];
        LOG("[height190] tile %llx level %u bank %u xy %u %u fraction %.6f %.6f values %u %u %u %u height %.6f\n",
            (unsigned long long)tile, level, bank, x, y, f[0], f[1], v[0], v[1], v[2], v[3],
            (h0+(h1-h0)*f[1])*(800.0f/65535.0f));
    }
}

volatile int g_fnprof;

#define NSLOT (1 << 18)
static struct { _Atomic uint64_t addr; _Atomic uint64_t count; } slots[NSLOT];

// LARECOMP_FNPROF_BT=<hex addr>[,...]: also print the host backtrace (guest functions are f_<addr>) the first time
// each listed function is entered inside the window - who calls it
static uint64_t bt_addrs[256];
static int bt_n = -1;
static _Atomic uint8_t bt_done[256];

uint64_t g_bt_first_x0;

static void bt_check(uint64_t a, uint64_t lr, uint64_t x0) {
    if (bt_n < 0) {
        bt_n = 0;
        const char *e = getenv("LARECOMP_FNPROF_BT");
        for (const char *p = e; p && *p && bt_n < 256;) {
            bt_addrs[bt_n++] = strtoull(p, (char **)&p, 16);
            while (*p == ',' || *p == ' ') p++;
        }
    }
    for (int i = 0; i < bt_n; i++)
        if (bt_addrs[i] == a && !atomic_exchange(&bt_done[i], 1)) {
            if (i == 0) g_bt_first_x0 = x0;
            uint64_t vt = x0 >= GUEST_LO && x0 < GUEST_LO + 0x800000000ull ? *(uint64_t *)(uintptr_t)x0 : 0;
            uint64_t v100 = vt >= GUEST_LO && vt < GUEST_LO + 0x800000000ull ? *(uint64_t *)(uintptr_t)(vt + 0x100) : 0;
            uint64_t p8 = x0 >= GUEST_LO && x0 < GUEST_LO + 0x800000000ull ? *(uint64_t *)(uintptr_t)(x0 + 8) : 0;
            fprintf(stderr, "[fnprof-bt] %llx entered from guest lr %llx x0 %llx (vtable %llx, +0x100 %llx, [x0+8] %llx):\n",
                    (unsigned long long)a, (unsigned long long)lr, (unsigned long long)x0, (unsigned long long)vt,
                    (unsigned long long)v100, (unsigned long long)p8);
            plat_print_backtrace();  // (execinfo is macOS / glibc only)
        }
}

// (g_fnprof bit 0: profiling; bit 1: a thread of core 0 was preempted - runtime/kernel.c)
void fnprof_hit(uint64_t a, uint64_t lr, uint64_t x0) {
#ifdef __ANDROID__
    if (g_fnprof & 2) { extern void kernel_preempt_point(void); kernel_preempt_point(); }
#endif
    if (!(g_fnprof & 1)) return;
    if (foot_trace) foot_trace_hit(a);
    if (bt_n) bt_check(a, lr, x0);
    uint32_t h = (uint32_t)((a >> 2) * 0x9E3779B1u) & (NSLOT - 1);
    for (int n = 0; n < NSLOT; n++, h = (h + 1) & (NSLOT - 1)) {
        uint64_t cur = atomic_load_explicit(&slots[h].addr, memory_order_relaxed);
        if (cur == a || (cur == 0 && atomic_compare_exchange_strong(&slots[h].addr, &cur, a)) || cur == a) {
            atomic_fetch_add_explicit(&slots[h].count, 1, memory_order_relaxed);
            return;
        }
    }
}

static int cmp(const void *x, const void *y) {
    uint64_t a = ((const uint64_t *)x)[1], b = ((const uint64_t *)y)[1];
    return a < b ? 1 : a > b ? -1 : 0;
}

static void fnprof_write(long from, long to) {
    uint64_t (*rows)[2] = malloc(sizeof(uint64_t[2]) * NSLOT);
    int n = 0;
    for (int i = 0; i < NSLOT; i++)
        if (slots[i].addr) { rows[n][0] = slots[i].addr; rows[n][1] = slots[i].count; n++; }
    qsort(rows, n, sizeof rows[0], cmp);
    const char *out = getenv("LARECOMP_FNPROF_OUT") ? getenv("LARECOMP_FNPROF_OUT") : "build/fnprof.txt";  // (parallel runs)
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", out);  // (then renamed: a session stopped mid-write keeps the last whole file)
    FILE *f = fopen(tmp, "w");
    // (the game version: a hot list is per version, tools/hot_functions.py; the guest window moves on Android: the
    // generated code's names are main at 0x7000000000)
    if (f) fprintf(f, "# game_version %d\n", g_game_version);
#ifdef __ANDROID__
    if (f) fprintf(f, "# main_base %llx\n", (unsigned long long)MAIN_BASE);
#endif
    for (int i = 0; f && i < n; i++) fprintf(f, "%llx %llu\n", (unsigned long long)rows[i][0], (unsigned long long)rows[i][1]);
    if (f && fclose(f) == 0) rename(tmp, out);
    free(rows);
    LOG("[fnprof] %d functions, written %s (frames %ld-%ld)\n", n, out, from, to);
}

void fnprof_frame(uint64_t frame) {
    static long from = -2, to, every;
    if (from == -2) {
        from = -1;
        const char *e = getenv("LARECOMP_FNPROF");
        foot_trace = getenv("LARECOMP_FOOT_TRACE") != NULL;
        if (e && sscanf(e, "%ld:%ld", &from, &to) != 2) from = -1;
        // LARECOMP_FNPROF_EVERY=<frames>: also rewrite the file that often while counting - a long play session
        // (the hot functions list, tools/hot_functions.py) has its counts on disk whenever it stops
        e = getenv("LARECOMP_FNPROF_EVERY");
        every = e ? atol(e) : 0;
    }
    if (from < 0) return;
    if ((long)frame == from) { __atomic_fetch_or(&g_fnprof, 1, __ATOMIC_SEQ_CST); LOG("[fnprof] on at frame %llu\n", (unsigned long long)frame); }
    if (every > 0 && (long)frame > from && (long)frame < to && ((long)frame - from) % every == 0) fnprof_write(from, (long)frame);
    if ((long)frame != to) return;
    __atomic_fetch_and(&g_fnprof, ~1, __ATOMIC_SEQ_CST);
    fnprof_write(from, to);
    from = -1;
}

// LARECOMP_FNPROF=0:<to>: profile from process start (threads the game starts before the first frame)
__attribute__((constructor)) static void fnprof_early(void) {
    const char *e = getenv("LARECOMP_FNPROF");
    if (e && e[0] == '0' && e[1] == ':') __atomic_fetch_or(&g_fnprof, 1, __ATOMIC_SEQ_CST);
}
