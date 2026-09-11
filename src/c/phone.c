#include <pebble.h>
#include "phone.h"
#include "config.h"
#include "schedule.h"

void phone_send_next(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  time_t next;
  const int idx = schedule_next(time(NULL), &next);
  dict_write_int32(out, MESSAGE_KEY_GLASSES, schedule_goal());
  dict_write_int32(out, MESSAGE_KEY_NEXT_TIME, (int32_t)next);
  dict_write_int32(out, MESSAGE_KEY_NEXT_INDEX, idx);
  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_REQUEST)) phone_send_next();
}

void phone_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(64, 64);
}
