// Attrappe fuer die Host-Tests: Log, Dictionary, AppMessage, Zeitgeber,
// Sprache. Verhaelt sich dort, wo es fuer die App zaehlt, wie die Uhr:
//   - ein Dictionary zaehlt Platz wie auf der Uhr (1 + je Tupel 7 + Daten),
//     und der Postausgang ist so gross, wie app_message_open verlangt;
//   - der Postausgang fasst EINE Nachricht: bis ACK oder NACK ist er besetzt
//     (APP_MSG_BUSY);
//   - outbox_sent/outbox_failed bekommen die Nachricht, die hinausging.
// Die Firmware selbst steht in pebbleos/src/fw/applib/app_message.
// (Gleich gebaut wie die Attrappe in SupCycle/tools/host.)
#include <pebble.h>
#include <stdarg.h>

// --- Log ---
char attrappe_log_text[8192];
void attrappe_log_leeren(void) { attrappe_log_text[0] = 0; }
void attrappe_log(int level, const char *fmt, ...) {
  const size_t l = strlen(attrappe_log_text);
  if (l + 4 >= sizeof(attrappe_log_text)) return;
  snprintf(attrappe_log_text + l, sizeof(attrappe_log_text) - l, "%s ", level ? "W" : "I");
  const size_t m = strlen(attrappe_log_text);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(attrappe_log_text + m, sizeof(attrappe_log_text) - m, fmt, ap);
  va_end(ap);
  const size_t n = strlen(attrappe_log_text);
  if (n + 1 < sizeof(attrappe_log_text)) { attrappe_log_text[n] = '\n'; attrappe_log_text[n + 1] = 0; }
}

// --- Dictionary ---
DictionaryResult dict_write_begin(DictionaryIterator *iter, uint8_t *buffer, uint16_t size) {
  if (!iter || !buffer || size < 1) return DICT_INVALID_ARGS;
  iter->puffer = buffer;
  iter->groesse = size;
  iter->belegt = 1;
  buffer[0] = 0;
  return DICT_OK;
}

static DictionaryResult prv_tupel(DictionaryIterator *iter, uint32_t key, uint8_t typ,
                                  const void *daten, uint16_t laenge) {
  if (!iter || !iter->puffer) return DICT_INVALID_ARGS;
  if (iter->belegt + 7u + laenge > iter->groesse) return DICT_NOT_ENOUGH_STORAGE;
  uint8_t *p = iter->puffer + iter->belegt;
  memcpy(p, &key, 4);
  p[4] = typ;
  memcpy(p + 5, &laenge, 2);
  if (laenge) memcpy(p + 7, daten, laenge);
  iter->belegt = (uint16_t)(iter->belegt + 7 + laenge);
  iter->puffer[0]++;
  return DICT_OK;
}

DictionaryResult dict_write_data(DictionaryIterator *iter, uint32_t key, const uint8_t *data, uint16_t size) {
  return prv_tupel(iter, key, TUPLE_BYTE_ARRAY, data, size);
}
DictionaryResult dict_write_cstring(DictionaryIterator *iter, uint32_t key, const char *cstring) {
  const char *s = cstring ? cstring : "";
  return prv_tupel(iter, key, TUPLE_CSTRING, s, (uint16_t)(strlen(s) + 1));
}
DictionaryResult dict_write_int32(DictionaryIterator *iter, uint32_t key, int32_t value) {
  return prv_tupel(iter, key, TUPLE_INT, &value, 4);
}
uint32_t dict_write_end(DictionaryIterator *iter) { return iter ? iter->belegt : 0; }

Tuple *dict_find(const DictionaryIterator *iter, uint32_t key) {
  if (!iter || !iter->puffer) return NULL;
  uint8_t *p = iter->puffer + 1;
  for (int i = 0; i < iter->puffer[0]; i++) {
    Tuple *t = (Tuple *)p;
    if (t->key == key) return t;
    p += 7 + t->length;
  }
  return NULL;
}

// --- AppMessage ---
static AppMessageInboxReceived s_eingang;
static AppMessageOutboxSent s_gesendet_cb;
static AppMessageOutboxFailed s_fehlgeschlagen_cb;
static uint32_t s_ausgang_groesse;
static bool s_schreibt, s_unterwegs;
static uint8_t s_ausgang_puffer[2048];
static DictionaryIterator s_ausgang;
static uint8_t s_letzte_puffer[2048];
static DictionaryIterator s_letzte;
static int s_anzahl;
static uint8_t s_eingang_puffer[1024];
static DictionaryIterator s_eingang_it;
// Fuer die Fehlerfaelle: ein Postausgang, der kleiner ist, als die App
// verlangte, und ein outbox_send, das nicht klappt.
static uint16_t s_begrenzt;
static AppMessageResult s_scheitert;
void attrappe_ausgang_begrenzen(uint16_t groesse) { s_begrenzt = groesse; }
void attrappe_senden_scheitert(AppMessageResult grund) { s_scheitert = grund; }

AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb) { s_eingang = cb; return cb; }
AppMessageOutboxSent app_message_register_outbox_sent(AppMessageOutboxSent cb) { s_gesendet_cb = cb; return cb; }
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed cb) { s_fehlgeschlagen_cb = cb; return cb; }
AppMessageResult app_message_open(uint32_t size_inbound, uint32_t size_outbound) {
  (void)size_inbound;
  s_ausgang_groesse = size_outbound;
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator) {
  if (s_unterwegs) return APP_MSG_BUSY;
  if (s_schreibt) return APP_MSG_INVALID_STATE;
  if (s_ausgang_groesse == 0 || s_ausgang_groesse > sizeof(s_ausgang_puffer)) return APP_MSG_INVALID_STATE;
  const uint16_t platz = (s_begrenzt && s_begrenzt < s_ausgang_groesse) ? s_begrenzt : (uint16_t)s_ausgang_groesse;
  dict_write_begin(&s_ausgang, s_ausgang_puffer, platz);
  *iterator = &s_ausgang;
  s_schreibt = true;
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_send(void) {
  if (s_unterwegs) return APP_MSG_BUSY;
  if (!s_schreibt) return APP_MSG_INVALID_STATE;
  s_schreibt = false;
  if (s_scheitert != APP_MSG_OK) {
    const AppMessageResult r = s_scheitert;
    s_scheitert = APP_MSG_OK;
    return r;
  }
  s_unterwegs = true;
  memcpy(s_letzte_puffer, s_ausgang_puffer, s_ausgang.belegt);
  s_letzte = (DictionaryIterator){ s_letzte_puffer, s_ausgang.groesse, s_ausgang.belegt };
  s_anzahl++;
  return APP_MSG_OK;
}
void attrappe_nachrichten_leeren(void) {
  s_schreibt = s_unterwegs = false;
  s_begrenzt = 0;
  s_scheitert = APP_MSG_OK;
  s_anzahl = 0;
  s_letzte = (DictionaryIterator){ 0 };
}
uint32_t attrappe_ausgang_groesse(void) { return s_ausgang_groesse; }
int attrappe_gesendet(void) { return s_anzahl; }
DictionaryIterator *attrappe_letzte(void) { return s_letzte.puffer ? &s_letzte : NULL; }
bool attrappe_unterwegs(void) { return s_unterwegs; }
void attrappe_ack(void) {
  if (!s_unterwegs) return;
  s_unterwegs = false;
  if (s_gesendet_cb) s_gesendet_cb(&s_letzte, NULL);
}
void attrappe_nack(AppMessageResult grund) {
  if (!s_unterwegs) return;
  s_unterwegs = false;
  if (s_fehlgeschlagen_cb) s_fehlgeschlagen_cb(&s_letzte, grund, NULL);
}
DictionaryIterator *attrappe_eingang_beginn(void) {
  dict_write_begin(&s_eingang_it, s_eingang_puffer, sizeof(s_eingang_puffer));
  return &s_eingang_it;
}
void attrappe_eingang_zustellen(void) {
  if (s_eingang) s_eingang(&s_eingang_it, NULL);
}

// --- Zeitgeber ---
struct AppTimer { bool aktiv; uint32_t ms; AppTimerCallback cb; void *daten; };
static struct AppTimer s_zeitgeber[16];
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *callback_data) {
  for (unsigned i = 0; i < sizeof(s_zeitgeber) / sizeof(s_zeitgeber[0]); i++) {
    if (!s_zeitgeber[i].aktiv) {
      s_zeitgeber[i] = (struct AppTimer){ true, timeout_ms, callback, callback_data };
      return &s_zeitgeber[i];
    }
  }
  return NULL;
}
void app_timer_cancel(AppTimer *timer) { if (timer) timer->aktiv = false; }
int attrappe_zeitgeber_offen(void) {
  int n = 0;
  for (unsigned i = 0; i < sizeof(s_zeitgeber) / sizeof(s_zeitgeber[0]); i++) n += s_zeitgeber[i].aktiv;
  return n;
}
uint32_t attrappe_zeitgeber_ms(int nummer) {
  for (unsigned i = 0; i < sizeof(s_zeitgeber) / sizeof(s_zeitgeber[0]); i++) {
    if (s_zeitgeber[i].aktiv && nummer-- == 0) return s_zeitgeber[i].ms;
  }
  return 0;
}
int attrappe_zeitgeber_ablaufen(void) {
  // Erst sammeln, dann ausloesen: was ein Rueckruf neu stellt, laeuft erst in
  // der naechsten Runde - wie auf der Uhr, wo es Zeit braucht.
  struct AppTimer faellig[16];
  int n = 0;
  for (unsigned i = 0; i < sizeof(s_zeitgeber) / sizeof(s_zeitgeber[0]); i++) {
    if (s_zeitgeber[i].aktiv) { faellig[n++] = s_zeitgeber[i]; s_zeitgeber[i].aktiv = false; }
  }
  for (int i = 0; i < n; i++) faellig[i].cb(faellig[i].daten);
  return n;
}

const char *attrappe_sprache = "en_US";
const char *i18n_get_system_locale(void) { return attrappe_sprache; }
