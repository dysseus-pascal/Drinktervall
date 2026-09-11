#pragma once
#include <stdbool.h>

// Vom Erinnerungs-Screen gerufen, wenn er geschlossen wird. Wurde er mit
// Zurueck weggedrueckt (Glas verpasst) und die App durch das Wakeup gestartet,
// beendet sie sich, damit die Watch zum Zifferblatt zurueckkehrt.
void drinktervall_reminder_closed(bool dismissed);
