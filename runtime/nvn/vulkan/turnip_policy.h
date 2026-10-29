#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum VkTurnipPath { VK_TURNIP_HEAP, VK_TURNIP_TARGET_READS, VK_TURNIP_LRZ };

// Keep choices independent: mapping guest memory and reading render targets in
// place have different costs and compatibility, even on the same GPU.
static inline bool vk_turnip_path_auto(const char *name, enum VkTurnipPath path) {
    if (path < VK_TURNIP_HEAP || path > VK_TURNIP_LRZ) return false;
    const char *a = name ? strstr(name, "Adreno") : NULL;
    if (!a) return false;
    while (*a && (*a < '0' || *a > '9')) a++;
    int model = atoi(a);
    if (model >= 800 && model < 900) return true; // Existing upstream profile.
    // Odin 2 Portal / Turnip: target reads tested separately from guest memory.
    return model == 740 && path == VK_TURNIP_TARGET_READS;
}

static inline bool vk_turnip_path_setting(const char *name, int setting, enum VkTurnipPath path) {
    return setting < 0 ? vk_turnip_path_auto(name, path) : setting != 0;
}
