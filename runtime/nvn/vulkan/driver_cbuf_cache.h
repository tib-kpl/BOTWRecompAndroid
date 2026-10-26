// Driver constants contain binding metadata, never the contents of guest buffers. Reusing an immutable
// transient copy is safe only within its submission and for exactly the same metadata and descriptor range.
#pragma once

static VkDescriptorBufferInfo driver_cbuf_info(int stage, uint64_t requested_size, const Range storage[16],
                                              const Range uniform[16], uint32_t gx, uint32_t gy, uint32_t gz,
                                              bool cached) {
    static struct {
        bool valid;
        uint64_t seq, size;
        Range storage[16], uniform[16];
        uint32_t groups[3];
        VkDescriptorBufferInfo info;
    } last[6];
    uint64_t size = requested_size > 0x800 ? 0x800 : requested_size ? requested_size : 256;
    if (stage == 5 && size < 0x500) size = 0x500;
    // Small graphics driver buffers contain only zeros: their binding tables start at 0x110 / 0x510.
    // Metadata beyond the descriptor range cannot affect its contents and must not invalidate this copy.
    unsigned base = stage == 5 ? 0x310 : stage == 1 ? 0x510 : 0x110;
    unsigned nstorage = size > base ? (unsigned)((size - base + 15) / 16) : 0;
    if (nstorage > 16) nstorage = 16;
    uint64_t seq = vk_pending_seq();
    if (cached && last[stage].valid && last[stage].seq == seq && last[stage].size == size &&
        !memcmp(last[stage].storage, storage, nstorage * sizeof *storage) &&
        (stage != 5 || (!memcmp(last[stage].uniform, uniform, sizeof last[stage].uniform) &&
                       last[stage].groups[0] == gx && last[stage].groups[1] == gy && last[stage].groups[2] == gz)))
        return last[stage].info;

    uint32_t drv[0x800 / 4] = {0};
    if (stage == 5) {
        drv[0x200 / 4] = gx; drv[0x204 / 4] = gy; drv[0x208 / 4] = gz;
        for (int i = 0; i < 16; i++) {
            uint32_t *u = &drv[(0x210 + 16 * i) / 4];
            u[0] = (uint32_t)uniform[i].addr; u[1] = (uint32_t)(uniform[i].addr >> 32); u[2] = (uint32_t)uniform[i].size;
            uint32_t *e = &drv[(0x310 + 16 * i) / 4];
            e[0] = (uint32_t)storage[i].addr; e[1] = (uint32_t)(storage[i].addr >> 32); e[2] = (uint32_t)storage[i].size;
        }
    } else {
        uint32_t base = 0x110 + 0x100 * (stage == 1 ? 4 : 0);
        for (int i = 0; i < 16; i++) {
            uint32_t *e = &drv[(base + 16 * i) / 4];
            e[0] = (uint32_t)storage[i].addr; e[1] = (uint32_t)(storage[i].addr >> 32); e[2] = (uint32_t)storage[i].size;
        }
    }
    VkDescriptorBufferInfo info = ubo_span(copy_dedupe(stage == 5 ? DEDUPE_DRV_C :
                                         stage == 1 ? DEDUPE_DRV_F : DEDUPE_DRV_V, drv, size, 256), size);
    if (cached) {
        last[stage].valid = true;
        last[stage].seq = seq; last[stage].size = size; last[stage].info = info;
        memcpy(last[stage].storage, storage, nstorage * sizeof *storage);
        if (stage == 5) {
            memcpy(last[stage].uniform, uniform, sizeof last[stage].uniform);
            last[stage].groups[0] = gx; last[stage].groups[1] = gy; last[stage].groups[2] = gz;
        }
    }
    return info;
}
