#include "glass_fx.h"
#include "theme.h"

#ifndef FX_MS
#define FX_MS 1300          // Gesamtdauer; Testbuilds koennen sie ueberschreiben
#endif
#define DRINK_END 650       // Promille der Laufzeit: bis hier trinken, danach verpuffen
#define GULPS 3             // Schlucke
#define WOBBLE_DEG 6        // Wackeln pro Schluck, abwechselnd links/rechts
#define PUFFS 8             // Woelkchen beim Verpuffen

static Layer *s_layer;
static Animation *s_anim;
static int32_t s_p;         // Fortschritt 0..1000
static GPoint s_anchor;     // Glasmitte
static GlassFxDone s_done;

// Glas-Trapez um den Ursprung: links oben, links unten, rechts unten, rechts
// oben - so zeichnet gpath_draw_outline_open Seiten und Boden, aber keinen
// Deckel. Das Wasser ist das Trapez bis zur Fuellhoehe.
static GPoint s_glass_pts[4];
static const GPathInfo s_glass_info = { .num_points = 4, .points = s_glass_pts };
static GPoint s_water_pts[4];
static const GPathInfo s_water_info = { .num_points = 4, .points = s_water_pts };

static int16_t prv_mul(int32_t v, int32_t permille) {
  return (int16_t)(v * permille / 1000);
}

// smoothstep, Ein- und Ausgabe in Promille
static int32_t prv_smooth(int32_t f) {
  return f * f * (3000 - 2 * f) / 1000000;
}

static void prv_draw(Layer *layer, GContext *ctx) {
  const GRect b = layer_get_bounds(layer);
  const GPoint c = s_anchor;
  const int32_t gw = b.size.w * 36 / 100;
  const int32_t gh = gw * 12 / 10;
  int32_t level = 0, scale = 1000, rot = 0, puff = -1;

  if (s_p < DRINK_END) {
    const int32_t t = s_p * 1000 / DRINK_END;         // Trinkphase 0..1000
    const int32_t g = t * GULPS / 1000;                // Schluck-Nummer
    const int32_t f = t * GULPS - g * 1000;            // Anteil im Schluck
    level = 1000 - (g * 1000 + prv_smooth(f)) / GULPS;
    const int32_t s = sin_lookup(f * (TRIG_MAX_ANGLE / 2) / 1000);   // sin(f * pi)
    rot = ((g % 2) ? -1 : 1) * (WOBBLE_DEG * TRIG_MAX_ANGLE / 360) * s / TRIG_MAX_RATIO;
  } else {
    const int32_t u = (s_p - DRINK_END) * 1000 / (1000 - DRINK_END);
    scale = 1000 - u * u / 1000;                       // schrumpft beschleunigt
    puff = u;
  }
  if (level < 0) level = 0;
  if (scale < 0) scale = 0;

  const int32_t hw_top = gw / 2, hw_bot = gw * 3 / 8, hh = gh / 2;
  s_glass_pts[0] = GPoint(prv_mul(-hw_top, scale), prv_mul(-hh, scale));
  s_glass_pts[1] = GPoint(prv_mul(-hw_bot, scale), prv_mul(hh, scale));
  s_glass_pts[2] = GPoint(prv_mul(hw_bot, scale), prv_mul(hh, scale));
  s_glass_pts[3] = GPoint(prv_mul(hw_top, scale), prv_mul(-hh, scale));
  const int32_t wy = hh - gh * level / 1000;
  const int32_t whw = hw_bot + (hw_top - hw_bot) * level / 1000;
  s_water_pts[0] = GPoint(prv_mul(-hw_bot, scale), prv_mul(hh, scale));
  s_water_pts[1] = GPoint(prv_mul(hw_bot, scale), prv_mul(hh, scale));
  s_water_pts[2] = GPoint(prv_mul(whw, scale), prv_mul(wy, scale));
  s_water_pts[3] = GPoint(prv_mul(-whw, scale), prv_mul(wy, scale));

  if (scale > 0) {
    GPath *glass = gpath_create(&s_glass_info);
    GPath *water = gpath_create(&s_water_info);
    if (glass && water) {
      gpath_rotate_to(glass, rot);
      gpath_move_to(glass, c);
      gpath_rotate_to(water, rot);
      gpath_move_to(water, c);
      graphics_context_set_fill_color(ctx, DT_COLOR_LEVEL_LIGHT);
      gpath_draw_filled(ctx, glass);
      if (level > 0) {
        graphics_context_set_fill_color(ctx, DT_COLOR_LEVEL_DARK);
        gpath_draw_filled(ctx, water);
      }
      // Doppelter Rand (weiss breit, dunkel schmal), damit das Glas auf hellem
      // wie dunklem Grund sichtbar bleibt
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 5);
      gpath_draw_outline_open(ctx, glass);
      graphics_context_set_stroke_color(ctx, DT_COLOR_ON_LIGHT);
      graphics_context_set_stroke_width(ctx, 3);
      gpath_draw_outline_open(ctx, glass);
    }
    gpath_destroy(glass);
    gpath_destroy(water);
  }

  if (puff >= 0) {
    const int32_t ease = 1000 - (1000 - puff) * (1000 - puff) / 1000;   // schnell raus, dann langsam
    const int32_t dist = gw * 9 / 10 * ease / 1000;
    const int32_t r = 2 + 6 * (1000 - puff) / 1000;
    graphics_context_set_stroke_width(ctx, 1);
    for (int i = 0; i < PUFFS; i++) {
      const int32_t a = i * TRIG_MAX_ANGLE / PUFFS + puff * 20;   // Wolke dreht leicht
      const GPoint p = GPoint(c.x + dist * cos_lookup(a) / TRIG_MAX_RATIO,
                              c.y + dist * sin_lookup(a) / TRIG_MAX_RATIO);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, p, (uint16_t)r);
      graphics_context_set_stroke_color(ctx, DT_COLOR_ON_LIGHT);
      graphics_draw_circle(ctx, p, (uint16_t)r);
    }
  }
}

static void prv_update(Animation *animation, const AnimationProgress progress) {
  s_p = (int32_t)progress * 1000 / ANIMATION_NORMALIZED_MAX;
  layer_mark_dirty(s_layer);
}

static const AnimationImplementation s_impl = { .update = prv_update };

static void prv_stopped(Animation *animation, bool finished, void *context) {
  // Das SDK gibt beendete Animationen nicht selbst frei
  animation_destroy(animation);
  s_anim = NULL;
  layer_set_hidden(s_layer, true);
  GlassFxDone done = s_done;
  s_done = NULL;
  if (finished && done) done();
}

void glass_fx_init(Layer *parent) {
  s_layer = layer_create(layer_get_bounds(parent));
  layer_set_update_proc(s_layer, prv_draw);
  layer_set_hidden(s_layer, true);
  layer_add_child(parent, s_layer);
}

void glass_fx_deinit(void) {
  s_done = NULL;
  if (s_anim) {
    animation_unschedule(s_anim);
    s_anim = NULL;
  }
  layer_destroy(s_layer);
  s_layer = NULL;
}

bool glass_fx_is_playing(void) {
  return s_anim != NULL;
}

void glass_fx_play(GPoint anchor, GlassFxDone done) {
  if (!s_layer || s_anim) return;
  s_anchor = anchor;
  s_done = done;
  s_p = 0;
  layer_set_hidden(s_layer, false);
  layer_mark_dirty(s_layer);
  s_anim = animation_create();
  if (!s_anim) {
    layer_set_hidden(s_layer, true);
    s_done = NULL;
    if (done) done();
    return;
  }
  animation_set_implementation(s_anim, &s_impl);
  animation_set_duration(s_anim, FX_MS);
  animation_set_curve(s_anim, AnimationCurveLinear);
  animation_set_handlers(s_anim, (AnimationHandlers) { .stopped = prv_stopped }, NULL);
  animation_schedule(s_anim);
}
