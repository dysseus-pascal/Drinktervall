#include "main_window.h"
#include "config.h"
#include "theme.h"
#include "draw.h"
#include "schedule.h"
#include "plan_window.h"

static Window *s_window;
static StatusBarLayer *s_status;
static Layer *s_canvas;

static void prv_update(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);
  const bool wide = bounds.size.w >= 180;
  const int16_t margin = PBL_IF_ROUND_ELSE(34, wide ? 12 : 8);
  const int16_t hint_space = 46;   // Platz fuer die Tasten-Hinweise rechts

  // Glas links, Zaehler rechts daneben
  const GRect glass = GRect(margin, bounds.origin.y + 10,
                            bounds.size.w * 36 / 100, bounds.size.h * 58 / 100);
  const int count = schedule_count();
  draw_glass(ctx, glass, count * 1000 / AT_GLASSES, AT_COLOR_PRIMARY, AT_COLOR_WATER);

  const int16_t col_x = glass.origin.x + glass.size.w + 8;
  const int16_t col_w = bounds.size.w - col_x - hint_space;
  char num[4];
  snprintf(num, sizeof(num), "%d", count);
  graphics_context_set_text_color(ctx, AT_COLOR_TEXT);
  GFont big = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  const int16_t num_y = glass.origin.y + glass.size.h / 2 - 34;
  graphics_draw_text(ctx, num, big, GRect(col_x, num_y, col_w, 46),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  char of[12];
  snprintf(of, sizeof(of), "von %d", AT_GLASSES);
  GFont small = fonts_get_system_font(col_w >= 44 ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14);
  graphics_draw_text(ctx, of, small, GRect(col_x, num_y + 48, col_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Naechste Erinnerung unter dem Glas
  time_t now = time(NULL);
  time_t next;
  schedule_next(now, &next);
  char hhmm[8];
  schedule_format_time(next, hhmm, sizeof(hhmm));
  char line[20];
  if (next >= schedule_midnight(now) + 86400) {
    snprintf(line, sizeof(line), "Morgen %s", hhmm);
  } else {
    snprintf(line, sizeof(line), "%s", hhmm);
  }
  const int16_t band_y = glass.origin.y + glass.size.h + 6;
  const int16_t band_w = bounds.size.w - margin - hint_space;
  graphics_draw_text(ctx, wide ? "Nächste Erinnerung" : "Nächste", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin, band_y, band_w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, line,
                     fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin, band_y + 14, band_w, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  draw_button_hints(ctx, bounds, "Plan", "+1", "-1", AT_COLOR_PRIMARY, AT_COLOR_ON_PRIMARY);
}

static void prv_up(ClickRecognizerRef recognizer, void *context) {
  plan_window_push();
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (schedule_count() >= AT_GLASSES) return;
  schedule_set_count(schedule_count() + 1);
  vibes_short_pulse();
  layer_mark_dirty(s_canvas);
}

static void prv_down(ClickRecognizerRef recognizer, void *context) {
  schedule_set_count(schedule_count() - 1);
  layer_mark_dirty(s_canvas);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  s_status = status_bar_layer_create();
  status_bar_layer_set_colors(s_status, AT_COLOR_PRIMARY, AT_COLOR_ON_PRIMARY);
  status_bar_layer_set_separator_mode(s_status, StatusBarLayerSeparatorModeNone);
  layer_add_child(root, status_bar_layer_get_layer(s_status));
  s_canvas = layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, bounds.size.w,
                                bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  layer_set_update_proc(s_canvas, prv_update);
  layer_add_child(root, s_canvas);
}

static void prv_appear(Window *window) {
  layer_mark_dirty(s_canvas);
}

static void prv_unload(Window *window) {
  layer_destroy(s_canvas);
  status_bar_layer_destroy(s_status);
  window_destroy(s_window);
  s_window = NULL;
  s_canvas = NULL;
}

void main_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, AT_COLOR_BG);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .appear = prv_appear, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}

void main_window_refresh(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}
