#!/bin/sh
# Den Tag von Zaehler und Tagesziel (schedule.c) mit dem C-Compiler des
# Rechners pruefen - pebble.h kommt als Attrappe aus tools/host.
#
#   sh tools/schedule_host_test.sh
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu99 -Wall -Wextra -I "$DIR/tools/host" -I "$DIR/src/c" \
   "$DIR/tools/schedule_host_test.c" "$DIR/src/c/schedule.c" \
   "$DIR/tools/host/attrappe.c" "$DIR/tools/host/attrappe_persist.c" \
   -o "$OUT/schedule_test" || exit 1
TZ=UTC "$OUT/schedule_test"
