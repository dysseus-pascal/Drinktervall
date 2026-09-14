#include "reminder_window.h"
#include "drinktervall.h"
#include "config.h"
#include "theme.h"
#include "glass_fx.h"
#include "drink_window.h"
#include "schedule.h"
#include "phone.h"
#include "strings.h"

// Erinnerung im Stil eines Timeline-Pin-Details: Kopfband mit Glas und Zeit,
// schwarze Linie, weisse Karte mit dem Aufruf, rechts die schwarze
// Aktionsleiste mit Haekchen (Getrunken) und Zz (Spaeter).
// Bleibt stehen, bis eine Taste gedrueckt wird; alle drei Wege halten die
// Unterbrechung kurz:
//   Mitte   Getrunken: zaehlt +1, zeigt die Trink-Animation, App beendet sich
//   Unten   Spaeter: in DT_SNOOZE_MIN Minuten nochmals, App beendet sich
//   Zurueck schliesst ohne zu zaehlen (Glas verpasst); nach einem
//           Wakeup-Start beendet sich die App, sonst zurueck zum Hauptscreen

#define VIBE_REPEATS 3
#define VIBE_INTERVAL_MS 20000

static Window *s_window;
static Layer *s_canvas;
static ActionBarLayer *s_bar;
static GBitmap *s_icon_check, *s_icon_snooze;
static AppTimer *s_vibe_timer;
static int s_vibes_left;

static void prv_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);      // ohne Aktionsleiste
  const bool wide = b.size.w >= 150;
  const int16_t margin = PBL_IF_ROUND_ELSE(40, 8);
  const int16_t head_h = wide ? 74 : 60;
  const int count = schedule_count();

  // Kopf wie im Detail eines Timeline-Pins, aber auf weissem Grund: Uhrzeit
  // klein, Glas links, Uhrzeit der Erinnerung gross in LECO, darunter die
  // schwarze Trennlinie zur Karte
  graphics_context_set_text_color(ctx, DT_COLOR_TEXT);
  char clock[10];
  clock_copy_time_string(clock, sizeof(clock));
  graphics_draw_text(ctx, clock, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, PBL_IF_ROUND_ELSE(6, 0), b.size.w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  const int16_t glass_w = wide ? 40 : 32;
  glass_fx_draw_still(ctx, GPoint(margin + glass_w / 2, head_h / 2 + 8), glass_w, 850,
                      DT_COLOR_FX_WATER);
  graphics_draw_text(ctx, clock,
                     fonts_get_system_font(wide ? FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
                                                : FONT_KEY_LECO_20_BOLD_NUMBERS),
                     GRect(margin + glass_w + 10, head_h / 2 - (wide ? 10 : 8),
                           b.size.w - margin - glass_w - 12, 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, head_h, b.size.w, 2), 0, GCornerNone);

  // Karte
  const int16_t text_w = b.size.w - 2 * margin;
  int16_t y = head_h + 6;
  GFont title_font = fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD);
  const char *title = S(STR_TIME_FOR_WATER);
  GRect title_box = GRect(margin, y, text_w, 90);
  GSize title_size = graphics_text_layout_get_content_size(title, title_font, title_box,
                                                           GTextOverflowModeWordWrap,
                                                           GTextAlignmentLeft);
  graphics_draw_text(ctx, title, title_font, title_box, GTextOverflowModeWordWrap,
                     GTextAlignmentLeft, NULL);
  y += title_size.h + 4;
  char sub[24];
  if (count >= schedule_goal()) {
    snprintf(sub, sizeof(sub), "%s", S(STR_DAILY_GOAL_MET));
  } else {
    snprintf(sub, sizeof(sub), S(STR_GLASS_N_OF_M), count + 1, schedule_goal());
  }
  graphics_draw_text(ctx, sub, fonts_get_system_font(wide ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14),
                     GRect(margin, y, text_w, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}

static void prv_cancel_vibes(void) {
  if (s_vibe_timer) {
    app_timer_cancel(s_vibe_timer);
    s_vibe_timer = NULL;
  }
}

static void prv_vibe(void *data) {
  s_vibe_timer = NULL;
  vibes_double_pulse();
  if (--s_vibes_left > 0) s_vibe_timer = app_timer_register(VIBE_INTERVAL_MS, prv_vibe, NULL);
}

// Getrunken: zaehlen, die Trink-Animation als kurze Rueckmeldung zeigen und
// danach die App verlassen. Das Trink-Fenster kommt ueber die Erinnerung,
// die darunter weggenommen wird - so bleibt nach der Animation nichts stehen.
static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (schedule_count() < schedule_goal()) {
    schedule_set_count(schedule_count() + 1);
    phone_note_drink();
  }
  vibes_short_pulse();
  phone_send_next();
  prv_cancel_vibes();
  drink_window_push(true);
  window_stack_remove(s_window, false);
}

// Spaeter: in DT_SNOOZE_MIN Minuten nochmals erinnern und die App sofort
// beenden, damit die Uhr zum Zifferblatt zurueckkehrt
static void prv_down(ClickRecognizerRef recognizer, void *context) {
  schedule_plan_wakeups(time(NULL) + DT_SNOOZE_MIN * 60);
  prv_cancel_vibes();
  window_stack_pop_all(false);
}

// Zurueck: schliessen, ohne zu zaehlen - das Glas gilt als verpasst
static void prv_back(ClickRecognizerRef recognizer, void *context) {
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
  light_enable_interaction();
}

static void prv_unload(Window *window) {
  prv_cancel_vibes();
  action_bar_layer_destroy(s_bar);
  gbitmap_destroy(s_icon_check);
  gbitmap_destroy(s_icon_snooze);
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
  window_set_background_color(s_window, DT_COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
