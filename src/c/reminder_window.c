#include "reminder_window.h"
#include "drinktervall.h"
#include "config.h"
#include "theme.h"
#include "draw.h"
#include "schedule.h"
#include "main_window.h"

#define VIBE_REPEATS 3
#define VIBE_INTERVAL_MS 20000

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_vibe_timer;
static AppTimer *s_close_timer;
static int s_vibes_left;

static void prv_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const bool wide = bounds.size.w >= 180;
  const int16_t margin = PBL_IF_ROUND_ELSE(30, 10);
  const int16_t text_w = bounds.size.w - 2 * margin;
  const int count = schedule_count();

  graphics_context_set_text_color(ctx, DT_COLOR_ON_PRIMARY);
  const char *title = "Zeit für ein Glas Wasser!";
  GFont title_font = fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  GRect title_box = GRect(margin, PBL_IF_ROUND_ELSE(16, 6), text_w, 90);
  GSize title_size = graphics_text_layout_get_content_size(title, title_font, title_box,
                                                           GTextOverflowModeWordWrap,
                                                           GTextAlignmentCenter);
  graphics_draw_text(ctx, title, title_font, title_box, GTextOverflowModeWordWrap,
                     GTextAlignmentCenter, NULL);

  char sub[24];
  if (count >= DT_GLASSES) {
    snprintf(sub, sizeof(sub), "Tagesziel erreicht");
  } else {
    snprintf(sub, sizeof(sub), "Glas %d von %d", count + 1, DT_GLASSES);
  }
  const int16_t sub_y = title_box.origin.y + title_size.h + 2;
  graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(margin, sub_y, text_w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Glas unten links; die Tasten-Hinweise sitzen rechts
  const int16_t glass_w = bounds.size.w * 34 / 100;
  const int16_t avail_y = sub_y + 26;
  const int16_t avail_h = bounds.size.h - avail_y - PBL_IF_ROUND_ELSE(30, 8);
  int16_t glass_h = avail_h;
  if (glass_h > glass_w * 3 / 2) glass_h = glass_w * 3 / 2;
  if (glass_h > 30) {
    GRect glass = GRect(margin, avail_y + (avail_h - glass_h) / 2, glass_w, glass_h);
    draw_glass(ctx, glass, count * 1000 / DT_GLASSES, DT_COLOR_ON_PRIMARY, DT_COLOR_WATER_DARK);
  }

  draw_button_hints(ctx, bounds, NULL, "Getrunken", "Später", DT_COLOR_BG, DT_COLOR_PRIMARY);
}

static void prv_cancel_timers(void) {
  if (s_vibe_timer) { app_timer_cancel(s_vibe_timer); s_vibe_timer = NULL; }
  if (s_close_timer) { app_timer_cancel(s_close_timer); s_close_timer = NULL; }
}

static void prv_close(bool timed_out) {
  prv_cancel_timers();
  window_stack_remove(s_window, !timed_out);
  main_window_refresh();
  drinktervall_reminder_closed(timed_out);
}

static void prv_vibe(void *data) {
  s_vibe_timer = NULL;
  vibes_double_pulse();
  if (--s_vibes_left > 0) s_vibe_timer = app_timer_register(VIBE_INTERVAL_MS, prv_vibe, NULL);
}

static void prv_timeout(void *data) {
  s_close_timer = NULL;
  prv_close(true);
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (schedule_count() < DT_GLASSES) schedule_set_count(schedule_count() + 1);
  vibes_short_pulse();
  prv_close(false);
}

static void prv_down(ClickRecognizerRef recognizer, void *context) {
  schedule_plan_wakeups(time(NULL) + DT_SNOOZE_MIN * 60);
  prv_close(false);
}

static void prv_back(ClickRecognizerRef recognizer, void *context) {
  prv_close(false);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, prv_update);
  layer_add_child(root, s_canvas);
  s_vibes_left = VIBE_REPEATS;
  prv_vibe(NULL);
  s_close_timer = app_timer_register(DT_REMINDER_TIMEOUT_S * 1000, prv_timeout, NULL);
  light_enable_interaction();
}

static void prv_unload(Window *window) {
  prv_cancel_timers();
  layer_destroy(s_canvas);
  window_destroy(s_window);
  s_window = NULL;
  s_canvas = NULL;
}

void reminder_window_push(void) {
  if (s_window) {
    // Schon offen (zweites Wakeup waehrend die App laeuft): nur neu vibrieren.
    s_vibes_left = VIBE_REPEATS;
    if (!s_vibe_timer) prv_vibe(NULL);
    layer_mark_dirty(s_canvas);
    return;
  }
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_PRIMARY);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
