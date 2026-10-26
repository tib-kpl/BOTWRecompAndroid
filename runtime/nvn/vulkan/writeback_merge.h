#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Compare large unchanged spans in one call. Changed spans still use the original
// 256-byte ownership checks: neither newer CPU uploads nor intervening CPU writes
// may be overwritten by an older GPU result.
static inline void writeback_merge(uint8_t *guest, uint8_t *shadow, const uint8_t *gpu,
                                   const uint64_t *uploaded, uint64_t completed, size_t len, bool grouped) {
    for (size_t page = 0; page < len;) {
        size_t end = len - page > 4096 ? page + 4096 : len;
        if (grouped && end - page > 256 && !memcmp(gpu + page, shadow + page, end - page)) {
            page = end;
            continue;
        }
        for (size_t b = page; b < end; b += 256) {
            size_t n = end - b < 256 ? end - b : 256;
            if (uploaded[b / 256] > completed) continue;
            if (!memcmp(gpu + b, shadow + b, n) || memcmp(guest + b, shadow + b, n)) continue;
            memcpy(guest + b, gpu + b, n);
            memcpy(shadow + b, gpu + b, n);
        }
        page = end;
    }
}
