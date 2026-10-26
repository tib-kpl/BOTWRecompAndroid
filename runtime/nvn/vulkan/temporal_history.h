#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t key, frame, time_ns;
    uint32_t width, height;
    unsigned next;
    bool valid;
} TemporalHistory;

// Consecutive frames only. Reset after a resize, option change, game reset, suspended app or disabled filter.
static bool temporal_history_step(TemporalHistory *h, uint32_t w, uint32_t height, uint64_t key,
                                  uint64_t frame, uint64_t now_ns, bool enabled, unsigned *write) {
    bool use = enabled && h->valid && h->width == w && h->height == height && h->key == key &&
               frame == h->frame + 1 && now_ns >= h->time_ns && now_ns - h->time_ns < 200000000ull;
    *write = h->next;
    h->next ^= 1;
    h->width = w; h->height = height; h->key = key; h->frame = frame; h->time_ns = now_ns;
    h->valid = enabled;
    return use;
}
