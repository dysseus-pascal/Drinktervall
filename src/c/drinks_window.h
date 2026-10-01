#pragma once
#include <pebble.h>

// Getraenkeauswahl auf der oberen Taste des Hauptscreens: einen Kaffee oder
// Energy-Drink ausserhalb des Plans eintragen. Oben die eingestellten Kaffees
// genau wie im Plan (ein Druck), darunter die vier Sorten - dort folgen Milch
// (nur beim Kaffee) und Zucker zum Abhaken.
void drinks_window_push(void);
