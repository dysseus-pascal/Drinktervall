#pragma once
#include <pebble.h>

// Vollbild-Fenster fuer die Trink-Animation: das Glas gross in der Bildmitte,
// sonst nichts. Es schliesst sich nach der Animation von selbst. `quit_after`
// beendet dabei gleich die ganze App - so bleibt der Weg ueber die Erinnerung
// oder einen Timeline-Pin auf eine kurze Rueckmeldung beschraenkt. Zurueck
// bricht nur die Animation ab; das Glas ist da schon gezaehlt.
void drink_window_push(bool quit_after);
bool drink_window_is_open(void);
