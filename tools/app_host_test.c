// Die App als Ganzes (drinktervall.c mit Erinnerungen, Kaffee-Fenster und
// Trink-Fenster) - auf dem Rechner, mit Attrappen fuer Fenster, Tasten,
// Start, Glance und AppMessage (tools/host).
//
//   sh tools/app_host_test.sh       (Europe/Zurich, Europe/London, America/New_York)
//
// Was hier leicht falsch und teuer ist:
//
//   - GLANCE (Audit N4): bis 1.20 lief die eine Scheibe nie ab; wer die App
//     abends schloss, sah nach Mitternacht den Vortag ("6 von 8"). Jetzt
//     laeuft der Stand um Mitternacht ab, und der Morgen liegt als zweite
//     Scheibe bereit - auch in der Nacht der Umstellung.
//   - PIN VON GESTERN (Audit N5, so entschieden): "Nachholen" an einem Pin
//     von gestern zaehlt kein Glas fuer heute. Ein Pin von heute oder die
//     naechste Erinnerung von morgen zaehlt, ein alter Pin ohne Tag wie
//     bisher.
//   - TAGESZIEL ERREICHT (Audit N5): ein Wasser-Wecker, der noch stand,
//     bleibt still - kein Fenster, kein Vibrieren.
//   - NEUE KAFFEE-ERINNERUNG NACH DEM HAKEN (Audit N7): bis 1.20 verwarf
//     "Enjoy!" sie, und die App ging mitsamt Fenster zu.
//
// Den Hauptscreen gibt es hier nicht - main_window_push tut nichts. Was er
// zeigt, pruefen die Emulator-Bilder.
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include <sys/wait.h>
#include <unistd.h>
#include "schedule.h"
#include "coffee.h"
#include "strings.h"

time_t stub_jetzt;
void main_window_push(void) {}
void main_window_refresh(void) {}
int dt_main(void);   // main() aus drinktervall.c, umbenannt (app_host_test.sh)

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

static time_t lokal(int jahr, int monat, int tag, int std, int min) {
  struct tm t = { .tm_year = jahr - 1900, .tm_mon = monat - 1, .tm_mday = tag,
                  .tm_hour = std, .tm_min = min, .tm_isdst = -1 };
  return mktime(&t);
}

static Tuple *feld(uint32_t key) {
  return attrappe_letzte() ? dict_find(attrappe_letzte(), key) : NULL;
}
static int32_t zahl(uint32_t key) {
  Tuple *t = feld(key);
  return t ? t->value->int32 : -999;
}
static const char *text(uint32_t key) {
  Tuple *t = feld(key);
  return t && t->type == TUPLE_CSTRING ? t->value->cstring : "";
}

// Frisch installiert, zur Uhrzeit `jetzt`.
static void vorbereiten(time_t jetzt, bool animation) {
  attrappe_persist_leeren();
  attrappe_nachrichten_leeren();
  attrappe_log_leeren();
  stub_jetzt = jetzt;
  strings_refresh();
  schedule_init();
  coffee_init();
  schedule_set_animation(animation);
}

// Was vom letzten Lauf noch tickt, zu Ende laufen lassen - das Telefon
// bestaetigt alles.
static void ausklingen(void) {
  for (int i = 0; i < 100 && (attrappe_zeitgeber_offen() || attrappe_unterwegs()); i++) {
    attrappe_ack();
    attrappe_zeitgeber_ablaufen();
  }
}

static void starten(AppLaunchReason grund, uint32_t args, int32_t cookie, void (*ereignisse)(void)) {
  attrappe_start_grund = grund;
  attrappe_start_args = args;
  attrappe_start_cookie = cookie;
  attrappe_ereignisse = ereignisse;
  dt_main();
}

static void abschnitt_glance(void) {
  printf("\nGlance (N4): der Stand von heute laeuft um Mitternacht ab\n");
  // Der 24.10.2026 - in Europa die Nacht der Umstellung.
  vorbereiten(lokal(2026, 10, 24, 23, 0), true);
  for (int i = 0; i < 6; i++) schedule_set_count(schedule_count() + 1);
  starten(APP_LAUNCH_USER, 0, 0, NULL);
  const time_t mitternacht = lokal(2026, 10, 25, 0, 0);
  pruefe("zwei Scheiben", attrappe_glance_anzahl() == 2);
  pruefe("heute: \"6 of 8 glasses\", laeuft um 00:00 ab",
         strncmp(attrappe_glance_text(0), "6 of 8 glasses", 14) == 0 && attrappe_glance_ablauf(0) == mitternacht);
  int std = -1, min = -1;
  const bool gelesen = sscanf(attrappe_glance_text(1), "0 of 8 glasses, next %d:%d", &std, &min) == 2;
  char was[96];
  snprintf(was, sizeof(was), "danach: \"%s\", laeuft nicht ab", attrappe_glance_text(1));
  pruefe(was, gelesen && std * 60 + min >= 8 * 60 && std * 60 + min <= 8 * 60 + 10 &&
         attrappe_glance_ablauf(1) == APP_GLANCE_SLICE_NO_EXPIRATION);
  ausklingen();
  attrappe_glance_grenze = 1;
  starten(APP_LAUNCH_USER, 0, 0, NULL);
  pruefe("nur eine Scheibe erlaubt: sie laeuft trotzdem um 00:00 ab",
         attrappe_glance_anzahl() == 1 && attrappe_glance_ablauf(0) == mitternacht);
}

static void abschnitt_pin_tag(void) {
  printf("\nPin-Aktion (N5): ein Pin von gestern zaehlt nicht fuer heute\n");
  static const struct { uint32_t code; int dazu; const char *was; } faelle[] = {
    { 202610031, 0, "\"Nachholen\" am Pin von gestern (202610031): kein Glas" },
    { 202609301, 0, "Pin vom Vormonat (202609301): kein Glas" },
    { 202610041, 1, "\"Getrunken\" am Pin von heute (202610041): ein Glas" },
    { 202610051, 1, "naechste Erinnerung von morgen (202610051): ein Glas fuer heute" },
    { 1, 1, "alter Pin ohne Tag (1): ein Glas wie bisher" },
    { 2, 0, "\"App oeffnen\" (2): kein Glas" },
  };
  for (unsigned i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    vorbereiten(lokal(2026, 10, 4, 9, 0), false);
    schedule_set_count(2);
    starten(APP_LAUNCH_TIMELINE_ACTION, faelle[i].code, 0, NULL);
    pruefe(faelle[i].was, schedule_count() == 2 + faelle[i].dazu);
    ausklingen();
  }
}

static void abschnitt_ziel(void) {
  printf("\nTagesziel erreicht (N5): ein alter Wasser-Wecker bleibt still\n");
  vorbereiten(lokal(2026, 10, 3, 14, 0), true);
  while (schedule_count() < schedule_goal()) schedule_set_count(schedule_count() + 1);
  starten(APP_LAUNCH_WAKEUP, 0, 4, NULL);
  pruefe("kein Fenster, kein Vibrieren", attrappe_fenster_gezeigt() == 0 && attrappe_vibes_doppelt() == 0);
  pruefe("steht im Log", strstr(attrappe_log_text, "Wasser-Erinnerung 4 entfaellt") != NULL);
  ausklingen();
  // Gegenprobe: ein Glas unter dem Ziel kommt die Erinnerung.
  vorbereiten(lokal(2026, 10, 3, 14, 0), true);
  schedule_set_count(schedule_goal() - 1);
  starten(APP_LAUNCH_WAKEUP, 0, 4, NULL);
  pruefe("Gegenprobe, ein Glas fehlt: Erinnerung mit Vibrieren",
         attrappe_fenster_gezeigt() == 1 && attrappe_vibes_doppelt() == 1);
}

static void ereignisse_kaffee(void) {
  pruefe("Kaffee-Erinnerung offen, einmal vibriert", attrappe_fenster_offen() == 1 && attrappe_vibes_doppelt() == 1);
  attrappe_taste(BUTTON_ID_SELECT);                  // Haken: "Enjoy!", Fenster schliesst gleich
  attrappe_ack();
  attrappe_wecker_ausloesen(SCHEDULE_COOKIE_CUSTOM + 0);
  pruefe("der naechste Wecker kommt dazwischen: vibriert wieder", attrappe_vibes_doppelt() == 2);
  ausklingen();
  pruefe("das alte Schliessen schliesst die neue Erinnerung nicht", attrappe_fenster_offen() == 1);
  attrappe_taste(BUTTON_ID_SELECT);
  pruefe("ihr Haken traegt das eigene Getraenk ein",
         zahl(MESSAGE_KEY_COFFEE_KIND) == COFFEE_KIND_CUSTOM && strcmp(text(MESSAGE_KEY_DRINK_NAME), "Mate") == 0);
}

static void abschnitt_kaffee(void) {
  printf("\nNeue Kaffee-Erinnerung nach dem Haken (N7)\n");
  vorbereiten(lokal(2026, 10, 3, 8, 30), false);
  const uint8_t plan[] = { 1, 510 & 0xFF, 510 >> 8, CoffeeCoffee, 0 };
  coffee_from_bytes(plan, sizeof(plan));
  custom_from_string("Mate|20|80|511");
  starten(APP_LAUNCH_WAKEUP, 0, SCHEDULE_COOKIE_COFFEE + 0, ereignisse_kaffee);
}

// Jeder Abschnitt in einem eigenen Prozess: frische statische Variablen wie
// bei jedem Start auf der Uhr.
static void (*const ABSCHNITTE[])(void) = {
  abschnitt_glance, abschnitt_pin_tag, abschnitt_ziel, abschnitt_kaffee,
};

int main(void) {
  const char *tz = getenv("TZ");
  printf("\nZeitzone %s\n", tz ? tz : "(keine)");
  int fehler = 0;
  for (unsigned i = 0; i < sizeof(ABSCHNITTE) / sizeof(ABSCHNITTE[0]); i++) {
    fflush(stdout);
    const pid_t kind = fork();
    if (kind == 0) {
      ABSCHNITTE[i]();
      fflush(stdout);
      _exit(s_fehler > 100 ? 100 : s_fehler);
    }
    int status = 0;
    waitpid(kind, &status, 0);
    fehler += WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }
  printf("%s\n", fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return fehler ? 1 : 0;
}
