#pragma once

// Vom Erinnerungs-Screen gerufen, wenn er mit Zurueck weggedrueckt wurde
// (Glas verpasst). Wurde die App durch das Wakeup gestartet, beendet sie sich
// dabei, damit die Watch zum Zifferblatt zurueckkehrt. "Getrunken" und
// "Spaeter" beenden die App selbst und rufen hier nicht.
void drinktervall_reminder_closed(void);
