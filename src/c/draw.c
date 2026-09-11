#include "draw.h"

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
