#pragma once
#include <pebble.h>

// Tasten-Hinweise am rechten Rand auf Hoehe von Oben/Auswahl/Unten.
// NULL laesst den jeweiligen Hinweis weg.
void draw_button_hints(GContext *ctx, GRect bounds, const char *up, const char *select,
                       const char *down, GColor bg, GColor fg);
