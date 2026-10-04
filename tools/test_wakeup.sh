#!/bin/sh
# Wakeup-Test im Emulator (emery): ein Pruefbau mit Erinnerung 60 s nach jedem
# Start (DT_TEST_WAKEUP), gebaut in einer Kopie unter $TMPDIR - die Quelle
# bleibt unberuehrt. Spielt Erinnerung -> Getrunken und die naechste
# Erinnerung -> Spaeter durch und sammelt das Log.
#
#   sh tools/test_wakeup.sh [<ausgabeordner>]   (voreingestellt $TMPDIR/drinktervall-wake)
#
# ACHTUNG: `pebble wipe` loescht die Daten ALLER Emulatoren dieser SDK.
export PATH=$HOME/.local/bin:$PATH
SRC="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-${TMPDIR:-/tmp}/drinktervall-wake}"
E="--emulator emery"
ARBEIT=$(mktemp -d)
trap 'rm -rf "$ARBEIT"' EXIT
rm -rf "$OUT"; mkdir -p "$OUT"

(cd "$SRC" && tar --exclude=./build --exclude=./node_modules --exclude=./.git \
                  --exclude=./screenshots -cf - .) | tar -xf - -C "$ARBEIT"
if [ -d "$SRC/node_modules" ]; then ln -s "$SRC/node_modules" "$ARBEIT/node_modules"
else (cd "$ARBEIT" && npm install --no-audit --no-fund >/dev/null 2>&1); fi
sed -i '0,/^#pragma once/s//#pragma once\n#define DT_TEST_WAKEUP 1/' "$ARBEIT/src/c/config.h"
grep -q DT_TEST_WAKEUP "$ARBEIT/src/c/config.h" || { echo "Pruefbau: Schalter nicht gesetzt"; exit 1; }
(cd "$ARBEIT" && pebble build 2>&1 | grep -iE 'error|Build failed')
PBW=$(ls "$ARBEIT"/build/*.pbw 2>/dev/null) || { echo "Bau fehlgeschlagen"; exit 1; }

click() { pebble emu-button $E click "$1"; sleep 1; }
shot()  { pebble screenshot $E "$OUT/$1.png" >/dev/null 2>&1; echo "  $(date +%T) $1"; }

pebble kill >/dev/null 2>&1; sleep 2
pebble wipe >/dev/null 2>&1
pebble install $E "$PBW" >/dev/null 2>&1
sleep 5
pebble logs $E > "$OUT/log.txt" 2>&1 &
LOGPID=$!
sleep 3
# Noch einmal installieren: die App startet neu, waehrend das Log mitlaeuft,
# und stellt ihren Wecker auf 60 s nach diesem Start.
pebble install $E "$PBW" >/dev/null 2>&1
T0=$(date +%s)
sleep 8
shot 10-start
click back
sleep 2; shot 11-nach-verlassen
NOW=$(date +%s); sleep $((T0 + 70 - NOW))
shot 12-wakeup-erinnerung
click select
sleep 1; shot 13-nach-getrunken
NOW=$(date +%s); sleep $((T0 + 136 - NOW))
shot 14-zweite-erinnerung
click down
sleep 1; shot 15-nach-spaeter
kill $LOGPID 2>/dev/null
pebble kill >/dev/null 2>&1
grep -av PHONESIM "$OUT/log.txt" | head -20
echo "fertig -> $OUT"
