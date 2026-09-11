#pragma once
#include <pebble.h>

// Vollbild-Fenster fuer die Trink-Animation (wie das Popup beim Loeschen
// eines Timers): grosses Glas in der Mitte, darunter "Glas n von 8".
// Schliesst sich nach der Animation von selbst; Zurueck bricht ab.
void drink_window_push(void);
bool drink_window_is_open(void);
