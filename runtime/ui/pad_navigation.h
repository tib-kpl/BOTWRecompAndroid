// One logical controller layout for the game, launcher, settings and software keyboard.
#pragma once
#include <array>
#include "imgui.h"
#include "../input_bind.h"

inline ImGuiKey bound_pad_navigation_key(int host) {
    if (host < 0 || host >= PADB_HOST_COUNT) return ImGuiKey_None;
    static constexpr ImGuiKey keys[BIND_PAD_COUNT] = {
        ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight, ImGuiKey_GamepadFaceUp, ImGuiKey_GamepadFaceLeft,
        ImGuiKey_GamepadL3, ImGuiKey_GamepadR3, ImGuiKey_GamepadL1, ImGuiKey_GamepadR1,
        ImGuiKey_GamepadL2, ImGuiKey_GamepadR2, ImGuiKey_GamepadStart, ImGuiKey_GamepadBack,
        ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadDpadDown
    };
    for (int slot = 0; slot < BIND_PAD_COUNT; slot++)
        if (g_bind_pad[slot] == host) return keys[slot];
    return ImGuiKey_None;
}

class PadNavigation {
    std::array<ImGuiKey, PADB_HOST_COUNT> held{};
public:
    void clear() { held.fill(ImGuiKey_None); }
    ImGuiKey button(int host, bool down) {
        if (host < 0 || host >= PADB_HOST_COUNT) return ImGuiKey_None;
        ImGuiKey &key = held[host];
        // Release the action pressed before a remap, rather than leaving the old UI action held forever.
        if (!down) { ImGuiKey old = key; key = ImGuiKey_None; return old; }
        if (key == ImGuiKey_None) key = bound_pad_navigation_key(host);
        return key;
    }
    struct Analog { ImGuiKey key; float value; };
    Analog trigger(int host, float value) {
        if (host < 0 || host >= PADB_HOST_COUNT) return {ImGuiKey_None, 0};
        bool down = value > (held[host] == ImGuiKey_None ? 0.5f : 0.3f);
        return {button(host, down), down ? value : 0.0f};
    }
};
