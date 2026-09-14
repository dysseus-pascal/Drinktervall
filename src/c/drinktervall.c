#include <pebble.h>
#include "drinktervall.h"
#include "schedule.h"
#include "main_window.h"
#include "reminder_window.h"
#include "drink_window.h"
#include "phone.h"
#include "strings.h"

// Launch-Code der Timeline-Pin-Aktion "Getrunken"/"Nachholen" (siehe
// src/pkjs/index.js). Code 2 ("App oeffnen") braucht hier keinen Fall: die
// App startet ohnehin.
#define LAUNCH_CODE_DRUNK 1

static bool s_launched_by_wakeup;

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  reminder_window_push();
  schedule_plan_wakeups(0);
  phone_send_next();   // Timeline-Pins auf den neuen Stand bringen
}

static void prv_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) return;
  time_t next;
  schedule_next(time(NULL), &next);
  char hhmm[8];
  schedule_format_time(next, hhmm, sizeof(hhmm));
  char text[48];
  snprintf(text, sizeof(text), S(STR_GLANCE_FMT),
           schedule_count(), schedule_goal(), hhmm);
  const AppGlanceSlice slice = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = text },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(session, slice);
}

void drinktervall_reminder_closed(void) {
  if (s_launched_by_wakeup) window_stack_pop_all(false);
}

static void prv_init(void) {
  // Sprache der Uhr uebernehmen, bevor das erste Fenster Texte holt
  strings_refresh();
  schedule_init();
  main_window_push();

  const AppLaunchReason reason = launch_reason();
  WakeupId id;
  int32_t cookie;
  if (reason == APP_LAUNCH_WAKEUP && wakeup_get_launch_event(&id, &cookie)) {
    s_launched_by_wakeup = true;
    reminder_window_push();
  } else if (reason == APP_LAUNCH_TIMELINE_ACTION && launch_get_args() == LAUNCH_CODE_DRUNK
             && schedule_count() < schedule_goal()) {
    // Aus einem Timeline-Pin: zaehlen, kurz zeigen, App wieder verlassen
    schedule_set_count(schedule_count() + 1);
    phone_note_drink();
    vibes_short_pulse();
    drink_window_push(true);
  }

  wakeup_service_subscribe(prv_wakeup_handler);
  // Bei jedem Start neu planen: haelt die 8 Slots ueber Tagesgrenzen aktuell.
  schedule_plan_wakeups(0);
  phone_init();
}

static void prv_deinit(void) {
  app_glance_reload(prv_glance_reload, NULL);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
  return 0;
}
