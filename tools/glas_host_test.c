// Milch auf der Schwarz-Weiss-Uhr (glass_fx.c) - auf dem Rechner.
//
//   sh tools/glas_host_test.sh
//
// Zugesagt (README, Audit M12): auf flint ist jedes Gefaess MIT MILCH heller
// als ohne. Eine Farbe allein kann das dort nicht: die Firmware rundet jede
// Fuellfarbe auf vier Graustufen, und Dunkel- wie Hellgrau werden dasselbe
// Schachbrett, halb schwarz. Milch traegt darum ein eigenes, lichteres Raster
// - ein Viertel schwarz. Geprueft wird der Anteil Schwarz in der Fuellung:
//
//   - ohne Milch ein Grau (45 bis 55 % schwarz): weder eine leere Tasse noch
//     ein schwarzer Klotz, in dem das Gesicht verschwaende,
//   - mit Milch das lichtere Raster (20 bis 30 % schwarz),
//   - mit Milch mindestens 15 Prozentpunkte heller als ohne,
// fuer Espresso, Kaffee und Tee, in den Breiten, in denen flint sie gross
// zeigt: 80 (Trink-Animation, 56 % von 144) und 28 (Kopf der
// Kaffee-Erinnerung). In der Getraenkeauswahl (22) ist die Fuellung des
// Espresso nur etwa ein Dutzend Pixel gross - ein Anteil sagt dort nichts.
// Dort wird nur geprueft, dass Milch das Raster traegt und kein Grau, und ohne
// Milch umgekehrt.
//
// Gezeichnet wird mit tools/host/attrappe_grafik.c, die die Fuellmuster der
// Firmware nachbildet. Dass das Ergebnis dem der Uhr gleicht, zeigen die
// Emulator-Bilder screenshots/flint/09 und 10. Mit GLAS_BILD=<ordner> legt der
// Test seine Bilder als PGM ab.
//
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include "glass_fx.h"

time_t stub_jetzt;

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

// Ein Pixel der Fuellung: aus einer grauen Fuellung oder ein Punkt des Rasters.
static bool prv_fuellpixel(int x, int y) {
  return (attrappe_bild.herkunft[y][x] == HerkunftFuellung && attrappe_bild.grau[y][x]) ||
         attrappe_bild.herkunft[y][x] == HerkunftPunkt;
}

typedef struct {
  int schwarz;
  int gesamt;
} Anteil;

// Der Anteil Schwarz in der Fuellung. Je Zeile zaehlt alles zwischen dem
// ersten und dem letzten Fuellpixel - auch die weissen Luecken des Rasters;
// Zeilen des Rasters ohne Punkte nehmen die Grenzen ihrer Nachbarzeilen.
// Striche (Rahmen, Gesicht, Beutelschnur) zaehlen nicht mit: sie sind mit und
// ohne Milch dieselben.
static Anteil prv_anteil(void) {
  int lo[ATTRAPPE_BILD_H], hi[ATTRAPPE_BILD_H];
  int erste = -1, letzte = -1;
  for (int y = 0; y < ATTRAPPE_BILD_H; y++) {
    lo[y] = ATTRAPPE_BILD_B;
    hi[y] = -1;
    for (int x = 0; x < ATTRAPPE_BILD_B; x++) {
      if (!prv_fuellpixel(x, y)) continue;
      if (x < lo[y]) lo[y] = x;
      if (x > hi[y]) hi[y] = x;
    }
    if (hi[y] >= 0) {
      if (erste < 0) erste = y;
      letzte = y;
    }
  }
  Anteil a = { 0, 0 };
  for (int y = erste; erste >= 0 && y <= letzte; y++) {
    int l = lo[y], h = hi[y];
    if (h < 0) {
      // Eine Rasterzeile ohne Punkte: die Grenzen der Nachbarn.
      if (y > 0 && hi[y - 1] >= 0) { l = lo[y - 1]; h = hi[y - 1]; }
      if (y + 1 < ATTRAPPE_BILD_H && hi[y + 1] >= 0) {
        if (lo[y + 1] < l) l = lo[y + 1];
        if (hi[y + 1] > h) h = hi[y + 1];
      }
    }
    for (int x = l; x <= h; x++) {
      if (attrappe_bild.herkunft[y][x] == HerkunftStrich) continue;
      a.gesamt++;
      if (!attrappe_bild.weiss[y][x]) a.schwarz++;
    }
  }
  return a;
}

// Wie viele Pixel eine graue Fuellung bzw. ein Rasterpunkt gesetzt hat.
typedef struct {
  int grau;
  int punkte;
} Zaehlung;

static Zaehlung prv_zaehle(void) {
  Zaehlung z = { 0, 0 };
  for (int y = 0; y < ATTRAPPE_BILD_H; y++) {
    for (int x = 0; x < ATTRAPPE_BILD_B; x++) {
      if (attrappe_bild.herkunft[y][x] == HerkunftFuellung && attrappe_bild.grau[y][x]) z.grau++;
      if (attrappe_bild.herkunft[y][x] == HerkunftPunkt) z.punkte++;
    }
  }
  return z;
}

static Anteil prv_zeichne(Vessel gefaess, int breite, const char *name) {
  attrappe_bild_leeren();
  glass_fx_draw_vessel_still(attrappe_kontext(), GPoint(ATTRAPPE_BILD_B / 2, ATTRAPPE_BILD_H / 2),
                             (int16_t)breite, gefaess);
  const char *ordner = getenv("GLAS_BILD");
  if (ordner) {
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s-%d.pgm", ordner, name, breite);
    attrappe_bild_speichern(pfad);
  }
  return prv_anteil();
}

int main(void) {
  static const struct {
    Vessel ohne, mit;
    const char *name;
  } paare[] = {
    { VesselEspresso, VesselEspressoMilk, "espresso" },
    { VesselCoffee, VesselCoffeeMilk, "kaffee" },
    { VesselTea, VesselTeaMilk, "tee" },
  };
  static const int breiten[] = { 80, 28 };
  for (unsigned b = 0; b < sizeof(breiten) / sizeof(breiten[0]); b++) {
    printf("\nBreite %d\n", breiten[b]);
    for (unsigned i = 0; i < sizeof(paare) / sizeof(paare[0]); i++) {
      char was[160], name[40];
      snprintf(name, sizeof(name), "%s", paare[i].name);
      const Anteil ohne = prv_zeichne(paare[i].ohne, breiten[b], name);
      const int fehler_ohne = attrappe_grafik_fehler();
      snprintf(name, sizeof(name), "%s-milch", paare[i].name);
      const Anteil mit = prv_zeichne(paare[i].mit, breiten[b], name);
      const int fehler_mit = attrappe_grafik_fehler();
      const int p_ohne = ohne.gesamt ? 100 * ohne.schwarz / ohne.gesamt : -1;
      const int p_mit = mit.gesamt ? 100 * mit.schwarz / mit.gesamt : -1;
      snprintf(was, sizeof(was), "%s: nur Fuellfarben, die flint kennt", paare[i].name);
      pruefe(was, fehler_ohne == 0 && fehler_mit == 0);
      snprintf(was, sizeof(was), "%s ohne Milch: Grau, %d %% schwarz von %d Pixeln (45..55)",
               paare[i].name, p_ohne, ohne.gesamt);
      pruefe(was, ohne.gesamt >= 20 && p_ohne >= 45 && p_ohne <= 55);
      snprintf(was, sizeof(was), "%s mit Milch: lichtes Raster, %d %% schwarz von %d Pixeln (20..30)",
               paare[i].name, p_mit, mit.gesamt);
      pruefe(was, mit.gesamt >= 20 && p_mit >= 20 && p_mit <= 30);
      snprintf(was, sizeof(was), "%s: mit Milch %d Prozentpunkte heller (mindestens 15)",
               paare[i].name, p_ohne - p_mit);
      pruefe(was, p_ohne - p_mit >= 15);
    }
  }

  printf("\nBreite 22 (Getraenkeauswahl): Milch ist Raster, ohne Milch Grau\n");
  for (unsigned i = 0; i < sizeof(paare) / sizeof(paare[0]); i++) {
    char was[160], name[40];
    snprintf(name, sizeof(name), "%s", paare[i].name);
    prv_zeichne(paare[i].ohne, 22, name);
    const Zaehlung ohne = prv_zaehle();
    snprintf(name, sizeof(name), "%s-milch", paare[i].name);
    prv_zeichne(paare[i].mit, 22, name);
    const Zaehlung mit = prv_zaehle();
    snprintf(was, sizeof(was), "%s ohne Milch: %d Pixel Grau, %d Rasterpunkte", paare[i].name,
             ohne.grau, ohne.punkte);
    pruefe(was, ohne.grau > 0 && ohne.punkte == 0);
    snprintf(was, sizeof(was), "%s mit Milch: %d Pixel Grau, %d Rasterpunkte", paare[i].name,
             mit.grau, mit.punkte);
    pruefe(was, mit.grau == 0 && mit.punkte > 0);
  }
  printf("%s\n", s_fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return s_fehler ? 1 : 0;
}
