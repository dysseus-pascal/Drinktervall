#pragma once
#include <pebble.h>

// Vollbild-Erinnerung "Zeit fuer ein Glas Wasser". Vibriert dreimal im Abstand
// von 20 s und bleibt stehen, bis Getrunken, Spaeter oder Zurueck gedrueckt
// wird. Erneuter Aufruf bei schon offenem Fenster: nur neu vibrieren.
void reminder_window_push(void);
