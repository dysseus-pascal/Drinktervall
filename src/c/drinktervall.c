#include <pebble.h>
#include "drinktervall.h"
#include "schedule.h"
#include "main_window.h"
#include "reminder_window.h"
#include "coffee_window.h"
#include "coffee.h"
#include "drink_window.h"
#include "phone.h"
#include "strings.h"

// Launch-Code der Timeline-Pin-Aktion "Getrunken"/"Nachholen" (siehe
// src/pkjs/index.js): der Tag des Pins mal zehn plus diese 1, also
// JJJJMMTT1. Pins aelterer Fassungen tragen nur die 1. Code 2 ("App
// oeffnen") braucht hier keinen Fall: die App startet ohnehin.
#define LAUNCH_CODE_DRUNK 1

// EIN PIN VON GESTERN ZAEHLT NICHT FUER HEUTE (Audit N5, so entschieden).
// Bis 1.20 trug die Aktion keinen Tag, und "Nachholen" an einem verpassten
// Glas von gestern zaehlte ein Glas fuer heute. Ein Pin von morgen (die
// naechste Erinnerung am Abend) zaehlt weiter fuer heute, wie bisher. Nur
// die 1 ohne Tag gilt wie frueher - von wann sie ist, weiss niemand.
static bool prv_pin_zaehlt(uint32_t code) {
  if (code == LAUNCH_CODE_DRUNK) return true;
  if (code < 10 || code % 10 != LAUNCH_CODE_DRUNK) return false;
  return (int32_t)(code / 10) >= schedule_day();
}

static bool s_launched_by_wakeup;
// Gesetzt, wenn die App gehen wollte, waehrend eine Erinnerung offen stand -
// etwa das Trink-Fenster eines Pin-Starts, in dessen Animation ein Wecker
// fiel. Ohne Wakeup-Start bliebe die App sonst nach dieser Erinnerung auf
// dem Hauptscreen stehen.
static bool s_verlassen_vorgemerkt;

// Welche Erinnerung ein Wecker meint, steht in seinem Cookie: Kaffees tragen
// SCHEDULE_COOKIE_COFFEE + Platz, alles darunter ist Wasser.
static void prv_remind(int32_t cookie) {
  if (cookie >= SCHEDULE_COOKIE_CUSTOM) {
    coffee_window_push_custom((int)(cookie - SCHEDULE_COOKIE_CUSTOM));
  } else if (cookie >= SCHEDULE_COOKIE_COFFEE) {
    coffee_window_push((int)(cookie - SCHEDULE_COOKIE_COFFEE));
  } else if (schedule_count() >= schedule_goal()) {
    // Ziel erreicht: keine Wasser-Erinnerung mehr (Audit N5). Die Wecker
    // werden dann ohnehin neu geplant; kommt doch noch einer, der vorher
    // stand, bleibt die Uhr still.
    APP_LOG(APP_LOG_LEVEL_INFO, "Tagesziel erreicht: Wasser-Erinnerung %d entfaellt", (int)cookie);
    drinktervall_reminder_closed();
  } else {
    reminder_window_push();
  }
}

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  prv_remind(cookie);
  schedule_plan_wakeups(0, 0);
  phone_send_next();   // Timeline-Pins auf den neuen Stand bringen
}

// DER STAND VON HEUTE GILT BIS MITTERNACHT. Bis 1.20 lief die eine Scheibe
// nie ab: wer die App abends schloss, sah bis zum ersten Wecker am Morgen
// den Vortag ("8 von 8 Glaesern", Audit N4). Gebaut wird nur beim Beenden -
// darum liegt der Morgen schon als zweite Scheibe bereit.
static void prv_glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) return;
  const time_t now = time(NULL);
  const time_t morgen = schedule_tag(schedule_tag(now) + 86400);
  time_t next;
  schedule_next(now, &next);
  char hhmm[8];
  schedule_format_time(next, hhmm, sizeof(hhmm));
  char text[48];
  snprintf(text, sizeof(text), S(STR_GLANCE_FMT),
           schedule_count(), schedule_goal(), hhmm);
  const AppGlanceSlice heute = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = text },
    .expiration_time = schedule_wandzeit(morgen, 0),
  };
  app_glance_add_slice(session, heute);
  if (limit < 2) return;
  // Ab Mitternacht: null Glaeser, das Soll als Ziel, das erste Glas.
  schedule_format_time(schedule_slot(morgen, 0), hhmm, sizeof(hhmm));
  snprintf(text, sizeof(text), S(STR_GLANCE_FMT), 0, schedule_target(), hhmm);
  const AppGlanceSlice danach = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = text },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION,
  };
  app_glance_add_slice(session, danach);
}

// Bis 1.20 ging die App hier immer mit pop_all - auch wenn waehrend "Enjoy!"
// oder der Trink-Animation schon die naechste Erinnerung gekommen war.
bool drinktervall_verlassen(void) {
  if (reminder_window_offen() || coffee_window_offen()) return false;
  window_stack_pop_all(false);
  return true;
}

void drinktervall_verlassen_vormerken(void) {
  s_verlassen_vorgemerkt = true;
}

void drinktervall_reminder_closed(void) {
  if (s_launched_by_wakeup || s_verlassen_vorgemerkt) drinktervall_verlassen();
}

// --- Ruhezeit ---
// quiet_time_is_active() gibt es seit SDK 3.12; sie beantwortet genau die
// Frage, die hier zu stellen ist - ob die Uhr gerade still sein soll. Selbst
// Nachtstunden auszurechnen waere eine zweite, schlechtere Antwort daneben:
// die Ruhezeit kennt auch den Kalender und den Schalter von Hand.

bool drinktervall_quiet(void) { return quiet_time_is_active(); }

void drinktervall_buzz_short(void) {
  if (!drinktervall_quiet()) vibes_short_pulse();
}

void drinktervall_buzz_double(void) {
  if (!drinktervall_quiet()) vibes_double_pulse();
}

void drinktervall_light(void) {
  if (!drinktervall_quiet()) light_enable_interaction();
}

static void prv_init(void) {
  // Sprache der Uhr uebernehmen, bevor das erste Fenster Texte holt
  strings_refresh();
  schedule_init();
  coffee_init();
  main_window_push();

  const AppLaunchReason reason = launch_reason();
  WakeupId id;
  int32_t cookie;
  if (reason == APP_LAUNCH_WAKEUP && wakeup_get_launch_event(&id, &cookie)) {
    s_launched_by_wakeup = true;
    prv_remind(cookie);
  } else if (reason == APP_LAUNCH_TIMELINE_ACTION && prv_pin_zaehlt(launch_get_args())
             && schedule_count() < schedule_goal()) {
    // Aus einem Timeline-Pin: zaehlen, kurz zeigen, App wieder verlassen
    schedule_set_count(schedule_count() + 1);
    phone_note_drink();
    drinktervall_buzz_short();
    drink_window_push(true);
  }

  wakeup_service_subscribe(prv_wakeup_handler);
  // Bei jedem Start neu planen: haelt die 8 Slots ueber Tagesgrenzen aktuell.
  schedule_plan_wakeups(0, 0);
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
