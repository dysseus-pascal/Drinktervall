#!/bin/sh
# Wakeup-Test auf emery: setzt DT_TEST_WAKEUP nur in der WSL-Kopie (Erinnerung
# 60 s nach dem Start), spielt Erinnerung / Getrunken / Spaeter durch und
# sammelt das pkjs-Log. Danach sync_drinktervall.sh ausfuehren, um das Define wieder
# loszuwerden. Ergebnis in /tmp/drinktervall/wake/ und /tmp/drinktervall/pkjs.log.
export PATH=$HOME/.local/bin:$PATH
cd ~/drinktervall || exit 1
E="--emulator emery"
OUT=/tmp/drinktervall/wake
click() { pebble emu-button $E click "$1"; sleep 1; }
shot()  { pebble screenshot $E "$OUT/$1.png" >/dev/null 2>&1; echo "  $(date +%T) $1"; }
rm -rf "$OUT"; mkdir -p "$OUT"
grep -q DT_TEST_WAKEUP src/c/config.h || sed -i '0,/^#pragma once/s//#pragma once\n#define DT_TEST_WAKEUP 1/' src/c/config.h
pebble build 2>&1 | grep -iE 'error|finished successfully|Build failed'
pebble kill >/dev/null 2>&1; sleep 2
pebble wipe >/dev/null 2>&1
pebble install $E >/dev/null 2>&1
sleep 5
pebble logs $E > /tmp/drinktervall/pkjs.log 2>&1 &
LOGPID=$!
sleep 3
pebble install $E >/dev/null 2>&1
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
grep -v PHONESIM /tmp/drinktervall/pkjs.log | head -20
echo "done -> $OUT"
