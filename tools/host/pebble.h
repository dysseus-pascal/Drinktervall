// Nur fuer die Host-Tests in tools/: so viel vom Pebble-SDK, wie schedule.c,
// coffee.c, strings.c und phone.c brauchen. Persist liegt im Speicher
// (attrappe_persist.c), die Uhrzeit stellt der Test, Wecker gehen ins Leere,
// Nachrichten und Zeitgeber spielt die Attrappe (attrappe.c).
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
// Das Log landet in attrappe_log_text: ein Test kann pruefen, dass ein
// Fehler NICHT still bleibt.
void attrappe_log(int level, const char *fmt, ...);
extern char attrappe_log_text[8192];
void attrappe_log_leeren(void);
#define APP_LOG(level, fmt, ...) attrappe_log((level), (fmt), ##__VA_ARGS__)

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

// --- Wecker (attrappe.c) ---
// Wie die Firmware: hoechstens acht je App, und keiner naeher als eine Minute
// an einem anderen (E_RANGE). Die Tests lesen mit, was gestellt ist.
typedef int32_t WakeupId;
#define E_RANGE (-8)
#define E_OUT_OF_RESOURCES (-7)
WakeupId wakeup_schedule(time_t t, int32_t cookie, bool notify);
void wakeup_cancel_all(void);
int attrappe_wecker_anzahl(void);
time_t attrappe_wecker_zeit(int nummer);
int32_t attrappe_wecker_cookie(int nummer);
static inline bool clock_is_24h_style(void) { return true; }

bool persist_exists(uint32_t key);
int persist_read_data(uint32_t key, void *buf, size_t size);
int32_t persist_read_int(uint32_t key);
bool persist_read_bool(uint32_t key);
int persist_write_data(uint32_t key, const void *data, size_t size);
int persist_write_int(uint32_t key, int32_t value);
int persist_write_bool(uint32_t key, bool value);
int persist_delete(uint32_t key);
void attrappe_persist_leeren(void);

// --- Dictionary im Format der Pebble: 1 Byte Anzahl, je Tupel 4 Byte
// Schluessel, 1 Byte Typ, 2 Byte Laenge, dann die Daten. So zaehlt auch der
// Platz wie auf der Uhr - ein Feld, das dort nicht passt, passt hier nicht.
typedef enum { TUPLE_BYTE_ARRAY = 0, TUPLE_CSTRING = 1, TUPLE_UINT = 2, TUPLE_INT = 3 } TupleType;
typedef struct __attribute__((__packed__)) {
  uint32_t key;
  uint8_t type;
  uint16_t length;
  union {
    uint8_t data[0];
    char cstring[0];
    uint8_t uint8;
    uint16_t uint16;
    uint32_t uint32;
    int8_t int8;
    int16_t int16;
    int32_t int32;
  } value[];
} Tuple;
typedef struct {
  uint8_t *puffer;
  uint16_t groesse;
  uint16_t belegt;
} DictionaryIterator;
// Werte wie im SDK (DictionaryResult).
typedef enum {
  DICT_OK = 0,
  DICT_NOT_ENOUGH_STORAGE = 1 << 1,
  DICT_INVALID_ARGS = 1 << 2,
} DictionaryResult;
DictionaryResult dict_write_begin(DictionaryIterator *iter, uint8_t *buffer, uint16_t size);
DictionaryResult dict_write_data(DictionaryIterator *iter, uint32_t key, const uint8_t *data, uint16_t size);
DictionaryResult dict_write_cstring(DictionaryIterator *iter, uint32_t key, const char *cstring);
DictionaryResult dict_write_int32(DictionaryIterator *iter, uint32_t key, int32_t value);
DictionaryResult dict_write_uint8(DictionaryIterator *iter, uint32_t key, uint8_t value);
DictionaryResult dict_write_int16(DictionaryIterator *iter, uint32_t key, int16_t value);
uint32_t dict_write_end(DictionaryIterator *iter);
Tuple *dict_find(const DictionaryIterator *iter, uint32_t key);

// --- AppMessage (Werte wie im SDK) ---
typedef enum {
  APP_MSG_OK = 0,
  APP_MSG_SEND_TIMEOUT = 1 << 1,
  APP_MSG_SEND_REJECTED = 1 << 2,
  APP_MSG_NOT_CONNECTED = 1 << 3,
  APP_MSG_BUSY = 1 << 6,
  APP_MSG_INVALID_STATE = 1 << 15,
} AppMessageResult;
typedef void (*AppMessageInboxReceived)(DictionaryIterator *iterator, void *context);
typedef void (*AppMessageOutboxSent)(DictionaryIterator *iterator, void *context);
typedef void (*AppMessageOutboxFailed)(DictionaryIterator *iterator, AppMessageResult reason, void *context);
AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb);
AppMessageOutboxSent app_message_register_outbox_sent(AppMessageOutboxSent cb);
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed cb);
AppMessageResult app_message_open(uint32_t size_inbound, uint32_t size_outbound);
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator);
AppMessageResult app_message_outbox_send(void);
// Was der Test damit tut: das Telefon antworten lassen und nachsehen.
void attrappe_nachrichten_leeren(void);
uint32_t attrappe_ausgang_groesse(void);           //< was app_message_open verlangte
void attrappe_ausgang_begrenzen(uint16_t groesse); //< kleinerer Postausgang (Fehlerfall)
void attrappe_senden_scheitert(AppMessageResult grund); //< naechstes outbox_send liefert das
int attrappe_gesendet(void);                       //< wie viele Nachrichten hinausgingen
DictionaryIterator *attrappe_letzte(void);         //< Abschrift der zuletzt gesendeten
bool attrappe_unterwegs(void);                     //< wartet eine auf ACK/NACK?
void attrappe_ack(void);
void attrappe_nack(AppMessageResult grund);
// Eine Nachricht des Telefons an die Uhr: beginnen, mit dict_write fuellen,
// zustellen.
DictionaryIterator *attrappe_eingang_beginn(void);
void attrappe_eingang_zustellen(void);

// --- Zeitgeber ---
typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *callback_data);
void app_timer_cancel(AppTimer *timer);
int attrappe_zeitgeber_offen(void);
uint32_t attrappe_zeitgeber_ms(int nummer);        //< Dauer des n-ten offenen
int attrappe_zeitgeber_ablaufen(void);             //< alle offenen einmal ausloesen

const char *i18n_get_system_locale(void);
extern const char *attrappe_sprache;

// Grafik nur fuer tools/glas_host_test.c und tools/app_host_test.c (siehe
// attrappe_grafik.h), Fenster, Start und Glance nur fuer den zweiten (siehe
// attrappe_app.h).
#ifdef ATTRAPPE_GRAFIK
#include "attrappe_grafik.h"
#endif
#ifdef ATTRAPPE_APP
#include "attrappe_app.h"
#endif
