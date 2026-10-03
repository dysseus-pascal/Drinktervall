// Nur fuer die Host-Tests in tools/: so viel vom Pebble-SDK, wie schedule.c
// und seine Kopfdateien brauchen. Persist liegt im Speicher, die Uhrzeit
// stellt der Test, Wecker gehen ins Leere.
//
// WARUM NICHT IM EMULATOR: dort laesst sich die Zeit nicht halten (jeder
// pebble-Befehl stellt sie neu), und ein Neustart mit falscher Uhrzeit ist
// kaum nachzustellen. Hier ist beides eine Zeile.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APP_LOG_LEVEL_INFO 0
#define APP_LOG_LEVEL_WARNING 1
#define APP_LOG(level, fmt, ...) ((void)(level))

// Nur, damit die Kopfdateien der Grafik durchgehen.
typedef struct Layer Layer;
typedef struct GContext GContext;
typedef struct { int16_t x, y; } GPoint;
typedef uint8_t GColor;

// Die Uhrzeit der Uhr - der Test setzt sie.
extern time_t stub_jetzt;
static inline time_t stub_time(time_t *t) {
  if (t) *t = stub_jetzt;
  return stub_jetzt;
}
#define time(t) stub_time(t)

typedef int32_t WakeupId;
#define E_RANGE (-8)
static inline WakeupId wakeup_schedule(time_t t, int32_t cookie, bool notify) {
  (void)t; (void)cookie; (void)notify;
  return 1;
}
static inline void wakeup_cancel_all(void) {}
static inline bool clock_is_24h_style(void) { return true; }

bool persist_exists(uint32_t key);
int persist_read_data(uint32_t key, void *buf, size_t size);
int32_t persist_read_int(uint32_t key);
bool persist_read_bool(uint32_t key);
int persist_write_data(uint32_t key, const void *data, size_t size);
int persist_write_int(uint32_t key, int32_t value);
int persist_write_bool(uint32_t key, bool value);
int persist_delete(uint32_t key);
