#pragma once
#include <pebble.h>

// Farbschema blau/weiss im Stil der Pebble-Timeline. Alle Farben nur hier
// aendern.
//
//  BG           Hintergrund aller Screens, TEXT die Schrift darauf.
//  SIDEBAR      Seitenleiste rechts auf Hauptscreen und Trinkplan,
//               ON_SIDEBAR die Schrift darin.
//  BAND         Pegelband des Hauptscreens und der "kommt noch"-Teil der
//               Plan-Leiste. S/W: Grauraster, damit es auf Weiss sichtbar ist.
//  LEVEL_LIGHT  Kopfband des Erinnerungs-Screens (heller Streifen wie im
//               Detail eines Timeline-Pins).
//  FX_BG/WATER  Vollbild der Trink-Animation: weisser Grund, hellblaues
//               Wasser im Glas.
//
// Pebble-Palette (Auswahl Blau): BlueMoon #0055FF, DukeBlue #0000AA,
// VividCerulean #00AAFF, PictonBlue #55AAFF, OxfordBlue #000055.
#define DT_COLOR_BG           GColorWhite
#define DT_COLOR_TEXT         GColorBlack

#define DT_SIDEBAR_W          PBL_IF_ROUND_ELSE(51, (PBL_DISPLAY_WIDTH >= 180 ? 34 : 30))
#define DT_COLOR_SIDEBAR      PBL_IF_COLOR_ELSE(GColorDukeBlue, GColorBlack)
#define DT_COLOR_ON_SIDEBAR   GColorWhite

// Glas-Symbol oben in der Seitenleiste: Hauptscreen und Trinkplan zeichnen es
// an derselben Stelle in derselben Groesse.
#define DT_SIDEBAR_GLASS_DX   PBL_IF_ROUND_ELSE(9, 0)   // rund: sichtbarer Teil der Leiste
#define DT_SIDEBAR_GLASS_Y    PBL_IF_ROUND_ELSE(58, 20)
#define DT_SIDEBAR_GLASS_W    22
#define DT_SIDEBAR_GLASS_FILL 700

#define DT_COLOR_BAND         PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorLightGray)
#define DT_COLOR_LEVEL_LIGHT  PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite)
#define DT_COLOR_FX_BG        GColorWhite
#define DT_COLOR_FX_WATER     PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorLightGray)
