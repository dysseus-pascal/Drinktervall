#pragma once
#include <pebble.h>

// Vollbild-Fenster fuer die Trink-Animation: das Glas gross in der Bildmitte,
// sonst nichts. Es schliesst sich nach der Animation von selbst. `quit_after`
// beendet dabei gleich die ganze App - so bleibt der Weg ueber die Erinnerung
// oder einen Timeline-Pin auf eine kurze Rueckmeldung beschraenkt. Zurueck
// bricht nur die Animation ab; das Glas ist da schon gezaehlt.
//
// Rueckgabe FALSE heisst: es wurde nichts gezeigt, weil die Animation auf der
// Konfigseite abgeschaltet ist. Der Aufrufer muss dann selbst dafuer sorgen,
// dass man die Aenderung mitbekommt - beim Hauptscreen ist das der Pegel, der
// sonst hinter dem Fenster gestiegen waere. Ums Beenden kuemmert sich auch
// dann diese Datei, damit es dafuer nur eine Stelle gibt.
bool drink_window_push(bool quit_after);
bool drink_window_is_open(void);
