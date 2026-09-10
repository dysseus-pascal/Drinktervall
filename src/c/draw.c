#include "draw.h"

void draw_glass(GContext *ctx, GRect box, int level_permille, GColor outline, GColor water) {
  if (level_permille < 0) level_permille = 0;
  if (level_permille > 1000) level_permille = 1000;
  const int16_t top = box.origin.y;
  const int16_t bottom = box.origin.y + box.size.h - 1;
  const int16_t h = box.size.h - 1;
  // Boden ist 3/4 so breit wie der Rand
  const int16_t inset = box.size.w / 8;
  const int16_t left_top = box.origin.x;
  const int16_t right_top = box.origin.x + box.size.w - 1;
  const int16_t left_bot = left_top + inset;
  const int16_t right_bot = right_top - inset;

  if (level_permille > 0) {
    const int16_t water_h = (int16_t)((int32_t)h * level_permille / 1000);
    const int16_t y = bottom - water_h;
    // x-Kante auf Hoehe y linear zwischen Boden und Rand
    const int16_t dx = (int16_t)((int32_t)inset * water_h / h);
    GPoint pts[4] = {
      { left_bot, bottom }, { right_bot, bottom },
      { (int16_t)(right_bot + dx), y }, { (int16_t)(left_bot - dx), y },
    };
    GPathInfo info = { .num_points = 4, .points = pts };
    GPath *path = gpath_create(&info);
    graphics_context_set_fill_color(ctx, water);
    gpath_draw_filled(ctx, path);
    gpath_destroy(path);
  }

  graphics_context_set_stroke_color(ctx, outline);
  graphics_context_set_stroke_width(ctx, 3);
  graphics_draw_line(ctx, GPoint(left_top, top), GPoint(left_bot, bottom));
  graphics_draw_line(ctx, GPoint(left_bot, bottom), GPoint(right_bot, bottom));
  graphics_draw_line(ctx, GPoint(right_bot, bottom), GPoint(right_top, top));
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(left_top, top), GPoint(right_top, top));
}

static void prv_hint(GContext *ctx, GRect bounds, const char *text, int16_t center_y,
                     GColor bg, GColor fg) {
  if (!text) return;
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  const int16_t h = 18;
  GSize size = graphics_text_layout_get_content_size(text, font, GRect(0, 0, 200, h),
                                                     GTextOverflowModeTrailingEllipsis,
                                                     GTextAlignmentLeft);
  const int16_t w = size.w + 10;
  // Auf runden Displays liegen die Tasten oben/unten weiter innen.
  const int16_t edge = PBL_IF_ROUND_ELSE(
      bounds.origin.x + bounds.size.w - (center_y == bounds.origin.y + bounds.size.h / 2 ? 10 : 24),
      bounds.origin.x + bounds.size.w - 2);
  GRect box = GRect(edge - w, center_y - h / 2, w, h);
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, text, font, GRect(box.origin.x + 5, box.origin.y - 1, size.w, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

void draw_button_hints(GContext *ctx, GRect bounds, const char *up, const char *select,
                       const char *down, GColor bg, GColor fg) {
  const int16_t y0 = bounds.origin.y;
  const int16_t hgt = bounds.size.h;
  prv_hint(ctx, bounds, up, y0 + hgt / 4, bg, fg);
  prv_hint(ctx, bounds, select, y0 + hgt / 2, bg, fg);
  prv_hint(ctx, bounds, down, y0 + hgt * 3 / 4, bg, fg);
}
