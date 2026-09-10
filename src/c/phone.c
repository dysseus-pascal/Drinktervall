#include <pebble.h>
#include "phone.h"
#include "config.h"
#include "schedule.h"

static void prv_send_config(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_START_HOUR, AT_START_HOUR);
  dict_write_int32(out, MESSAGE_KEY_INTERVAL_MIN, AT_INTERVAL_MIN);
  dict_write_int32(out, MESSAGE_KEY_GLASSES, AT_GLASSES);
  dict_write_int32(out, MESSAGE_KEY_COUNT, schedule_count());
  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, MESSAGE_KEY_REQUEST)) prv_send_config();
}

void phone_init(void) {
  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(64, 64);
}
