#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum VkTurnipPath { VK_TURNIP_HEAP, VK_TURNIP_TARGET_READS, VK_TURNIP_LRZ };

// Keep choices independent: mapping guest memory and reading render targets in
// place have different costs and compatibility, even on the same GPU.
static inline int vk_turnip_gpu_model(const char *name) {
    const char *a = name ? strstr(name, "Adreno") : NULL;
    if (!a) return 0;
    while (*a && (*a < '0' || *a > '9')) a++;
    return atoi(a);
}

static inline bool vk_turnip_path_auto(const char *name, enum VkTurnipPath path) {
    if (path < VK_TURNIP_HEAP || path > VK_TURNIP_LRZ) return false;
    int model = vk_turnip_gpu_model(name);
    if (model >= 800 && model < 900) return true; // Existing upstream profile.
    // Odin 2 Portal / Turnip: target reads tested separately from guest memory.
    return model == 740 && path == VK_TURNIP_TARGET_READS;
}

static inline bool vk_turnip_path_setting(const char *name, int setting, enum VkTurnipPath path) {
    return setting < 0 ? vk_turnip_path_auto(name, path) : setting != 0;
}

static inline bool vk_turnip_heap_setting(const char *name, int setting, bool map_placed, bool turnip) {
    if (setting >= 0) return setting != 0;
    if (vk_turnip_path_auto(name, VK_TURNIP_HEAP)) return true;
    // Upstream: mapped Turnip heaps keep high-offset text glyphs reachable
    // on pre-8xx GPUs, including the 732 reported as 735. Preserve the
    // separately tested Odin 2 Portal / 740 policy and other path choices.
    return map_placed && turnip && vk_turnip_gpu_model(name) != 740;
}
