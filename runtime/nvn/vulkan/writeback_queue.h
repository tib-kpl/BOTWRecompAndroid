#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *src;
    uint64_t addr, len, seq;
    uint8_t *shadow;
    const uint64_t *bseq;
} GcBack;

// Arena writebacks observe the buffer at submission completion, not at each draw.
// An identical resident range registered twice for the SAME submission therefore
// needs one ownership merge. Different submissions, allocations, shadows and
// unshadowed snapshots keep their original ordering and remain separate.
static inline int writeback_queue_add(GcBack *queue, int *count, int capacity, GcBack item, bool deduplicate) {
    if (deduplicate && item.shadow) {
        for (int i = *count; i-- > 0;) {
            GcBack *e = &queue[i];
            if (e->seq == item.seq && e->src == item.src && e->addr == item.addr && e->len == item.len &&
                e->shadow == item.shadow && e->bseq == item.bseq) return 2;
        }
    }
    if (*count >= capacity) return 0;
    queue[(*count)++] = item;
    return 1;
}
