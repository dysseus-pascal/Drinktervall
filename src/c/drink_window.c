#include "drink_window.h"
#include "theme.h"
#include "glass_fx.h"

// Nach dem Ende der Animation bleibt der leere Grund noch so lange stehen
#define HOLD_AFTER_MS 350

static Window *s_window;
static AppTimer *s_close_timer;

static void prv_close(void *data) {
  s_close_timer = NULL;
  if (s_window) window_stack_remove(s_window, true);
}

static void prv_done(void) {
  if (!s_close_timer) s_close_timer = app_timer_register(HOLD_AFTER_MS, prv_close, NULL);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  glass_fx_init(root);
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

void drink_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_FX_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .appear = prv_appear, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}

bool drink_window_is_open(void) {
  return s_window != NULL;
}
