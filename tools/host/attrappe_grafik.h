// Nur fuer tools/glas_host_test.c: so viel Grafik, wie glass_fx.c braucht,
// gezeichnet in ein Bild im Speicher - wie auf der SCHWARZ-WEISS-UHR (flint).
// Eingebunden von pebble.h, wenn ATTRAPPE_GRAFIK gesetzt ist; die anderen
// Host-Tests sehen davon nichts.
//
// WIE DIE FIRMWARE FUELLT (pebbleos, Werte hier festgenagelt):
//   - graphics_context_set_fill_color rundet auf Schwarz-Weiss jede Farbe auf
//     Schwarz, Dunkelgrau, Hellgrau oder Weiss (graphics.c ->
//     gtypes.c gcolor_get_grayscale). glass_fx.c nimmt auf flint nur diese
//     vier; eine andere Fuellfarbe meldet die Attrappe als Fehler.
//   - Gefuellt wird mit einem Muster je Zeile aus der Helligkeit
//     (graphics_private.c, graphics_private_get_1bit_grayscale_pattern und
//     grays[]): Dunkel- wie Hellgrau werden dasselbe Schachbrett.
//   - Striche und einzelne Punkte nehmen die Strichfarbe, Schwarz oder Weiss.
// Ob das Bild dem der Uhr gleicht, zeigt der Emulator (tools/screenshots.sh
// flint); hier geht es um den Anteil Schwarz in der Fuellung.
#pragma once

#ifndef PBL_IF_COLOR_ELSE
#define PBL_IF_COLOR_ELSE(farbe, sw) (sw)
#endif
#define PBL_IF_ROUND_ELSE(rund, eckig) (eckig)
#define PBL_DISPLAY_WIDTH 144
#define PBL_DISPLAY_HEIGHT 168

// GColor8 als ARGB-Byte, wie in der SDK (gcolor_definitions.h).
#define GColorClear     ((GColor)0x00)
#define GColorBlack     ((GColor)0xC0)
#define GColorDarkGray  ((GColor)0xD5)
#define GColorLightGray ((GColor)0xEA)
#define GColorWhite     ((GColor)0xFF)

#define GPoint(x, y) ((GPoint){ (int16_t)(x), (int16_t)(y) })
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GRect(x, y, w, h) ((GRect){ { (int16_t)(x), (int16_t)(y) }, { (int16_t)(w), (int16_t)(h) } })

typedef struct {
  uint32_t num_points;
  GPoint *points;
} GPathInfo;
typedef struct GPath GPath;
GPath *gpath_create(const GPathInfo *info);
void gpath_destroy(GPath *path);
void gpath_draw_filled(GContext *ctx, GPath *path);
void gpath_draw_outline(GContext *ctx, GPath *path);
void gpath_draw_outline_open(GContext *ctx, GPath *path);

void graphics_context_set_fill_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_color(GContext *ctx, GColor color);
void graphics_context_set_stroke_width(GContext *ctx, uint8_t width);
void graphics_draw_line(GContext *ctx, GPoint a, GPoint b);
void graphics_draw_pixel(GContext *ctx, GPoint p);
void graphics_draw_circle(GContext *ctx, GPoint c, uint16_t r);
void graphics_fill_circle(GContext *ctx, GPoint c, uint16_t r);

#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
int32_t sin_lookup(int32_t angle);
int32_t cos_lookup(int32_t angle);

// Layer und Animation: glass_fx.c legt sie an, gezeichnet wird hier nur
// ueber glass_fx_draw_vessel_still.
typedef void (*LayerUpdateProc)(Layer *layer, GContext *ctx);
Layer *layer_create(GRect frame);
void layer_destroy(Layer *layer);
GRect layer_get_bounds(const Layer *layer);
void layer_set_update_proc(Layer *layer, LayerUpdateProc proc);
void layer_set_hidden(Layer *layer, bool hidden);
void layer_add_child(Layer *parent, Layer *child);
void layer_mark_dirty(Layer *layer);

typedef struct Animation Animation;
typedef uint32_t AnimationProgress;
#define ANIMATION_NORMALIZED_MAX 65535
typedef enum { AnimationCurveLinear = 0 } AnimationCurve;
typedef void (*AnimationUpdateImplementation)(Animation *animation, const AnimationProgress progress);
typedef struct { void *setup; AnimationUpdateImplementation update; void *teardown; } AnimationImplementation;
typedef void (*AnimationStoppedHandler)(Animation *animation, bool finished, void *context);
typedef struct { void *started; AnimationStoppedHandler stopped; } AnimationHandlers;
Animation *animation_create(void);
bool animation_destroy(Animation *animation);
bool animation_schedule(Animation *animation);
bool animation_unschedule(Animation *animation);
bool animation_set_curve(Animation *animation, AnimationCurve curve);
bool animation_set_duration(Animation *animation, uint32_t ms);
bool animation_set_handlers(Animation *animation, AnimationHandlers handlers, void *context);
bool animation_set_implementation(Animation *animation, const AnimationImplementation *impl);

// --- Was der Test damit tut ---
#define ATTRAPPE_BILD_B 200
#define ATTRAPPE_BILD_H 228
// Wer ein Pixel zuletzt gesetzt hat.
typedef enum { HerkunftLeer = 0, HerkunftFuellung, HerkunftPunkt, HerkunftStrich } Herkunft;
typedef struct {
  uint8_t weiss[ATTRAPPE_BILD_H][ATTRAPPE_BILD_B];      //< 1 weiss, 0 schwarz
  uint8_t herkunft[ATTRAPPE_BILD_H][ATTRAPPE_BILD_B];   //< Herkunft
  uint8_t grau[ATTRAPPE_BILD_H][ATTRAPPE_BILD_B];       //< 1: aus einer grauen Fuellung
} AttrappeBild;
extern AttrappeBild attrappe_bild;
void attrappe_bild_leeren(void);          //< weiss, nichts gezeichnet
int attrappe_grafik_fehler(void);         //< Fuellfarben, die die Uhr nicht kennt
GContext *attrappe_kontext(void);
// Das Bild als PGM (zum Ansehen; tools/glas_host_test.sh wandelt es um).
void attrappe_bild_speichern(const char *pfad);
