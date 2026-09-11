#!/bin/sh
# Kuratierter Screenshot-Satz fuer das Repo.
# Aufruf: screenshots.sh <emery|flint|gabbro> <Quellordner>
# Erwartet ein gespiegeltes ~/drinktervall (sync_drinktervall.sh). Legt die
# Bilder unter <Quellordner>/screenshots/<plattform>/ ab.
export PATH=$HOME/.local/bin:$PATH
P="$1"; E="--emulator $P"; SRC="$2"
[ -d "$SRC" ] || { echo "Quellordner fehlt: '$SRC'"; exit 1; }
TOOLS="$(cd "$(dirname "$0")" && pwd)"
OUT=/tmp/drinktervall/screens/$P
click() { pebble emu-button $E click "$1"; sleep 1; }
shot()  { pebble screenshot $E "$OUT/$1.png" >/dev/null 2>&1; echo "  $1"; }
fresh() { pebble kill >/dev/null 2>&1; sleep 2; pebble wipe >/dev/null 2>&1; pebble install $E >/dev/null 2>&1; sleep 6; }
rm -rf "$OUT"; mkdir -p "$OUT"

# 1) Normalbuild: Start, Hauptscreen mit drei Glaesern, Trinkplan
sh "$TOOLS/sync_drinktervall.sh" "$SRC" | grep -E "error|Build failed"
cd ~/drinktervall || exit 1
fresh
shot 01-start
for i in 1 2 3; do pebble emu-button $E click select; sleep 4; done
shot 02-hauptscreen
click up; sleep 1
shot 03-trinkplan
click back

# 2) Testbuild: Zeitlupe (FX_MS 9000) und Erinnerung 60 s nach dem Start
sed -i 's/^#define FX_MS [0-9]* /#define FX_MS 9000 /' src/c/glass_fx.c
grep -q DT_TEST_WAKEUP src/c/config.h || sed -i '0,/^#pragma once/s//#pragma once\n#define DT_TEST_WAKEUP 1/' src/c/config.h
pebble build 2>&1 | grep -E "error|Build failed"
fresh
T0=$(date +%s)
pebble emu-button $E click select
sleep 2.5
shot 04-trinken
sleep 8
click back
NOW=$(date +%s); sleep $((T0 + 68 - NOW))
shot 05-erinnerung
click back

# Quellen zuruecksetzen
sh "$TOOLS/sync_drinktervall.sh" "$SRC" | grep -E "error|Build failed"
mkdir -p "$SRC/screenshots/$P"
cp "$OUT"/*.png "$SRC/screenshots/$P/"
echo "done $P -> $SRC/screenshots/$P"
