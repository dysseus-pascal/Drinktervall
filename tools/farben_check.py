#!/usr/bin/env python3
# Die Getraenkefarben, wie die Uhr sie zeigt (glass_fx.c, FX_*).
#
#   sh tools/farben_check.sh
#
# Zugesagt ist: MIT MILCH IST JEDES GEFAESS HELLER, KEINE ZWEI DER SECHS
# FARBEN SIND GLEICH, UND JEDE HEBT SICH VOM WEISSEN INNERN DES GEFAESSES AB.
# Die Palette allein sagt das nicht: das Farbdisplay ist blass, und
# `pebble screenshot` rechnet die 64 Farben in das um, was man auf der Uhr
# sieht (pebble_tool, ScreenshotCommand._correct_colours). Bis 1.19 waren Tee
# (#FFAA00) und Tee mit Milch (#FFAA55) dort praktisch gleich (Audit M12). Und
# ein blasses Gelb wie PastelYellow (#FFFFAA) hebt sich kaum vom Weiss der
# Tasse ab, in das es fliesst (Delta E 18).
#
# Geprueft wird nach der Umrechnung, in CIELAB:
#   - je Getraenk: mit Milch mindestens MILCH_HELLER L* heller,
#   - je Paar der sechs: mindestens ABSTAND_MIN Delta E auseinander,
#   - jede der sechs: mindestens ABSTAND_MIN Delta E vom Weiss der Tasse.
# Die Farbwerte kommen aus der SDK (gcolor_definitions.h), nicht aus einer
# eigenen Liste.
#
# SCHWARZ-WEISS (flint): dort rundet die Firmware jede Fuellfarbe auf
# Schwarz, Dunkelgrau, Hellgrau oder Weiss (pebbleos, graphics.c
# graphics_context_set_fill_color -> gtypes.c gcolor_get_grayscale; Grenzen
# und Formel stehen hier MIT IHREN WERTEN). Geprueft: die Fuellung ohne Milch
# wird ein Grau - Weiss waere eine leere Tasse, Schwarz verschluckte das
# Gesicht. Milch traegt dort ein eigenes Raster; das prueft
# tools/glas_host_test.sh.
#
# Laeuft mit dem Python des pebble-Werkzeugs (fuer die Umrechnungstabelle).
# Exitcode 0 = alles wie zugesagt.
import glob
import math
import os
import re
import sys

from pebble_tool.commands.screenshot import ScreenshotCommand

MILCH_HELLER = 10.0   # L*: deutlich, nicht nur messbar heller
ABSTAND_MIN = 20.0    # Delta E: auf einen Blick verschieden

# gtypes.c gcolor_get_grayscale: Leuchtdichte auf 10000, Grenzen 3333/6666.
DUNKELGRAU_AB = 3333
HELLGRAU_BIS = 6666

HIER = os.path.dirname(os.path.abspath(__file__))
QUELLE = os.environ.get("DT_GLASS_FX", os.path.join(HIER, "..", "src", "c", "glass_fx.c"))


def sdk_kopf():
    orte = []
    for basis in (os.environ.get("XDG_DATA_HOME"), os.path.expanduser("~/.local/share")):
        if basis:
            orte += glob.glob(os.path.join(basis, "pebble-sdk", "SDKs", "*", "sdk-core", "pebble",
                                           "emery", "include", "gcolor_definitions.h"))
    if not orte:
        sys.exit("gcolor_definitions.h nicht gefunden - ist die Pebble-SDK installiert?")
    return sorted(orte)[-1]


def palette():
    """Name -> (r, g, b) in 0..255."""
    farben = {}
    for name, bits in re.findall(r"#define (GColor\w+)ARGB8\s+\(\(uint8_t\)0b([01]{8})\)", open(sdk_kopf()).read()):
        v = int(bits, 2)
        farben[name] = tuple(((v >> s) & 3) * 85 for s in (4, 2, 0))
    return farben


def auf_der_uhr(rgb):
    return tuple(ScreenshotCommand._correct_colours(None, [list(rgb)])[0])


def lab(rgb):
    def lin(u):
        u /= 255
        return u / 12.92 if u <= 0.04045 else ((u + 0.055) / 1.055) ** 2.4
    r, g, b = map(lin, rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    return (116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z)))


def graustufe(rgb):
    """Worauf die Schwarz-Weiss-Uhr eine Fuellfarbe rundet."""
    r, g, b = (c // 85 for c in rgb)
    l = (2126 * r + 7152 * g + 722 * b) // 3
    if l < DUNKELGRAU_AB:
        return "Schwarz"
    if l < (HELLGRAU_BIS + DUNKELGRAU_AB) // 2:
        return "Dunkelgrau"
    if l <= HELLGRAU_BIS:
        return "Hellgrau"
    return "Weiss"


def main():
    pal = palette()
    text = open(QUELLE).read()
    paare_def = re.findall(r"#define (FX_\w+)\s+PBL_IF_COLOR_ELSE\((GColor\w+),\s*(\w+)\)", text)
    farbe = {k: f for k, f, _ in paare_def}
    sw = {k: s for k, _, s in paare_def}
    paare = [("Espresso", "FX_ESPRESSO", "FX_ESPRESSO_MILK"),
             ("Kaffee", "FX_COFFEE", "FX_MILKCOFFEE"),
             ("Tee", "FX_TEA", "FX_TEA_MILK")]
    fehler = 0
    werte = {}
    for _, ohne, mit in paare:
        for k in (ohne, mit):
            if k not in farbe or farbe[k] not in pal or sw.get(k) not in pal:
                print("  FEHLER %s fehlt oder ist keine SDK-Farbe" % k)
                return 1
            werte[k] = lab(auf_der_uhr(pal[farbe[k]]))

    print("Farbe (emery, gabbro)")
    for name, ohne, mit in paare:
        dl = werte[mit][0] - werte[ohne][0]
        ok = dl >= MILCH_HELLER
        fehler += not ok
        print("%s %s mit Milch %+.1f L* (%s -> %s)" % ("  ok    " if ok else "  FEHLER", name, dl, farbe[ohne], farbe[mit]))
    namen = [k for _, a, b in paare for k in (a, b)]
    abst = sorted((math.dist(werte[a], werte[b]), a, b) for i, a in enumerate(namen) for b in namen[i + 1:])
    d, a, b = abst[0]
    ok = d >= ABSTAND_MIN
    fehler += not ok
    print("%s naechstes Paar %s/%s: Delta E %.1f (mindestens %.0f)" % ("  ok    " if ok else "  FEHLER", a, b, d, ABSTAND_MIN))
    weiss = lab(auf_der_uhr(pal["GColorWhite"]))
    d, k = min((math.dist(werte[k], weiss), k) for k in namen)
    ok = d >= ABSTAND_MIN
    fehler += not ok
    print("%s am naechsten am Weiss der Tasse: %s, Delta E %.1f (mindestens %.0f)" % (
        "  ok    " if ok else "  FEHLER", k, d, ABSTAND_MIN))

    print("Schwarz-Weiss (flint)")
    for name, ohne, _ in paare:
        stufe = graustufe(pal[sw[ohne]])
        ok = stufe in ("Dunkelgrau", "Hellgrau")
        fehler += not ok
        print("%s %s ohne Milch: %s wird %s" % ("  ok    " if ok else "  FEHLER", name, sw[ohne], stufe))
    print("Fehler: %d" % fehler)
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
