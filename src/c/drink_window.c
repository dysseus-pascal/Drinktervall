#include "drink_window.h"
#include "theme.h"
#include "glass_fx.h"
#include "schedule.h"
#include "phone.h"

// Nach dem Ende der Animation bleibt der leere Grund noch so lange stehen
#define HOLD_AFTER_MS 350

// Ist die Animation abgeschaltet, bleibt statt ihrer der Hauptscreen mit dem
// neuen Stand so lange stehen. Laenger als HOLD_AFTER_MS, weil es diesmal das
// Einzige ist, was man zu sehen bekommt - unter einer halben Sekunde spraenge
// der Zaehler weg, bevor man ihn gelesen hat.
#define HOLD_NO_FX_MS 900

static Window *s_window;
static AppTimer *s_close_timer;
static bool s_quit_after;

// So lange haelt das Fenster die App hoechstens offen, bis das Telefon das
// Glas bestaetigt hat. Danach geht sie trotzdem zu; das Glas bleibt in der
// Schlange und geht beim naechsten Start.
#define WAIT_PHONE_MS 5000
#define WAIT_STEP_MS 250
static uint16_t s_waited_ms;

static void prv_close(void *data) {
  s_close_timer = NULL;
  // ERST WENN DAS TELEFON DAS GLAS HAT. Die App ging frueher nach der
  // Animation sofort zu - oft bevor die Nachricht draussen war.
  if (s_quit_after && phone_pending() && s_waited_ms < WAIT_PHONE_MS) {
    s_waited_ms += WAIT_STEP_MS;
    s_close_timer = app_timer_register(WAIT_STEP_MS, prv_close, NULL);
    return;
  }
  // OHNE FENSTER GIBT ES TROTZDEM ETWAS ZU TUN. Ist die Animation aus, laeuft
  // dieser Zeitgeber ohne ein s_window - und dann ist gerade das Beenden der
  // App seine ganze Aufgabe. Das fruehere "if (!s_window) return" haette die
  // App offen stehen lassen.
  if (s_quit_after) {
    window_stack_pop_all(false);      // App verlassen, Watch zeigt das Zifferblatt
  } else if (s_window) {
    window_stack_remove(s_window, true);
  }
}

static void prv_done(void) {
  if (!s_close_timer) s_close_timer = app_timer_register(HOLD_AFTER_MS, prv_close, NULL);
}

static void prv_load(Window *window) {
  glass_fx_init(window_get_root_layer(window));
}

static void prv_appear(Window *window) {
  const GRect b = layer_get_bounds(window_get_root_layer(window));
  glass_fx_play(GPoint(b.size.w / 2, b.size.h / 2), b.size.w * 56 / 100, prv_done);
}

static void prv_unload(Window *window) {
  if (s_close_timer) {
    app_timer_cancel(s_close_timer);
    s_close_timer = NULL;
  }
  glass_fx_deinit();
  window_destroy(s_window);
  s_window = NULL;
}

bool drink_window_push(bool quit_after) {
  if (s_window) return true;
  s_quit_after = quit_after;
  s_waited_ms = 0;

  // Abgeschaltet: nichts zeigen. Der Weg muss trotzdem gleich enden wie mit
  // Animation - wer ueber die Erinnerung oder einen Timeline-Pin hierher kam,
  // erwartet, dass die App sich danach wieder schliesst. Nur der Hauptscreen
  // bekommt seinen Pegelanstieg selbst hin, der braucht keinen Zeitgeber.
  if (!schedule_animation()) {
    if (quit_after && !s_close_timer) {
      s_close_timer = app_timer_register(HOLD_NO_FX_MS, prv_close, NULL);
    }
    return false;
  }

  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_FX_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .appear = prv_appear, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
  return true;
}

bool drink_window_is_open(void) {
  return s_window != NULL;
}
