// Wecker, Plan-Liste und Pins (schedule.c, coffee.c) - auf dem Rechner, unter
// Zeitzonen mit Sommerzeit.
//
//   sh tools/plan_host_test.sh      (Europe/Zurich, Europe/London, America/New_York)
//
// Was hier leicht falsch und teuer ist:
//
//   - DER TAG HAT NICHT IMMER 24 STUNDEN (Audit M2). Bis 1.20 war "morgen"
//     Mitternacht + 86400 und eine Uhrzeit Mitternacht + Minuten*60. Am
//     25.10.2026 in Zuerich kamen die Wecker vom Vorabend eine Stunde zu
//     frueh (Glas 1 um 07:00, der Kaffee von 08:30 um 07:30), die Plan-Liste
//     zeigte vor 03:00 andere Zeiten als danach, und nach der
//     Fruehlings-Umstellung galt der Versatz (Jitter) des Vortags. Wecker,
//     Plan-Liste und Pins muessen an JEDEM Tag dieselben Zeiten zeigen.
//   - AN NORMALEN TAGEN BLEIBT ALLES, WIE ES WAR: die Zeiten unten sind mit
//     1.20 gerechnet (unter UTC, wo es keine Umstellung gibt) und hier
//     festgenagelt - samt der Tage, an denen 1.20 sich verrechnete.
//   - TAGESZIEL ERREICHT (Audit N5, so entschieden): fuer den Rest des Tages
//     keine Wasser-Erinnerung mehr, auch kein "Spaeter" dazu; Kaffee und
//     eigene Getraenke bleiben. Nach dem Erhoehen des Ziels kommen sie wieder.
//
// Das Orakel ist die C-Bibliothek (mktime, localtime_r), nicht schedule.c.
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include "schedule.h"
#include "coffee.h"
#include "strings.h"

time_t stub_jetzt;

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

// Ortszeit nach der Zeitzone des Laufs; Tage ueber das Monatsende rechnet
// mktime selbst.
static time_t lokal(int jahr, int monat, int tag, int std, int min) {
  struct tm t = { .tm_year = jahr - 1900, .tm_mon = monat - 1, .tm_mday = tag,
                  .tm_hour = std, .tm_min = min, .tm_isdst = -1 };
  return mktime(&t);
}
static int wand(time_t t) {
  struct tm x;
  localtime_r(&t, &x);
  return x.tm_hour * 60 + x.tm_min;
}
static int32_t datum(time_t t) {
  struct tm x;
  localtime_r(&t, &x);
  return (x.tm_year + 1900) * 10000 + (x.tm_mon + 1) * 100 + x.tm_mday;
}
static const char *zeit(time_t t) {
  static char puffer[4][40];
  static int n;
  n = (n + 1) % 4;
  struct tm x;
  localtime_r(&t, &x);
  strftime(puffer[n], sizeof(puffer[n]), "%d.%m. %H:%M %Z", &x);
  return puffer[n];
}

// Kaffee um 00:30 (vor jeder Umstellung) und 08:30, Tee um 10:00 - wie
// schedule_plan_wakeups sie mit dem Wasser mischt.
#define KAFFEE_NACHT  (SCHEDULE_COOKIE_COFFEE + 0)
#define KAFFEE_MORGEN (SCHEDULE_COOKIE_COFFEE + 1)
#define TEE           (SCHEDULE_COOKIE_CUSTOM + 0)
static void einrichten(time_t jetzt) {
  attrappe_persist_leeren();
  stub_jetzt = jetzt;
  strings_refresh();
  schedule_init();
  coffee_init();
  const uint8_t plan[] = { 2, 30, 0, CoffeeEspresso, 0, 510 & 0xFF, 510 >> 8, CoffeeCoffee, 0 };
  coffee_from_bytes(plan, sizeof(plan));
  custom_from_string("Tee|0|0|600");
}

// Die Plan-Liste des Tages, in dem `jetzt` liegt - so rechnen sie
// plan_window.c und phone.c (SLOTS).
static int liste(time_t jetzt, time_t *aus) {
  stub_jetzt = jetzt;
  const time_t tag = schedule_tag(jetzt);
  for (int i = 0; i < schedule_target(); i++) aus[i] = schedule_slot(tag, i);
  return schedule_target();
}

static void tag_pruefen(int jahr, int monat, int tag) {
  const time_t mittag = lokal(jahr, monat, tag, 12, 0);
  const int32_t d = datum(mittag);
  time_t l[DT_GLASSES_MAX];
  const int n = liste(mittag, l);
  char was[160];

  bool ok = true;
  for (int i = 0; i < n; i++) {
    ok = ok && datum(l[i]) == d && wand(l[i]) >= DT_START_HOUR * 60 && wand(l[i]) <= DT_END_HOUR * 60;
    if (i) ok = ok && l[i] > l[i - 1];
  }
  snprintf(was, sizeof(was), "%d: Liste am Tag, 08:00 bis 20:00, aufsteigend (Glas 1 %s)", (int)d, zeit(l[0]));
  pruefe(was, ok);

  // Dieselbe Liste zu jeder Stunde des Tages - auch vor der Umstellung.
  static const int stunden[][2] = { { 0, 30 }, { 1, 30 }, { 3, 30 }, { 23, 30 } };
  ok = true;
  for (unsigned s = 0; s < sizeof(stunden) / sizeof(stunden[0]); s++) {
    const time_t jetzt = lokal(jahr, monat, tag, stunden[s][0], stunden[s][1]);
    time_t m[DT_GLASSES_MAX];
    liste(jetzt, m);
    for (int i = 0; i < n; i++) {
      if (m[i] != l[i]) {
        printf("           um %s: Glas %d %s statt %s\n", zeit(jetzt), i + 1, zeit(m[i]), zeit(l[i]));
        ok = false;
        break;
      }
    }
  }
  snprintf(was, sizeof(was), "%d: um 00:30, 01:30, 03:30 und 23:30 dieselbe Liste wie mittags", (int)d);
  pruefe(was, ok);

  // Die Wecker, gestellt am Vorabend um 21:00.
  stub_jetzt = lokal(jahr, monat, tag - 1, 21, 0);
  schedule_plan_wakeups(0, 0);
  int wasser = 0, kaffee = 0;
  ok = true;
  for (int k = 0; k < attrappe_wecker_anzahl(); k++) {
    const time_t z = attrappe_wecker_zeit(k);
    const int32_t c = attrappe_wecker_cookie(k);
    if (datum(z) != d) continue;
    bool passt = true;
    if (c < DT_GLASSES_MAX) {
      wasser++;
      passt = c < n && z == l[c];
    } else if (c == KAFFEE_NACHT || c == KAFFEE_MORGEN || c == TEE) {
      kaffee++;
      passt = wand(z) == (c == KAFFEE_NACHT ? 30 : c == KAFFEE_MORGEN ? 510 : 600);
    }
    if (!passt) {
      printf("           Wecker %d um %s passt nicht zur Liste (Glas 1 %s)\n", (int)c, zeit(z), zeit(l[0]));
      ok = false;
    }
  }
  snprintf(was, sizeof(was), "%d: %d Wasser-Wecker vom Vorabend = Liste, %d Kaffee/Tee zur Uhrzeit",
           (int)d, wasser, kaffee);
  pruefe(was, ok && wasser >= 4 && kaffee == 3);

  // Pin und Glance vom Vorabend: die naechste Erinnerung ist Glas 1.
  time_t naechste = 0;
  schedule_next(stub_jetzt, &naechste);
  snprintf(was, sizeof(was), "%d: naechste Erinnerung vom Vorabend = Glas 1 (%s)", (int)d, zeit(naechste));
  pruefe(was, naechste == l[0]);
}

// Wanduhrzeiten (Minute des Tages), gerechnet von 1.20 unter TZ=UTC.
typedef struct {
  int32_t tag;
  int soll;
  int minuten[16];
} Fest;
static const Fest FEST[] = {
  // Normale Tage: muessen bitgleich bleiben.
  { 20261003, 8,  { 488, 569, 659, 752, 830, 932, 1028, 1112 } },
  { 20270115, 8,  { 485, 569, 653, 756, 845, 924, 1023, 1117 } },
  { 20261003, 13, { 488, 534, 589, 647, 690, 757, 818, 867, 915, 974, 1025, 1087, 1137 } },
  // Umstellungstage: so sieht der Tag aus, wenn er richtig gerechnet ist.
  { 20261025, 8,  { 480, 573, 661, 750, 838, 929, 1019, 1119 } },
  { 20270328, 8,  { 490, 561, 655, 759, 847, 939, 1026, 1105 } },
  { 20260329, 8,  { 485, 571, 657, 752, 833, 926, 1026, 1113 } },
  { 20261101, 8,  { 481, 571, 667, 751, 831, 927, 1012, 1112 } },
  { 20260308, 8,  { 480, 569, 652, 757, 836, 924, 1019, 1116 } },
};

static void abschnitt_fest(void) {
  printf("\nZeiten wie 1.20 an normalen Tagen, richtig an Umstellungstagen\n");
  einrichten(lokal(2026, 10, 3, 9, 0));
  for (unsigned f = 0; f < sizeof(FEST) / sizeof(FEST[0]); f++) {
    schedule_set_target(FEST[f].soll);
    const int32_t t = FEST[f].tag;
    time_t l[DT_GLASSES_MAX];
    const int n = liste(lokal(t / 10000, (t / 100) % 100, t % 100, 12, 0), l);
    bool ok = n == FEST[f].soll;
    for (int i = 0; ok && i < n; i++) ok = wand(l[i]) == FEST[f].minuten[i];
    char was[96];
    snprintf(was, sizeof(was), "%d, Soll %d: Glas 1 um %02d:%02d wie festgenagelt", (int)t, FEST[f].soll,
             wand(l[0]) / 60, wand(l[0]) % 60);
    pruefe(was, ok);
  }
  schedule_set_target(DT_GLASSES_DEFAULT);
}

// Tage um jede Umstellung in Europa (letzter Sonntag im Maerz und Oktober)
// und in den USA (zweiter Sonntag im Maerz, erster im November), mit Rand.
static void abschnitt_umstellung(void) {
  printf("\nUmstellungstage und ihre Nachbarn\n");
  einrichten(lokal(2026, 3, 1, 9, 0));
  static const int bereiche[][4] = {
    { 2026, 3, 6, 5 }, { 2026, 3, 27, 5 }, { 2026, 10, 20, 15 }, { 2027, 3, 24, 8 },
  };
  for (unsigned b = 0; b < sizeof(bereiche) / sizeof(bereiche[0]); b++) {
    for (int k = 0; k < bereiche[b][3]; k++) {
      const time_t t = lokal(bereiche[b][0], bereiche[b][1], bereiche[b][2] + k, 12, 0);
      const int32_t d = datum(t);
      tag_pruefen(d / 10000, (d / 100) % 100, d % 100);
    }
  }
}

static int wecker_am(int32_t tag, int32_t von, int32_t bis) {
  int n = 0;
  for (int k = 0; k < attrappe_wecker_anzahl(); k++) {
    const int32_t c = attrappe_wecker_cookie(k);
    if (datum(attrappe_wecker_zeit(k)) == tag && c >= von && c <= bis) n++;
  }
  return n;
}
static bool wecker_um(time_t z, int32_t cookie) {
  for (int k = 0; k < attrappe_wecker_anzahl(); k++) {
    if (attrappe_wecker_zeit(k) == z && attrappe_wecker_cookie(k) == cookie) return true;
  }
  return false;
}

static void abschnitt_ziel(void) {
  printf("\nTagesziel erreicht (N5): keine Wasser-Erinnerung mehr fuer heute\n");
  const time_t jetzt = lokal(2026, 10, 3, 9, 0);
  einrichten(jetzt);
  const int32_t heute = datum(jetzt), morgen = datum(lokal(2026, 10, 4, 12, 0));
  schedule_plan_wakeups(0, 0);
  pruefe("vorher: Wasser-Wecker fuer heute", wecker_am(heute, 0, DT_GLASSES_MAX - 1) > 0);
  while (schedule_count() < schedule_goal()) schedule_set_count(schedule_count() + 1);
  // Kein eigenes Neuplanen: das Glas allein muss die Wecker nachziehen, wie
  // nach "Getrunken" in der Erinnerung oder auf dem Hauptscreen.
  pruefe("Ziel erreicht: kein Wasser-Wecker mehr fuer heute", wecker_am(heute, 0, DT_GLASSES_MAX - 1) == 0);
  pruefe("Ziel erreicht: der Tee um 10:00 bleibt", wecker_am(heute, TEE, TEE) == 1);
  pruefe("Ziel erreicht: morgen wieder Wasser", wecker_am(morgen, 0, DT_GLASSES_MAX - 1) > 0);
  schedule_plan_wakeups(0, 0);
  pruefe("auch neu geplant: kein Wasser-Wecker fuer heute", wecker_am(heute, 0, DT_GLASSES_MAX - 1) == 0);
  time_t naechste = 0;
  schedule_next(jetzt, &naechste);
  pruefe("naechste Erinnerung (Pin, Glance): morgen frueh",
         datum(naechste) == morgen && wand(naechste) >= 8 * 60 && wand(naechste) <= 8 * 60 + 10);
  schedule_plan_wakeups(jetzt + 600, schedule_snooze_cookie());
  pruefe("Spaeter beim Wasser: kein Wecker", wecker_am(heute, schedule_snooze_cookie(), schedule_snooze_cookie()) == 0);
  schedule_plan_wakeups(jetzt + 600, KAFFEE_MORGEN);
  pruefe("Spaeter beim Kaffee: bleibt", wecker_um(jetzt + 600, KAFFEE_MORGEN));
  schedule_raise_goal();
  pruefe("Ziel erhoeht: Wasser-Wecker fuer heute wieder da", wecker_am(heute, 0, DT_GLASSES_MAX - 1) > 0);
  schedule_next(jetzt, &naechste);
  pruefe("Ziel erhoeht: naechste Erinnerung wieder heute", datum(naechste) == heute);
}

int main(void) {
  const char *tz = getenv("TZ");
  printf("\nZeitzone %s\n", tz ? tz : "(keine)");
  // OHNE ZEITZONENDATEN prueft dieser Test nichts: die C-Bibliothek fiele
  // still auf UTC zurueck, und es gaebe keine Umstellung.
  const time_t sommer = lokal(2026, 7, 1, 12, 0), winter = lokal(2026, 1, 15, 12, 0);
  struct tm s, w;
  localtime_r(&sommer, &s);
  localtime_r(&winter, &w);
  pruefe("die Zeitzone kennt Sommerzeit", s.tm_isdst == 1 && w.tm_isdst == 0 && s.tm_gmtoff != w.tm_gmtoff);
  abschnitt_fest();
  abschnitt_umstellung();
  abschnitt_ziel();
  printf("%s\n", s_fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return s_fehler ? 1 : 0;
}
