#pragma once
#include <pebble.h>

// Vom Erinnerungs-Screen gerufen, wenn er mit Zurueck weggedrueckt wurde
// (Glas verpasst). Wurde die App durch das Wakeup gestartet, beendet sie sich
// dabei, damit die Watch zum Zifferblatt zurueckkehrt. "Getrunken" und
// "Spaeter" beenden die App selbst und rufen hier nicht.
void drinktervall_reminder_closed(void);

/**
 * Die Ruhezeit der Uhr.
 *
 * Sie steht an EINER Stelle, weil sonst jede Stelle, die summt, ihre eigene
 * Meinung dazu haette - und eine davon wuerde man vergessen. Genau das ist der
 * Sinn der Ruhezeit: sie gilt ueberall oder gar nicht.
 *
 * Was sie bedeutet, ist von der Uhr uebernommen und nicht neu erfunden:
 * Pebble laesst Mitteilungen waehrend der Ruhezeit ankommen, nur stumm und
 * ohne Licht. Ebenso hier - die Erinnerung erscheint, sie klopft bloss nicht.
 * Sie ganz zu unterschlagen waere etwas anderes als still zu sein, und die
 * Uhr macht es bei Mitteilungen auch nicht so.
 */
bool drinktervall_quiet(void);

// Summen, ausser in der Ruhezeit.
void drinktervall_buzz_short(void);
void drinktervall_buzz_double(void);

// Licht anmachen, ausser in der Ruhezeit.
void drinktervall_light(void);
