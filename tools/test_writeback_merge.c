#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../runtime/nvn/vulkan/writeback_merge.h"

static uint32_t rng = 19;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

// Independent reference: the original per-block retirement rules.
static void reference(uint8_t *g, uint8_t *s, const uint8_t *v, const uint64_t *seq, size_t len) {
    for (size_t b = 0; b < len; b += 256) {
        size_t n = len - b < 256 ? len - b : 256;
        if (seq[b / 256] > 7) continue;
        if (!memcmp(v + b, s + b, n) || memcmp(g + b, s + b, n)) continue;
        memcpy(g + b, v + b, n);
        memcpy(s + b, v + b, n);
    }
}

int main(void) {
    enum { N = 17013 };
    uint8_t g[N], s[N], v[N], expected_g[N], expected_s[N];
    uint64_t seq[(N + 255) / 256];
    const size_t sizes[] = {0, 1, 255, 256, 257, 4095, 4096, 4097, 8192, N};
    for (unsigned trial = 0; trial < 1000; trial++) {
        size_t len = sizes[trial % (sizeof sizes / sizeof sizes[0])];
        for (size_t b = 0; b < N; b++) g[b] = s[b] = v[b] = (uint8_t)next();
        for (size_t b = 0; b < len; b += 256) {
            seq[b / 256] = next() % 12;
            unsigned state = next() % 4;
            size_t n = len - b < 256 ? len - b : 256;
            if (state & 1) v[b + next() % n] ^= 123; // GPU changed.
            if (state & 2) g[b + next() % n] ^= 67;  // CPU changed: must win.
        }
        memcpy(expected_g, g, N); memcpy(expected_s, s, N);
        reference(expected_g, expected_s, v, seq, len);
        writeback_merge(g, s, v, seq, 7, len, true);
        assert(!memcmp(g, expected_g, N)); assert(!memcmp(s, expected_s, N));
    }
    puts("GPU writeback matches per-block ownership rules, including partial tails and newer CPU writes");
}
