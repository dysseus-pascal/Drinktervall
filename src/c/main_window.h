#pragma once
#include <pebble.h>

// Hauptscreen: naechste Erinnerung, Zaehler und Pegelband. Gleicht sich beim
// Erscheinen selbst mit schedule_count() ab.
void main_window_push(void);
