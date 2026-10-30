#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/turnip_policy.h"

int main(void) {
    const char *names[] = {"Turnip Adreno (TM) 740", "Adreno740v3", "Adreno (TM) 740v1"};
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        assert(!vk_turnip_path_auto(names[i], VK_TURNIP_HEAP));
        assert(vk_turnip_path_auto(names[i], VK_TURNIP_TARGET_READS));
        assert(!vk_turnip_path_auto(names[i], VK_TURNIP_LRZ));
    }
    const char *supported[] = {"Turnip Adreno (TM) 830", "Adreno840v2", "Adreno (TM) 800", "Adreno899"};
    const char *others[] = {NULL, "", "Mali-G715", "Adreno", "Adreno730", "Adreno750", "Adreno7400", "Adreno900"};
    for (int path = VK_TURNIP_HEAP; path <= VK_TURNIP_LRZ; path++) {
        for (size_t i = 0; i < sizeof supported / sizeof *supported; i++) {
            assert(vk_turnip_path_auto(supported[i], path));
            assert(!vk_turnip_path_setting(supported[i], 0, path));
        }
        for (size_t i = 0; i < sizeof others / sizeof *others; i++) {
            assert(!vk_turnip_path_auto(others[i], path));
            assert(vk_turnip_path_setting(others[i], 1, path));
            assert(!vk_turnip_path_setting(others[i], -1, path));
        }
        assert(vk_turnip_path_setting(names[0], 1, path));
        assert(!vk_turnip_path_setting(names[0], 0, path));
    }
    assert(!vk_turnip_path_auto("Adreno830", (enum VkTurnipPath)99));
    const char *mapped[] = {"Adreno (TM) 735", "Adreno732", "Adreno750"};
    for (size_t i = 0; i < sizeof mapped / sizeof *mapped; i++) {
        assert(vk_turnip_heap_setting(mapped[i], -1, true, true));
        assert(!vk_turnip_heap_setting(mapped[i], -1, false, true));
        assert(!vk_turnip_heap_setting(mapped[i], -1, true, false));
        assert(!vk_turnip_heap_setting(mapped[i], 0, true, true));
        assert(vk_turnip_heap_setting(mapped[i], 1, false, false));
        assert(!vk_turnip_path_setting(mapped[i], -1, VK_TURNIP_TARGET_READS));
        assert(!vk_turnip_path_setting(mapped[i], -1, VK_TURNIP_LRZ));
    }
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        assert(!vk_turnip_heap_setting(names[i], -1, true, true));
    assert(vk_turnip_heap_setting("Adreno830", -1, false, false));
    puts("Turnip profiles: per-option 740 policy, 8xx compatibility, explicit overrides passed");
}
