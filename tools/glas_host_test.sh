#!/bin/sh
# Milch auf der Schwarz-Weiss-Uhr (glass_fx.c) mit dem C-Compiler des Rechners
# pruefen - pebble.h und die Grafik kommen als Attrappe aus tools/host.
# Gebaut wird wie fuer flint: ohne PBL_COLOR.
#
#   sh tools/glas_host_test.sh
#   GLAS_BILD=<ordner> sh tools/glas_host_test.sh   # dazu die Bilder als PGM
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu99 -Wall -Wextra -Wno-unused-parameter -DATTRAPPE_GRAFIK -I "$DIR/tools/host" -I "$DIR/src/c" \
   "$DIR/tools/glas_host_test.c" "$DIR/src/c/glass_fx.c" "$DIR/tools/host/attrappe_grafik.c" \
   -lm -o "$OUT/glas_test" || exit 1
"$OUT/glas_test"
