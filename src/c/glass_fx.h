#pragma once
#include <pebble.h>

typedef void (*GlassFxDone)(void);

// Overlay fuer die Trink-Animation: ein volles Glas erscheint in der Mitte,
// wird in Schlucken geleert (mit Wackeln) und verpufft dann in einer Wolke.
// Der Layer wird als oberstes Kind von `parent` angelegt und ist ausserhalb
// der Animation unsichtbar.
void glass_fx_init(Layer *parent);
void glass_fx_deinit(void);

// Animation starten; `anchor` ist der Mittelpunkt des Glases in Koordinaten
// des Parent-Layers, `width` die Breite des vollen Glases am oberen Rand.
// `done` wird nach dem regulaeren Ende gerufen (nicht bei Abbruch durch
// glass_fx_deinit). Laeuft schon eine, passiert nichts.
void glass_fx_play(GPoint anchor, int16_t width, GlassFxDone done);
bool glass_fx_is_playing(void);
