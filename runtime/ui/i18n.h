// Android menu catalogs: i18n_<language>.inc, selected by settings_host_language().
// Missing translations and unsupported locales use the English source. Desktop uses English.
// Display text only: labels used as ImGui IDs stay the English ones.
#pragma once
#include <string>

const char *tr(const char *en);
// In capitals (section titles, row labels): ASCII, and the accented Latin-1 letters in UTF-8 (á -> Á, ç -> Ç)
std::string ui_upper(const char *s);
