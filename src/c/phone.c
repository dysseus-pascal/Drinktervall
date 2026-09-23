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

// Zeitpunkt des zuletzt getrunkenen Glases, 0 = nichts zu melden.
static time_t s_drank_at;

void phone_note_drink(void) {
  s_drank_at = time(NULL);
}

void phone_send_next(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
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

  // Nur wenn gerade wirklich getrunken wurde. Der Vermerk ist danach
  // verbraucht - so traegt eine Companion-App jedes Glas genau einmal ein,
  // auch wenn danach noch zehn Standmeldungen folgen.
  if (s_drank_at != 0) {
    dict_write_int32(out, MESSAGE_KEY_DRANK_AT, (int32_t)s_drank_at);
    dict_write_int32(out, MESSAGE_KEY_GLASS_ML, (int32_t)schedule_glass_ml());
    s_drank_at = 0;
  }

  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  // Neues Soll von der Konfigseite. Zuerst anwenden, damit die Antwort unten
  // schon den neuen Plan traegt.
  // Glasgroesse von der Konfigseite. Aendert am Verhalten der Uhr nichts, sie
  // wird nur mitgeschickt, wenn getrunken wurde.
  Tuple *glass = dict_find(iter, MESSAGE_KEY_GLASS_ML);
  if (glass) schedule_set_glass_ml(glass->value->int32);

  // Trink-Animation an oder aus. Aendert nichts am Zaehlen und nichts am Plan,
  // deshalb muss danach auch nichts neu geplant werden.
  Tuple *anim = dict_find(iter, MESSAGE_KEY_ANIMATION);
  if (anim) schedule_set_animation(anim->value->int32 != 0);

  Tuple *target = dict_find(iter, MESSAGE_KEY_TARGET);
  if (target && schedule_set_target(target->value->int32)) {
    // Der Plan hat sich verschoben: Wecker neu stellen und den Hauptscreen
    // nachziehen, der Pegel haengt am Tagesziel.
    schedule_plan_wakeups(0);
    main_window_refresh();
    phone_send_next();
    return;
  }
  if (dict_find(iter, MESSAGE_KEY_REQUEST)) phone_send_next();
}

void phone_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(INBOX_SIZE, OUTBOX_SIZE);
}
