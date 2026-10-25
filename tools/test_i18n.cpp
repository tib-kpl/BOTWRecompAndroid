// Exercise the Android catalogs on the CI host without starting the game or an emulator.
#include <cassert>
#include <cstring>
#include <iostream>
#include "../runtime/ui/i18n.mm"

extern "C" const char *settings_host_language(void) { return "fr"; }

int main() {
    assert(tr(nullptr) == nullptr);
    assert(!strcmp(tr(""), ""));
    assert(!strcmp(tr("Settings"), "Paramètres"));
    assert(!strcmp(tr("An unknown English key"), "An unknown English key"));
    assert(ui_upper("français, cœur, été") == "FRANÇAIS, CŒUR, ÉTÉ");
    for (const char *locale : {"fr-FR", "fr-CA", "FR_fr"}) {
        g_menu_language.store(catalog_for_language(locale));
        assert(!strcmp(tr("PLAY"), "JOUER"));
    }
    g_menu_language.store(catalog_for_language("pt-BR"));
    assert(!strcmp(tr("PLAY"), "JOGAR"));
    for (const char *locale : {"en", "de-DE", "french", ""}) {
        g_menu_language.store(catalog_for_language(locale));
        assert(!strcmp(tr("PLAY"), "PLAY"));
    }
    g_menu_language.store(catalog_for_language("fr"));
    char text[256];
    snprintf(text, sizeof text, tr("%s is now %s; %s moved to %s (swapped)."), "A", "B", "C", "D");
    assert(!strcmp(text, "A est maintenant B ; C a été déplacé vers D (échange)."));
    std::cout << "Menu locale selection, fallback, uppercase and formatting OK\n";
}
