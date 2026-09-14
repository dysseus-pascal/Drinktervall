#pragma once

// Trinkplan: schedule_target() Erinnerungen gleichmaessig zwischen
// DT_START_HOUR und DT_END_HOUR. Wie viele es sind, waehlt man auf der
// Konfigseite der Telefon-App (src/pkjs/config.js); voreingestellt sind 8.
// Mit 8 Glaesern von 8 bis 20 Uhr ergibt das alle 90 Minuten das Grundraster
// 08:00, 09:30, 11:00, 12:30, 14:00, 15:30, 17:00, 18:30 - jeder Slot wird
// davon aber noch um bis zu DT_JITTER_MIN Minuten verschoben, diese Zeiten
// erscheinen also so gut wie nie genau so.
#define DT_START_HOUR    8
#define DT_END_HOUR      20

// Waehlbare Anzahl Glaeser. Die Grenzen ergeben sich aus dem Tagesfenster von
// zwoelf Stunden: bei 4 Glaesern liegen drei Stunden dazwischen, bei 16 noch
// 45 Minuten. Enger waere laestig statt hilfreich, weiter waere kein Plan mehr.
//
// DT_GLASSES_MAX darf nicht beliebig wachsen: phone.c schickt je Glas 5 Byte
// an das Telefon, und der Ausgangspuffer ist endlich (siehe phone_init).
// Pebble erlaubt ausserdem nur 8 geplante Wakeups pro App - das ist keine
// Obergrenze fuer die Glaeser, denn schedule_plan_wakeups plant immer nur die
// naechsten acht Slots und beim naechsten Start die darauf folgenden.
#define DT_GLASSES_DEFAULT  8
#define DT_GLASSES_MIN      4
#define DT_GLASSES_MAX     16

// Jede Erinnerung wird pro Tag und Slot deterministisch um bis zu so viele
// Minuten vor- oder nachverlegt, damit sie nicht immer exakt zur gleichen
// Zeit kommt. Bleibt innerhalb DT_START_HOUR..DT_END_HOUR.
#define DT_JITTER_MIN    10

// "Spaeter" im Erinnerungs-Screen verschiebt um so viele Minuten.
#define DT_SNOOZE_MIN    10

// Persist-Schluessel
#define DT_PERSIST_DAY     1   // Tag (JJJJMMTT), zu dem COUNT und GOAL gehoeren
#define DT_PERSIST_COUNT   2   // heute getrunkene Glaeser
#define DT_PERSIST_GOAL    3   // heutiges Tagesziel (Glaeser), morgen wieder das Soll
#define DT_PERSIST_TARGET  4   // gewaehltes Soll (Glaeser), gilt ueber Tage hinweg
#define DT_PERSIST_GLASS   5   // Glasgroesse in ml, gilt ueber Tage hinweg

// Wie viel in ein Glas geht. Waehlbar auf der Konfigseite; gebraucht wird die
// Zahl nur, um getrunkenes Wasser an eine Gesundheitsakte weiterzureichen -
// auf der Uhr selbst wird weiter in Glaesern gezaehlt, nicht in Millilitern.
#define DT_GLASS_ML_DEFAULT 300
#define DT_GLASS_ML_MIN     100
#define DT_GLASS_ML_MAX     1000

// Das Tagesziel laesst sich mit der unteren Taste bis hierher erhoehen.
// Muss mindestens DT_GLASSES_MAX sein, sonst liesse sich das Soll nicht halten.
#define DT_GOAL_MAX      24
