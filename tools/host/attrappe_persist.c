// Persist im Speicher fuer die Host-Tests - so viele Faecher, wie die App
// belegt (config.h, DT_PERSIST_*, bis 13), mit Luft. Wie auf der Uhr nimmt
// ein Fach hoechstens 256 Byte (PERSIST_DATA_MAX_LENGTH).
#include <pebble.h>

#define FAECHER 24
#define FACH_MAX 256
static struct { bool da; uint8_t daten[FACH_MAX]; size_t laenge; } s_persist[FAECHER];

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
  if (key >= FAECHER || size > FACH_MAX) return -1;
  s_persist[key].da = true;
  s_persist[key].laenge = size;
  memcpy(s_persist[key].daten, data, size);
  return (int)size;
}
int persist_write_int(uint32_t key, int32_t value) { return persist_write_data(key, &value, sizeof(value)); }
int persist_write_bool(uint32_t key, bool value) { return persist_write_int(key, value ? 1 : 0); }
int persist_delete(uint32_t key) { if (key < FAECHER) s_persist[key].da = false; return 0; }
void attrappe_persist_leeren(void) { memset(s_persist, 0, sizeof(s_persist)); }
