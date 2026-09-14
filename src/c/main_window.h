#pragma once
#include <pebble.h>

// Hauptscreen: naechste Erinnerung, Zaehler und Pegelband. Gleicht sich beim
// Erscheinen selbst mit schedule_count() ab.
void main_window_push(void);

// Neu zeichnen und den Pegel nachfuehren. Fuer Aenderungen, die nicht vom
// Screen selbst kommen - etwa ein neues Soll von der Konfigseite.
void main_window_refresh(void);
