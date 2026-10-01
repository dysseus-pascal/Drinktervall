#include "coffee_window.h"
#include "drinktervall.h"
#include "config.h"
#include "theme.h"
#include "coffee.h"
#include "schedule.h"
#include "phone.h"
#include "strings.h"

// Aufgebaut wie die Wasser-Erinnerung (reminder_window.c), damit beide als
// eine App erkennbar sind: Kopf mit Tasse und Uhrzeit, schwarze Linie, Karte,
// rechts die Aktionsleiste.

#define VIBE_REPEATS 3
#define VIBE_INTERVAL_MS 20000
// Nach dem Haken bleibt "Enjoy!" kurz stehen und die App wartet, bis das
// Telefon den Kaffee hat - hoechstens so lange.
#define DANKE_MS 900
#define WAIT_PHONE_MS 5000
#define WAIT_STEP_MS 250

static Window *s_window;
static Layer *s_canvas;
static ActionBarLayer *s_bar;
static GBitmap *s_icon_check, *s_icon_snooze;
static AppTimer *s_vibe_timer, *s_close_timer;
static int s_vibes_left;
static int s_idx;
static bool s_done;
static uint16_t s_waited_ms;

// Eine Tasse aus Grundformen - eine eigene Bitmap je Plattform waere fuer
// ein Symbol dieser Groesse zu viel.
static void prv_draw_cup(GContext *ctx, GPoint mitte, int16_t w) {
  const int16_t h = w * 3 / 4;
  const GRect becher = GRect(mitte.x - w / 2, mitte.y - h / 2, w * 3 / 4, h);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorBlack));
  graphics_fill_rect(ctx, becher, 4, GCornersBottom);
  graphics_context_set_stroke_color(ctx, PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorBlack));
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_circle(ctx, GPoint(becher.origin.x + becher.size.w, mitte.y - h / 8), h / 4);
  graphics_context_set_stroke_width(ctx, 1);
}

static void prv_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const bool wide = b.size.w >= 150;
  const int16_t margin = PBL_IF_ROUND_ELSE(40, 8);
  const int16_t head_h = wide ? 74 : 60;
  const CoffeeSlot *slot = coffee_slot(s_idx);

  graphics_context_set_text_color(ctx, DT_COLOR_TEXT);
  char clock[10];
  clock_copy_time_string(clock, sizeof(clock));
  graphics_draw_text(ctx, clock, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, PBL_IF_ROUND_ELSE(6, 0), b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const int16_t cup_w = wide ? 36 : 28;
  prv_draw_cup(ctx, GPoint(margin + cup_w / 2, head_h / 2 + 8), cup_w);
  graphics_draw_text(ctx, clock,
                     fonts_get_system_font(wide ? FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
                                                : FONT_KEY_LECO_20_BOLD_NUMBERS),
                     GRect(margin + cup_w + 14, head_h / 2 - (wide ? 10 : 8),
                           b.size.w - margin - cup_w - 16, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, head_h, b.size.w, 2), 0, GCornerNone);

  const int16_t text_w = b.size.w - 2 * margin;
  int16_t y = head_h + 6;
  GFont title_font = fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const char *title = s_done ? S(STR_ENJOY) : S(STR_COFFEE_TIME);
  GRect title_box = GRect(margin, y, text_w, 90);
  GSize title_size = graphics_text_layout_get_content_size(title, title_font, title_box,
                                                           GTextOverflowModeWordWrap,
                                                           GTextAlignmentLeft);
  graphics_draw_text(ctx, title, title_font, title_box, GTextOverflowModeWordWrap,
                     GTextAlignmentLeft, NULL);
  y += title_size.h + 4;
  if (slot) {
    char sub[40];
    coffee_describe(slot, sub, sizeof(sub));
    graphics_draw_text(ctx, sub, fonts_get_system_font(wide ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14),
                       GRect(margin, y, text_w, 44), GTextOverflowModeWordWrap,
                       GTextAlignmentLeft, NULL);
  }
}

static void prv_cancel_vibes(void) {
  if (s_vibe_timer) {
    app_timer_cancel(s_vibe_timer);
    s_vibe_timer = NULL;
  }
}

static void prv_vibe(void *data) {
  s_vibe_timer = NULL;
  drinktervall_buzz_double();
  if (--s_vibes_left > 0) s_vibe_timer = app_timer_register(VIBE_INTERVAL_MS, prv_vibe, NULL);
}

// ERST ZU, WENN DAS TELEFON DEN KAFFEE HAT - wie beim Glas (drink_window.c).
// Danach geht die App so oder so; was nicht ankam, geht beim naechsten Start.
static void prv_close(void *data) {
  s_close_timer = NULL;
  if (phone_pending() && s_waited_ms < WAIT_PHONE_MS) {
    s_waited_ms += WAIT_STEP_MS;
    s_close_timer = app_timer_register(WAIT_STEP_MS, prv_close, NULL);
    return;
  }
  window_stack_remove(s_window, true);
  drinktervall_reminder_closed();
}

// Getrunken: fuer die Akte vormerken, kurz bestaetigen, dann zu.
static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (s_done) return;
  const CoffeeSlot *slot = coffee_slot(s_idx);
  if (slot) phone_note_coffee(slot->kind, slot->flags);
  s_done = true;
  prv_cancel_vibes();
  drinktervall_buzz_short();
  phone_send_next();
  layer_mark_dirty(s_canvas);
  s_waited_ms = 0;
  s_close_timer = app_timer_register(DANKE_MS, prv_close, NULL);
}

// Spaeter: in DT_SNOOZE_MIN Minuten dieselbe Kaffee-Erinnerung nochmals.
static void prv_down(ClickRecognizerRef recognizer, void *context) {
  if (s_done) return;
  schedule_plan_wakeups(time(NULL) + DT_SNOOZE_MIN * 60, SCHEDULE_COOKIE_COFFEE + s_idx);
  prv_cancel_vibes();
  window_stack_pop_all(false);
}

// Zurueck: diesmal keinen. Nichts geht an die Akte.
static void prv_back(ClickRecognizerRef recognizer, void *context) {
  if (s_done) return;
  prv_cancel_vibes();
  window_stack_remove(s_window, true);
  drinktervall_reminder_closed();
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  s_canvas = layer_create(GRect(0, 0, bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h));
  layer_set_update_proc(s_canvas, prv_update);
  layer_add_child(root, s_canvas);

  s_icon_check = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK);
  s_icon_snooze = gbitmap_create_with_resource(RESOURCE_ID_ICON_SNOOZE);
  s_bar = action_bar_layer_create();
  action_bar_layer_set_background_color(s_bar, GColorBlack);
  action_bar_layer_set_icon(s_bar, BUTTON_ID_SELECT, s_icon_check);
  action_bar_layer_set_icon(s_bar, BUTTON_ID_DOWN, s_icon_snooze);
  action_bar_layer_set_click_config_provider(s_bar, prv_click_config);
  action_bar_layer_add_to_window(s_bar, window);

  s_vibes_left = VIBE_REPEATS;
  prv_vibe(NULL);
  drinktervall_light();
}

static void prv_unload(Window *window) {
  prv_cancel_vibes();
  if (s_close_timer) {
    app_timer_cancel(s_close_timer);
    s_close_timer = NULL;
  }
  action_bar_layer_destroy(s_bar);
  gbitmap_destroy(s_icon_check);
  gbitmap_destroy(s_icon_snooze);
  layer_destroy(s_canvas);
  window_destroy(s_window);
  s_window = NULL;
  s_canvas = NULL;
}

void coffee_window_push(int idx) {
  // Ein Platz, den es nicht mehr gibt (Plan geaendert, Wecker noch alt):
  // dann keine Erinnerung statt einer leeren.
  if (!coffee_slot(idx)) {
    drinktervall_reminder_closed();
    return;
  }
  if (s_window) {
    // Schon offen: auf den neuen Kaffee umstellen und neu vibrieren.
    if (s_done) return;
    s_idx = idx;
    s_vibes_left = VIBE_REPEATS;
    if (!s_vibe_timer) prv_vibe(NULL);
    layer_mark_dirty(s_canvas);
    return;
  }
  s_idx = idx;
  s_done = false;
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
