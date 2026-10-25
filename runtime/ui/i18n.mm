// See i18n.h.
#include "i18n.h"

#include <ctype.h>
#include <string.h>
#include <unordered_map>
#ifdef __ANDROID__
#include <array>
#include <atomic>
#include <jni.h>
#endif

extern "C" {
#include "../settings.h"
}

#ifdef __ANDROID__
static const char *const kPt[][2] = {
#include "i18n_pt.inc"
};
static const char *const kFr[][2] = {
#include "i18n_fr.inc"
};
// Register future catalogs here; missing entries always fall back to the English source.
struct Catalog {
    const char *language;
    const char *const (*entries)[2];
    size_t count;
};
static const Catalog kCatalogs[] = {
    {"pt", kPt, sizeof kPt / sizeof *kPt},
    {"fr", kFr, sizeof kFr / sizeof *kFr},
};
using Translations = std::unordered_map<std::string, const char *>;
static const Translations *catalog_for_language(const char *locale) {
    static const auto *maps = [] {
        auto *result = new std::array<Translations, sizeof kCatalogs / sizeof *kCatalogs>();
        for (size_t i = 0; i < result->size(); i++)
            for (size_t j = 0; j < kCatalogs[i].count; j++)
                (*result)[i].emplace(kCatalogs[i].entries[j][0], kCatalogs[i].entries[j][1]);
        return result;
    }();
    static const Translations english;
    for (size_t i = 0; i < maps->size(); i++) {
        size_t n = strlen(kCatalogs[i].language);
        if (!strncasecmp(locale, kCatalogs[i].language, n) &&
            (!locale[n] || locale[n] == '-' || locale[n] == '_')) return &(*maps)[i];
    }
    return &english;
}
static std::atomic<const Translations *> g_menu_language{nullptr};

extern "C" JNIEXPORT void JNICALL
Java_com_botwrecompandroid_BOTWRecompAndroidActivity_nativeMenuLanguage(JNIEnv *env, jclass, jstring locale) {
    const char *code = env->GetStringUTFChars(locale, nullptr);
    if (!code) return;
    g_menu_language.store(catalog_for_language(code));
    env->ReleaseStringUTFChars(locale, code);
}
#endif

const char *tr(const char *en) {
#ifdef __ANDROID__
    if (!en || !*en) return en;
    static const auto *initial = catalog_for_language(settings_host_language());
    const auto *catalog = g_menu_language.load();
    if (!catalog) catalog = initial;
    auto it = catalog->find(en);
    return it == catalog->end() ? en : it->second;
#else
    return en;
#endif
}

std::string ui_upper(const char *s) {
    std::string t = s ? s : "";
    for (size_t i = 0; i < t.size(); i++) {
        unsigned char c = (unsigned char)t[i];
        if (c < 0x80) t[i] = (char)toupper(c);
#ifdef __ANDROID__  // (the desktop's menus are English: bytes as before)
        else if (c == 0xC5 && i + 1 < t.size() && (unsigned char)t[i + 1] == 0x93) {
            t[++i] = (char)0x92;  // œ -> Œ
        }
        else if (c == 0xC3 && i + 1 < t.size()) {  // U+00E0..U+00FE: the lowercase accented letters (but ÷, U+00F7)
            unsigned char d = (unsigned char)t[i + 1];
            if (d >= 0xA0 && d <= 0xBE && d != 0xB7) t[i + 1] = (char)(d - 0x20);
            i++;
        }
#endif
    }
    return t;
}
