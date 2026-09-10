#pragma once
#include <pebble.h>

// Vollbild-Erinnerung "Zeit fuer ein Glas Wasser". Vibriert beim Oeffnen
// und schliesst sich nach AT_REMINDER_TIMEOUT_S von selbst.
void reminder_window_push(void);
