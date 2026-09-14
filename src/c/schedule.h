#pragma once
#include <pebble.h>

// Soll, Zaehler UND Tagesziel laden; bei Tageswechsel den Zaehler auf 0 und
// das Ziel zurueck auf das Soll setzen.
void schedule_init(void);

// Gewaehltes Soll: so viele Glaeser sieht der Plan vor (DT_GLASSES_MIN bis
// DT_GLASSES_MAX, voreingestellt DT_GLASSES_DEFAULT). Kommt von der
// Konfigseite der Telefon-App.
int schedule_target(void);

// Soll setzen (wird auf DT_GLASSES_MIN..DT_GLASSES_MAX begrenzt) und
// persistieren. Rueckgabe: true, wenn sich dadurch etwas geaendert hat - dann
// muessen Wakeups und Anzeige nachziehen.
bool schedule_set_target(int target);

// Heute getrunkene Glaeser (0..Tagesziel).
int schedule_count(void);

// Zaehler setzen (wird auf 0..Tagesziel begrenzt) und persistieren.
void schedule_set_count(int count);

// Heutiges Tagesziel (das Soll, per Taste erhoehbar bis DT_GOAL_MAX).
int schedule_goal(void);
void schedule_raise_goal(void);

// Abstand zweier Erinnerungen in Minuten, aus Tagesfenster und Soll.
int schedule_interval_min(void);

// Glasgroesse in Millilitern (DT_GLASS_ML_MIN..MAX, voreingestellt
// DT_GLASS_ML_DEFAULT). Kommt von der Konfigseite der Telefon-App.
int schedule_glass_ml(void);
bool schedule_set_glass_ml(int ml);

// Lokale Mitternacht des Tages, in dem `t` liegt (Epoch-Sekunden).
time_t schedule_midnight(time_t t);

// Zeitpunkt der Erinnerung `idx` (0..schedule_target()-1) an dem Tag mit
// Mitternacht `midnight`.
time_t schedule_slot(time_t midnight, int idx);

// Naechste Erinnerung nach `now` (heute oder morgen). Rueckgabe: Slot-Index.
int schedule_next(time_t now, time_t *when);

// Alle Wakeups neu planen: optional zuerst ein Snooze-Wakeup, dann die
// naechsten regulaeren Slots bis zum Limit von 8. `snooze_until` = 0 fuer keins.
void schedule_plan_wakeups(time_t snooze_until);

// "HH:MM" gemaess Uhr-Einstellung der Watch.
void schedule_format_time(time_t t, char *buf, size_t len);
