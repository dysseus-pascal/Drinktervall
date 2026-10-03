// Der Tag zum Tagesziel (schedule.c) - auf dem Rechner.
//
//   sh tools/schedule_host_test.sh
//
// Die Uhr schickt mit dem Tagesziel (GLASSES) seinen Tag (GOAL_DAY). Das
// erhoehte Ziel gilt nur fuer diesen Tag; das Telefon nahm bis dahin den Tag
// der Ankunft und hob so nach Mitternacht das Ziel des neuen Tages an.
//
// Was hier leicht falsch ist: der Tag muss DER DES ZIELS sein, nicht der der
// Uhrzeit. Steht die Uhr nach einem Neustart kurz zu frueh, behaelt sie Ziel
// und Zaehler des gemerkten Tages - also muss auch der gemerkte Tag hinaus.
//
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include "schedule.h"
#include "coffee.h"

time_t stub_jetzt;

// --- Persist im Speicher ---
#define FAECHER 16
static struct { bool da; uint8_t daten[256]; size_t laenge; } s_persist[FAECHER];

bool persist_exists(uint32_t key) { return key < FAECHER && s_persist[key].da; }
int persist_read_data(uint32_t key, void *buf, size_t size) {
  if (!persist_exists(key)) return -1;
  const size_t n = s_persist[key].laenge < size ? s_persist[key].laenge : size;
  memcpy(buf, s_persist[key].daten, n);
  return (int)n;
}
int32_t persist_read_int(uint32_t key) {
  int32_t v = 0;
  if (persist_exists(key)) memcpy(&v, s_persist[key].daten, sizeof(v));
  return v;
}
bool persist_read_bool(uint32_t key) { return persist_read_int(key) != 0; }
int persist_write_data(uint32_t key, const void *data, size_t size) {
  s_persist[key].da = true;
  s_persist[key].laenge = size;
  memcpy(s_persist[key].daten, data, size);
  return (int)size;
}
int persist_write_int(uint32_t key, int32_t value) { return persist_write_data(key, &value, sizeof(value)); }
int persist_write_bool(uint32_t key, bool value) { return persist_write_int(key, value ? 1 : 0); }
int persist_delete(uint32_t key) { s_persist[key].da = false; return 0; }

// Kaffees und eigene Getraenke spielen hier keine Rolle.
int coffee_count(void) { return 0; }
time_t coffee_time(time_t midnight, int idx) { (void)idx; return midnight; }
int custom_count(void) { return 0; }
time_t custom_time(time_t midnight, int idx) { (void)idx; return midnight; }

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

// 03.10.2026, 23:59 UTC - der Test laeuft mit TZ=UTC.
#define SPAET 1791071940

int main(void) {
  stub_jetzt = SPAET;
  schedule_init();
  const int soll = schedule_target();
  pruefe("frisch: der Tag ist heute", schedule_day() == 20261003);
  pruefe("frisch: das Ziel ist das Soll", schedule_goal() == soll);

  schedule_raise_goal();
  pruefe("erhoeht: Ziel und Tag gehoeren zusammen", schedule_goal() == soll + 1 && schedule_day() == 20261003);

  // Neustart eine Minute nach Mitternacht: neuer Tag, das Soll gilt wieder.
  stub_jetzt = SPAET + 120;
  schedule_init();
  pruefe("neuer Tag: der Tag ist morgen", schedule_day() == 20261004);
  pruefe("neuer Tag: das Ziel ist wieder das Soll", schedule_goal() == soll);
  schedule_raise_goal();

  // Neustart, die Uhr steht kurz einen Tag zu frueh: Ziel und Zaehler bleiben
  // die des gemerkten Tages - und dessen Tag geht hinaus.
  stub_jetzt = SPAET - 600;
  schedule_init();
  pruefe("Uhr zu frueh: Ziel des gemerkten Tages", schedule_goal() == soll + 1);
  pruefe("Uhr zu frueh: der gemerkte Tag", schedule_day() == 20261004);

  printf("%s\n", s_fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return s_fehler ? 1 : 0;
}
