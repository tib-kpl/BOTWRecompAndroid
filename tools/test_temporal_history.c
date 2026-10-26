#include "../runtime/nvn/vulkan/temporal_history.h"
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int test(void) {
    TemporalHistory h = {0}; unsigned slot;
    CHECK(!temporal_history_step(&h, 1920, 1080, 1, 100, 0, true, &slot) && slot == 0);
    CHECK(temporal_history_step(&h, 1920, 1080, 1, 101, 16000000, true, &slot) && slot == 1);
    CHECK(temporal_history_step(&h, 1920, 1080, 1, 102, 32000000, true, &slot) && slot == 0);
    CHECK(!temporal_history_step(&h, 1280, 720, 1, 103, 48000000, true, &slot)); // resize
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 104, 64000000, true, &slot)); // options / game reset
    CHECK(temporal_history_step(&h, 1280, 720, 2, 105, 80000000, true, &slot));
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 105, 96000000, true, &slot)); // repeated present
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 107, 112000000, true, &slot)); // missing frame
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 108, 412000000, true, &slot)); // app suspended
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 109, 400000000, true, &slot)); // clock went back
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 110, 416000000, false, &slot)); // toggle off / menu
    CHECK(!temporal_history_step(&h, 1280, 720, 2, 111, 432000000, true, &slot)); // re-enable: no old history
    CHECK(temporal_history_step(&h, 1280, 720, 2, 112, 448000000, true, &slot));
    return 0;
}
int main(void) { return test(); }
