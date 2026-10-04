// Der Tag von Zaehler und Tagesziel (schedule.c) - auf dem Rechner.
//
//   sh tools/schedule_host_test.sh
//
// Was hier leicht falsch und teuer ist:
//
//   - TAG ZUM ZIEL (1.19): die Uhr schickt mit dem Tagesziel seinen Tag. Das
//     ist der Tag, zu dem Zaehler und Ziel gehoeren, nicht der der Uhrzeit.
//   - VORWAERTS: ein neuer Tag macht Zaehler und Ziel frisch - auch wenn die
//     App ueber Mitternacht offen steht.
//   - RUECKWAERTS (1.16.2): springt die Uhr nach einem Neustart zurueck,
//     bleiben Glaeser und Ziel - auch ein Glas, das man trinkt, waehrend sie
//     auf gestern steht (zoege es den Tag auf gestern, waere das Stellen der
//     Uhr ein neuer Tag und alle Glaeser weg). Liegt der gemerkte Tag mehr als zwei Tage
//     voraus und geht die Uhr plausibel (ab 2025), war er falsch: dann gilt
//     heute, die Werte bleiben. "Zwei Tage" genau, auch ueber Monatsenden.
//   - VORLAUF (Audit M10): stand die Uhr ein bis zwei Tage vor und wurde
//     zurueckgestellt, sind die Glaeser des echten Tages am Folgetag weg -
//     sobald das Telefon die zurueckgestellte Uhr bestaetigt hat. Ohne das
//     Telefon kann die Uhr Vorlauf und Neustart nicht unterscheiden.
//   - BESTAETIGEN: hoechstens 300 s Abweichung und derselbe Tag.
//   - EIN NEUER TAG speichert Zaehler, Ziel und Tag - auch der, der bei
//     offener App beginnt.
//
// Die Zeiten rechnet timegm aus der C-Bibliothek, nicht schedule.c.
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include "schedule.h"
#include "coffee.h"

time_t stub_jetzt;

// Persist-Faecher aus config.h - hier mit Wert festgenagelt: eine
// Aktualisierung der App liest dieselben.
#define FACH_TAG     1
#define FACH_ZAEHLER 2
#define FACH_ZIEL    3

// Kaffees und eigene Getraenke spielen hier keine Rolle.
int coffee_count(void) { return 0; }
time_t coffee_time(time_t tag, int idx) { (void)idx; return tag; }
int custom_count(void) { return 0; }
time_t custom_time(time_t tag, int idx) { (void)idx; return tag; }

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

// Ortszeit = UTC (der Test laeuft mit TZ=UTC).
static time_t um(int jahr, int monat, int tag, int std, int min, int sek) {
  struct tm t = { .tm_year = jahr - 1900, .tm_mon = monat - 1, .tm_mday = tag,
                  .tm_hour = std, .tm_min = min, .tm_sec = sek };
  return timegm(&t);
}

// App-Start zur Uhrzeit `t`.
static void start(time_t t) {
  stub_jetzt = t;
  schedule_init();
}
// Frisch installiert.
static void frisch(time_t t) {
  attrappe_persist_leeren();
  start(t);
}
static void trinke(int n) {
  for (int i = 0; i < n; i++) schedule_set_count(schedule_count() + 1);
}

// 03.10.2026, 23:59 UTC
#define SPAET 1791071940

int main(void) {
  printf("\nTag zum Ziel (1.19)\n");
  frisch(SPAET);
  const int soll = schedule_target();
  pruefe("Soll ist voreingestellt 8", soll == 8);
  pruefe("frisch: der Tag ist heute", schedule_day() == 20261003);
  pruefe("frisch: das Ziel ist das Soll", schedule_goal() == soll);
  schedule_raise_goal();
  pruefe("erhoeht: Ziel und Tag gehoeren zusammen", schedule_goal() == soll + 1 && schedule_day() == 20261003);
  start(SPAET + 120);                                   // eine Minute nach Mitternacht
  pruefe("neuer Tag: der Tag ist morgen", schedule_day() == 20261004);
  pruefe("neuer Tag: das Ziel ist wieder das Soll", schedule_goal() == soll);
  schedule_raise_goal();
  start(SPAET - 600);                                   // Uhr steht kurz zu frueh
  pruefe("Uhr zu frueh: Ziel des gemerkten Tages", schedule_goal() == soll + 1);
  pruefe("Uhr zu frueh: der gemerkte Tag", schedule_day() == 20261004);

  printf("\nVorwaerts\n");
  frisch(um(2026, 10, 3, 10, 0, 0));
  trinke(3);
  pruefe("drei Glaeser", schedule_count() == 3 && persist_read_int(FACH_ZAEHLER) == 3);
  stub_jetzt = um(2026, 10, 3, 23, 59, 59);
  pruefe("23:59:59, App offen: noch drei", schedule_count() == 3 && schedule_day() == 20261003);
  stub_jetzt = um(2026, 10, 4, 0, 0, 0);
  pruefe("00:00:00, App offen: null, neuer Tag", schedule_count() == 0 && schedule_day() == 20261004);
  pruefe("00:00:00, App offen: Ziel ist das Soll", schedule_goal() == soll);
  trinke(1);
  pruefe("Glas nach Mitternacht zaehlt zum neuen Tag",
         schedule_count() == 1 && persist_read_int(FACH_ZAEHLER) == 1 && persist_read_int(FACH_TAG) == 20261004);
  start(um(2026, 10, 4, 7, 0, 0));
  pruefe("Neustart am Morgen: das Glas von 00:00 steht", schedule_count() == 1);

  printf("\nRueckwaerts (1.16.2)\n");
  frisch(um(2026, 10, 3, 10, 0, 0));
  trinke(2);
  schedule_raise_goal();
  start(um(2026, 10, 3, 8, 0, 0));                      // Zeitzone neu: Stunden zurueck
  pruefe("Stunden zurueck: Glaeser und Ziel bleiben", schedule_count() == 2 && schedule_goal() == soll + 1);
  start(um(2026, 10, 2, 23, 0, 0));                     // ueber Mitternacht zurueck
  pruefe("einen Tag zurueck: Glaeser bleiben", schedule_count() == 2);
  pruefe("einen Tag zurueck: der gemerkte Tag bleibt",
         schedule_day() == 20261003 && persist_read_int(FACH_TAG) == 20261003);
  start(um(2000, 1, 1, 12, 0, 0));                      // Uhr ohne Zeit
  pruefe("Jahr 2000: Glaeser und Tag bleiben", schedule_count() == 2 && schedule_day() == 20261003);
  trinke(1);                                            // und dabei getrunken
  pruefe("Jahr 2000, getrunken: der Tag bleibt (keine Zeit, kein Tag)",
         schedule_count() == 3 && persist_read_int(FACH_TAG) == 20261003);
  start(um(2026, 10, 3, 10, 30, 0));
  pruefe("Uhr gestellt: auch das Glas aus dem Jahr 2000 steht", schedule_count() == 3);
  schedule_set_count(2);                                // fuer die Faelle unten wie vorher
  start(um(2026, 10, 3, 11, 0, 0));                     // das Telefon stellt die Uhr
  pruefe("wieder richtig: Glaeser und Ziel stehen", schedule_count() == 2 && schedule_goal() == soll + 1);
  start(um(2026, 10, 1, 11, 0, 0));                     // genau zwei Tage zurueck
  pruefe("zwei Tage zurueck: der gemerkte Tag bleibt", persist_read_int(FACH_TAG) == 20261003);
  start(um(2026, 9, 30, 11, 0, 0));                     // drei Tage zurueck, plausibel
  pruefe("drei Tage zurueck: der gemerkte Tag war falsch, heute gilt",
         persist_read_int(FACH_TAG) == 20260930);
  pruefe("drei Tage zurueck: Glaeser bleiben", schedule_count() == 2);

  printf("\nZwei Tage genau, auch ueber Monatsenden\n");
  frisch(um(2027, 3, 1, 10, 0, 0));
  trinke(1);
  start(um(2027, 2, 28, 23, 0, 0));                     // 1 Tag zurueck (kein Schaltjahr)
  pruefe("01.03. -> 28.02.2027: der gemerkte Tag bleibt", persist_read_int(FACH_TAG) == 20270301);
  start(um(2027, 3, 1, 10, 5, 0));
  pruefe("01.03. -> 28.02. -> 01.03.2027: das Glas steht", schedule_count() == 1);
  frisch(um(2028, 3, 1, 10, 0, 0));
  start(um(2028, 2, 28, 10, 0, 0));                     // 2 Tage zurueck (29.02.2028)
  pruefe("01.03. -> 28.02.2028 (Schaltjahr): zwei Tage, bleibt", persist_read_int(FACH_TAG) == 20280301);
  start(um(2028, 2, 27, 10, 0, 0));                     // 3 Tage zurueck
  pruefe("01.03. -> 27.02.2028: drei Tage, heute gilt", persist_read_int(FACH_TAG) == 20280227);
  frisch(um(2026, 1, 1, 10, 0, 0));
  start(um(2025, 12, 30, 10, 0, 0));                    // ueber den Jahreswechsel: 2 Tage
  pruefe("01.01.2026 -> 30.12.2025: zwei Tage, bleibt", persist_read_int(FACH_TAG) == 20260101);
  start(um(2025, 12, 29, 10, 0, 0));
  pruefe("01.01.2026 -> 29.12.2025: drei Tage, heute gilt", persist_read_int(FACH_TAG) == 20251229);

  printf("\nPlausibel heisst ab 2025\n");
  frisch(um(2025, 1, 3, 23, 0, 0));
  start(um(2024, 12, 31, 23, 0, 0));
  pruefe("31.12.2024: nicht plausibel, der gemerkte Tag bleibt", persist_read_int(FACH_TAG) == 20250103);
  frisch(um(2025, 1, 4, 1, 0, 0));
  start(um(2025, 1, 1, 1, 0, 0));
  pruefe("01.01.2025: plausibel, heute gilt", persist_read_int(FACH_TAG) == 20250101);

  printf("\nRueckwaerts: getrunken, waehrend die Uhr auf gestern steht\n");
  frisch(um(2026, 10, 3, 10, 0, 0));
  trinke(4);
  schedule_raise_goal();
  start(um(2026, 10, 2, 23, 30, 0));                    // nach Neustart kurz auf gestern ...
  pruefe("auf gestern: die Uhr fragt das Telefon", schedule_uhr_fraglich());
  trinke(1);                                            // ... und dabei getrunken
  pruefe("auf gestern getrunken: fuenf, der gemerkte Tag bleibt",
         schedule_count() == 5 && persist_read_int(FACH_TAG) == 20261003);
  // Das Telefon nennt seine (echte) Zeit, bevor es die Uhr gestellt hat.
  pruefe("Telefon 11 h voraus: nicht bestaetigt", !schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 3, 10, 25, 0)));
  pruefe("nicht bestaetigt: der gemerkte Tag bleibt", persist_read_int(FACH_TAG) == 20261003);
  start(um(2026, 10, 3, 10, 30, 0));                    // das Telefon stellt die Uhr
  pruefe("Uhr gestellt: alle fuenf Glaeser und das Ziel stehen",
         schedule_count() == 5 && schedule_goal() == soll + 1);
  pruefe("Uhr gestellt: nicht mehr fraglich", !schedule_uhr_fraglich());
  stub_jetzt = um(2026, 10, 4, 0, 0, 0);                // echter Folgetag, App offen
  pruefe("echter Folgetag: null", schedule_count() == 0);

  printf("\nVorlauf (M10): ohne Telefon bleibt der vorausgeeilte Tag\n");
  frisch(um(2026, 10, 3, 8, 0, 0));
  start(um(2026, 10, 4, 9, 0, 0));                      // Uhr einen Tag voraus
  pruefe("einen Tag voraus: fuer die Uhr ein neuer Tag", schedule_day() == 20261004);
  pruefe("einen Tag voraus: nicht fraglich", !schedule_uhr_fraglich());
  trinke(2);                                            // waehrend des Vorlaufs
  start(um(2026, 10, 3, 9, 30, 0));                     // zurueckgestellt
  trinke(1);                                            // nach dem Zurueckstellen
  pruefe("zurueckgestellt: die Uhr fragt, drei Glaeser am gemerkten Tag",
         schedule_uhr_fraglich() && schedule_count() == 3 && persist_read_int(FACH_TAG) == 20261004);
  start(um(2026, 10, 4, 8, 0, 0));                      // echter Folgetag
  pruefe("ohne Bestaetigung: am echten Folgetag stehen sie noch (Grenze)", schedule_count() == 3);

  printf("\nVorlauf (M10): das Telefon bestaetigt die zurueckgestellte Uhr\n");
  frisch(um(2026, 10, 3, 8, 0, 0));
  start(um(2026, 10, 4, 9, 0, 0));                      // Uhr einen Tag voraus
  trinke(2);
  schedule_raise_goal();
  start(um(2026, 10, 3, 9, 30, 0));                     // zurueckgestellt
  pruefe("Telefonzeit passt: bestaetigt", schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 3, 9, 30, 0)));
  pruefe("bestaetigt: der Tag ist heute", schedule_day() == 20261003 && persist_read_int(FACH_TAG) == 20261003);
  pruefe("bestaetigt: Glaeser und Ziel bleiben", schedule_count() == 2 && schedule_goal() == soll + 1);
  pruefe("bestaetigt: nicht mehr fraglich", !schedule_uhr_fraglich());
  pruefe("ein zweites Mal: nichts zu tun", !schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 3, 9, 30, 0)));
  trinke(1);
  start(um(2026, 10, 3, 20, 0, 0));
  pruefe("Neustart am selben Tag: drei Glaeser", schedule_count() == 3);
  stub_jetzt = um(2026, 10, 4, 8, 0, 0);                // echter Folgetag, App offen
  pruefe("echter Folgetag, App offen: null und das Soll", schedule_count() == 0 && schedule_goal() == soll);
  start(um(2026, 10, 4, 8, 5, 0));
  pruefe("echter Folgetag, Neustart: null", schedule_count() == 0 && schedule_day() == 20261004);

  frisch(um(2026, 10, 3, 8, 0, 0));
  start(um(2026, 10, 5, 9, 0, 0));                      // zwei Tage voraus
  start(um(2026, 10, 3, 12, 0, 0));                     // zurueckgestellt
  pruefe("zwei Tage voraus, zurueckgestellt: fraglich, der Tag bleibt",
         schedule_uhr_fraglich() && persist_read_int(FACH_TAG) == 20261005);
  pruefe("zwei Tage voraus: bestaetigt", schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 3, 12, 0, 10)));
  schedule_raise_goal();
  start(um(2026, 10, 4, 10, 0, 0));
  pruefe("zwei Tage voraus, bestaetigt: am echten Folgetag das Soll", schedule_goal() == soll);

  frisch(um(2026, 10, 3, 8, 0, 0));
  start(um(2026, 10, 4, 9, 0, 0));
  start(um(2026, 10, 3, 9, 30, 0));
  schedule_set_target(10);
  trinke(1);
  pruefe("Einstellung und Glas ziehen den Tag nicht zurueck", persist_read_int(FACH_TAG) == 20261004);

  printf("\nBestaetigen: hoechstens 300 s Abweichung, derselbe Tag\n");
  {
    const time_t t = um(2026, 10, 3, 8, 0, 0);
    frisch(um(2026, 10, 4, 8, 0, 0));                   // gemerkt der 04.10. ...
    start(t);                                           // ... die Uhr auf dem 03.10.
    attrappe_log_leeren();
    pruefe("Telefon 301 s voraus: nicht", !schedule_uhr_bestaetigt((uint32_t)(t + 301)) &&
           persist_read_int(FACH_TAG) == 20261004);
    pruefe("die Abweichung steht im Log", strstr(attrappe_log_text, "Telefonzeit weicht -301 s ab") != NULL);
    pruefe("Telefon 301 s zurueck: nicht", !schedule_uhr_bestaetigt((uint32_t)(t - 301)));
    pruefe("Telefon 300 s voraus: bestaetigt", schedule_uhr_bestaetigt((uint32_t)(t + 300)) &&
           persist_read_int(FACH_TAG) == 20261003);
    frisch(um(2026, 10, 4, 8, 0, 0));
    start(t);
    pruefe("Telefon 300 s zurueck: bestaetigt", schedule_uhr_bestaetigt((uint32_t)(t - 300)));
    frisch(um(2026, 10, 5, 8, 0, 0));                   // gemerkt der 05.10.
    start(um(2026, 10, 3, 23, 59, 0));
    pruefe("90 s, aber Mitternacht dazwischen: nicht",
           !schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 4, 0, 0, 30)));
    pruefe("30 s, derselbe Tag: bestaetigt", schedule_uhr_bestaetigt((uint32_t)um(2026, 10, 3, 23, 59, 30)) &&
           persist_read_int(FACH_TAG) == 20261003);
    frisch(t);
    pruefe("Uhr auf dem gemerkten Tag: nicht fraglich", !schedule_uhr_fraglich());
    pruefe("Uhr auf dem gemerkten Tag: Bestaetigen aendert nichts", !schedule_uhr_bestaetigt((uint32_t)t));
    start(um(2000, 1, 1, 12, 0, 0));
    pruefe("Uhr im Jahr 2000: nicht fraglich", !schedule_uhr_fraglich());
    pruefe("Uhr im Jahr 2000: auch passende Telefonzeit gibt keinen Tag",
           !schedule_uhr_bestaetigt((uint32_t)um(2000, 1, 1, 12, 0, 0)) && persist_read_int(FACH_TAG) == 20261003);
  }

  printf("\nEin neuer Tag bei offener App wird gespeichert\n");
  frisch(um(2026, 10, 3, 20, 0, 0));
  trinke(3);
  schedule_raise_goal();                                // Ziel 9 fuer den 03.10.
  stub_jetzt = um(2026, 10, 4, 7, 0, 0);                // App offen ueber Nacht
  pruefe("neuer Tag bei offener App: null und das Soll", schedule_count() == 0 && schedule_goal() == soll);
  pruefe("im Persist: Tag, Zaehler und Ziel des neuen Tages",
         persist_read_int(FACH_TAG) == 20261004 && persist_read_int(FACH_ZAEHLER) == 0 &&
         persist_read_int(FACH_ZIEL) == soll);
  start(um(2026, 10, 4, 7, 5, 0));                      // Neustart am neuen Tag
  pruefe("Neustart am neuen Tag: das Soll, nicht das Ziel von gestern", schedule_goal() == soll);

  // OHNE VORHERIGES LESEN. Die untere Taste ruft schedule_raise_goal direkt
  // (main_window.c), und seit Mitternacht muss nichts neu gezeichnet haben.
  // Erhoehte sie das Ziel des alten Tages, fiele der Druck beim naechsten
  // Lesen mit dem neuen Tag weg.
  printf("\nNach Mitternacht prueft jede Aenderung zuerst den Tag\n");
  frisch(um(2026, 10, 3, 20, 0, 0));
  trinke(3);
  schedule_raise_goal();                                // Ziel 9 fuer den 03.10.
  stub_jetzt = um(2026, 10, 4, 0, 0, 30);
  schedule_raise_goal();                                // erster Druck des neuen Tages
  pruefe("Taste unten gleich nach Mitternacht: im Persist das Soll plus eins, am neuen Tag",
         persist_read_int(FACH_TAG) == 20261004 && persist_read_int(FACH_ZIEL) == soll + 1);
  pruefe("Taste unten gleich nach Mitternacht: der Druck bleibt", schedule_goal() == soll + 1 &&
         schedule_count() == 0);
  frisch(um(2026, 10, 3, 20, 0, 0));
  trinke(3);
  stub_jetzt = um(2026, 10, 4, 0, 0, 30);
  schedule_set_count(1);                                // ein Glas, ohne vorher zu lesen
  pruefe("Glas gleich nach Mitternacht: im Persist eins, am neuen Tag",
         persist_read_int(FACH_TAG) == 20261004 && persist_read_int(FACH_ZAEHLER) == 1);
  pruefe("Glas gleich nach Mitternacht: es bleibt", schedule_count() == 1);

  printf("%s\n", s_fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return s_fehler ? 1 : 0;
}
