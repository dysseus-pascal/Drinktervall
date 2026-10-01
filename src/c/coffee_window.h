#pragma once
#include <pebble.h>

// Vollbild-Erinnerung "Zeit fuer einen Kaffee" fuer den Kaffee am Platz `idx`.
// Wie die Wasser-Erinnerung: Haken = getrunken (geht ans Telefon), Zz = in
// zehn Minuten nochmals, Zurueck = diesmal nicht.
void coffee_window_push(int idx);
