#include <pebble.h>
#include "strings.h"

// Tabelle aus strings_table.h. Sie liegt im App-Abbild und zaehlt damit zum
// Speicherabdruck, belegt aber keinen Heap.
static const char *const s_table[STR_COUNT][STRINGS_LANG_COUNT] = {
#define STR(id, maxbytes, en, de) { en, de },
#include "strings_table.h"
#undef STR
};

static StringLang s_lang = STRINGS_EN;

// Der Locale-String ist "de_DE", "fr_FR", "en_US" ... Ein Sprachpaket darf
// aber auch nur den Zwei-Buchstaben-Code liefern (die Firmware selbst tut das
// bei uk). Deshalb wird NIE auf "de_DE" verglichen, sondern auf die ersten
// zwei Zeichen - so macht es auch pebble-hacks/locale_framework.
static StringLang prv_pick_language(const char *locale) {
  if (!locale) return STRINGS_EN;
  if (strncmp(locale, "de", 2) == 0) return STRINGS_DE;
  return STRINGS_EN;
}

void strings_refresh(void) {
  s_lang = prv_pick_language(i18n_get_system_locale());
}

StringLang strings_language(void) {
  return s_lang;
}

const char *S(StringId id) {
  // Kein Test auf id < 0: der Aufzaehlungstyp ist vorzeichenlos, der Vergleich
  // waere immer falsch und -Werror=type-limits bricht daran ab.
  if (id >= STR_COUNT) return "";
  const char *text = s_table[id][s_lang];
  if (!text) text = s_table[id][STRINGS_EN];
  return text ? text : "";
}
