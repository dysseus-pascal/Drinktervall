#pragma once

// Trinkplan: AT_GLASSES Erinnerungen, gleichmaessig ab AT_START_HOUR bis
// AT_END_HOUR. Mit 8 Glaesern von 8 bis 20 Uhr ergibt das alle 90 Minuten:
// 08:00, 09:30, 11:00, 12:30, 14:00, 15:30, 17:00, 18:30.
//
// AT_GLASSES darf 8 nicht ueberschreiten: Pebble erlaubt pro App hoechstens
// 8 geplante Wakeup-Events (siehe schedule.c).
#define AT_START_HOUR    8
#define AT_END_HOUR      20
#define AT_GLASSES       8
#define AT_INTERVAL_MIN  (((AT_END_HOUR - AT_START_HOUR) * 60) / AT_GLASSES)

// Jede Erinnerung wird pro Tag und Slot deterministisch um bis zu so viele
// Minuten vor- oder nachverlegt, damit sie nicht immer exakt zur gleichen
// Zeit kommt. Bleibt innerhalb AT_START_HOUR..AT_END_HOUR.
#define AT_JITTER_MIN    10

// "Spaeter" im Erinnerungs-Screen verschiebt um so viele Minuten.
#define AT_SNOOZE_MIN    10

// Erinnerungs-Screen schliesst sich nach so vielen Sekunden von selbst.
#define AT_REMINDER_TIMEOUT_S  60

// Persist-Schluessel
#define AT_PERSIST_DAY    1   // Tag (JJJJMMTT), zu dem AT_PERSIST_COUNT gehoert
#define AT_PERSIST_COUNT  2   // heute getrunkene Glaeser
