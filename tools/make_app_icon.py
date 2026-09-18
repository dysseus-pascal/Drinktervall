#!/usr/bin/env python3
"""App-Symbol: das Glas.

Aufruf: make_app_icon.py <zielordner>          -> system_icon.png (25x25)
        make_app_icon.py --store <zielordner>  -> icon-144.png, icon-48.png

Das Uhr-Symbol ist eine schwarze Linie auf durchsichtigem Grund. Die alte
Fassung war blau gefuellt; der Starter zeichnet Symbole ohnehin einfarbig, und
aus dem Blau wurde dort ein grauer Fleck. Damit fielen auch die beiden
Blautoene zusammen, an denen man Glas und Wasserstand unterschied.

Der Wasserstand steht als eigene Linie quer im Glas. Sie gehoert dazu: ohne sie
waere es ein leeres Gefaess und kein Trinkglas.

MASSSTAB IST DAS SYSTEMSYMBOL. Die Uhr-Kachel von "Watchfaces" im Starter wurde
Punkt fuer Punkt nachgemessen: 24 von 25 Punkten hoch, Linien 2 bis 3 Punkte
stark, rund 180 schwarze Punkte. Danach richten sich Groesse und Strichstaerke
hier - eine duennere Linie sieht daneben aus wie ein Versehen.

KEINE ~bw-FASSUNG: eine schwarze Linie ist auf jeder Uhr dieselbe Datei.

DIE STORE-SYMBOLE SIND ETWAS ANDERES ALS DAS UHR-SYMBOL, und das ist kein
Versehen des Stores, sondern seine Bauart: er nimmt nichts aus der .pbw. Im
Entwicklerportal liegen zwei eigene Bilder, `icon_large` und `icon_small`, und
er fordert sie in festen Massen an (gross 80 und 144, klein 28 und 48) - jeweils
mit `exact` in der Adresse, also erzwungen statt eingepasst. Wer etwas
Nicht-Quadratisches hochlaedt, bekommt es verzogen zurueck.

Darum eine gefuellte Kachel statt einer freistehenden Linie: der Store legt das
grosse Symbol fuer sein Teilen-Bild durch eine abgerundete Maske. Ueber einer
durchsichtigen Strichzeichnung taete diese Maske nichts, und auf hellem Grund
verschwaende die schwarze Linie.

DIE FORM STEHT NUR EINMAL DA. Alle Masse gelten auf einem Raster von 25
Punkten und werden mit S hochgerechnet; mit S = 1 ist das Ergebnis Punkt fuer
Punkt das alte. Zwei Beschreibungen derselben Form waeren eine zu viel - eine
davon wuerde man beim naechsten Mal vergessen.
"""
import os
import struct
import sys
import zlib

RASTER = 25                      # Bezugsraster, auf dem alle Masse gelten
SS = 4                           # Ueberabtastung je Achse
LINE = 2                         # Strichstaerke in Punkten, wie beim Vorbild

CX = 12.0                        # Mittelachse
TOP, BOT = 1.0, 23.5             # Ober- und Unterkante
HW_TOP, HW_BOT = 8.5, 5.5        # halbe Breite oben und unten
LEVEL = 11.5                     # Hoehe des Wasserstands

# Store-Kachel: Grund und Linien. Die Werte stammen aus src/c/theme.h
# (DT_COLOR_SIDEBAR bzw. DT_COLOR_FX_WATER), nachgeschlagen in
# gcolor_definitions.h des SDK - nicht aus dem Gedaechtnis.
GRUND = (0x00, 0x00, 0xAA)       # GColorDukeBlue
STRICH = (0xFF, 0xFF, 0xFF)      # weiss
WASSER = (0x55, 0xAA, 0xFF)      # GColorPictonBlue
# Wie viel der Kachel das Glas einnimmt. Randlos gesetzt wirkt ein Symbol
# gedraengt, und die abgerundete Maske des Stores schnitte die Ecken an.
FUELL = 0.72
STORE_GROESSEN = (144, 48)


def png(path, w, h, rows):
    """Minimaler PNG-Schreiber, 8 Bit RGBA, ohne Fremdbibliothek."""
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def raster(test, n):
    """Vierfach ueberabtasten, bei halber Deckung schneiden. Harte Kanten."""
    grid = []
    for py in range(n):
        row = []
        for px in range(n):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    if test(px + (sx + 0.5) / SS, py + (sy + 0.5) / SS):
                        hits += 1
            row.append(hits * 2 >= SS * SS)
        grid.append(row)
    return grid


def erode(grid, k, n):
    """k-mal den Rand abtragen."""
    cur = grid
    for _ in range(k):
        nxt = []
        for y in range(n):
            row = []
            for x in range(n):
                keep = cur[y][x]
                if keep:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if nx < 0 or ny < 0 or nx >= n or ny >= n or not cur[ny][nx]:
                            keep = False
                            break
                row.append(keep)
            nxt.append(row)
        cur = nxt
    return cur


def ring(grid, thick, n):
    """Der Rand der Form, thick Punkte stark.

    Gerechnet als Flaeche minus abgetragener Flaeche - so ist die Linie ueberall
    gleich stark, auch in flachen Winkeln, wo ein Nachbarschaftstest duenner
    wuerde.
    """
    inner = erode(grid, thick, n)
    return [[grid[y][x] and not inner[y][x] for x in range(n)] for y in range(n)]


def glas(n):
    """Kontur und Wasserlinie auf einem Raster von n Punkten.

    Rueckgabe: (kontur, wasser) als zwei Punktfelder. Getrennt, weil die
    Store-Kachel sie verschieden einfaerbt - auf der Uhr sind beide schwarz.
    """
    s = n / float(RASTER)
    cx, top, bot = CX * s, TOP * s, BOT * s
    hw_top, hw_bot = HW_TOP * s, HW_BOT * s
    dicke = max(1, int(round(LINE * s)))

    def inside(x, y):
        """Das Glas: ein Trapez, oben breiter als unten."""
        if y < top or y > bot:
            return False
        t = (y - top) / (bot - top)
        hw = hw_top + (hw_bot - hw_top) * t
        return abs(x - cx) <= hw

    voll = raster(inside, n)
    kontur = ring(voll, dicke, n)

    # Wasserstand: quer durch das Glas, so stark wie die Kontur.
    wasser = [[False] * n for _ in range(n)]
    mid = int(round(LEVEL * s))
    for dy in range(dicke):
        y = mid - dicke // 2 + dy
        if 0 <= y < n:
            for x in range(n):
                if voll[y][x]:
                    wasser[y][x] = True
    return kontur, wasser


def schreibe_uhr(dest):
    kontur, wasser = glas(RASTER)
    n = RASTER
    rows = []
    for y in range(n):
        r = []
        for x in range(n):
            gesetzt = kontur[y][x] or wasser[y][x]
            r += [0, 0, 0, 255] if gesetzt else [0, 0, 0, 0]
        rows.append(r)
    png(os.path.join(dest, "system_icon.png"), n, n, rows)
    old = os.path.join(dest, "system_icon~bw.png")
    if os.path.exists(old):
        os.remove(old)
        print("system_icon~bw.png entfernt - die Linie gilt fuer alle Uhren")
    punkte = sum(1 for y in range(n) for x in range(n) if kontur[y][x] or wasser[y][x])
    ys = [y for y in range(n) if any(kontur[y][x] or wasser[y][x] for x in range(n))]
    print("system_icon.png: %d Punkte schwarz, %d hoch (Vorbild: 180 / 24)"
          % (punkte, (ys[-1] - ys[0] + 1) if ys else 0))


def schreibe_store(dest):
    for gross in STORE_GROESSEN:
        innen = int(round(gross * FUELL))
        kontur, wasser = glas(innen)
        rand = (gross - innen) // 2
        rows = []
        for y in range(gross):
            r = []
            for x in range(gross):
                iy, ix = y - rand, x - rand
                farbe = GRUND
                if 0 <= iy < innen and 0 <= ix < innen:
                    if kontur[iy][ix]:
                        farbe = STRICH
                    elif wasser[iy][ix]:
                        farbe = WASSER
                r += [farbe[0], farbe[1], farbe[2], 255]
            rows.append(r)
        name = "icon-%d.png" % gross
        png(os.path.join(dest, name), gross, gross, rows)
        print("%s: Kachel %s, Glas weiss, Wasser %s"
              % (name, "#%02X%02X%02X" % GRUND, "#%02X%02X%02X" % WASSER))


def main():
    args = sys.argv[1:]
    store = "--store" in args
    if store:
        args.remove("--store")
    dest = args[0] if args else ("store" if store else "resources/images")
    os.makedirs(dest, exist_ok=True)
    if store:
        schreibe_store(dest)
    else:
        schreibe_uhr(dest)


if __name__ == "__main__":
    main()
