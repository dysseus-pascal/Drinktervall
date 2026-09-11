#include "plan_window.h"
#include "config.h"
#include "theme.h"
#include "glass_fx.h"
#include "schedule.h"

// Trinkplan als kleine Timeline: Zeit in LECO, Glas-Nummer und Status;
// rechts die Seitenleiste, dunkel fuer vergangene und hell fuer kommende
// Slots, mit weisser Pfeilkerbe am gewaehlten Eintrag statt invertierter
// Zeile.

#define ROW_H  (PBL_DISPLAY_HEIGHT >= 200 ? 44 : 40)

static Window *s_window;
static MenuLayer *s_menu;
static Layer *s_sidebar;
static int s_past_rows;   // Slots, die schon vorbei sind

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return DT_GLASSES;
}

static int16_t prv_cell_height(MenuLayer *menu, MenuIndex *index, void *ctx) {
  return ROW_H;
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  const GRect b = layer_get_bounds(cell);
  const bool wide = b.size.w >= 130;
  const int16_t margin = PBL_IF_ROUND_ELSE(34, 9);
  time_t now = time(NULL);
  time_t slot = schedule_slot(schedule_midnight(now), index->row);
  char hhmm[8];
  schedule_format_time(slot, hhmm, sizeof(hhmm));
  char title[12];
  snprintf(title, sizeof(title), "Glas %d", index->row + 1);

  const int count = schedule_count();
  time_t next;
  const int next_idx = schedule_next(now, &next);
  const bool next_today = next < schedule_midnight(now) + 86400;
  const char *state;
  if (index->row < count) {
    state = "getrunken";
  } else if (next_today && index->row == next_idx) {
    state = "nächste Erinnerung";
  } else if (slot <= now) {
    state = "verpasst";
  } else {
    state = "offen";
  }

  graphics_context_set_text_color(ctx, DT_COLOR_TEXT);
  graphics_draw_text(ctx, hhmm, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS),
                     GRect(margin, 2, 64, 24), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  const int16_t tx = margin + (wide ? 62 : 60);
  graphics_draw_text(ctx, title,
                     fonts_get_system_font(wide ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD),
                     GRect(tx, wide ? 2 : 4, b.size.w - tx, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, state, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(margin, wide ? 22 : 20, b.size.w - margin, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  // Die Kerbe in der Seitenleiste folgt dem Scrollen
  if (s_sidebar) layer_mark_dirty(s_sidebar);
}

static void prv_sidebar_update(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const GPoint off = scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_menu));
  int16_t split = s_past_rows * ROW_H + off.y;
  if (split < 0) split = 0;
  if (split > b.size.h) split = b.size.h;
  graphics_context_set_fill_color(ctx, DT_COLOR_SIDEBAR);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, split), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, DT_COLOR_BAND);
  graphics_fill_rect(ctx, GRect(0, split, b.size.w, b.size.h - split), 0, GCornerNone);

  // Weisse Pfeilkerbe am gewaehlten Eintrag
  const MenuIndex sel = menu_layer_get_selected_index(s_menu);
  const int16_t cy = sel.row * ROW_H + ROW_H / 2 + off.y;
  GPoint pts[3] = { GPoint(0, cy - 10), GPoint(10, cy), GPoint(0, cy + 10) };
  const GPathInfo info = { .num_points = 3, .points = pts };
  GPath *notch = gpath_create(&info);
  if (notch) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    gpath_draw_filled(ctx, notch);
    gpath_destroy(notch);
  }
  glass_fx_draw_still(ctx, GPoint(b.size.w / 2 - PBL_IF_ROUND_ELSE(9, 0), PBL_IF_ROUND_ELSE(58, 20)), 22, 700);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(root);
  s_menu = menu_layer_create(GRect(0, 0, bounds.size.w - DT_SIDEBAR_W, bounds.size.h));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_cell_height = prv_cell_height,
    .draw_row = prv_draw_row,
  });
  menu_layer_set_normal_colors(s_menu, DT_COLOR_BG, DT_COLOR_TEXT);
  menu_layer_set_highlight_colors(s_menu, DT_COLOR_BG, DT_COLOR_TEXT);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  s_sidebar = layer_create(GRect(bounds.size.w - DT_SIDEBAR_W, 0, DT_SIDEBAR_W, bounds.size.h));
  layer_set_update_proc(s_sidebar, prv_sidebar_update);
  layer_add_child(root, s_sidebar);

  // Vergangene Slots zaehlen und auf die naechste Erinnerung springen
  time_t now = time(NULL);
  const time_t midnight = schedule_midnight(now);
  s_past_rows = 0;
  for (int i = 0; i < DT_GLASSES; i++) {
    if (schedule_slot(midnight, i) <= now) s_past_rows++;
  }
  time_t next;
  int idx = schedule_next(now, &next);
  if (next >= midnight + 86400) idx = DT_GLASSES - 1;
  menu_layer_set_selected_index(s_menu, MenuIndex(0, idx), MenuRowAlignCenter, false);
}

static void prv_unload(Window *window) {
  layer_destroy(s_sidebar);
  s_sidebar = NULL;
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void plan_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, DT_COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
