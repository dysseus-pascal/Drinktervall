// Grafik-Attrappe fuer tools/glas_host_test.c - siehe attrappe_grafik.h.
// Gezeichnet wird in attrappe_bild wie auf der Schwarz-Weiss-Uhr.
#include <math.h>
#include <pebble.h>

AttrappeBild attrappe_bild;

static GColor s_fuellfarbe = GColorBlack;
static GColor s_strichfarbe = GColorBlack;
static int s_strichbreite = 1;
static int s_fehler;

struct GContext { int unbenutzt; };
static struct GContext s_kontext;

GContext *attrappe_kontext(void) { return &s_kontext; }
int attrappe_grafik_fehler(void) { return s_fehler; }

void attrappe_bild_leeren(void) {
  memset(&attrappe_bild, 0, sizeof(attrappe_bild));
  memset(attrappe_bild.weiss, 1, sizeof(attrappe_bild.weiss));
  s_fehler = 0;
}

// pebbleos graphics_private.c, grays[] - mit den Werten der Firmware. Ein
// gesetztes Bit ist ein weisses Pixel; je Zeile das Muster der geraden bzw.
// ungeraden Zeile.
static const uint16_t s_grau_muster[14] = { 0x0000, 0x0000, 0x1111, 0x4444, 0x5555, 0xAAAA, 0x5555,
                                            0xAAAA, 0x5555, 0xAAAA, 0xEEEE, 0xBBBB, 0xFFFF, 0xFFFF };

static bool prv_im_bild(int x, int y) {
  return x >= 0 && y >= 0 && x < ATTRAPPE_BILD_B && y < ATTRAPPE_BILD_H;
}

// Ein Pixel einer Fuellung: Farbe -> Helligkeit 0..12 aus den 2-Bit-Anteilen
// (graphics_private_get_1bit_grayscale_pattern), dann das Muster der Zeile.
static void prv_fuell_pixel(int x, int y) {
  if (!prv_im_bild(x, y)) return;
  const int r = (s_fuellfarbe >> 4) & 3, g = (s_fuellfarbe >> 2) & 3, b = s_fuellfarbe & 3;
  const int luma = ((r << 1) + r + (g << 2) + b) >> 1;
  const uint16_t muster = s_grau_muster[(luma & ~1) + (y & 1)];
  attrappe_bild.weiss[y][x] = (uint8_t)((muster >> (x & 15)) & 1);
  attrappe_bild.herkunft[y][x] = HerkunftFuellung;
  attrappe_bild.grau[y][x] = (s_fuellfarbe == GColorDarkGray || s_fuellfarbe == GColorLightGray);
}

static void prv_strich_pixel(int x, int y, Herkunft herkunft) {
  if (!prv_im_bild(x, y)) return;
  attrappe_bild.weiss[y][x] = (uint8_t)(s_strichfarbe != GColorBlack);
  attrappe_bild.herkunft[y][x] = (uint8_t)herkunft;
  attrappe_bild.grau[y][x] = 0;
}

void graphics_context_set_fill_color(GContext *ctx, GColor color) {
  (void)ctx;
  // Auf Schwarz-Weiss kennt die Uhr nach dem Runden nur diese vier.
  if (color != GColorBlack && color != GColorDarkGray && color != GColorLightGray &&
      color != GColorWhite) {
    printf("  Attrappe: Fuellfarbe 0x%02X gibt es auf Schwarz-Weiss nicht\n", color);
    s_fehler++;
  }
  s_fuellfarbe = color;
}
void graphics_context_set_stroke_color(GContext *ctx, GColor color) { (void)ctx; s_strichfarbe = color; }
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width) { (void)ctx; s_strichbreite = width ? width : 1; }

void graphics_draw_pixel(GContext *ctx, GPoint p) {
  (void)ctx;
  prv_strich_pixel(p.x, p.y, HerkunftPunkt);
}

// Ein Strich: um jeden Punkt der Linie eine Scheibe in Strichbreite.
static void prv_scheibe(int cx, int cy, double r) {
  const int ri = (int)ceil(r);
  for (int y = cy - ri; y <= cy + ri; y++) {
    for (int x = cx - ri; x <= cx + ri; x++) {
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) prv_strich_pixel(x, y, HerkunftStrich);
    }
  }
}

void graphics_draw_line(GContext *ctx, GPoint a, GPoint b) {
  (void)ctx;
  const int dx = abs(b.x - a.x), dy = abs(b.y - a.y);
  const int n = dx > dy ? dx : dy;
  const double r = s_strichbreite / 2.0;
  for (int i = 0; i <= n; i++) {
    const double t = n ? (double)i / n : 0.0;
    prv_scheibe((int)lround(a.x + (b.x - a.x) * t), (int)lround(a.y + (b.y - a.y) * t), r);
  }
}

void graphics_draw_circle(GContext *ctx, GPoint c, uint16_t r) {
  (void)ctx;
  const double halb = s_strichbreite / 2.0;
  const int aussen = r + (int)ceil(halb);
  for (int y = c.y - aussen; y <= c.y + aussen; y++) {
    for (int x = c.x - aussen; x <= c.x + aussen; x++) {
      const double d = sqrt((double)(x - c.x) * (x - c.x) + (double)(y - c.y) * (y - c.y));
      if (fabs(d - r) <= halb) prv_strich_pixel(x, y, HerkunftStrich);
    }
  }
}

void graphics_fill_circle(GContext *ctx, GPoint c, uint16_t r) {
  (void)ctx;
  for (int y = c.y - r; y <= c.y + r; y++) {
    for (int x = c.x - r; x <= c.x + r; x++) {
      if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= r * r) prv_fuell_pixel(x, y);
    }
  }
}

struct GPath {
  uint32_t n;
  GPoint p[16];
};

GPath *gpath_create(const GPathInfo *info) {
  if (!info || info->num_points > 16) return NULL;
  GPath *path = calloc(1, sizeof(GPath));
  if (!path) return NULL;
  path->n = info->num_points;
  memcpy(path->p, info->points, info->num_points * sizeof(GPoint));
  return path;
}
void gpath_destroy(GPath *path) { free(path); }

// Vieleck fuellen: je Pixelzeile die Kanten an der Zeilenmitte schneiden
// (gerade-ungerade), gefuellt wird jedes Pixel, dessen Mitte innen liegt.
void gpath_draw_filled(GContext *ctx, GPath *path) {
  (void)ctx;
  if (!path || path->n < 3) return;
  int ymin = path->p[0].y, ymax = path->p[0].y;
  for (uint32_t i = 1; i < path->n; i++) {
    if (path->p[i].y < ymin) ymin = path->p[i].y;
    if (path->p[i].y > ymax) ymax = path->p[i].y;
  }
  for (int y = ymin; y <= ymax; y++) {
    const double ym = y + 0.5;
    double xs[16];
    int nx = 0;
    for (uint32_t i = 0; i < path->n; i++) {
      const GPoint a = path->p[i], b = path->p[(i + 1) % path->n];
      if ((a.y <= ym && b.y > ym) || (b.y <= ym && a.y > ym)) {
        xs[nx++] = a.x + (ym - a.y) * (b.x - a.x) / (double)(b.y - a.y);
      }
    }
    for (int i = 1; i < nx; i++) {                    // sortieren
      const double v = xs[i];
      int j = i - 1;
      while (j >= 0 && xs[j] > v) { xs[j + 1] = xs[j]; j--; }
      xs[j + 1] = v;
    }
    for (int i = 0; i + 1 < nx; i += 2) {
      for (int x = (int)ceil(xs[i] - 0.5); x + 0.5 <= xs[i + 1]; x++) prv_fuell_pixel(x, y);
    }
  }
}

static void prv_umriss(GPath *path, bool geschlossen) {
  if (!path) return;
  for (uint32_t i = 0; i + 1 < path->n; i++) graphics_draw_line(NULL, path->p[i], path->p[i + 1]);
  if (geschlossen && path->n > 2) graphics_draw_line(NULL, path->p[path->n - 1], path->p[0]);
}
void gpath_draw_outline(GContext *ctx, GPath *path) { (void)ctx; prv_umriss(path, true); }
void gpath_draw_outline_open(GContext *ctx, GPath *path) { (void)ctx; prv_umriss(path, false); }

int32_t sin_lookup(int32_t angle) {
  return (int32_t)lround(sin(2.0 * M_PI * angle / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
int32_t cos_lookup(int32_t angle) {
  return (int32_t)lround(cos(2.0 * M_PI * angle / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

// Layer und Animation tun nichts; glass_fx.c legt sie nur an.
struct Layer { GRect rahmen; };
static struct Layer s_layer;
Layer *layer_create(GRect frame) { s_layer.rahmen = frame; return &s_layer; }
void layer_destroy(Layer *layer) { (void)layer; }
GRect layer_get_bounds(const Layer *layer) {
  return layer ? GRect(0, 0, layer->rahmen.size.w, layer->rahmen.size.h)
               : GRect(0, 0, ATTRAPPE_BILD_B, ATTRAPPE_BILD_H);
}
void layer_set_update_proc(Layer *layer, LayerUpdateProc proc) { (void)layer; (void)proc; }
void layer_set_hidden(Layer *layer, bool hidden) { (void)layer; (void)hidden; }
void layer_add_child(Layer *parent, Layer *child) { (void)parent; (void)child; }
void layer_mark_dirty(Layer *layer) { (void)layer; }

Animation *animation_create(void) { return NULL; }
bool animation_destroy(Animation *a) { (void)a; return true; }
bool animation_schedule(Animation *a) { (void)a; return true; }
bool animation_unschedule(Animation *a) { (void)a; return true; }
bool animation_set_curve(Animation *a, AnimationCurve c) { (void)a; (void)c; return true; }
bool animation_set_duration(Animation *a, uint32_t ms) { (void)a; (void)ms; return true; }
bool animation_set_handlers(Animation *a, AnimationHandlers h, void *c) { (void)a; (void)h; (void)c; return true; }
bool animation_set_implementation(Animation *a, const AnimationImplementation *i) { (void)a; (void)i; return true; }

void attrappe_bild_speichern(const char *pfad) {
  FILE *f = fopen(pfad, "wb");
  if (!f) {
    printf("  Attrappe: %s nicht zu schreiben\n", pfad);
    return;
  }
  fprintf(f, "P5\n%d %d\n255\n", ATTRAPPE_BILD_B, ATTRAPPE_BILD_H);
  for (int y = 0; y < ATTRAPPE_BILD_H; y++) {
    for (int x = 0; x < ATTRAPPE_BILD_B; x++) fputc(attrappe_bild.weiss[y][x] ? 255 : 0, f);
  }
  fclose(f);
}
