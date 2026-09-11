#!/bin/sh
# Screenshot-Durchlauf im Emulator. Aufruf: test_screens.sh <emery|flint|gabbro>
# Ergebnis in /tmp/drinktervall/<plattform>/. Erwartet ein gebautes ~/drinktervall.
export PATH=$HOME/.local/bin:$PATH
cd ~/drinktervall || exit 1
P="$1"; E="--emulator $P"
OUT=/tmp/drinktervall/$P
click() { pebble emu-button $E click "$1"; sleep 1; }
shot()  { sleep 1; pebble screenshot $E "$OUT/$1.png" >/dev/null 2>&1; echo "  $1"; }
rm -rf "$OUT"; mkdir -p "$OUT"
pebble kill >/dev/null 2>&1; sleep 2
pebble wipe >/dev/null 2>&1
pebble install $E >/dev/null 2>&1
sleep 6
shot 01-start
click select; click select; click select
shot 02-drei-glaeser
click up
shot 03-plan
click back
click down
shot 04-zwei-glaeser
echo "done $P -> $OUT"
