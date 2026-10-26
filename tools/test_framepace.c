// Native frame pacing against a simulated guest and clock, without copyrighted game files.
#include <assert.h>
#include <string.h>
#include "../runtime/hle/framepace.c"

uint64_t g_guest_lo;
int g_game_version;
volatile int g_movie_playing;
Settings g_settings;
static uint64_t now;
uint64_t plat_now_ns(void) { return now; }

int main(int argc, char **argv) {
    assert(argc == 3);
    g_game_version = atoi(argv[1]);
    int fps = atoi(argv[2]);
    if (g_game_version != 182 && g_game_version != 190) {
        assert(!framepace_on());
        assert(!framepace_in_menu());
        assert(!framepace_on_title());
        return 0;
    }
    g_guest_lo = (uintptr_t)calloc(1, 0x2000000);
    assert(g_guest_lo && framepace_on());
    g_settings.fps_mode = fps == 60 ? 2 : 1;
    assert(!framepace_on_title());  // screen holder not initialized yet
    assert(framepace_target() == fps);

    uint64_t *holder = calloc(1, 8), *screens = calloc(2, 8);
    uint8_t *manager = calloc(1, 0x40), *event = calloc(1, 0x1d300);
    *holder = (uintptr_t)manager;
    *(uint64_t *)(uintptr_t)SCREEN_HOLDER = (uintptr_t)holder;
    VF(uint32_t, (uintptr_t)manager, 0x28) = 2;
    VF(uint64_t, (uintptr_t)manager, 0x30) = (uintptr_t)screens;
    screens[1] = 1;
    assert(framepace_on_title() && framepace_in_menu());
    assert(framepace_target() == 30);
    screens[1] = 0;
    for (int state = 1; state <= 2; state++) {
        *(uint32_t *)(uintptr_t)PAUSE_STATE = state;
        assert(!framepace_on_title() && framepace_in_menu());
        assert(framepace_target() == 30);
    }
    *(uint32_t *)(uintptr_t)PAUSE_STATE = 0;
    assert(!framepace_on_title() && !framepace_in_menu());
    assert(framepace_target() == fps);
    *(uint64_t *)(uintptr_t)EVENT_MANAGER = (uintptr_t)event;
    VF(uint64_t, (uintptr_t)event, 0x1d2b8) = 1;
    assert(framepace_target() == 30);
    VF(uint64_t, (uintptr_t)event, 0x1d2b8) = 0;
    g_movie_playing = 1; assert(framepace_target() == 30);
    g_movie_playing = 0; assert(framepace_target() == fps);

    uint8_t vfr[0x180] = {0};
    double elapsed = 0;
    for (int i = 0; i < fps * 60; i++) {
        now += 1000000000ull / fps;
        framepace_presented(now, true);
        rt_vfr_update((uintptr_t)vfr);
        float ratio = VF(float, (uintptr_t)vfr, VFR_RATIO);
        assert(isfinite(ratio) && ratio > 0 && ratio <= 1.5f);
        elapsed += ratio / 30.0;
    }
    assert(fabs(elapsed - 60.0) < 0.1);  // 60 seconds in both modes, not double-speed at 60 fps
    assert(fabs(VF(float, (uintptr_t)vfr, VFR_RATIO) - 30.0 / fps) < 0.01);
    assert(VF(uint32_t, (uintptr_t)vfr, VFR_INTERVAL) == (fps == 60 ? 1 : 2));
    assert(fabs(*(float *)(uintptr_t)CONTROLLER_STEP - 30.0 / fps) < 0.01);
    printf("Version %d at %d fps: menus/events/movie caps and 60-second clock/controller steps OK\n", g_game_version, fps);
    free(event); free(manager); free(screens); free(holder); free((void *)(uintptr_t)g_guest_lo);
}
