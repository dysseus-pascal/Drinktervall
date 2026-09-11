#include "main_window.h"
#include "config.h"
#include "theme.h"
#include "draw.h"
#include "schedule.h"
#include "plan_window.h"

// Der Hauptscreen ist ein Glas ohne Glas: s_canvas zeichnet die ganze Flaeche
// hell mit dunkler Schrift, s_water liegt als Kind-Layer ueber dem unteren
// Teil (Hoehe = getrunkene Glaeser / Tagesziel), zeichnet dort denselben
// Inhalt dunkel mit heller Schrift und wird vom eigenen Rahmen beschnitten.
// So dreht die Schrift an der Wasserlinie die Farbe.

#define FILL_ANIM_MS 350

static Window *s_window;
static Layer *s_canvas;
static Layer *s_water;
static PropertyAnimation *s_anim;

static GRect prv_water_frame(GRect full) {
  const int16_t h = (int16_t)((int32_t)full.size.h * schedule_count() / DT_GLASSES);
  return GRect(0, full.size.h - h, full.size.w, h);
}

// Gemeinsamer Inhalt beider Layer. `full` ist die ganze Flaeche; `shift_y`
// verschiebt sie fuer den Wasser-Layer nach oben, damit beide Zeichnungen
// auf dem Display deckungsgleich liegen.
static void prv_draw_content(GContext *ctx, GRect full, int16_t shift_y, bool on_dark) {
  const GColor text = on_dark ? DT_COLOR_ON_DARK : DT_COLOR_ON_LIGHT;
  const GColor tag_bg = on_dark ? DT_COLOR_ON_DARK : DT_COLOR_LEVEL_DARK;
  const GColor tag_fg = on_dark ? DT_COLOR_LEVEL_DARK : DT_COLOR_ON_DARK;
  const GRect area = GRect(full.origin.x, full.origin.y - shift_y, full.size.w, full.size.h);
  const bool wide = area.size.w >= 180;
  const int16_t margin = PBL_IF_ROUND_ELSE(28, 6);
  const int16_t hint_space = 46;   // Spalte der Tasten-Hinweise rechts
  const int16_t col_w = area.size.w - margin - hint_space;
  graphics_context_set_text_color(ctx, text);

  char clock[10];
  clock_copy_time_string(clock, sizeof(clock));
  graphics_draw_text(ctx, clock, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin, area.origin.y + PBL_IF_ROUND_ELSE(14, 2), col_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char num[4];
  snprintf(num, sizeof(num), "%d", schedule_count());
  const int16_t num_y = area.origin.y + area.size.h * 26 / 100;
  graphics_draw_text(ctx, num, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(margin, num_y, col_w, 46),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  char of[12];
  snprintf(of, sizeof(of), "von %d", DT_GLASSES);
  graphics_draw_text(ctx, of, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(margin, num_y + 48, col_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

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
  const int16_t next_y = area.origin.y + area.size.h * 66 / 100;
  graphics_draw_text(ctx, wide ? "Nächste Erinnerung" : "Nächste",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin, next_y, col_w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, line,
                     fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin, next_y + 14, col_w, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  draw_button_hints(ctx, area, "Plan", "+1", "-1", tag_bg, tag_fg);
}

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  const GRect full = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, DT_COLOR_LEVEL_LIGHT);
  graphics_fill_rect(ctx, full, 0, GCornerNone);
  prv_draw_content(ctx, full, 0, false);
}

static void prv_water_update(Layer *layer, GContext *ctx) {
  const GRect frame = layer_get_frame(layer);
  graphics_context_set_fill_color(ctx, DT_COLOR_LEVEL_DARK);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  prv_draw_content(ctx, layer_get_bounds(s_canvas), frame.origin.y, true);
}

static void prv_anim_stopped(Animation *animation, bool finished, void *context) {
  s_anim = NULL;
}

static void prv_set_level(bool animate) {
  GRect to = prv_water_frame(layer_get_bounds(s_canvas));
  if (s_anim) {
    animation_unschedule((Animation *)s_anim);
    s_anim = NULL;
  }
  if (!animate) {
    layer_set_frame(s_water, to);
    layer_mark_dirty(s_canvas);
    return;
  }
  GRect from = layer_get_frame(s_water);
  s_anim = property_animation_create_layer_frame(s_water, &from, &to);
  if (!s_anim) {
    layer_set_frame(s_water, to);
    return;
  }
  Animation *anim = (Animation *)s_anim;
  animation_set_duration(anim, FILL_ANIM_MS);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_handlers(anim, (AnimationHandlers) { .stopped = prv_anim_stopped }, NULL);
  animation_schedule(anim);
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
}

static void prv_up(ClickRecognizerRef recognizer, void *context) {
  plan_window_push();
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (schedule_count() >= DT_GLASSES) return;
  schedule_set_count(schedule_count() + 1);
  vibes_short_pulse();
  prv_set_level(true);
}

static void prv_down(ClickRecognizerRef recognizer, void *context) {
  schedule_set_count(schedule_count() - 1);
  prv_set_level(true);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, prv_canvas_update);
  layer_add_child(root, s_canvas);
  s_water = layer_create(prv_water_frame(bounds));
  layer_set_update_proc(s_water, prv_water_update);
  layer_add_child(s_canvas, s_water);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
}

static void prv_appear(Window *window) {
  prv_set_level(false);
}

static void prv_unload(Window *window) {
  tick_timer_service_unsubscribe();
  if (s_anim) {
    animation_unschedule((Animation *)s_anim);
    s_anim = NULL;
  }
  layer_destroy(s_water);
  layer_destroy(s_canvas);
  window_destroy(s_window);
  s_window = NULL;
  s_canvas = NULL;
  s_water = NULL;
}

void main_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_LEVEL_LIGHT);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .appear = prv_appear, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}

void main_window_refresh(void) {
  if (s_canvas) prv_set_level(true);
}
