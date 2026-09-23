#pragma once
#include <pebble.h>

// Die Nacht, wie die Uhr sie gesehen hat - fuer das Telefon.
//
// WARUM AUSGERECHNET DRINKTERVALL: es redet ohnehin mehrmals am Tag mit dem
// Telefon, bei jedem Wecker. Kiesel-Helper hoert mit und traegt ein, was in
// die Gesundheitsakte gehoert; Schlaf und Ruhepuls fahren bei der
// Standmeldung einfach mit. Eine eigene App, die morgens dafuer aufwacht,
// waere ein Schirm mehr, der angeht.

typedef struct {
  time_t beginn;       //< 0 = keine Nacht gefunden
  time_t ende;
  uint32_t erholsam_s; //< erholsame Sekunden darin
  uint16_t ruhepuls;   //< 0 = keiner zu haben
} Nacht;

// Die letzte Nacht (Schlaf, der heute endete) und der Ruhepuls von heute.
Nacht nacht_lesen(void);
