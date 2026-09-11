#pragma once

// Trinkplan: DT_GLASSES Erinnerungen, gleichmaessig ab DT_START_HOUR bis
// DT_END_HOUR. Mit 8 Glaesern von 8 bis 20 Uhr ergibt das alle 90 Minuten:
// 08:00, 09:30, 11:00, 12:30, 14:00, 15:30, 17:00, 18:30.
//
// DT_GLASSES darf 8 nicht ueberschreiten: Pebble erlaubt pro App hoechstens
// 8 geplante Wakeup-Events (siehe schedule.c).
#define DT_START_HOUR    8
#define DT_END_HOUR      20
#define DT_GLASSES       8
#define DT_INTERVAL_MIN  (((DT_END_HOUR - DT_START_HOUR) * 60) / DT_GLASSES)

// Jede Erinnerung wird pro Tag und Slot deterministisch um bis zu so viele
// Minuten vor- oder nachverlegt, damit sie nicht immer exakt zur gleichen
// Zeit kommt. Bleibt innerhalb DT_START_HOUR..DT_END_HOUR.
#define DT_JITTER_MIN    10

// "Spaeter" im Erinnerungs-Screen verschiebt um so viele Minuten.
#define DT_SNOOZE_MIN    10

// Persist-Schluessel
#define DT_PERSIST_DAY    1   // Tag (JJJJMMTT), zu dem DT_PERSIST_COUNT gehoert
#define DT_PERSIST_COUNT  2   // heute getrunkene Glaeser
#define DT_PERSIST_GOAL   3   // heutiges Tagesziel (Glaeser), morgen wieder DT_GLASSES

// Das Tagesziel laesst sich mit der unteren Taste bis hierher erhoehen.
#define DT_GOAL_MAX      24
