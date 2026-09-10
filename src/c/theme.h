#pragma once
#include <pebble.h>

// Farbschema blau/weiss. Alle Farben nur hier aendern.
//
//  PRIMARY     Kopfzeile, Glas-Umriss, Tasten-Hinweise, Hintergrund des
//              Erinnerungs-Screens. S/W: Schwarz.
//  WATER       Wasserfuellung im Glas (auf weissem Grund). S/W: helles
//              Grau, das auf 1-Bit-Displays als Raster erscheint.
//  WATER_DARK  Wasserfuellung auf PRIMARY (Erinnerungs-Screen).
//  BG          Hintergrund der normalen Screens.
//  TEXT        Schrift auf BG.
//  ON_PRIMARY  Schrift auf PRIMARY.
//
// Der Hex-Wert von PRIMARY ist in src/pkjs/index.js (backgroundColor der
// Timeline-Pins) von Hand kopiert - dort nachziehen.
//
// Pebble-Palette (Auswahl Blau): BlueMoon #0055FF, DukeBlue #0000AA,
// CobaltBlue #0055AA, VividCerulean #00AAFF, PictonBlue #55AAFF,
// Celeste #AAFFFF, OxfordBlue #000055.
#define AT_COLOR_PRIMARY     PBL_IF_COLOR_ELSE(GColorBlueMoon, GColorBlack)
#define AT_COLOR_WATER       PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorLightGray)
#define AT_COLOR_WATER_DARK  PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray)
#define AT_COLOR_BG          GColorWhite
#define AT_COLOR_TEXT        PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define AT_COLOR_ON_PRIMARY  GColorWhite
