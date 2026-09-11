#pragma once
#include <pebble.h>

// Wakeup-Cookie fuer die "Spaeter"-Erinnerung; regulaere Slots tragen 0..DT_GLASSES-1.
#define SCHEDULE_COOKIE_SNOOZE 100

// Zaehler laden; bei Tageswechsel auf 0 setzen.
void schedule_init(void);

// Heute getrunkene Glaeser (0..Tagesziel).
int schedule_count(void);

// Zaehler setzen (wird auf 0..Tagesziel begrenzt) und persistieren.
void schedule_set_count(int count);

// Heutiges Tagesziel (DT_GLASSES, per Taste erhoehbar bis DT_GOAL_MAX).
int schedule_goal(void);
void schedule_raise_goal(void);

// Lokale Mitternacht des Tages, in dem `t` liegt (Epoch-Sekunden).
time_t schedule_midnight(time_t t);

// Zeitpunkt der Erinnerung `idx` (0..DT_GLASSES-1) an dem Tag mit Mitternacht `midnight`.
time_t schedule_slot(time_t midnight, int idx);

// Naechste Erinnerung nach `now` (heute oder morgen). Rueckgabe: Slot-Index.
int schedule_next(time_t now, time_t *when);

// Alle Wakeups neu planen: optional zuerst ein Snooze-Wakeup, dann die
// naechsten regulaeren Slots bis zum Limit von 8. `snooze_until` = 0 fuer keins.
void schedule_plan_wakeups(time_t snooze_until);

// "HH:MM" gemaess Uhr-Einstellung der Watch.
void schedule_format_time(time_t t, char *buf, size_t len);
