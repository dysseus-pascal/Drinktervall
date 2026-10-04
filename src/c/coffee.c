#include "coffee.h"
#include "config.h"
#include "strings.h"

static CoffeeSlot s_slots[DT_COFFEE_MAX];
static int s_count;

// Ein Plan aus dem Persist oder vom Telefon wird geprueft, bevor er gilt: eine
// Minute ueber Mitternacht oder eine unbekannte Sorte haette einen Wecker zur
// falschen Zeit oder einen leeren Text zur Folge.
static int prv_parse(const uint8_t *buf, int len, CoffeeSlot *out) {
  if (len < 1) return -1;
  const int n = buf[0];
  if (n > DT_COFFEE_MAX || len < 1 + n * 4) return -1;
  for (int i = 0; i < n; i++) {
    const uint8_t *b = buf + 1 + i * 4;
    out[i].minute = (uint16_t)(b[0] | (b[1] << 8));
    out[i].kind = b[2];
    out[i].flags = b[3] & (COFFEE_MILK | COFFEE_SUGAR | COFFEE_DECAF);
    if (out[i].minute >= 24 * 60 || out[i].kind >= CoffeeKindCount) return -1;
  }
  return n;
}

static CustomDrink s_custom[DT_CUSTOM_MAX];
static int s_custom_count;

// Die eigenen Getraenke von 1.16, noch ohne Erinnerung.
typedef struct {
  char name[DT_CUSTOM_NAME];
  uint16_t kcal;
  uint16_t mg;
} CustomAlt;

static void prv_custom_load(void) {
  s_custom_count = 0;
  if (persist_exists(DT_PERSIST_CUSTOM2)) {
    const int n = persist_read_data(DT_PERSIST_CUSTOM2, s_custom, sizeof(s_custom));
    if (n > 0) s_custom_count = n / (int)sizeof(CustomDrink);
  } else if (persist_exists(DT_PERSIST_CUSTOM)) {
    // Was 1.16 gespeichert hat, uebernehmen - ohne Erinnerung.
    CustomAlt alt[DT_CUSTOM_MAX];
    const int n = persist_read_data(DT_PERSIST_CUSTOM, alt, sizeof(alt));
    for (int i = 0; n > 0 && i < n / (int)sizeof(CustomAlt); i++) {
      memcpy(s_custom[i].name, alt[i].name, DT_CUSTOM_NAME);
      s_custom[i].kcal = alt[i].kcal;
      s_custom[i].mg = alt[i].mg;
      s_custom[i].minute = -1;
      s_custom_count++;
    }
  }
  for (int i = 0; i < s_custom_count; i++) {
    s_custom[i].name[DT_CUSTOM_NAME - 1] = 0;
    if (s_custom[i].minute >= 24 * 60) s_custom[i].minute = -1;
  }
}

time_t custom_time(time_t midnight, int idx) {
  const CustomDrink *d = custom_drink(idx);
  return (d && d->minute >= 0) ? midnight + (time_t)d->minute * 60 : 0;
}

int custom_count(void) {
  return s_custom_count;
}

const CustomDrink *custom_drink(int idx) {
  return (idx >= 0 && idx < s_custom_count) ? &s_custom[idx] : NULL;
}

// Zeilen "Name|kcal|mg". Ohne Namen zaehlt eine Zeile nicht - auf der Uhr
// waere sie eine leere Zeile in der Liste.
bool custom_from_string(const char *text) {
  CustomDrink neu[DT_CUSTOM_MAX];
  memset(neu, 0, sizeof(neu));
  int n = 0;
  const char *p = text;
  while (*p && n < DT_CUSTOM_MAX) {
    const char *ende = strchr(p, '\n');
    if (!ende) ende = p + strlen(p);
    const char *a = memchr(p, '|', ende - p);
    const char *b = a ? memchr(a + 1, '|', ende - a - 1) : NULL;
    const char *c = b ? memchr(b + 1, '|', ende - b - 1) : NULL;
    if (a && b && a > p) {
      size_t l = (size_t)(a - p);
      if (l > DT_CUSTOM_NAME - 1) {
        l = DT_CUSTOM_NAME - 1;
        // NIE MITTEN IM ZEICHEN: Folgebytes (10xxxxxx) gehoeren zum Zeichen
        // davor, das dann ganz wegfaellt. Ein halbes "ü" stuende sonst als
        // kaputtes UTF-8 auf der Uhr und in der Gesundheitsakte.
        while (l > 0 && ((uint8_t)p[l] & 0xC0) == 0x80) l--;
      }
      memcpy(neu[n].name, p, l);
      neu[n].kcal = (uint16_t)atoi(a + 1);
      neu[n].mg = (uint16_t)atoi(b + 1);
      const int minute = c ? atoi(c + 1) : -1;
      neu[n].minute = (int16_t)((minute >= 0 && minute < 24 * 60) ? minute : -1);
      n++;
    }
    p = *ende ? ende + 1 : ende;
  }
  if (n == s_custom_count && memcmp(neu, s_custom, n * sizeof(CustomDrink)) == 0) return false;
  memcpy(s_custom, neu, sizeof(neu));
  s_custom_count = n;
  if (n) persist_write_data(DT_PERSIST_CUSTOM2, s_custom, n * sizeof(CustomDrink));
  else persist_delete(DT_PERSIST_CUSTOM2);
  persist_delete(DT_PERSIST_CUSTOM);
  return true;
}

bool custom_to_string(char *buf, size_t len) {
  if (len == 0) return false;
  buf[0] = 0;
  for (int i = 0; i < s_custom_count; i++) {
    const size_t l = strlen(buf);
    const int n = snprintf(buf + l, len - l, "%s%s|%d|%d|%d", i ? "\n" : "", s_custom[i].name,
                           (int)s_custom[i].kcal, (int)s_custom[i].mg, (int)s_custom[i].minute);
    if (n < 0 || (size_t)n >= len - l) return false;
  }
  return true;
}

void coffee_init(void) {
  prv_custom_load();
  s_count = 0;
  if (!persist_exists(DT_PERSIST_COFFEE)) return;
  uint8_t buf[COFFEE_BYTES_MAX];
  const int len = persist_read_data(DT_PERSIST_COFFEE, buf, sizeof(buf));
  const int n = prv_parse(buf, len, s_slots);
  s_count = n > 0 ? n : 0;
}

int coffee_count(void) {
  return s_count;
}

const CoffeeSlot *coffee_slot(int idx) {
  return (idx >= 0 && idx < s_count) ? &s_slots[idx] : NULL;
}

time_t coffee_time(time_t midnight, int idx) {
  const CoffeeSlot *s = coffee_slot(idx);
  return s ? midnight + (time_t)s->minute * 60 : 0;
}

int coffee_to_bytes(uint8_t *buf) {
  buf[0] = (uint8_t)s_count;
  for (int i = 0; i < s_count; i++) {
    uint8_t *b = buf + 1 + i * 4;
    b[0] = (uint8_t)(s_slots[i].minute & 0xFF);
    b[1] = (uint8_t)(s_slots[i].minute >> 8);
    b[2] = s_slots[i].kind;
    b[3] = s_slots[i].flags;
  }
  return 1 + s_count * 4;
}

bool coffee_from_bytes(const uint8_t *buf, int len) {
  CoffeeSlot neu[DT_COFFEE_MAX];
  const int n = prv_parse(buf, len, neu);
  if (n < 0) return false;
  if (n == s_count && memcmp(neu, s_slots, n * sizeof(CoffeeSlot)) == 0) return false;
  memcpy(s_slots, neu, n * sizeof(CoffeeSlot));
  s_count = n;
  uint8_t out[COFFEE_BYTES_MAX];
  persist_write_data(DT_PERSIST_COFFEE, out, coffee_to_bytes(out));
  return true;
}

void coffee_describe(const CoffeeSlot *slot, char *buf, size_t len) {
  static const StringId names[CoffeeKindCount] = {
    STR_ESPRESSO, STR_COFFEE, STR_TEA, STR_ENERGY_DRINK,
  };
  snprintf(buf, len, "%s", S(names[slot->kind]));
  if (coffee_decaf_possible(slot->kind) && (slot->flags & COFFEE_DECAF)) {
    const size_t l = strlen(buf);
    snprintf(buf + l, len - l, ", %s", S(STR_DECAF));
  }
  // Milch zu allem ausser dem Energy-Drink.
  if (coffee_milk_possible(slot->kind) && (slot->flags & COFFEE_MILK)) {
    const size_t l = strlen(buf);
    snprintf(buf + l, len - l, ", %s", S(STR_MILK));
  }
  if (slot->flags & COFFEE_SUGAR) {
    const size_t l = strlen(buf);
    snprintf(buf + l, len - l, ", %s", S(STR_SUGAR));
  }
}

bool coffee_milk_possible(uint8_t kind) {
  return kind == CoffeeEspresso || kind == CoffeeCoffee || kind == CoffeeTea;
}

bool coffee_decaf_possible(uint8_t kind) {
  return kind == CoffeeEspresso || kind == CoffeeCoffee || kind == CoffeeTea;
}

Vessel coffee_vessel(const CoffeeSlot *slot) {
  const bool milch = (slot->flags & COFFEE_MILK) != 0;
  switch (slot->kind) {
    case COFFEE_KIND_CUSTOM: return VesselCustom;
    case CoffeeEspresso: return milch ? VesselEspressoMilk : VesselEspresso;
    case CoffeeTea: return milch ? VesselTeaMilk : VesselTea;
    case CoffeeEnergyDrink: return VesselCan;
    default: return milch ? VesselCoffeeMilk : VesselCoffee;
  }
}
