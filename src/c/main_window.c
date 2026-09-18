#include "main_window.h"
#include "theme.h"
#include "drinktervall.h"
#include "drink_window.h"
#include "glass_fx.h"
#include "schedule.h"
#include "plan_window.h"
#include "phone.h"
#include "strings.h"

// Hauptscreen im Stil der Timeline: weisser Grund, schwarze Schrift, rechts
// die dunkle Seitenleiste mit Glas-Symbol und Tasten-Hinweisen. Der Pegel
// (getrunkene Glaeser / Tagesziel) steigt als hellblaues Band ueber den
// Inhalt; s_water ist dieses Band, s_canvas zeichnet Text und Leiste darueber.
//
// Ein neues Glas (mittlere Taste) oeffnet zuerst das Vollbild-Fenster mit der
// Trink-Animation (drink_window); wenn es sich schliesst, ziehen Anzeige und
// Pegel nach. s_shown_count ist der angezeigte Stand, schedule_count() der
// echte - der steigt schon beim Tastendruck, also bevor die Animation laeuft.
// Ist die Animation auf der Konfigseite abgeschaltet, faellt das Fenster weg
// und der Pegel steigt sofort; gezaehlt wird in beiden Faellen gleich.

#define FILL_ANIM_MS 350

static Window *s_window;
static Layer *s_canvas;
static Layer *s_water;
static PropertyAnimation *s_anim;
static int s_shown_count;
static int s_fx_count = -1;   // Zaehlerstand, fuer den das Trink-Fenster schon lief

static GRect prv_water_frame(GRect full) {
  const int16_t h = (int16_t)((int32_t)full.size.h * s_shown_count / schedule_goal());
  return GRect(0, full.size.h - h, full.size.w - DT_SIDEBAR_W, h);
}

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const bool wide = PBL_DISPLAY_WIDTH >= 180;    // emery/gabbro breit, flint schmal
  const int16_t margin = PBL_IF_ROUND_ELSE(38, 9);
  const int16_t col_w = b.size.w - DT_SIDEBAR_W - margin - 4;

  // Inhalt links, wie ein Timeline-Eintrag: Uhr, naechste Erinnerung in
  // LECO, Titel, Untertitel
  graphics_context_set_text_color(ctx, DT_COLOR_TEXT);
  char clock[10];
  clock_copy_time_string(clock, sizeof(clock));
  graphics_draw_text(ctx, clock, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, PBL_IF_ROUND_ELSE(10, 0), b.size.w - DT_SIDEBAR_W, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  time_t now = time(NULL);
  time_t next;
  schedule_next(now, &next);
  char hhmm[8];
  schedule_format_time(next, hhmm, sizeof(hhmm));
  const bool tomorrow = next >= schedule_midnight(now) + 86400;
  int16_t y = PBL_IF_ROUND_ELSE(46, 18);
  graphics_draw_text(ctx, tomorrow ? S(STR_TOMORROW) : S(STR_NEXT_REMINDER),
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin, y, col_w, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += 14;
  graphics_draw_text(ctx, hhmm,
                     fonts_get_system_font(wide ? FONT_KEY_LECO_32_BOLD_NUMBERS
                                                : FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM),
                     GRect(margin, y, col_w, wide ? 38 : 32),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += wide ? 46 : 38;

  char title[20];
  const int count = s_shown_count, goal = schedule_goal();
  if (count >= goal) {
    snprintf(title, sizeof(title), "%s", S(STR_GOAL_REACHED));
  } else {
    snprintf(title, sizeof(title), S(STR_GLASS_N_OF_M), count + 1, goal);
  }
  graphics_draw_text(ctx, title,
                     fonts_get_system_font(wide ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin, y, col_w, wide ? 30 : 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  y += wide ? 30 : 24;
  char sub[20];
  snprintf(sub, sizeof(sub), S(STR_N_DONE), count);
  graphics_draw_text(ctx, sub,
                     fonts_get_system_font(wide ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_14),
                     GRect(margin, y, col_w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Seitenleiste: Glas-Symbol oben, Tasten-Hinweise auf Hoehe der Tasten
  const int16_t sx = b.size.w - DT_SIDEBAR_W;
  graphics_context_set_fill_color(ctx, DT_COLOR_SIDEBAR);
  graphics_fill_rect(ctx, GRect(sx, 0, DT_SIDEBAR_W, b.size.h), 0, GCornerNone);
  const int16_t cx = sx + DT_SIDEBAR_W / 2 - DT_SIDEBAR_GLASS_DX;
  glass_fx_draw_still(ctx, GPoint(cx, DT_SIDEBAR_GLASS_Y),
                      DT_SIDEBAR_GLASS_W, DT_SIDEBAR_GLASS_FILL, DT_COLOR_FX_WATER);
  graphics_context_set_text_color(ctx, DT_COLOR_ON_SIDEBAR);
  const char *hints[3] = { S(STR_HINT_PLAN), S(STR_HINT_PLUS_ONE), S(STR_HINT_GOAL_UP) };
  for (int i = 0; i < 3; i++) {
    const int16_t hy = b.size.h * (i + 1) / 4;
    graphics_draw_text(ctx, hints[i], fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(cx - 24, hy - 9, 48, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void prv_water_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, DT_COLOR_BAND);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  // Wasserlinie als Kante, wie die schwarzen Linien der Timeline
  graphics_context_set_fill_color(ctx, DT_COLOR_SIDEBAR);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, 2), 0, GCornerNone);
}

static void prv_anim_stopped(Animation *animation, bool finished, void *context) {
  // Das SDK gibt beendete Animationen nicht selbst frei; auch nach
  // animation_unschedule landet man hier.
  property_animation_destroy((PropertyAnimation *)animation);
  s_anim = NULL;
}

static void prv_set_level(bool animate) {
  GRect to = prv_water_frame(layer_get_bounds(s_canvas));
  if (s_anim) {
    animation_unschedule((Animation *)s_anim);
    s_anim = NULL;
  }
  GRect from = layer_get_frame(s_water);
  if (animate) s_anim = property_animation_create_layer_frame(s_water, &from, &to);
  if (!s_anim) {
    // kein Animationswunsch oder kein Speicher dafuer: direkt auf den Stand
    layer_set_frame(s_water, to);
    return;
  }
  Animation *anim = (Animation *)s_anim;
  animation_set_duration(anim, FILL_ANIM_MS);
  animation_set_curve(anim, AnimationCurveEaseOut);
  animation_set_handlers(anim, (AnimationHandlers) { .stopped = prv_anim_stopped }, NULL);
  animation_schedule(anim);
}

// Anzeige mit dem echten Stand abgleichen. Ein neues Glas oeffnet zuerst das
// Trink-Fenster; wenn es sich schliesst, steigt der Pegel animiert. Alles
// andere springt direkt.
static void prv_sync(void) {
  if (drink_window_is_open()) return;
  const int count = schedule_count();
  const bool grew = count > s_shown_count;
  if (grew && s_fx_count != count) {
    s_fx_count = count;
    // Ist die Animation abgeschaltet, kommt kein Fenster - dann uebernimmt der
    // steigende Pegel unten die Rueckmeldung, statt hinter dem Fenster zu
    // passieren, wo ihn niemand saehe.
    if (drink_window_push(false)) return;
  }
  s_shown_count = count;
  layer_mark_dirty(s_canvas);
  prv_set_level(grew);
}

// Aenderung von aussen (neues Soll von der Konfigseite): Anzeige und Pegel
// nachziehen. Laeuft der Screen gerade nicht, ist nichts zu tun - beim
// naechsten Erscheinen gleicht prv_sync ohnehin ab.
void main_window_refresh(void) {
  if (!s_window || !s_canvas) return;
  s_shown_count = schedule_count();
  layer_mark_dirty(s_canvas);
  prv_set_level(true);
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas);
}

static void prv_up(ClickRecognizerRef recognizer, void *context) {
  plan_window_push();
}

static void prv_select(ClickRecognizerRef recognizer, void *context) {
  if (schedule_count() >= schedule_goal()) return;
  schedule_set_count(schedule_count() + 1);
  phone_note_drink();
  drinktervall_buzz_short();
  phone_send_next();
  prv_sync();
}

// Tagesziel um ein Glas erhoehen, damit ueber das Maximum hinaus geloggt werden kann
static void prv_down(ClickRecognizerRef recognizer, void *context) {
  schedule_raise_goal();
  phone_send_next();
  layer_mark_dirty(s_canvas);
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
  s_shown_count = schedule_count();
  s_water = layer_create(prv_water_frame(bounds));
  layer_set_update_proc(s_water, prv_water_update);
  layer_add_child(root, s_water);
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, prv_canvas_update);
  layer_add_child(root, s_canvas);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);
}

static void prv_appear(Window *window) {
  prv_sync();
}

static void prv_unload(Window *window) {
  tick_timer_service_unsubscribe();
  if (s_anim) {
    animation_unschedule((Animation *)s_anim);
    s_anim = NULL;
  }
  layer_destroy(s_canvas);
  layer_destroy(s_water);
  window_destroy(s_window);
  s_window = NULL;
  s_canvas = NULL;
  s_water = NULL;
}

void main_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_BG);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .appear = prv_appear, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
