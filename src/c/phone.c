#include <pebble.h>
#include "phone.h"
#include "config.h"
#include "schedule.h"
#include "main_window.h"
#include "strings.h"

// Status eines heutigen Slots (siehe src/pkjs/index.js)
enum { SlotFuture = 0, SlotDrunk = 2, SlotMissed = 3 };

// Ausgangspuffer: fuenf Zahlenfelder zu je 11 Byte, dazu die Slot-Daten mit
// 7 Byte Kopf und 5 Byte je Glas, plus ein Byte fuer das Woerterbuch selbst.
// Bei DT_GLASSES_MAX = 16 sind das 143 Byte; 256 laesst Luft fuer ein
// weiteres Feld, ohne dass jemand nachrechnen muss.
#define OUTBOX_SIZE 256
#define INBOX_SIZE  128

// --- Die Glaeser, die noch zum Telefon muessen ---
//
// EINE WARTESCHLANGE IM PERSIST, NICHT EIN VERMERK IM SPEICHER. Bis 1.10
// stand hier ein einziger Zeitpunkt, der beim Schreiben der Nachricht
// verbraucht war - ob sie ankam oder nicht. War der Postausgang in diesem
// Augenblick besetzt, antwortete das Telefon nicht rechtzeitig, oder ging die
// App nach der Animation zu, bevor die Nachricht draussen war, war das Glas
// auf der Uhr gezaehlt und fuer das Telefon verloren. Und zwei Glaeser vor
// einer erfolgreichen Nachricht wurden zu einem.
//
// Jetzt steht jedes Glas in der Schlange, bis das Telefon die Nachricht, die
// es trug, bestaetigt hat. Jede Nachricht traegt das AELTESTE; nach der
// Bestaetigung geht das naechste. Was beim Beenden noch drinsteht, geht beim
// naechsten Start. Kiesel-Helper traegt ein Glas je Zeitpunkt nur einmal ein
// - ein zweites Mal geschickt ist also harmlos, verloren ist es nicht mehr.
#define QUEUE_MAX 12
typedef struct __attribute__((packed)) {
  uint32_t at;
  uint16_t ml;
} Drink;

static Drink s_queue[QUEUE_MAX];
static uint8_t s_queue_len;
static bool s_queue_loaded;
static bool s_carried;       // die letzte Nachricht trug s_queue[0]
static AppTimer *s_retry;
static uint8_t s_attempts;
// Eine Standmeldung ist faellig, auch ohne Glas: nach einer Aenderung der
// Einstellungen soll das Telefon den neuen Stand hoeren, und ein besetzter
// Postausgang darf das nicht verschlucken.
static bool s_report_due;

static void prv_queue_load(void) {
  if (s_queue_loaded) return;
  s_queue_loaded = true;
  s_queue_len = 0;
  if (!persist_exists(DT_PERSIST_QUEUE)) return;
  const int n = persist_read_data(DT_PERSIST_QUEUE, s_queue, sizeof(s_queue));
  if (n > 0) s_queue_len = (uint8_t)(n / sizeof(Drink));
}

static void prv_queue_save(void) {
  if (s_queue_len == 0) {
    persist_delete(DT_PERSIST_QUEUE);
  } else {
    persist_write_data(DT_PERSIST_QUEUE, s_queue, s_queue_len * sizeof(Drink));
  }
}

void phone_note_drink(void) {
  prv_queue_load();
  if (s_queue_len == QUEUE_MAX) {
    // Voll: das aelteste faellt weg. Zwoelf unbestaetigte Glaeser heissen,
    // dass seit Stunden kein Telefon zuhoert - das dreizehnte ist wichtiger.
    memmove(&s_queue[0], &s_queue[1], (QUEUE_MAX - 1) * sizeof(Drink));
    s_queue_len--;
  }
  s_queue[s_queue_len].at = (uint32_t)time(NULL);
  s_queue[s_queue_len].ml = (uint16_t)schedule_glass_ml();
  s_queue_len++;
  prv_queue_save();
}

bool phone_pending(void) {
  prv_queue_load();
  return s_queue_len > 0;
}

static void prv_retry_cb(void *data) {
  s_retry = NULL;
  if (phone_pending() || s_report_due) phone_send_next();
}

static void prv_schedule_retry(uint32_t ms) {
  if (s_retry || (!phone_pending() && !s_report_due)) return;
  // Ein paar Anlaeufe in kurzem Abstand, dann Ruhe - beim naechsten Start,
  // Wecker oder Glas geht es ohnehin wieder los.
  if (s_attempts >= 5) return;
  s_attempts++;
  s_retry = app_timer_register(ms, prv_retry_cb, NULL);
}

static void prv_sent(DictionaryIterator *iter, void *context) {
  s_report_due = false;
  if (s_carried && s_queue_len > 0) {
    memmove(&s_queue[0], &s_queue[1], (s_queue_len - 1) * sizeof(Drink));
    s_queue_len--;
    prv_queue_save();
    s_attempts = 0;
  }
  s_carried = false;
  // Noch mehr in der Schlange: gleich das naechste.
  if (phone_pending() && !s_retry) s_retry = app_timer_register(150, prv_retry_cb, NULL);
}

static void prv_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht nicht angekommen: %d", (int)reason);
  s_carried = false;
  prv_schedule_retry(1500);
}

void phone_send_next(void) {
  prv_queue_load();
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    // BESETZT: nicht still aufgeben, wenn ein Glas oder ein Stand wartet.
    prv_schedule_retry(700);
    return;
  }
  const time_t now = time(NULL);
  const int count = schedule_count();
  const int slot_count = schedule_target();
  time_t next;
  const int idx = schedule_next(now, &next);
  dict_write_int32(out, MESSAGE_KEY_GLASSES, schedule_goal());
  dict_write_int32(out, MESSAGE_KEY_COUNT, count);
  dict_write_int32(out, MESSAGE_KEY_NEXT_TIME, (int32_t)next);
  dict_write_int32(out, MESSAGE_KEY_NEXT_INDEX, idx);
  // Sprache der Uhr: die Telefonseite baut die Pin-Texte und kann sie nicht
  // von sich aus erfahren (0 = Englisch, 1 = Deutsch).
  dict_write_int32(out, MESSAGE_KEY_LANG, (int32_t)strings_language());

  // Heutige Slots: je 4 Byte Zeit (little endian) + 1 Byte Status. Zukuenftige
  // Slots sind SlotFuture; von den vergangenen gelten die ersten `count` als
  // getrunken, der Rest als verpasst. Es sind so viele, wie das Soll vorsieht -
  // das Telefon liest die Anzahl aus der Laenge.
  uint8_t slots[DT_GLASSES_MAX * 5];
  const time_t midnight = schedule_midnight(now);
  for (int i = 0; i < slot_count; i++) {
    const uint32_t t = (uint32_t)schedule_slot(midnight, i);
    slots[i * 5 + 0] = (uint8_t)(t & 0xFF);
    slots[i * 5 + 1] = (uint8_t)((t >> 8) & 0xFF);
    slots[i * 5 + 2] = (uint8_t)((t >> 16) & 0xFF);
    slots[i * 5 + 3] = (uint8_t)((t >> 24) & 0xFF);
    slots[i * 5 + 4] = (time_t)t > now ? SlotFuture : (i < count ? SlotDrunk : SlotMissed);
  }
  dict_write_data(out, MESSAGE_KEY_SLOTS, slots, (uint16_t)(slot_count * 5));

  // DIE EINSTELLUNGEN DER UHR FAHREN IMMER MIT. Die Uhr ist die eine
  // Stelle, an der sie gelten; die Konfigseite der Pebble-App und
  // Kiesel-Helper aendern sie beide hier - und lesen hier ab, was gilt.
  // Ohne diese Zeilen zeigte jede Seite ihren eigenen, womoeglich alten Stand.
  dict_write_int32(out, MESSAGE_KEY_TARGET, schedule_target());
  dict_write_int32(out, MESSAGE_KEY_ANIMATION, schedule_animation() ? 1 : 0);

  // Das aelteste unbestaetigte Glas. Verbraucht ist es erst in prv_sent -
  // wenn das Telefon die Nachricht bestaetigt hat. GLASS_ML steht nur EINMAL
  // in der Nachricht: mit einem Glas dessen Menge, sonst die Glasgroesse.
  s_carried = false;
  if (s_queue_len > 0) {
    dict_write_int32(out, MESSAGE_KEY_DRANK_AT, (int32_t)s_queue[0].at);
    dict_write_int32(out, MESSAGE_KEY_GLASS_ML, (int32_t)s_queue[0].ml);
    s_carried = true;
  } else {
    dict_write_int32(out, MESSAGE_KEY_GLASS_ML, schedule_glass_ml());
  }

  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  // Einstellungen - von der Konfigseite der Pebble-App oder von Kiesel-Helper,
  // der Uhr ist das gleich. Danach geht der neue Stand an beide zurueck.
  bool einstellung = false;

  // Glasgroesse. Aendert am Verhalten der Uhr nichts, sie geht mit jedem Glas
  // hinaus.
  Tuple *glass = dict_find(iter, MESSAGE_KEY_GLASS_ML);
  if (glass) { schedule_set_glass_ml(glass->value->int32); einstellung = true; }

  // Trink-Animation an oder aus. Aendert nichts am Zaehlen und nichts am Plan.
  Tuple *anim = dict_find(iter, MESSAGE_KEY_ANIMATION);
  if (anim) { schedule_set_animation(anim->value->int32 != 0); einstellung = true; }

  Tuple *target = dict_find(iter, MESSAGE_KEY_TARGET);
  if (target) {
    einstellung = true;
    if (schedule_set_target(target->value->int32)) {
      // Der Plan hat sich verschoben: Wecker neu stellen und den Hauptscreen
      // nachziehen, der Pegel haengt am Tagesziel.
      schedule_plan_wakeups(0);
      main_window_refresh();
    }
  }

  if (einstellung || dict_find(iter, MESSAGE_KEY_REQUEST)) {
    s_report_due = true;
    phone_send_next();
  }
}

void phone_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_outbox_sent(prv_sent);
  app_message_register_outbox_failed(prv_failed);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
  // Liegen noch Glaeser vom letzten Mal da - weil die App zuging, bevor das
  // Telefon antwortete -, gehen sie jetzt. Mit etwas Abstand, damit die
  // Verbindung erst steht.
  prv_queue_load();
  if (s_queue_len > 0) s_retry = app_timer_register(1000, prv_retry_cb, NULL);
}
