#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/writeback_queue.h"

int main(void) {
    GcBack q[16]; int n = 0;
    uint8_t gpu[512], shadow[512], other[512]; uint64_t seq[2];
    GcBack base = {gpu, 0x1000, 256, 5, shadow, seq};
    assert(writeback_queue_add(q, &n, 16, base, true) == 1);
    for (int i = 0; i < 1000; i++) assert(writeback_queue_add(q, &n, 16, base, true) == 2);
    assert(n == 1);
    GcBack variants[] = {
        {gpu, 0x1000, 256, 6, shadow, seq}, // Different submission.
        {gpu + 256, 0x1000, 256, 5, shadow, seq}, // Different GPU allocation/offset.
        {gpu, 0x1100, 256, 5, shadow, seq}, // Different guest destination.
        {gpu, 0x1000, 512, 5, shadow, seq}, // Different extent.
        {gpu, 0x1000, 256, 5, other, seq}, // Resident merge replaced the shadow.
        {gpu, 0x1000, 256, 5, shadow, seq + 1}, // Different ownership metadata.
        {gpu, 0x1000, 256, 5, NULL, NULL}, // Unshadowed copies preserve ordering.
    };
    for (size_t i = 0; i < sizeof variants / sizeof variants[0]; i++)
        assert(writeback_queue_add(q, &n, 16, variants[i], true) == 1);
    assert(writeback_queue_add(q, &n, 16, variants[6], true) == 1);
    assert(writeback_queue_add(q, &n, 16, base, false) == 1);
    assert(n == 10);
    // A duplicate still succeeds when full; a distinct request cannot overwrite existing entries.
    assert(writeback_queue_add(q, &n, n, base, true) == 2);
    GcBack next = base; next.seq++;
    assert(writeback_queue_add(q, &n, n, next, false) == 0);
    assert(n == 10 && q[0].seq == 5 && q[1].seq == 6);
    puts("Only identical resident writebacks from the same submission coalesce");
}
