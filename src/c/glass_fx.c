#include "glass_fx.h"
#include "theme.h"

// Trink-Animation, Stil wie das Tagestrenner-Symbol der Timeline: schwarzer
// Rahmen, weisse Fuellung, Strich-Augen, geknickter Mund, alles mit derselben
// Strichstaerke. Das Gesicht schaut leicht zur Seite (asymmetrisch wie die
// Sonne). Ablauf in Promille der Gesamtdauer:
//   0..POP_END     Glas ploppt mit Ueberschwingen auf, laechelt
//   ..HOLD_END     kurz still, voll
//   ..DRINK_END    Pegel faellt gleichmaessig, Schluck-Gesicht, Glas wackelt
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

#define SHAKE_PX     2      // Schuetteln beim Leeren: Versatz links/rechts
#define SHAKE_MS    50      // ... und Wechsel alle 50 ms

// Glas und Gesicht sind in Einheiten eines 72 Pixel breiten Glases
// beschrieben und werden ueber s_g.k auf die echte Breite skaliert.
#define BASE_W 72

static Layer *s_layer;
static Animation *s_anim;
static int32_t s_p;         // Fortschritt 0..1000
static GPoint s_anchor;     // Glasmitte
static int16_t s_width;     // Glasbreite oben in Pixeln
static GlassFxDone s_done;
static Vessel s_vessel;

// Farben der Getraenke. Auf Schwarz-Weiss bleibt nur Grau: dort traegt die
// Form, nicht die Farbe.
#define FX_ESPRESSO   PBL_IF_COLOR_ELSE(GColorBulgarianRose, GColorDarkGray)
#define FX_COFFEE     PBL_IF_COLOR_ELSE(GColorWindsorTan, GColorDarkGray)
#define FX_MILKCOFFEE PBL_IF_COLOR_ELSE(GColorRajah, GColorLightGray)
#define FX_MILK       PBL_IF_COLOR_ELSE(GColorPastelYellow, GColorLightGray)
#define FX_CAN_BLUE   PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorDarkGray)
#define FX_CAN_SILVER PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite)
#define FX_SUN        PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite)
#define FX_CUSTOM     PBL_IF_COLOR_ELSE(GColorInchworm, GColorLightGray)
#define FX_RED        PBL_IF_COLOR_ELSE(GColorRed, GColorBlack)

// Aktuelle Transformation und Strichstaerke; prv_set_metrics setzt beides.
// Die Striche sind 5 % der Glasbreite breit (ungerade) wie bei der
// Timeline-Sonne, der weisse Saum darunter 2 Pixel mehr.
static struct {
  int32_t gw;               // Breite oben in Pixeln (volles Glas)
  int32_t k;                // Promille: Basis-Einheiten -> Pixel, inkl. Skalierung
  int16_t stroke;
  GPoint c;
} s_g;

static void prv_set_metrics(GPoint c, int32_t gw, int32_t scale) {
  s_g.c = c;
  s_g.gw = gw;
  s_g.k = scale * gw / BASE_W;
  s_g.stroke = (int16_t)((gw / 20) | 1);
}

static GPoint prv_gp(int32_t x, int32_t y) {
  return GPoint(s_g.c.x + (int16_t)(x * s_g.k / 1000),
                s_g.c.y + (int16_t)(y * s_g.k / 1000));
}

// Jeder Strich wird zweimal gezogen: erst der breite weisse Saum, dann
// schwarz darueber. So bleibt er auch auf dem Wasser lesbar.
static void prv_pen(GContext *ctx, bool halo) {
  graphics_context_set_stroke_color(ctx, halo ? GColorWhite : GColorBlack);
  graphics_context_set_stroke_width(ctx, halo ? s_g.stroke + 2 : s_g.stroke);
}

static void prv_line(GContext *ctx, GPoint a, GPoint b) {
  prv_pen(ctx, true);
  graphics_draw_line(ctx, a, b);
  prv_pen(ctx, false);
  graphics_draw_line(ctx, a, b);
}

// Offene Polylinie in einem Zug, damit am Knick keine Naht entsteht
static void prv_polyline(GContext *ctx, GPoint *points, uint32_t n) {
  const GPathInfo info = { .num_points = n, .points = points };
  GPath *path = gpath_create(&info);
  if (!path) return;
  prv_pen(ctx, true);
  gpath_draw_outline_open(ctx, path);
  prv_pen(ctx, false);
  gpath_draw_outline_open(ctx, path);
  gpath_destroy(path);
}

typedef enum { FaceSmile, FaceGulp } Face;

// Gesicht wie die Timeline-Sonne, leicht nach links versetzt (Seitenblick):
// linkes Auge bei -15, rechtes bei +8, Mundknick bei -4.
// `dx`/`dy` verschieben das Gesicht: in der flachen Espressotasse sitzt es
// tiefer, im schmalen Latte-Glas etwas weiter rechts.
static void prv_face_at(GContext *ctx, Face face, int32_t dx, int32_t dy) {
  const int32_t fy = -6 + dy;
  const int32_t eyes[2] = { -15 + dx, 8 + dx };
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
    // offener Mund: gefuellter Kreis mit Rand, kein Saum noetig
    const GPoint m = prv_gp(-4 + dx, fy + 9);
    const int16_t r = (int16_t)(4 * s_g.k / 1000);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, m, r);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, s_g.stroke);
    graphics_draw_circle(ctx, m, r);
  } else {
    GPoint mouth[3] = { prv_gp(-15 + dx, fy + 7), prv_gp(-4 + dx, fy + 10), prv_gp(8 + dx, fy + 7) };
    prv_polyline(ctx, mouth, 3);
  }
}

static void prv_face(GContext *ctx, Face face) {
  prv_face_at(ctx, face, 0, 0);
}

// --- Bausteine fuer die Gefaesse ---

// Ein gefuelltes Vieleck in Basis-Einheiten.
static void prv_fill(GContext *ctx, const int32_t (*pts)[2], int n, GColor color) {
  GPoint p[10];
  for (int i = 0; i < n && i < 10; i++) p[i] = prv_gp(pts[i][0], pts[i][1]);
  const GPathInfo info = { .num_points = (uint32_t)n, .points = p };
  GPath *path = gpath_create(&info);
  if (!path) return;
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

// Der Rahmen eines geschlossenen Vielecks, mit Saum wie beim Glas.
static void prv_outline(GContext *ctx, const int32_t (*pts)[2], int n) {
  GPoint p[10];
  for (int i = 0; i < n && i < 10; i++) p[i] = prv_gp(pts[i][0], pts[i][1]);
  const GPathInfo info = { .num_points = (uint32_t)n, .points = p };
  GPath *path = gpath_create(&info);
  if (!path) return;
  prv_pen(ctx, true);
  gpath_draw_outline(ctx, path);
  prv_pen(ctx, false);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static void prv_ring(GContext *ctx, int32_t x, int32_t y, int32_t r) {
  const GPoint c = prv_gp(x, y);
  const int16_t rr = (int16_t)(r * s_g.k / 1000);
  prv_pen(ctx, true);
  graphics_draw_circle(ctx, c, rr);
  prv_pen(ctx, false);
  graphics_draw_circle(ctx, c, rr);
}

// Halbe Breite eines Trapezes (oben `top` bei y0, unten `bot` bei y1) auf Hoehe y.
static int32_t prv_hw(int32_t top, int32_t bot, int32_t y0, int32_t y1, int32_t y) {
  return top + (bot - top) * (y - y0) / (y1 - y0);
}

// Eine Fuellung zwischen den Hoehen ya (unten) und yb (oben) in einem Trapez.
static void prv_band(GContext *ctx, int32_t top, int32_t bot, int32_t y0, int32_t y1,
                     int32_t ya, int32_t yb, GColor color) {
  if (yb >= ya) return;
  const int32_t a = prv_hw(top, bot, y0, y1, ya), b = prv_hw(top, bot, y0, y1, yb);
  const int32_t pts[4][2] = { { -a, ya }, { a, ya }, { b, yb }, { -b, yb } };
  prv_fill(ctx, pts, 4, color);
}

// Dampf ueber dem vollen Heissgetraenk: zwei Wellen.
static void prv_steam(GContext *ctx, int32_t y) {
  for (int i = 0; i < 2; i++) {
    const int32_t x = i ? 8 : -10;
    GPoint w[4] = { prv_gp(x, y), prv_gp(x + 4, y - 6), prv_gp(x - 2, y - 12), prv_gp(x + 2, y - 18) };
    prv_polyline(ctx, w, 4);
  }
}

// Espresso: kleine Tasse auf der Untertasse, das Gesicht tiefer.
static void prv_draw_espresso(GContext *ctx, int32_t level, Face face, bool steam) {
  const int32_t top = 24, bot = 17, y0 = -8, y1 = 24;
  prv_ring(ctx, 28, 6, 8);                                     // Henkel
  const int32_t cup[4][2] = { { -top, y0 }, { -bot, y1 }, { bot, y1 }, { top, y0 } };
  prv_fill(ctx, cup, 4, GColorWhite);
  const int32_t full = y0 + 4;
  prv_band(ctx, top, bot, y0, y1, y1, y1 - (y1 - full) * level / 1000, FX_ESPRESSO);
  prv_outline(ctx, cup, 4);
  GPoint saucer[4] = { prv_gp(-40, 26), prv_gp(-33, 32), prv_gp(33, 32), prv_gp(40, 26) };
  prv_polyline(ctx, saucer, 4);
  if (steam) prv_steam(ctx, y0 - 6);
  prv_face_at(ctx, face, 0, 12);
}

// Kaffee: Becher mit grossem Henkel.
static void prv_draw_mug(GContext *ctx, int32_t level, Face face, GColor drink, bool steam) {
  const int32_t top = 28, bot = 26, y0 = -30, y1 = 38;
  prv_ring(ctx, 34, 2, 13);
  const int32_t mug[4][2] = { { -top, y0 }, { -bot, y1 }, { bot, y1 }, { top, y0 } };
  prv_fill(ctx, mug, 4, GColorWhite);
  const int32_t full = y0 + 6;
  prv_band(ctx, top, bot, y0, y1, y1, y1 - (y1 - full) * level / 1000, drink);
  prv_outline(ctx, mug, 4);
  if (steam) prv_steam(ctx, y0 - 6);
  prv_face(ctx, face);
}

// Latte macchiato: hohes Glas mit drei Schichten - Milch unten, Kaffee in der
// Mitte, Schaum oben - und einem Trinkhalm. Getrunken wird von oben.
static void prv_draw_latte(GContext *ctx, int32_t level, Face face) {
  const int32_t top = 21, bot = 17, y0 = -44, y1 = 44;
  const int32_t glass[4][2] = { { -top, y0 }, { -bot, y1 }, { bot, y1 }, { top, y0 } };
  prv_fill(ctx, glass, 4, GColorWhite);
  const int32_t full = y0 + 4, h = y1 - full;
  const int32_t pegel = y1 - h * level / 1000;
  const int32_t milch = y1 - h * 45 / 100, kaffee = y1 - h * 70 / 100;
  prv_band(ctx, top, bot, y0, y1, y1, pegel > milch ? pegel : milch, FX_MILK);
  if (pegel < milch) prv_band(ctx, top, bot, y0, y1, milch, pegel > kaffee ? pegel : kaffee, FX_COFFEE);
  // Der Schaum ist weiss wie das Glas - eine Linie trennt ihn vom Kaffee.
  if (pegel < kaffee) {
    const int32_t w = prv_hw(top, bot, y0, y1, kaffee);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, prv_gp(-w, kaffee), prv_gp(w, kaffee));
  }
  prv_line(ctx, prv_gp(13, -24), prv_gp(26, -62));           // Trinkhalm, rechts am Gesicht vorbei
  prv_outline(ctx, glass, 4);
  prv_face_at(ctx, face, 3, 6);
}

// Energy-Drink: eine Dose im Schachbrett aus Blau und Silber mit gelber Sonne
// und zwei roten Zeichen - ein Hinweis, kein Logo. In eine Dose sieht man
// nicht hinein: statt eines Pegels wird sie beim Trinken zerdrueckt.
static void prv_draw_can(GContext *ctx, int32_t level, Face face) {
  const int32_t hw = 24, y0 = -40, y1 = 40;
  const int32_t waist = hw - 9 * (1000 - level) / 1000;
  const int32_t cap_t[4][2] = { { -19, y0 - 6 }, { -hw, y0 }, { hw, y0 }, { 19, y0 - 6 } };
  const int32_t cap_b[4][2] = { { -hw, y1 }, { -19, y1 + 6 }, { 19, y1 + 6 }, { hw, y1 } };
  prv_fill(ctx, cap_t, 4, FX_CAN_SILVER);
  prv_fill(ctx, cap_b, 4, FX_CAN_SILVER);
  // Vier Felder, je zwischen Rand, Taille und Mitte.
  const int32_t lo[4][2] = { { -hw, y0 }, { -waist, 0 }, { 0, 0 }, { 0, y0 } };
  const int32_t ro[4][2] = { { 0, y0 }, { 0, 0 }, { waist, 0 }, { hw, y0 } };
  const int32_t lu[4][2] = { { -waist, 0 }, { -hw, y1 }, { 0, y1 }, { 0, 0 } };
  const int32_t ru[4][2] = { { 0, 0 }, { 0, y1 }, { hw, y1 }, { waist, 0 } };
  prv_fill(ctx, lo, 4, FX_CAN_BLUE);
  prv_fill(ctx, ro, 4, FX_CAN_SILVER);
  prv_fill(ctx, lu, 4, FX_CAN_SILVER);
  prv_fill(ctx, ru, 4, FX_CAN_BLUE);
  // Die Sonne hinter dem Gesicht, darunter zwei rote Keile, die aufeinander zulaufen.
  graphics_context_set_fill_color(ctx, FX_SUN);
  graphics_fill_circle(ctx, prv_gp(-3, -4), (uint16_t)(15 * s_g.k / 1000));
  const int32_t hl[3][2] = { { -21, 28 }, { -3, 16 }, { -7, 30 } };
  const int32_t hr[3][2] = { { 19, 28 }, { 1, 16 }, { 5, 30 } };
  prv_fill(ctx, hl, 3, FX_RED);
  prv_fill(ctx, hr, 3, FX_RED);
  const int32_t body[10][2] = { { -19, y0 - 6 }, { -hw, y0 }, { -waist, 0 }, { -hw, y1 },
                                { -19, y1 + 6 }, { 19, y1 + 6 }, { hw, y1 }, { waist, 0 },
                                { hw, y0 }, { 19, y0 - 6 } };
  prv_outline(ctx, body, 10);
  // Die Falze von Deckel und Boden.
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, prv_gp(-hw, y0), prv_gp(hw, y0));
  graphics_draw_line(ctx, prv_gp(-hw, y1), prv_gp(hw, y1));
  prv_face(ctx, face);
}

static void prv_draw_glass(GContext *ctx, int32_t level, Face face, GColor water_color) {
  // Basis-Einheiten: 72 breit oben, 54 am Boden, 80 hoch
  const int32_t hw_top = 36, hw_bot = 27, hh = 40;
  // Trapez um den Ursprung: links oben, links unten, rechts unten, rechts oben
  GPoint gp[4] = { prv_gp(-hw_top, -hh), prv_gp(-hw_bot, hh),
                   prv_gp(hw_bot, hh), prv_gp(hw_top, -hh) };
  const GPathInfo ginfo = { .num_points = 4, .points = gp };
  GPath *glass = gpath_create(&ginfo);
  if (!glass) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, glass);
  if (level > 0) {
    const int32_t wy = hh - 2 * hh * level / 1000;
    const int32_t whw = hw_bot + (hw_top - hw_bot) * level / 1000;
    GPoint wp[4] = { prv_gp(-hw_bot, hh), prv_gp(hw_bot, hh),
                     prv_gp(whw, wy), prv_gp(-whw, wy) };
    const GPathInfo winfo = { .num_points = 4, .points = wp };
    GPath *water = gpath_create(&winfo);
    if (water) {
      graphics_context_set_fill_color(ctx, water_color);
      gpath_draw_filled(ctx, water);
      gpath_destroy(water);
    }
  }
  prv_pen(ctx, true);                                  // Rahmen rundum
  gpath_draw_outline(ctx, glass);
  prv_pen(ctx, false);
  gpath_draw_outline(ctx, glass);
  gpath_destroy(glass);
  prv_face(ctx, face);
}

// Das Gefaess je nach Art; `steam` nur fuer das volle Heissgetraenk.
static void prv_draw_vessel(GContext *ctx, int32_t level, Face face, GColor water, bool steam) {
  switch (s_vessel) {
    case VesselEspresso:   prv_draw_espresso(ctx, level, face, steam); break;
    case VesselCoffee:     prv_draw_mug(ctx, level, face, FX_COFFEE, steam); break;
    case VesselCoffeeMilk: prv_draw_mug(ctx, level, face, FX_MILKCOFFEE, steam); break;
    case VesselLatte:      prv_draw_latte(ctx, level, face); break;
    case VesselCan:        prv_draw_can(ctx, level, face); break;
    case VesselCustom:
      // Das Wasserglas mit einem Trinkhalm - fuer alles, was keine eigene Form hat.
      prv_draw_glass(ctx, level, face, FX_CUSTOM);
      prv_line(ctx, prv_gp(16, -22), prv_gp(32, -64));
      break;
    default:               prv_draw_glass(ctx, level, face, water); break;
  }
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
  GPoint sp[8];
  for (int i = 0; i < 8; i++) {
    const int32_t r = (i % 2) ? ri : ro;
    const int32_t a = -TRIG_MAX_ANGLE / 4 + i * TRIG_MAX_ANGLE / 8;
    sp[i] = GPoint(s_g.c.x + r * cos_lookup(a) / TRIG_MAX_RATIO,
                   s_g.c.y + r * sin_lookup(a) / TRIG_MAX_RATIO);
  }
  const GPathInfo sinfo = { .num_points = 8, .points = sp };
  GPath *star = gpath_create(&sinfo);
  if (!star) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, star);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, s_g.stroke > 3 ? 2 : 1);
  gpath_draw_outline(ctx, star);
  gpath_destroy(star);
}

// smoothstep, Ein- und Ausgabe in Promille
static int32_t prv_smooth(int32_t f) {
  return f * f * (3000 - 2 * f) / 1000000;
}

static void prv_draw(Layer *layer, GContext *ctx) {
  GPoint c = s_anchor;
  int32_t scale = 1000;
  if (s_p >= HOLD_END && s_p < DRINK_END) {
    // Schuetteln nur waehrend der Pegel faellt
    const int32_t ms = (s_p - HOLD_END) * FX_MS / 1000;
    c.x += ((ms / SHAKE_MS) % 2) ? SHAKE_PX : -SHAKE_PX;
  }
  if (s_p < POP_END) {
    // Aufploppen mit Ueberschwingen: 0 -> 1150 -> 1000
    const int32_t t = s_p * 1000 / POP_END;
    scale = t < 600 ? 1150 * prv_smooth(t * 1000 / 600) / 1000
                    : 1150 - 150 * (t - 600) / 400;
  } else if (s_p >= SMILE_END && s_p < SHRINK_END) {
    const int32_t t = (s_p - SMILE_END) * 1000 / (SHRINK_END - SMILE_END);
    scale = 1000 - t * t / 1000;                                    // beschleunigt
  }
  prv_set_metrics(c, s_width, scale);

  if (s_p < HOLD_END) {
    prv_draw_vessel(ctx, 1000, FaceSmile, DT_COLOR_FX_WATER, s_p >= POP_END);
  } else if (s_p < DRINK_END) {
    // gleichmaessig leeren
    const int32_t t = (s_p - HOLD_END) * 1000 / (DRINK_END - HOLD_END);
    prv_draw_vessel(ctx, 1000 - t, FaceGulp, DT_COLOR_FX_WATER, false);
  } else if (s_p < SHRINK_END) {
    // leer, ab SMILE_END schrumpfend; das letzte Zwergenglas sparen wir uns
    if (scale > 60) prv_draw_vessel(ctx, 0, FaceSmile, DT_COLOR_FX_WATER, false);
  } else {
    prv_draw_burst(ctx, (s_p - SHRINK_END) * 1000 / (1000 - SHRINK_END));
  }
}

void glass_fx_draw_still(GContext *ctx, GPoint center, int16_t width, int32_t level_permille,
                         GColor water) {
  prv_set_metrics(center, width, 1000);
  prv_draw_glass(ctx, level_permille, FaceSmile, water);
}

void glass_fx_draw_vessel_still(GContext *ctx, GPoint center, int16_t width, Vessel vessel) {
  const Vessel vorher = s_vessel;
  s_vessel = vessel;
  prv_set_metrics(center, width, 1000);
  prv_draw_vessel(ctx, 1000, FaceSmile, DT_COLOR_FX_WATER, false);
  s_vessel = vorher;
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

void glass_fx_play(GPoint anchor, int16_t width, GlassFxDone done) {
  glass_fx_play_vessel(anchor, width, VesselGlass, done);
}

void glass_fx_play_vessel(GPoint anchor, int16_t width, Vessel vessel, GlassFxDone done) {
  if (!s_layer || s_anim) return;
  s_vessel = vessel;
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
