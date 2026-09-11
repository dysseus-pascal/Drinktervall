#!/usr/bin/env python3
"""Glas-Symbol mit Gesicht als Timeline-Ressource (25/50/80 px).

Aufruf: mkglass.py <zielordner>
Gleiche Bildsprache wie das Glas in src/c/glass_fx.c, aber fuer kleine
Groessen entzerrt: schlankeres Glas, groesseres Gesicht, weniger Wasser.
Weiss auf transparent, harte Kanten durch 4-fache Ueberabtastung -
Timeline-Symbole werden als Maske gezeichnet.
"""
import math, os, struct, sys, zlib

SS = 4
UNITS = 80.0
HW_TOP, HW_BOT, HH = 33, 24, 40
LEVEL = 0.0                       # leer: die Pins stehen fuer ausgetrunkene Glaeser
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


def build(size):
    k = 0.92 * size / UNITS
    cx = cy = size / 2.0
    P = lambda x, y: (cx + x * k, cy + y * k)
    stroke = max(2.0, round(0.07 * 2 * HW_TOP * k))
    half = stroke / 2.0

    water = []
    if LEVEL > 0:
        wy = HH - 2 * HH * LEVEL
        whw = HW_BOT + (HW_TOP - HW_BOT) * LEVEL
        water = [P(-HW_BOT, HH), P(HW_BOT, HH), P(whw, wy), P(-whw, wy)]

    strokes = [(P(a, b), P(c, d)) for a, b, c, d in EYES]
    m = [P(*p) for p in MOUTH]
    strokes += [(m[0], m[1]), (m[1], m[2])]
    glass = [P(-HW_TOP, -HH), P(-HW_BOT, HH), P(HW_BOT, HH), P(HW_TOP, -HH)]
    strokes += list(zip(glass, glass[1:]))      # Seiten und Boden, oben offen

    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    px, py = x + (sx + .5) / SS, y + (sy + .5) / SS
                    on = bool(water) and in_poly(px, py, water)
                    if not on:
                        for a, b in strokes:
                            if seg_dist(px, py, a[0], a[1], b[0], b[1]) <= half:
                                on = True
                                break
                    hits += on
            row += [255, 255, 255, 255 if hits * 2 >= SS * SS else 0]
        rows.append(row)
    return rows


def main():
    out = sys.argv[1]
    for name, size in (("glass_tiny", 25), ("glass_small", 50), ("glass_large", 80)):
        png(os.path.join(out, name + ".png"), size, size, build(size))
        print("geschrieben:", name, size)


if __name__ == "__main__":
    main()
