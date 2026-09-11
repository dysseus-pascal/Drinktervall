#!/usr/bin/env python3
"""Glas-Symbole mit Gesicht als Timeline-Ressourcen (25/50/80 px).

Aufruf: make_glass_icon.py <zielordner>
Erzeugt zwei Varianten:
  glass_tiny|small|large       leeres Glas  - Pins fuer ausgetrunkene Glaeser
  glass_full_tiny|small|large  volles Glas  - Pin der naechsten Erinnerung

Gleiche Bildsprache wie das Glas in src/c/glass_fx.c, fuer kleine Groessen
entzerrt: schlankeres Glas, groesseres Gesicht. Timeline-Symbole sind Masken
(undurchsichtig wird gezeichnet, transparent bleibt frei), darum wird das
Gesicht mit dem Rest exklusiv-verodert: ueber Wasser stanzt es sich frei,
ueber leerem Glas steht es selbst. Harte Kanten durch 4-fache Ueberabtastung.
"""
import math
import os
import struct
import sys
import zlib

SS = 4                            # Ueberabtastung
UNITS = 80.0                      # Hoehe des Glases in Basis-Einheiten
HW_TOP, HW_BOT, HH = 33, 24, 40   # Halbbreiten oben/unten, halbe Hoehe
EYES = ((-13, -19, -13, -10), (8, -19, 8, -10))
MOUTH = ((-14, 1), (-4, 5), (9, 1))


def png(path, w, h, rows):
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(data)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def in_poly(px, py, pts):
    inside = False
    for i in range(len(pts)):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % len(pts)]
        if (y1 > py) != (y2 > py):
            if px < x1 + (py - y1) * (x2 - x1) / (y2 - y1):
                inside = not inside
    return inside


def build(size, level):
    k = 0.92 * size / UNITS
    cx = cy = size / 2.0

    def P(x, y):
        return (cx + x * k, cy + y * k)

    half = max(2.0, round(0.07 * 2 * HW_TOP * k)) / 2.0

    water = []
    if level > 0:
        wy = HH - 2 * HH * level
        whw = HW_BOT + (HW_TOP - HW_BOT) * level
        water = [P(-HW_BOT, HH), P(HW_BOT, HH), P(whw, wy), P(-whw, wy)]

    glass = [P(-HW_TOP, -HH), P(-HW_BOT, HH), P(HW_BOT, HH), P(HW_TOP, -HH)]
    body = list(zip(glass, glass[1:]))          # Seiten und Boden, oben offen

    face = [(P(a, b), P(c, d)) for a, b, c, d in EYES]
    m = [P(*p) for p in MOUTH]
    face += [(m[0], m[1]), (m[1], m[2])]

    def hit(strokes, px, py):
        return any(seg_dist(px, py, a[0], a[1], b[0], b[1]) <= half for a, b in strokes)

    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    px, py = x + (sx + .5) / SS, y + (sy + .5) / SS
                    base = (bool(water) and in_poly(px, py, water)) or hit(body, px, py)
                    hits += base != hit(face, px, py)
            row += [255, 255, 255, 255 if hits * 2 >= SS * SS else 0]
        rows.append(row)
    return rows


def main():
    out = sys.argv[1]
    for prefix, level in (("glass", 0.0), ("glass_full", 0.80)):
        for suffix, size in (("tiny", 25), ("small", 50), ("large", 80)):
            name = "%s_%s.png" % (prefix, suffix)
            png(os.path.join(out, name), size, size, build(size, level))
            print("geschrieben:", name)


if __name__ == "__main__":
    main()
