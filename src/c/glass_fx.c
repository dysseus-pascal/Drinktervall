#include "glass_fx.h"
#include "theme.h"

// Trink-Animation, Stil wie das Tagestrenner-Symbol der Timeline: schwarzer
// Rahmen, weisse Fuellung, Strich-Augen, geknickter Mund, alles mit derselben
// Strichstaerke. Das Gesicht schaut leicht zur Seite (asymmetrisch wie die
// Sonne). Ablauf in Promille der Gesamtdauer:
//   0..POP_END     Glas ploppt mit Ueberschwingen auf, laechelt
//   ..HOLD_END     kurz still, voll
//   ..DRINK_END    Pegel faellt gleichmaessig, Schluck-Gesicht
//   ..SMILE_END    leer, wieder Laecheln
//   ..SHRINK_END   Glas schrumpft ins Zentrum
//   ..1000         Strahlenkranz dort, wo das Glas war

#ifndef FX_MS
#define FX_MS 1750          // Gesamtdauer; Testbuilds koennen sie ueberschreiben
#endif
#define POP_END      86
#define HOLD_END    143
#define DRINK_END   571
#define SMILE_END   657
#define SHRINK_END  800
#define RAYS         12

#define SHAKE_PX     3      // Schuetteln beim Leeren: Versatz links/rechts
#define SHAKE_MS    40      // ... und Wechsel alle 40 ms

// Strichstaerke wie die Timeline-Sonne: 5 % der Glasbreite (ungerade), das
// Gesicht in derselben Staerke; weisser Saum darunter, lesbar auf Wasser.
static int16_t s_stroke = 3, s_halo = 5;
#define STROKE  s_stroke
#define HALO    s_halo

static Layer *s_layer;
static Animation *s_anim;
static int32_t s_p;         // Fortschritt 0..1000
static GPoint s_anchor;     // Glasmitte
static int16_t s_width;     // Glasbreite oben in Pixeln
static GlassFxDone s_done;

// Glas und Gesicht sind in Einheiten eines 72 Pixel breiten Glases
// beschrieben und werden ueber s_g.k auf die echte Breite skaliert.
#define BASE_W 72

// Glas-Trapez um den Ursprung: links oben, links unten, rechts unten, rechts oben
static GPoint s_glass_pts[4];
static const GPathInfo s_glass_info = { .num_points = 4, .points = s_glass_pts };
static GPoint s_water_pts[4];
static const GPathInfo s_water_info = { .num_points = 4, .points = s_water_pts };
static GPoint s_star_pts[8];
static const GPathInfo s_star_info = { .num_points = 8, .points = s_star_pts };

// smoothstep, Ein- und Ausgabe in Promille
static int32_t prv_smooth(int32_t f) {
  return f * f * (3000 - 2 * f) / 1000000;
}

// Aktuelle Transformation des Glases
static struct {
  int32_t gw;           // Breite oben in Pixeln (volles Glas)
  int32_t scale;        // Promille, um die Glasmitte (Aufploppen, Schrumpfen)
  int32_t k;            // Promille: Basis-Einheiten -> Pixel, inkl. scale
  GPoint c;
} s_g;

static GPoint prv_gp(int32_t x, int32_t y) {
  return GPoint(s_g.c.x + (int16_t)(x * s_g.k / 1000),
                s_g.c.y + (int16_t)(y * s_g.k / 1000));
}

static void prv_line(GContext *ctx, GPoint a, GPoint b) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HALO);
  graphics_draw_line(ctx, a, b);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, STROKE);
  graphics_draw_line(ctx, a, b);
}

// Offene Polylinie in einem Zug (erst Saum, dann Strich), damit am Knick
// keine Naht entsteht
static void prv_polyline(GContext *ctx, GPoint *points, uint32_t n) {
  const GPathInfo info = { .num_points = n, .points = points };
  GPath *path = gpath_create(&info);
  if (!path) return;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HALO);
  gpath_draw_outline_open(ctx, path);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, STROKE);
  gpath_draw_outline_open(ctx, path);
  gpath_destroy(path);
}

typedef enum { FaceSmile, FaceGulp } Face;

// Gesicht wie die Timeline-Sonne, leicht nach links versetzt (Seitenblick):
// linkes Auge bei -15, rechtes bei +8, Mundknick bei -4.
static void prv_face(GContext *ctx, Face face) {
  const int32_t fy = -6;
  const int32_t eyes[2] = { -15, 8 };
  for (int i = 0; i < 2; i++) {
    const int32_t x = eyes[i];
    if (face == FaceGulp) {
      // zusammengekniffen: spitzer Bogen wie der Mund, Spitze oben
      GPoint eye[3] = { prv_gp(x - 5, fy - 3), prv_gp(x, fy - 8), prv_gp(x + 5, fy - 3) };
      prv_polyline(ctx, eye, 3);
    } else {
      prv_line(ctx, prv_gp(x, fy - 9), prv_gp(x, fy - 2));                   // Strich
    }
  }
  if (face == FaceGulp) {
    const GPoint m = prv_gp(-4, fy + 9);
    const int16_t r = (int16_t)(4 * s_g.k / 1000);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, m, r);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, STROKE);
    graphics_draw_circle(ctx, m, r);
  } else {
    GPoint mouth[3] = { prv_gp(-15, fy + 7), prv_gp(-4, fy + 10), prv_gp(8, fy + 7) };
    prv_polyline(ctx, mouth, 3);
  }
}

static void prv_draw_glass(GContext *ctx, int32_t level, Face face) {
  // Basis-Einheiten: 72 breit oben, 54 am Boden, 80 hoch
  const int32_t hw_top = 36, hw_bot = 27, hh = 40;
  s_glass_pts[0] = prv_gp(-hw_top, -hh);
  s_glass_pts[1] = prv_gp(-hw_bot, hh);
  s_glass_pts[2] = prv_gp(hw_bot, hh);
  s_glass_pts[3] = prv_gp(hw_top, -hh);
  GPath *glass = gpath_create(&s_glass_info);
  if (!glass) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, glass);
  if (level > 0) {
    const int32_t wy = hh - 2 * hh * level / 1000;
    const int32_t whw = hw_bot + (hw_top - hw_bot) * level / 1000;
    s_water_pts[0] = prv_gp(-hw_bot, hh);
    s_water_pts[1] = prv_gp(hw_bot, hh);
    s_water_pts[2] = prv_gp(whw, wy);
    s_water_pts[3] = prv_gp(-whw, wy);
    GPath *water = gpath_create(&s_water_info);
    if (water) {
      graphics_context_set_fill_color(ctx, DT_COLOR_FX_WATER);
      gpath_draw_filled(ctx, water);
      gpath_destroy(water);
    }
  }
  // Rahmen rundum: weisser Saum, dann schwarz
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, HALO);
  gpath_draw_outline(ctx, glass);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, STROKE);
  gpath_draw_outline(ctx, glass);
  gpath_destroy(glass);
  prv_face(ctx, face);
}

// Strahlenkranz: zwoelf Striche, die nach aussen laufen, abwechselnd lang
// und kurz, dazu ein schrumpfender Funken in der Mitte. u = 0..1000
static void prv_draw_burst(GContext *ctx, int32_t u) {
  const int32_t g = u < 500 ? u * 2 : (1000 - u) * 2;               // 0..1000..0
  const int32_t ge = 1000 - (1000 - g) * (1000 - g) / 1000;         // ease-out
  const int32_t r1 = 6 + 34 * u / 1000;
  const int32_t len = s_g.gw * 42 / 100 * ge / 1000;
  for (int k = 0; k < RAYS; k++) {
    const int32_t a = k * TRIG_MAX_ANGLE / RAYS + u * (TRIG_MAX_ANGLE / 24) / 1000;
    const int32_t l = (k % 2) ? len * 55 / 100 : len;
    const int32_t cs = cos_lookup(a), sn = sin_lookup(a);
    const GPoint p1 = GPoint(s_g.c.x + r1 * cs / TRIG_MAX_RATIO, s_g.c.y + r1 * sn / TRIG_MAX_RATIO);
    const GPoint p2 = GPoint(s_g.c.x + (r1 + l) * cs / TRIG_MAX_RATIO, s_g.c.y + (r1 + l) * sn / TRIG_MAX_RATIO);
    prv_line(ctx, p1, p2);
  }
  const int32_t ro = 2 + (s_g.gw * 20 / 100) * (1000 - u) / 1000;
  const int32_t ri = 1 + (s_g.gw * 7 / 100) * (1000 - u) / 1000;
  for (int i = 0; i < 8; i++) {
    const int32_t r = (i % 2) ? ri : ro;
    const int32_t a = -TRIG_MAX_ANGLE / 4 + i * TRIG_MAX_ANGLE / 8;
    s_star_pts[i] = GPoint(s_g.c.x + r * cos_lookup(a) / TRIG_MAX_RATIO,
                           s_g.c.y + r * sin_lookup(a) / TRIG_MAX_RATIO);
  }
  GPath *star = gpath_create(&s_star_info);
  if (!star) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, star);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, STROKE > 3 ? 2 : 1);
  gpath_draw_outline(ctx, star);
  gpath_destroy(star);
}

static void prv_draw(Layer *layer, GContext *ctx) {
  s_g.c = s_anchor;
  s_g.gw = s_width;
  s_g.scale = 1000;
  s_stroke = (int16_t)((s_g.gw / 20) | 1);
  s_halo = s_stroke + 2;
  if (s_p >= HOLD_END && s_p < DRINK_END) {
    // Schuetteln nur waehrend der Pegel faellt
    const int32_t ms = (s_p - HOLD_END) * FX_MS / 1000;
    s_g.c.x += ((ms / SHAKE_MS) % 2) ? SHAKE_PX : -SHAKE_PX;
  }

  if (s_p < POP_END) {
    // Aufploppen mit Ueberschwingen: 0 -> 1150 -> 1000
    const int32_t t = s_p * 1000 / POP_END;
    s_g.scale = t < 600 ? 1150 * prv_smooth(t * 1000 / 600) / 1000
                        : 1150 - 150 * (t - 600) / 400;
  } else if (s_p >= SMILE_END && s_p < SHRINK_END) {
    const int32_t t = (s_p - SMILE_END) * 1000 / (SHRINK_END - SMILE_END);
    s_g.scale = 1000 - t * t / 1000;                                    // beschleunigt
  }
  s_g.k = s_g.scale * s_g.gw / BASE_W;

  if (s_p < HOLD_END) {
    prv_draw_glass(ctx, 1000, FaceSmile);
  } else if (s_p < DRINK_END) {
    // gleichmaessig leeren
    const int32_t t = (s_p - HOLD_END) * 1000 / (DRINK_END - HOLD_END);
    prv_draw_glass(ctx, 1000 - t, FaceGulp);
  } else if (s_p < SMILE_END) {
    prv_draw_glass(ctx, 0, FaceSmile);
  } else if (s_p < SHRINK_END) {
    if (s_g.scale > 60) prv_draw_glass(ctx, 0, FaceSmile);
  } else {
    prv_draw_burst(ctx, (s_p - SHRINK_END) * 1000 / (1000 - SHRINK_END));
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

void glass_fx_play(GPoint anchor, int16_t width, GlassFxDone done) {
  if (!s_layer || s_anim) return;
  s_anchor = anchor;
  s_width = width;
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
