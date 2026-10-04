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

// Der Tag (JJJJMMTT), zu dem Zaehler und Tagesziel gehoeren. Meist heute -
// nach einem Neustart mit falscher Uhrzeit aber der zuletzt gemerkte.
int32_t schedule_day(void);

// Steht die Uhr hinter dem gemerkten Tag (und nach 2025)? Dann weiss sie
// nicht, ob sie jetzt falsch geht oder vorher falsch ging - und fragt das
// Telefon nach seiner Zeit (siehe prv_tag_pruefen in schedule.c).
bool schedule_uhr_fraglich(void);

// Das Telefon nennt seine Zeit (Sekunden seit 1970). Weicht die Uhr hoechstens
// 300 s davon ab und steht auf demselben Tag, geht sie richtig: dann war der
// gemerkte Tag der falsche, und heute gilt - Glaeser und Ziel bleiben.
// Rueckgabe: true, wenn der gemerkte Tag dadurch zurueckgesetzt wurde.
bool schedule_uhr_bestaetigt(uint32_t telefon);

// Abstand zweier Erinnerungen in Minuten, aus Tagesfenster und Soll.
int schedule_interval_min(void);

// Glasgroesse in Millilitern (DT_GLASS_ML_MIN..MAX, voreingestellt
// DT_GLASS_ML_DEFAULT). Kommt von der Konfigseite der Telefon-App.
int schedule_glass_ml(void);
bool schedule_set_glass_ml(int ml);

// Soll die Trink-Animation gezeigt werden? Kommt von der Konfigseite,
// voreingestellt an. Steht hier, weil hier schon die beiden anderen
// Einstellungen liegen, die den Tag ueberdauern - nicht, weil eine Animation
// etwas mit dem Trinkplan zu tun haette.
// Rueckgabe von set: true, wenn sich dadurch etwas geaendert hat.
bool schedule_animation(void);
bool schedule_set_animation(bool on);

// DER TAG ALS ANKER: der lokale Mittag des Tages, in dem `t` liegt. Ein Tag
// hat nicht immer 24 Stunden - an der Sommerzeit-Umstellung 23 oder 25. Bis
// 1.20 stand hier die Mitternacht als "jetzt minus Sekunden seit
// Mitternacht", Folgetage als +86400 und Uhrzeiten als Mitternacht +
// Minuten*60. Am Umstellungstag lagen Wecker, Plan-Liste und Pins dann eine
// Stunde daneben und nicht einmal untereinander gleich (Audit M2). Vom
// Mittag aus wechselt eine Stunde mehr oder weniger das Datum nicht:
// Folgetage sind schedule_tag(tag + n * 86400), zwei Zeitpunkte liegen am
// selben Tag, wenn ihr Anker gleich ist.
time_t schedule_tag(time_t t);

// Die Wanduhrzeit `minute` (0..1439) an dem Tag mit Anker `tag` - so, wie die
// Uhr sie zeigt, auch am Umstellungstag. Eine Zeit, die es an dem Tag nicht
// gibt (02:30 im Fruehling), wird um die ausgefallene Stunde spaeter.
time_t schedule_wandzeit(time_t tag, int minute);

// Zeitpunkt der Erinnerung `idx` (0..schedule_target()-1) an dem Tag mit
// Anker `tag` (schedule_tag).
time_t schedule_slot(time_t tag, int idx);

// Naechste Erinnerung nach `now` (heute oder morgen). Rueckgabe: Slot-Index.
// Ist das Tagesziel erreicht, gibt es heute keine mehr - dann der erste Slot
// von morgen, wie bei den Weckern.
int schedule_next(time_t now, time_t *when);

// Cookies der Wecker fuer Kaffees: SCHEDULE_COOKIE_COFFEE + Platz. Darunter
// liegen die Glaeser (0..DT_GLASSES_MAX-1) und das "Spaeter" fuers Wasser.
#define SCHEDULE_COOKIE_COFFEE 200
// Und eigene Getraenke mit Erinnerung: SCHEDULE_COOKIE_CUSTOM + Platz.
#define SCHEDULE_COOKIE_CUSTOM 300

// Alle Wakeups neu planen: optional zuerst ein Snooze-Wakeup mit `snooze_cookie`,
// dann die naechsten Termine aus Wasser und Kaffee bis zum Limit von 8.
// `snooze_until` = 0 fuer keins. Ist das Tagesziel erreicht, kommt fuer den
// Rest des Tages keine Wasser-Erinnerung mehr (auch kein "Spaeter" dazu);
// Kaffee und eigene Getraenke bleiben. schedule_set_count und
// schedule_raise_goal planen selbst neu, wenn sie das aendern.
void schedule_plan_wakeups(time_t snooze_until, int32_t snooze_cookie);

// Cookie fuer "Spaeter" beim Wasser.
int32_t schedule_snooze_cookie(void);

// "HH:MM" gemaess Uhr-Einstellung der Watch.
void schedule_format_time(time_t t, char *buf, size_t len);
