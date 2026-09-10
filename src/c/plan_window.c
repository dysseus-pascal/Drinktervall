#include "plan_window.h"
#include "config.h"
#include "theme.h"
#include "schedule.h"

static Window *s_window;
static MenuLayer *s_menu;

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return AT_GLASSES;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, "Trinkplan heute");
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  time_t now = time(NULL);
  time_t slot = schedule_slot(schedule_midnight(now), index->row);
  char hhmm[8];
  schedule_format_time(slot, hhmm, sizeof(hhmm));
  char title[24];
  snprintf(title, sizeof(title), "%s   Glas %d", hhmm, index->row + 1);

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
  menu_cell_basic_draw(ctx, cell, title, state, NULL);
}

static void prv_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
  });
  menu_layer_set_normal_colors(s_menu, AT_COLOR_BG, AT_COLOR_TEXT);
  menu_layer_set_highlight_colors(s_menu, AT_COLOR_PRIMARY, AT_COLOR_ON_PRIMARY);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));

  // Auf die naechste Erinnerung springen, sofern sie heute ist
  time_t now = time(NULL);
  time_t next;
  int idx = schedule_next(now, &next);
  if (next >= schedule_midnight(now) + 86400) idx = AT_GLASSES - 1;
  menu_layer_set_selected_index(s_menu, MenuIndex(0, idx), MenuRowAlignCenter, false);
}

static void prv_unload(Window *window) {
  menu_layer_destroy(s_menu);
  window_destroy(s_window);
  s_window = NULL;
}

void plan_window_push(void) {
  if (s_window) return;
  s_window = window_create();
  window_set_background_color(s_window, AT_COLOR_BG);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_load, .unload = prv_unload,
  });
  window_stack_push(s_window, true);
}
