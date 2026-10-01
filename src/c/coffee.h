#pragma once
#include <pebble.h>
#include "glass_fx.h"

// Kaffeezeiten: bis zu DT_COFFEE_MAX feste Uhrzeiten am Tag, je mit Sorte,
// Milch und Zucker. Eingestellt auf der Konfigseite, auf der Uhr gemerkt.
// Ohne Eintrag ist die Funktion aus - die App verhaelt sich dann wie zuvor.

// DIE NUMMERN SIND FEST: sie gehen als COFFEE_KIND ans Telefon, und die
// Companion-App rechnet daraus Koffein und kcal. Neue Sorten nur hinten an.
typedef enum {
  CoffeeEspresso = 0,
  CoffeeCoffee,
  CoffeeLatteMacchiato,
  CoffeeEnergyDrink,
  CoffeeKindCount,
} CoffeeKind;

#define COFFEE_MILK  0x01
#define COFFEE_SUGAR 0x02

typedef struct {
  uint16_t minute;   //< Minute des Tages, 0..1439
  uint8_t kind;      //< CoffeeKind
  uint8_t flags;     //< COFFEE_MILK | COFFEE_SUGAR
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

// "Espresso", "Coffee, milk, sugar" - fuer die Erinnerung.
void coffee_describe(const CoffeeSlot *slot, char *buf, size_t len);

// Das Gefaess der Animation zu diesem Kaffee: Kaffee mit Milch ist heller.
Vessel coffee_vessel(const CoffeeSlot *slot);
