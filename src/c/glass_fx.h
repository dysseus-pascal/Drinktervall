#pragma once
#include <pebble.h>

typedef void (*GlassFxDone)(void);

// Overlay fuer die Trink-Animation: ein volles Glas ploppt am `anchor` auf,
// leert sich gleichmaessig (dabei wackelt es), schrumpft ins Zentrum und
// zerplatzt in einem Strahlenkranz. Der genaue Ablauf steht am Kopf von
// glass_fx.c. Der Layer wird als oberstes Kind von `parent` angelegt und ist
// ausserhalb der Animation unsichtbar.
void glass_fx_init(Layer *parent);
void glass_fx_deinit(void);

// Animation starten; `anchor` ist der Mittelpunkt des Glases in Koordinaten
// des Parent-Layers, `width` die Breite des vollen Glases am oberen Rand.
// `done` wird nach dem regulaeren Ende gerufen (nicht bei Abbruch durch
// glass_fx_deinit), bei fehlendem Speicher auch sofort. Laeuft schon eine
// Animation, passiert nichts.
void glass_fx_play(GPoint anchor, int16_t width, GlassFxDone done);

// Stehendes Glas im selben Stil (Rahmen, Wasser bis `level_permille` in der
// Farbe `water`, laechelndes Gesicht) an beliebiger Stelle zeichnen, z.B. in
// der Seitenleiste. Die Wasserfarbe muss sich vom Untergrund abheben, sonst
// wirkt das Glas leer. Unabhaengig vom Overlay-Layer.
void glass_fx_draw_still(GContext *ctx, GPoint center, int16_t width, int32_t level_permille,
                         GColor water);
