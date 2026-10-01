#include <pebble.h>
#include "phone.h"
#include "config.h"
#include "schedule.h"
#include "main_window.h"
#include "strings.h"
#include "coffee.h"

// Status eines heutigen Slots (siehe src/pkjs/index.js)
enum { SlotFuture = 0, SlotDrunk = 2, SlotMissed = 3 };

// Ausgangspuffer: fuenf Zahlenfelder zu je 11 Byte, dazu die Slot-Daten mit
// 7 Byte Kopf und 5 Byte je Glas, plus ein Byte fuer das Woerterbuch selbst.
// Bei DT_GLASSES_MAX = 16 sind das 143 Byte; dazu der Kaffeeplan (7 + 17)
// und ein Kaffee (2 x 11) - 192 Byte; die eigenen Getraenke (bis 90 Zeichen)
// und ein eigenes Getraenk unterwegs (Name, kcal, Koffein) bringen es auf
// rund 340. Eingehend steht der ganze Plan samt eigener Getraenke drin.
#define OUTBOX_SIZE 384
#define INBOX_SIZE  256

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

// Die Kaffees gehen denselben Weg in einer eigenen Schlange: ein Kaffee ist
// kein Glas Wasser, und in einer Nachricht kann je eines von beiden mitfahren.
#define COFFEE_QUEUE_MAX 8
// Ein eigenes Getraenk reist mit Name, kcal und Koffein: aendert man es auf
// der Konfigseite, bevor es draussen ist, bleibt das Getrunkene, wie es war.
typedef struct __attribute__((packed)) {
  uint32_t at;
  uint8_t kind;
  uint8_t flags;
  uint16_t kcal;
  uint16_t mg;
  char name[DT_CUSTOM_NAME];
} Coffee;

// Die Schlange von 1.14/1.15 - noch ohne eigenes Getraenk.
typedef struct __attribute__((packed)) {
  uint32_t at;
  uint8_t kind;
  uint8_t flags;
} CoffeeAlt;

static Coffee s_coffees[COFFEE_QUEUE_MAX];
static uint8_t s_coffees_len;
static bool s_coffee_carried;
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
  s_coffees_len = 0;
  if (persist_exists(DT_PERSIST_QUEUE)) {
    const int n = persist_read_data(DT_PERSIST_QUEUE, s_queue, sizeof(s_queue));
    if (n > 0) s_queue_len = (uint8_t)(n / sizeof(Drink));
  }
  if (persist_exists(DT_PERSIST_COFFEE_QUEUE2)) {
    const int n = persist_read_data(DT_PERSIST_COFFEE_QUEUE2, s_coffees, sizeof(s_coffees));
    if (n > 0) s_coffees_len = (uint8_t)(n / sizeof(Coffee));
  } else if (persist_exists(DT_PERSIST_COFFEE_QUEUE)) {
    // Was 1.14/1.15 noch nicht losbrachte, in die neue Form uebernehmen.
    CoffeeAlt alt[COFFEE_QUEUE_MAX];
    const int n = persist_read_data(DT_PERSIST_COFFEE_QUEUE, alt, sizeof(alt));
    for (int i = 0; n > 0 && i < n / (int)sizeof(CoffeeAlt); i++) {
      s_coffees[s_coffees_len++] = (Coffee){ .at = alt[i].at, .kind = alt[i].kind, .flags = alt[i].flags };
    }
    persist_delete(DT_PERSIST_COFFEE_QUEUE);
  }
}

static void prv_queue_save(void) {
  if (s_queue_len == 0) {
    persist_delete(DT_PERSIST_QUEUE);
  } else {
    persist_write_data(DT_PERSIST_QUEUE, s_queue, s_queue_len * sizeof(Drink));
  }
  if (s_coffees_len == 0) {
    persist_delete(DT_PERSIST_COFFEE_QUEUE2);
  } else {
    persist_write_data(DT_PERSIST_COFFEE_QUEUE2, s_coffees, s_coffees_len * sizeof(Coffee));
  }
}

static void prv_coffee_push(const Coffee *c) {
  prv_queue_load();
  if (s_coffees_len == COFFEE_QUEUE_MAX) {
    memmove(&s_coffees[0], &s_coffees[1], (COFFEE_QUEUE_MAX - 1) * sizeof(Coffee));
    s_coffees_len--;
  }
  s_coffees[s_coffees_len++] = *c;
  prv_queue_save();
}

void phone_note_coffee(uint8_t kind, uint8_t flags) {
  const Coffee c = { .at = (uint32_t)time(NULL), .kind = kind, .flags = flags };
  prv_coffee_push(&c);
}

void phone_note_custom(const CustomDrink *drink) {
  Coffee c = { .at = (uint32_t)time(NULL), .kind = COFFEE_KIND_CUSTOM,
               .kcal = drink->kcal, .mg = drink->mg };
  strncpy(c.name, drink->name, DT_CUSTOM_NAME - 1);
  prv_coffee_push(&c);
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
  return s_queue_len > 0 || s_coffees_len > 0;
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
  if (s_coffee_carried && s_coffees_len > 0) {
    memmove(&s_coffees[0], &s_coffees[1], (s_coffees_len - 1) * sizeof(Coffee));
    s_coffees_len--;
    prv_queue_save();
    s_attempts = 0;
  }
  s_carried = false;
  s_coffee_carried = false;
  // Noch mehr in der Schlange: gleich das naechste.
  if (phone_pending() && !s_retry) s_retry = app_timer_register(150, prv_retry_cb, NULL);
}

static void prv_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht nicht angekommen: %d", (int)reason);
  s_carried = false;
  s_coffee_carried = false;
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
  uint8_t plan[COFFEE_BYTES_MAX];
  dict_write_data(out, MESSAGE_KEY_COFFEE, plan, (uint16_t)coffee_to_bytes(plan));
  char eigene[DT_CUSTOM_MAX * (DT_CUSTOM_NAME + 12)];
  custom_to_string(eigene, sizeof(eigene));
  dict_write_cstring(out, MESSAGE_KEY_CUSTOM, eigene);

  // Der aelteste unbestaetigte Kaffee, Sorte und Flags in einem Feld: die
  // Sorte in den unteren vier Bits, Milch und Zucker darueber.
  s_coffee_carried = false;
  if (s_coffees_len > 0) {
    dict_write_int32(out, MESSAGE_KEY_COFFEE_AT, (int32_t)s_coffees[0].at);
    dict_write_int32(out, MESSAGE_KEY_COFFEE_KIND, s_coffees[0].kind | (s_coffees[0].flags << 4));
    if (s_coffees[0].kind == COFFEE_KIND_CUSTOM) {
      char name[DT_CUSTOM_NAME];
      strncpy(name, s_coffees[0].name, DT_CUSTOM_NAME - 1);
      name[DT_CUSTOM_NAME - 1] = 0;
      dict_write_cstring(out, MESSAGE_KEY_DRINK_NAME, name);
      dict_write_int32(out, MESSAGE_KEY_DRINK_KCAL, s_coffees[0].kcal);
      dict_write_int32(out, MESSAGE_KEY_DRINK_MG, s_coffees[0].mg);
    }
    s_coffee_carried = true;
  }

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

  // Kaffeeplan. Neue Zeiten heissen neue Wecker.
  Tuple *coffee = dict_find(iter, MESSAGE_KEY_COFFEE);
  if (coffee && coffee->type == TUPLE_BYTE_ARRAY) {
    einstellung = true;
    if (coffee_from_bytes(coffee->value->data, coffee->length)) schedule_plan_wakeups(0, 0);
  }

  // Eigene Getraenke: nur fuer die Getraenkeauswahl, kein Wecker.
  Tuple *custom = dict_find(iter, MESSAGE_KEY_CUSTOM);
  if (custom && custom->type == TUPLE_CSTRING) {
    einstellung = true;
    if (custom_from_string(custom->value->cstring)) schedule_plan_wakeups(0, 0);
  }

  Tuple *target = dict_find(iter, MESSAGE_KEY_TARGET);
  if (target) {
    einstellung = true;
    if (schedule_set_target(target->value->int32)) {
      // Der Plan hat sich verschoben: Wecker neu stellen und den Hauptscreen
      // nachziehen, der Pegel haengt am Tagesziel.
      schedule_plan_wakeups(0, 0);
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
  if (phone_pending()) s_retry = app_timer_register(1000, prv_retry_cb, NULL);
}
