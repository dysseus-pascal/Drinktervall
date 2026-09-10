#pragma once
#include <pebble.h>

// Trinkglas (Trapez, oben breiter) mit Wasserstand `level_permille` (0..1000).
void draw_glass(GContext *ctx, GRect box, int level_permille, GColor outline, GColor water);

// Tasten-Hinweise am rechten Rand auf Hoehe von Oben/Auswahl/Unten.
// NULL laesst den jeweiligen Hinweis weg.
void draw_button_hints(GContext *ctx, GRect bounds, const char *up, const char *select,
                       const char *down, GColor bg, GColor fg);
