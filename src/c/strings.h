#pragma once
#include <pebble.h>

// Texte der Oberflaeche. Die Uhr gibt die Sprache vor
// (Settings -> Display -> Language); die App folgt ihr, es gibt keinen eigenen
// Sprachschalter. Alle Texte stehen in strings_table.h, eine Zeile je Text.
//
// Rueckfall ist ENGLISCH: die Pebble Time 2 kennt weitere eingebaute Sprachen,
// fuer die wir keine Spalte haben (Catala, Nederlands, Portugues, Polski und
// weitere). Eine deutsche Oberflaeche auf einer polnischen Uhr waere
// schlechter als eine englische.
//
// DIE REIHENFOLGE IST FEST: der Wert geht als MESSAGE_KEY_LANG ans Telefon
// (0 Englisch, 1 Deutsch, 2 Franzoesisch, 3 Italienisch, 4 Spanisch), und
// index.js waehlt danach die Pin-Texte und die Konfigseite. Neue Sprachen
// nur hinten anfuegen.

typedef enum {
  STRINGS_EN = 0,   //< Spalte 0, zugleich der Rueckfall
  STRINGS_DE,
  STRINGS_FR,
  STRINGS_IT,
  STRINGS_ES,
  STRINGS_LANG_COUNT,
} StringLang;

typedef enum {
#define STR(id, maxbytes, en, de, fr, it, es) id,
#include "strings_table.h"
#undef STR
  STR_COUNT,
} StringId;

// Liefert den Text in der aktuellen Sprache. Nie NULL: ein unbekannter
// Schluessel und eine fehlende Spalte fallen auf Englisch zurueck, eine
// fehlende englische Spalte auf den leeren String.
const char *S(StringId id);

// Sprache neu von der Uhr lesen. i18n_get_system_locale() ist ein Syscall und
// es gibt kein Ereignis fuer einen Sprachwechsel - also einmal beim Start und
// danach dort, wo ohnehin neu aufgebaut wird.
void strings_refresh(void);

// Fuer die wenigen Stellen, die sich sprachabhaengig anders verhalten muessen
// (etwa die Eindeutschung von Ortsnamen, die auf Englisch falsch waere).
StringLang strings_language(void);
