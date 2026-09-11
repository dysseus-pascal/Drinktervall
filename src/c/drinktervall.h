#pragma once
#include <stdbool.h>

// Vom Erinnerungs-Screen gerufen, wenn er geschlossen wird. Bei Timeout nach
// einem Wakeup-Start beendet sich die App, damit die Watch zum Zifferblatt
// zurueckkehrt.
void drinktervall_reminder_closed(bool timed_out);
