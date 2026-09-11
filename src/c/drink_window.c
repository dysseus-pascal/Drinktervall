#include "drink_window.h"
#include "config.h"
#include "theme.h"
#include "glass_fx.h"
#include "schedule.h"

// Nach dem Ende der Animation bleibt der leere Grund noch so lange stehen
#define HOLD_AFTER_MS 350

static Window *s_window;
static TextLayer *s_text;
static AppTimer *s_close_timer;
static char s_title[24];

static void prv_close(void *data) {
  s_close_timer = NULL;
  if (s_window) window_stack_remove(s_window, true);
}

static void prv_done(void) {
  if (!s_close_timer) s_close_timer = app_timer_register(HOLD_AFTER_MS, prv_close, NULL);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect b = layer_get_bounds(root);
  snprintf(s_title, sizeof(s_title), "Glas %d von %d", schedule_count(), DT_GLASSES);
  s_text = text_layer_create(GRect(0, b.size.h * 74 / 100, b.size.w, 34));
  text_layer_set_font(s_text, fonts_get_system_font(
      b.size.w >= 180 ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_background_color(s_text, GColorClear);
  text_layer_set_text_color(s_text, DT_COLOR_ON_LIGHT);
  text_layer_set_text(s_text, s_title);
  layer_add_child(root, text_layer_get_layer(s_text));
  glass_fx_init(root);
}

static void prv_appear(Window *window) {
  const GRect b = layer_get_bounds(window_get_root_layer(window));
  glass_fx_play(GPoint(b.size.w / 2, b.size.h * 40 / 100), b.size.w * 56 / 100, prv_done);
}

static void prv_unload(Window *window) {
  if (s_close_timer) {
    app_timer_cancel(s_close_timer);
    s_close_timer = NULL;
  }
  glass_fx_deinit();
  text_layer_destroy(s_text);
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
