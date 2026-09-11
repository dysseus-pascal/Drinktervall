#include <pebble.h>
#include "phone.h"
#include "config.h"
#include "schedule.h"

// Status eines heutigen Slots (siehe src/pkjs/index.js)
enum { SlotFuture = 0, SlotDrunk = 2, SlotMissed = 3 };

void phone_send_next(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  const time_t now = time(NULL);
  const int count = schedule_count();
  time_t next;
  const int idx = schedule_next(now, &next);
  dict_write_int32(out, MESSAGE_KEY_GLASSES, schedule_goal());
  dict_write_int32(out, MESSAGE_KEY_COUNT, count);
  dict_write_int32(out, MESSAGE_KEY_NEXT_TIME, (int32_t)next);
  dict_write_int32(out, MESSAGE_KEY_NEXT_INDEX, idx);

  // Heutige Slots: je 4 Byte Zeit (little endian) + 1 Byte Status. Zukuenftige
  // Slots sind SlotFuture; von den vergangenen gelten die ersten `count` als
  // getrunken, der Rest als verpasst.
  uint8_t slots[DT_GLASSES * 5];
  const time_t midnight = schedule_midnight(now);
  for (int i = 0; i < DT_GLASSES; i++) {
    const uint32_t t = (uint32_t)schedule_slot(midnight, i);
    slots[i * 5 + 0] = (uint8_t)(t & 0xFF);
    slots[i * 5 + 1] = (uint8_t)((t >> 8) & 0xFF);
    slots[i * 5 + 2] = (uint8_t)((t >> 16) & 0xFF);
    slots[i * 5 + 3] = (uint8_t)((t >> 24) & 0xFF);
    slots[i * 5 + 4] = (time_t)t > now ? SlotFuture : (i < count ? SlotDrunk : SlotMissed);
  }
  dict_write_data(out, MESSAGE_KEY_SLOTS, slots, sizeof(slots));
  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_REQUEST)) phone_send_next();
}

void phone_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(64, 128);
}
