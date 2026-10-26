// Exercise the production helper with immutable mock submission allocations.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t addr, size; } Range;
typedef struct { uint64_t buffer, offset, range; } VkDescriptorBufferInfo;
typedef struct { uint64_t offset; void *ptr; } VkSpan;
enum { DEDUPE_DRV_V, DEDUPE_DRV_F, DEDUPE_DRV_C };
static uint8_t arena[1 << 20];
static uint64_t seq = 1, top;
static unsigned copies;
static uint64_t vk_pending_seq(void) { return seq; }
static VkSpan copy_dedupe(int slot, const void *data, uint64_t size, uint64_t align) {
    assert(slot >= DEDUPE_DRV_V && slot <= DEDUPE_DRV_C);
    top = (top + align - 1) / align * align;
    assert(top + size <= sizeof arena);
    VkSpan s = {top, arena + top};
    memcpy(s.ptr, data, size); top += size; copies++;
    return s;
}
static VkDescriptorBufferInfo ubo_span(VkSpan s, uint64_t size) {
    return (VkDescriptorBufferInfo){1, s.offset, size};
}
#include "../runtime/nvn/vulkan/driver_cbuf_cache.h"
static bool same(VkDescriptorBufferInfo a, VkDescriptorBufferInfo b) {
    return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}
static uint32_t word(VkDescriptorBufferInfo a, unsigned offset) {
    assert(offset + 4 <= a.range);
    uint32_t v; memcpy(&v, arena + a.offset + offset, 4); return v;
}
int main(void) {
    Range storage[16] = {0}, uniform[16] = {0};
    storage[15] = (Range){0x123456789abcdef0ull, 0x3456};
    uniform[15] = (Range){0xfedcba9876543210ull, 0x789a};
    for (int stage = 0; stage < 6; stage++) {
        VkDescriptorBufferInfo a = driver_cbuf_info(stage, 0x800, storage, uniform, 3, 5, 7, true);
        unsigned n = copies;
        assert(same(a, driver_cbuf_info(stage, 0x800, storage, uniform, 3, 5, 7, true)));
        assert(copies == n);
        unsigned base = stage == 5 ? 0x310 : stage == 1 ? 0x510 : 0x110;
        assert(word(a, base + 15 * 16) == 0x9abcdef0);
        assert(word(a, base + 15 * 16 + 4) == 0x12345678);
        assert(word(a, base + 15 * 16 + 8) == 0x3456);
        assert(word(a, base + 15 * 16 + 12) == 0);
        assert(word(a, 0) == 0);
        if (stage == 5) {
            assert(word(a, 0x200) == 3 && word(a, 0x204) == 5 && word(a, 0x208) == 7);
            assert(word(a, 0x210 + 15 * 16) == 0x76543210);
            assert(word(a, 0x210 + 15 * 16 + 4) == 0xfedcba98);
            assert(word(a, 0x210 + 15 * 16 + 8) == 0x789a);
        }
        uniform[0].addr++;
        VkDescriptorBufferInfo b = driver_cbuf_info(stage, 0x800, storage, uniform, 3, 5, 7, true);
        assert(same(a, b) == (stage != 5));
        a = b;
        b = driver_cbuf_info(stage, 0x800, storage, uniform, 4, 6, 8, true);
        assert(same(a, b) == (stage != 5));
        a = b;
        storage[0].addr += 1ull << 32;
        b = driver_cbuf_info(stage, 0x800, storage, uniform, 4, 6, 8, true);
        assert(!same(a, b) && word(b, base + 4) == (uint32_t)(storage[0].addr >> 32));
        a = b; storage[0].size++;
        b = driver_cbuf_info(stage, 0x800, storage, uniform, 4, 6, 8, true);
        assert(!same(a, b) && word(b, base + 8) == storage[0].size);
        a = b; seq++;
        b = driver_cbuf_info(stage, 0x800, storage, uniform, 4, 6, 8, true);
        assert(!same(a, b));  // transient memory cannot be reused across submissions
        assert(!memcmp(arena + a.offset, arena + b.offset, a.range));
        a = b;
        b = driver_cbuf_info(stage, 0x800, storage, uniform, 4, 6, 8, false);
        assert(!same(a, b) && !memcmp(arena + a.offset, arena + b.offset, a.range));
        a = driver_cbuf_info(stage, 0, storage, uniform, 4, 6, 8, true);
        assert(a.range == (stage == 5 ? 0x500 : 256));
        if (stage != 5) {
            storage[0].addr++;
            storage[15].size++;
            // No binding table bytes in the default 256-byte graphics buffer.
            assert(same(a, driver_cbuf_info(stage, 0, storage, uniform, 4, 6, 8, true)));
            unsigned base = stage == 1 ? 0x510 : 0x110;
            a = driver_cbuf_info(stage, base + 16, storage, uniform, 4, 6, 8, true);
            storage[15].addr++;
            assert(same(a, driver_cbuf_info(stage, base + 16, storage, uniform, 4, 6, 8, true)));
            storage[0].addr++;
            assert(!same(a, driver_cbuf_info(stage, base + 16, storage, uniform, 4, 6, 8, true)));
        }
        b = driver_cbuf_info(stage, 0x900, storage, uniform, 4, 6, 8, true);
        assert(b.range == 0x800 && !same(a, b));
    }
    puts("Driver constants: layout, binding/group changes, range, stages and submission lifetime OK");
}
