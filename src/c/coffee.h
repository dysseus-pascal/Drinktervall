#pragma once
#include <pebble.h>
#include "glass_fx.h"
#include "config.h"

// Kaffeezeiten: bis zu DT_COFFEE_MAX feste Uhrzeiten am Tag, je mit Sorte,
// koffeinfrei, Milch und Zucker. Eingestellt auf der Konfigseite, auf der Uhr
// gemerkt.
// Ohne Eintrag ist die Funktion aus - die App verhaelt sich dann wie zuvor.

// DIE NUMMERN SIND FEST: sie gehen als COFFEE_KIND ans Telefon, und die
// Companion-App rechnet daraus Koffein und kcal. Neue Sorten nur hinten an.
typedef enum {
  CoffeeEspresso = 0,
  CoffeeCoffee,
  // Bis 1.16 stand hier der Latte macchiato; seit 1.17 ist Platz 2 der Tee.
  CoffeeTea,
  CoffeeEnergyDrink,
  CoffeeKindCount,
} CoffeeKind;

// Ein eigenes Getraenk traegt diese Nummer als Sorte. Es steht nie im Plan,
// darum liegt es ausserhalb von CoffeeKindCount.
#define COFFEE_KIND_CUSTOM 4

#define COFFEE_MILK  0x01
#define COFFEE_SUGAR 0x02
// Entkoffeiniert: wird eingetragen wie die Sorte, zaehlt aber kein Koffein.
// Ein eigenes Bit statt eigener Sorten, damit Kaffee und koffeinfreier
// Kaffee dieselbe Animation und dieselben kcal behalten. Zusammen mit der
// Sorte gehen die Flags in vier Bits ans Telefon - 0x08 ist das letzte freie.
#define COFFEE_DECAF 0x04

typedef struct {
  uint16_t minute;   //< Minute des Tages, 0..1439
  uint8_t kind;      //< CoffeeKind
  uint8_t flags;     //< COFFEE_DECAF | COFFEE_MILK | COFFEE_SUGAR
} CoffeeSlot;

void coffee_init(void);
int coffee_count(void);
const CoffeeSlot *coffee_slot(int idx);

// Zeitpunkt des Kaffees `idx` an dem Tag mit Mitternacht `midnight`.
time_t coffee_time(time_t midnight, int idx);

// Der Plan als Bytes, wie er zwischen Uhr und Telefon geht: ein Byte Anzahl,
// dann je Kaffee Minute (little endian, 2 Byte), Sorte, Flags. Die Anzahl
// vorn, weil ein leeres Bytefeld nicht jede Telefonseite verschicken kann -
// "aus" ist so ein einzelnes Null-Byte.
#define COFFEE_BYTES_MAX (1 + DT_COFFEE_MAX * 4)
int coffee_to_bytes(uint8_t *buf);
// Rueckgabe: true, wenn sich der Plan dadurch geaendert hat.
bool coffee_from_bytes(const uint8_t *buf, int len);

// "Espresso", "Coffee, decaf, milk, sugar" - fuer die Erinnerung. Der Puffer
// soll COFFEE_DESCRIBE_MAX Byte haben: so viel braucht die laengste Sorte mit
// allen drei Zusaetzen in jeder Sprache. tools/strings_check.js liest die
// Zahl hier heraus und prueft das gegen strings_table.h.
#define COFFEE_DESCRIBE_MAX 64
void coffee_describe(const CoffeeSlot *slot, char *buf, size_t len);

// Milch gibt es zu allem ausser dem Energy-Drink.
bool coffee_milk_possible(uint8_t kind);

// Entkoffeiniert gibt es Espresso, Kaffee und Tee - einen Energy-Drink ohne
// Koffein nicht.
bool coffee_decaf_possible(uint8_t kind);

// Das Gefaess der Animation zu diesem Kaffee: mit Milch ist er heller.
Vessel coffee_vessel(const CoffeeSlot *slot);

// --- Eigene Getraenke ---
typedef struct {
  char name[DT_CUSTOM_NAME];
  uint16_t kcal;
  uint16_t mg;       //< Koffein
  int16_t minute;    //< Erinnerung, Minute des Tages; -1 = keine
} CustomDrink;

// Zeitpunkt der Erinnerung an das eigene Getraenk `idx`, 0 ohne Erinnerung.
time_t custom_time(time_t midnight, int idx);

int custom_count(void);
const CustomDrink *custom_drink(int idx);

// Als Text zwischen Uhr und Telefon: je Getraenk eine Zeile
// "Name|kcal|mg|Minute", Minute -1 ohne Erinnerung. Ohne viertes Feld (bis
// 1.16) gibt es keine Erinnerung.
// Rueckgabe von from: true, wenn sich etwas geaendert hat.
bool custom_from_string(const char *text);
void custom_to_string(char *buf, size_t len);
