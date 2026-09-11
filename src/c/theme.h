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
#define DT_COLOR_PRIMARY     PBL_IF_COLOR_ELSE(GColorBlueMoon, GColorBlack)
#define DT_COLOR_WATER       PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorLightGray)
#define DT_COLOR_WATER_DARK  PBL_IF_COLOR_ELSE(GColorCeleste, GColorLightGray)
#define DT_COLOR_BG          GColorWhite
#define DT_COLOR_TEXT        PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define DT_COLOR_ON_PRIMARY  GColorWhite

// Hauptscreen: die ganze Flaeche ist das "Glas". LEVEL_LIGHT ist der leere
// Grund, LEVEL_DARK das Wasser, das pro getrunkenem Glas von unten steigt.
// ON_LIGHT/ON_DARK sind die Schriftfarben darauf; die Tasten-Hinweise nutzen
// jeweils die Gegenfarbe.
#define DT_COLOR_LEVEL_LIGHT PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite)
#define DT_COLOR_LEVEL_DARK  PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack)
#define DT_COLOR_ON_LIGHT    PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define DT_COLOR_ON_DARK     GColorWhite

// Trink-Animation (Vollbild): weisser Grund, hellblaues Wasser im Glas.
// S/W: schwarzes Wasser im weissen Glas.
#define DT_COLOR_FX_BG       GColorWhite
#define DT_COLOR_FX_WATER    PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack)
