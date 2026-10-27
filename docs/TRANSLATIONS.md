# Translating the Android application

The application menus support English (fallback), French, and Brazilian Portuguese. They follow Android's app
language, initially the phone's preferred language. Android 13 and later also expose an app language choice in
system settings, declared in `res/xml/locales_config.xml`. Regional French locales, such as `fr-CA`, use French.
The native menus follow the in-app Language setting; Auto uses Android's app language. Android resource-backed
dialogs follow Android's app language. Unsupported menu languages use English.

There are two translation catalogs, because most menus are drawn by the native runtime rather than Android views:

| UI | Translation files |
| --- | --- |
| Launcher, settings, controls, game import, mods | `runtime/ui/i18n_fr.inc`, `runtime/ui/i18n_pt.inc` |
| Android shortcuts, driver dialogs, watch map settings | `android/botwrecompandroid/app/src/main/res/values-*/strings.xml` |

Native catalogs contain `{ "English source text", "Translation" }` pairs in UTF-8. `tr()` uses the English source as
a lookup key; missing entries fall back to that source. Keep English keys, whitespace, newlines, printf placeholders
(`%s`, `%.0f`, `%%`, etc.), and ImGui hidden identifiers (`##...`) unchanged. Only translate the displayed text.
Menu widget identifiers remain independent of their translations.

## Add a language

1. Copy `runtime/ui/i18n_fr.inc` to `i18n_<language>.inc` and translate the values.
2. Include the new catalog and register its base language code in `kTables` in `runtime/ui/i18n.mm`.
3. Copy `res/values/strings.xml` into `res/values-<language>/strings.xml` and translate every resource. Use Android
   resource escaping and keep numbered placeholders such as `%1$s`. Add the language to `res/xml/locales_config.xml`
   so Android offers it in per-app language settings. Keep this explicit config; do not also enable Gradle's
   `generateLocaleConfig` option.
4. For a new writing system, extend the ImGui font glyph ranges and ship a suitable licensed font if needed. The
   current fonts/ranges cover French and Portuguese, Latin Extended-A, punctuation and the menu arrows.
5. Run `python tools/check_translations.py`, build the APK, and check the launcher, in-game settings, game import,
   driver shortcuts and watch map dialog on a device. Check wrapping at a small display size.

The validation script also runs in CI. It rejects duplicate keys, missing catalog entries, empty values, changed
format placeholders, and changed hidden widget identifiers. For test launches, `LARECOMP_HOST_LANG=fr` selects the
French native catalog. Android configuration changes update the native menu language without restarting the game.
