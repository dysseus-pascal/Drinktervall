#!/bin/sh
# Die Getraenkefarben pruefen, wie das Farbdisplay sie zeigt - mit dem Python
# des pebble-Werkzeugs, das die Umrechnungstabelle mitbringt.
#
#   sh tools/farben_check.sh
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")" && pwd)
PB=$(command -v pebble) || { echo "pebble nicht gefunden"; exit 1; }
PY="$(dirname "$(readlink -f "$PB")")/python"
[ -x "$PY" ] || { echo "Python des pebble-Werkzeugs nicht gefunden: $PY"; exit 1; }
"$PY" "$DIR/farben_check.py"
