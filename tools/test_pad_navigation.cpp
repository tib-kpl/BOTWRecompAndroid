#include <cassert>
#include <utility>
#include "../runtime/ui/pad_navigation.h"

// SDL positions: south/east/west/north = 0/1/2/3, triggers = 26/27.
int16_t g_bind_pad[BIND_PAD_COUNT] = {1,0,3,2,7,8,9,10,26,27,6,4,13,11,14,12};
int main() {
    assert(bound_pad_navigation_key(1) == ImGuiKey_GamepadFaceDown);
    assert(bound_pad_navigation_key(0) == ImGuiKey_GamepadFaceRight);
    std::swap(g_bind_pad[BIND_A], g_bind_pad[BIND_B]);
    assert(bound_pad_navigation_key(0) == ImGuiKey_GamepadFaceDown);
    assert(bound_pad_navigation_key(1) == ImGuiKey_GamepadFaceRight);
    PadNavigation nav;
    assert(nav.button(0, true) == ImGuiKey_GamepadFaceDown);
    std::swap(g_bind_pad[BIND_A], g_bind_pad[BIND_B]);
    assert(nav.button(0, false) == ImGuiKey_GamepadFaceDown); // release original action after a live remap
    assert(nav.button(0, true) == ImGuiKey_GamepadFaceRight);
    nav.clear();
    assert(nav.button(0, false) == ImGuiKey_None); // removed controller / reopened menu
    std::swap(g_bind_pad[BIND_A], g_bind_pad[BIND_ZL]);
    assert(nav.trigger(PADB_LTRIGGER, 0.4f).key == ImGuiKey_None);
    auto a = nav.trigger(PADB_LTRIGGER, 0.7f);
    assert(a.key == ImGuiKey_GamepadFaceDown && a.value == 0.7f);
    assert(nav.trigger(PADB_LTRIGGER, 0.4f).key == ImGuiKey_GamepadFaceDown);
    a = nav.trigger(PADB_LTRIGGER, 0.2f);
    assert(a.key == ImGuiKey_GamepadFaceDown && a.value == 0);
    assert(nav.trigger(PADB_LTRIGGER, 0).key == ImGuiKey_None);
    std::swap(g_bind_pad[BIND_X], g_bind_pad[BIND_RIGHT]);
    assert(bound_pad_navigation_key(3) == ImGuiKey_GamepadDpadRight);
    assert(bound_pad_navigation_key(14) == ImGuiKey_GamepadFaceUp);
    assert(bound_pad_navigation_key(20) == ImGuiKey_None); // unbound input must not retain the fixed layout
    assert(nav.button(-1, true) == ImGuiKey_None);
    assert(nav.button(PADB_HOST_COUNT, true) == ImGuiKey_None);
    g_bind_pad[BIND_B] = -1;
    assert(bound_pad_navigation_key(0) == ImGuiKey_None);
}
