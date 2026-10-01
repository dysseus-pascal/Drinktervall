#pragma once
#include <pebble.h>

// Vollbild-Erinnerung "Zeit fuer einen Kaffee" fuer den Kaffee am Platz `idx`.
// Wie die Wasser-Erinnerung: Haken = getrunken (geht ans Telefon), Zz = in
// zehn Minuten nochmals, Zurueck = diesmal nicht.
void coffee_window_push(int idx);

// Dasselbe fuer ein eigenes Getraenk mit Erinnerungszeit (Platz `idx`).
void coffee_window_push_custom(int idx);
