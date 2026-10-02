#include "drinks_window.h"
#include "drinktervall.h"
#include "theme.h"
#include "coffee.h"
#include "glass_fx.h"
#include "drink_window.h"
#include "schedule.h"
#include "phone.h"
#include "strings.h"

// Zwei Fenster: die Liste und, nach einer Sorte, die Zusaetze. Beide als
// MenuLayer wie der Trinkplan, mit dem Gefaess der Animation vor jeder Zeile.

#define ROW_H (PBL_DISPLAY_HEIGHT >= 200 ? 44 : 40)
#define ICON_W 22

static Window *s_list_window, *s_extra_window;
static MenuLayer *s_list_menu, *s_extra_menu;
static CoffeeSlot s_pick;    // die gewaehlte Sorte mit ihren Zusaetzen

static const StringId s_kind_names[CoffeeKindCount] = {
  STR_ESPRESSO, STR_COFFEE, STR_TEA, STR_ENERGY_DRINK,
};

// Eine Zeile: Gefaess links, Text daneben. Auf dem runden Schirm weiter
// eingerueckt, damit nichts am Rand verschwindet.
// `mark`: -1 ohne Kaestchen, 0 leer, 1 angehakt. Das Kaestchen ist gezeichnet:
// die Systemschrift der Uhr hat kein Haekchen-Zeichen.
static void prv_row(GContext *ctx, const Layer *cell, Vessel vessel, const char *text, int mark) {
  const GRect b = layer_get_bounds(cell);
  const int16_t margin = PBL_IF_ROUND_ELSE(30, 8);
  glass_fx_draw_vessel_still(ctx, GPoint(margin + ICON_W / 2, b.size.h / 2), ICON_W, vessel);
  const int16_t x = margin + ICON_W + 14;
  const int16_t mark_w = mark >= 0 ? 28 : 0;
  // Auf Schwarz-Weiss ist die gewaehlte Zeile schwarz (ein Grauraster machte
  // die Schrift unlesbar) - die Schrift dort weiss.
  const bool hell = PBL_IF_COLOR_ELSE(false, menu_cell_layer_is_highlighted(cell));
  graphics_context_set_text_color(ctx, hell ? GColorWhite : DT_COLOR_TEXT);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(x, b.size.h / 2 - 13, b.size.w - x - margin - mark_w, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  if (mark >= 0) {
    const GRect box = GRect(b.size.w - margin - 16, b.size.h / 2 - 8, 16, 16);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, box, 2, GCornersAll);
    graphics_draw_round_rect(ctx, box, 2);
    if (mark) {
      graphics_context_set_stroke_width(ctx, 3);
      graphics_draw_line(ctx, GPoint(box.origin.x + 3, box.origin.y + 8), GPoint(box.origin.x + 6, box.origin.y + 12));
      graphics_draw_line(ctx, GPoint(box.origin.x + 6, box.origin.y + 12), GPoint(box.origin.x + 13, box.origin.y + 3));
    }
  }
}

// Eintragen: wie der Haken in der Kaffee-Erinnerung. Danach zurueck zum
// Hauptscreen, mit Animation, wenn sie eingeschaltet ist.
static void prv_finish(const CoffeeSlot *slot);

static void prv_log(const CoffeeSlot *slot) {
  phone_note_coffee(slot->kind, slot->flags);
  prv_finish(slot);
}

// Rueckmeldung und zurueck - fuer Kaffee und eigenes Getraenk gleich.
static void prv_finish(const CoffeeSlot *slot) {
  drinktervall_buzz_short();
  phone_send_next();
  const Vessel v = coffee_vessel(slot);
  if (s_extra_window) window_stack_remove(s_extra_window, false);
  if (s_list_window) window_stack_remove(s_list_window, false);
  if (schedule_animation()) drink_window_push_vessel(false, v);
}

// --- Zusaetze: koffeinfrei, Milch, Zucker, Eintragen ---

// Die Haken der gewaehlten Sorte, in der Reihenfolge der Zeilen; danach kommt
// "Eintragen". Was eine Sorte nicht kennt (der Energy-Drink weder Milch noch
// koffeinfrei), fehlt ganz statt ausgegraut dazustehen.
static int prv_extra_bits(uint8_t *bits) {
  int n = 0;
  if (coffee_decaf_possible(s_pick.kind)) bits[n++] = COFFEE_DECAF;
  if (coffee_milk_possible(s_pick.kind)) bits[n++] = COFFEE_MILK;
  bits[n++] = COFFEE_SUGAR;
  return n;
}

static int prv_extra_rows(void) {
  uint8_t bits[3];
  return prv_extra_bits(bits) + 1;
}

static uint16_t prv_extra_num(MenuLayer *m, uint16_t section, void *ctx) {
  return prv_extra_rows();
}

static int16_t prv_height(MenuLayer *m, MenuIndex *i, void *ctx) {
  return ROW_H;
}

static StringId prv_bit_text(uint8_t bit) {
  switch (bit) {
    case COFFEE_DECAF: return STR_DECAF_ROW;
    case COFFEE_MILK: return STR_MILK_ROW;
    default: return STR_SUGAR_ROW;
  }
}

static void prv_extra_draw(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  uint8_t bits[3];
  const int n = prv_extra_bits(bits);
  const Vessel v = coffee_vessel(&s_pick);
  if (i->row >= n) {
    prv_row(ctx, cell, v, S(STR_LOG), -1);
    return;
  }
  prv_row(ctx, cell, v, S(prv_bit_text(bits[i->row])), (s_pick.flags & bits[i->row]) ? 1 : 0);
}

static void prv_extra_select(MenuLayer *m, MenuIndex *i, void *ctx) {
  uint8_t bits[3];
  const int n = prv_extra_bits(bits);
  if (i->row >= n) {
    prv_log(&s_pick);
    return;
  }
  s_pick.flags ^= bits[i->row];
  menu_layer_reload_data(m);
}

static void prv_extra_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_extra_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_extra_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_extra_num, .get_cell_height = prv_height,
    .draw_row = prv_extra_draw, .select_click = prv_extra_select,
  });
  menu_layer_set_highlight_colors(s_extra_menu, PBL_IF_COLOR_ELSE(DT_COLOR_BAND, GColorBlack), PBL_IF_COLOR_ELSE(DT_COLOR_TEXT, GColorWhite));
  menu_layer_set_click_config_onto_window(s_extra_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_extra_menu));
  // Gleich auf "Eintragen": ohne Zusaetze genuegt ein Druck.
  menu_layer_set_selected_index(s_extra_menu, MenuIndex(0, prv_extra_rows() - 1), MenuRowAlignCenter, false);
}

static void prv_extra_unload(Window *w) {
  menu_layer_destroy(s_extra_menu);
  window_destroy(s_extra_window);
  s_extra_window = NULL;
  s_extra_menu = NULL;
}

static void prv_extra_push(uint8_t kind) {
  s_pick = (CoffeeSlot){ 0, kind, 0 };
  s_extra_window = window_create();
  window_set_background_color(s_extra_window, DT_COLOR_BG);
  window_set_window_handlers(s_extra_window, (WindowHandlers) {
    .load = prv_extra_load, .unload = prv_extra_unload,
  });
  window_stack_push(s_extra_window, true);
}

// --- Die Liste: eigene Getraenke, dann die Sorten ---
//
// Ohne eigene Getraenke bleiben nur die Sorten.

typedef enum { SecCustom, SecKinds } Sec;

static int prv_sections(Sec *out) {
  int n = 0;
  // Die Kaffees aus dem Plan stehen hier bewusst nicht: sie doppelten nur
  // die Sorten darunter (auf der Uhr stand dreimal "Espresso" uebereinander).
  if (custom_count() > 0) out[n++] = SecCustom;
  out[n++] = SecKinds;
  return n;
}

static Sec prv_sec(uint16_t section) {
  Sec secs[2];
  const int n = prv_sections(secs);
  return secs[section < n ? section : n - 1];
}

static uint16_t prv_list_sections(MenuLayer *m, void *ctx) {
  Sec secs[2];
  return (uint16_t)prv_sections(secs);
}

static uint16_t prv_list_num(MenuLayer *m, uint16_t section, void *ctx) {
  switch (prv_sec(section)) {
    case SecCustom: return custom_count();
    default: return CoffeeKindCount;
  }
}

static int16_t prv_header_h(MenuLayer *m, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  static const StringId titel[2] = { STR_MY_DRINKS, STR_DRINKS };
  const GRect b = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, DT_COLOR_TEXT);
  graphics_draw_text(ctx, S(titel[prv_sec(section)]), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(PBL_IF_ROUND_ELSE(0, 8), -2, b.size.w - PBL_IF_ROUND_ELSE(0, 16), 16),
                     GTextOverflowModeTrailingEllipsis, PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft), NULL);
}

static void prv_list_draw(GContext *ctx, const Layer *cell, MenuIndex *i, void *data) {
  switch (prv_sec(i->section)) {
    case SecCustom: {
      const CustomDrink *d = custom_drink(i->row);
      if (d) prv_row(ctx, cell, VesselCustom, d->name, -1);
      return;
    }
    default: {
      const CoffeeSlot plain = { 0, (uint8_t)i->row, 0 };
      prv_row(ctx, cell, coffee_vessel(&plain), S(s_kind_names[i->row]), -1);
    }
  }
}

static void prv_list_select(MenuLayer *m, MenuIndex *i, void *ctx) {
  switch (prv_sec(i->section)) {
    case SecCustom: {
      // Ein eigenes Getraenk hat keine Zusaetze: ein Druck, eingetragen.
      const CustomDrink *d = custom_drink(i->row);
      if (!d) return;
      phone_note_custom(d);
      const CoffeeSlot als = { 0, COFFEE_KIND_CUSTOM, 0 };
      prv_finish(&als);
      return;
    }
    default:
      prv_extra_push((uint8_t)i->row);
  }
}

static void prv_list_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_list_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_list_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_list_sections, .get_num_rows = prv_list_num,
    .get_header_height = prv_header_h, .draw_header = prv_header,
    .get_cell_height = prv_height, .draw_row = prv_list_draw, .select_click = prv_list_select,
  });
  menu_layer_set_highlight_colors(s_list_menu, PBL_IF_COLOR_ELSE(DT_COLOR_BAND, GColorBlack), PBL_IF_COLOR_ELSE(DT_COLOR_TEXT, GColorWhite));
  menu_layer_set_click_config_onto_window(s_list_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_list_menu));
}

static void prv_list_unload(Window *w) {
  menu_layer_destroy(s_list_menu);
  window_destroy(s_list_window);
  s_list_window = NULL;
  s_list_menu = NULL;
}

void drinks_window_push(void) {
  if (s_list_window) return;
  s_list_window = window_create();
  window_set_background_color(s_list_window, DT_COLOR_BG);
  window_set_window_handlers(s_list_window, (WindowHandlers) {
    .load = prv_list_load, .unload = prv_list_unload,
  });
  window_stack_push(s_list_window, true);
}
