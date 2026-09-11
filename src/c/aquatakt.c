#include <pebble.h>
#include "aquatakt.h"
#include "config.h"
#include "schedule.h"
#include "main_window.h"
#include "reminder_window.h"
#include "phone.h"

// Launch-Codes der Timeline-Pin-Aktionen (siehe src/pkjs/index.js)
#define LAUNCH_CODE_DRUNK 1

static bool s_launched_by_wakeup;

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  reminder_window_push();
  schedule_plan_wakeups(0);
  phone_send_next();   // Timeline-Pin auf die naechste Erinnerung schieben
}

static void prv_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) return;
  time_t next;
  schedule_next(time(NULL), &next);
  char hhmm[8];
  schedule_format_time(next, hhmm, sizeof(hhmm));
  char text[48];
  snprintf(text, sizeof(text), "%d von %d Gläsern, nächste %s",
           schedule_count(), AT_GLASSES, hhmm);
  const AppGlanceSlice slice = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = text },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(session, slice);
}

void aquatakt_reminder_closed(bool timed_out) {
  if (timed_out && s_launched_by_wakeup) window_stack_pop_all(false);
}

static void prv_init(void) {
  schedule_init();
  main_window_push();

  switch (launch_reason()) {
    case APP_LAUNCH_WAKEUP: {
      WakeupId id;
      int32_t cookie;
      if (wakeup_get_launch_event(&id, &cookie)) {
        s_launched_by_wakeup = true;
        reminder_window_push();
      }
      break;
    }
    case APP_LAUNCH_TIMELINE_ACTION:
      if (launch_get_args() == LAUNCH_CODE_DRUNK && schedule_count() < AT_GLASSES) {
        schedule_set_count(schedule_count() + 1);
        vibes_short_pulse();
        main_window_refresh();
      }
      break;
    default:
      break;
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
