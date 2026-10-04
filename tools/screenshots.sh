#!/bin/sh
# Kuratierter Screenshot-Satz fuer das Repo, im Emulator.
#
#   sh tools/screenshots.sh <emery|flint|gabbro> [JJJJ-MM-TTTHH:MM:SS]
#
# Gebaut wird in einer Kopie unter $TMPDIR (waf vertraegt keine Leerzeichen,
# und die Pruefbauten aendern Quellen) - der Quellordner bleibt unberuehrt.
# Die Uhrzeit ist fest (voreingestellt 14:10), damit die Bilder nicht davon
# abhaengen, wann man sie macht: nach dem Installieren geht alles ueber
# tools/emu_treiber.py, das die Zeit nicht neu stellt (jeder `pebble`-Befehl
# taete das).
#
# ACHTUNG: `pebble wipe` loescht die Daten ALLER Emulatoren dieser SDK.
#
# Bilder unter screenshots/<plattform>/:
#   01-start        frisch, keine Glaeser
#   02-hauptscreen  drei Glaeser (Mitte kurz, dreimal)
#   03-trinkplan    Mitte lang
#   04-trinken      Trink-Animation (Pruefbau: Zeitlupe, FX_MS 9000)
#   05-erinnerung   Erinnerung (Pruefbau: Wecker 60 s nach dem Start)
#   08-getraenke    oben: die Getraenkeauswahl
#   09-tee-milch    Tee, Milch abgehakt: das Gefaess wird heller
#   10-tee-milch-trinken   seine Animation (Pruefbau, Zeitlupe)
#   nur emery:
#   06-sprache-de, 07-sprache-en   zwei Glaeser, deutsch und englisch
export PATH=$HOME/.local/bin:$PATH
P="$1"; ZEIT="${2:-2026-10-03T14:10:00}"
case "$P" in emery|flint|gabbro) ;; *) echo "Plattform: emery, flint oder gabbro"; exit 1 ;; esac
SRC="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$SRC/tools"
PB=$(command -v pebble) || { echo "pebble nicht gefunden"; exit 1; }
PY="$(dirname "$(readlink -f "$PB")")/python"
E="--emulator $P"
OUT="$SRC/screenshots/$P"
ARBEIT=$(mktemp -d)
trap 'rm -rf "$ARBEIT"' EXIT
mkdir -p "$OUT"

drv() { "$PY" "$TOOLS/emu_treiber.py" "$P" "$@"; }

# Eine Kopie bauen; $1 = Name, $2 = Art (normal | pruef | deutsch).
bau() {
  B="$ARBEIT/$1"
  mkdir -p "$B"
  (cd "$SRC" && tar --exclude=./build --exclude=./node_modules --exclude=./.git \
                    --exclude=./screenshots -cf - .) | tar -xf - -C "$B"
  if [ -d "$SRC/node_modules" ]; then ln -s "$SRC/node_modules" "$B/node_modules"
  else (cd "$B" && npm install --no-audit --no-fund >/dev/null 2>&1); fi
  case "$2" in
    pruef)
      sed -i 's/^#define FX_MS [0-9]* /#define FX_MS 9000 /' "$B/src/c/glass_fx.c"
      sed -i '0,/^#pragma once/s//#pragma once\n#define DT_TEST_WAKEUP 1/' "$B/src/c/config.h"
      grep -q "FX_MS 9000" "$B/src/c/glass_fx.c" && grep -q DT_TEST_WAKEUP "$B/src/c/config.h" \
        || { echo "Pruefbau: Schalter nicht gesetzt"; exit 1; } ;;
    deutsch)
      sed -i 's/prv_pick_language(i18n_get_system_locale())/prv_pick_language("de_DE")/' "$B/src/c/strings.c"
      grep -q 'prv_pick_language("de_DE")' "$B/src/c/strings.c" || { echo "Deutsch nicht gesetzt"; exit 1; } ;;
  esac
  (cd "$B" && pebble build 2>&1 | grep -E "error|Build failed")
  ls "$B"/build/*.pbw >/dev/null 2>&1 || { echo "Bau $1 fehlgeschlagen"; exit 1; }
}

# Frischer Emulator, App installieren, dann die Zeit setzen und die App neu
# starten - sie soll ihren Tag zu dieser Zeit beginnen.
frisch() {
  pebble kill >/dev/null 2>&1; sleep 2
  pebble wipe >/dev/null 2>&1
  pebble install $E "$1" >/dev/null 2>&1
  sleep 6
  drv t="$ZEIT" b=back w=1 b=select w=1 b=select w=3
}

echo "== $P, $ZEIT"
bau normal normal
frisch "$ARBEIT"/normal/build/*.pbw
drv s="$OUT/01-start.png" \
    b=select w=4 b=select w=4 b=select w=4 s="$OUT/02-hauptscreen.png" \
    lang=select w=2 s="$OUT/03-trinkplan.png" b=back w=1 \
    b=up w=2 s="$OUT/08-getraenke.png" \
    b=down w=0.5 b=down w=0.5 b=select w=1.5 b=up w=0.5 b=up w=0.5 b=select w=1 \
    s="$OUT/09-tee-milch.png" b=back w=1 b=back w=1

bau pruef pruef
frisch "$ARBEIT"/pruef/build/*.pbw
T0=$(date +%s)
drv b=select w=2.5 s="$OUT/04-trinken.png" w=8 b=back w=1
NOW=$(date +%s); sleep $((T0 + 70 - NOW))
drv s="$OUT/05-erinnerung.png"
# Tee mit Milch eintragen, frisch gestartet: ein Glas in Zeitlupe (9 s)
# wuerde sonst die Tasten schlucken.
frisch "$ARBEIT"/pruef/build/*.pbw
drv b=up w=2 b=down w=0.5 b=down w=0.5 b=select w=1.5 b=up w=0.5 b=up w=0.5 b=select w=1 \
    b=down w=0.5 b=down w=0.5 b=select w=2.5 s="$OUT/10-tee-milch-trinken.png"

if [ "$P" = emery ]; then
  frisch "$ARBEIT"/normal/build/*.pbw
  drv b=select w=4 b=select w=4 s="$OUT/07-sprache-en.png"
  bau deutsch deutsch
  frisch "$ARBEIT"/deutsch/build/*.pbw
  drv b=select w=4 b=select w=4 s="$OUT/06-sprache-de.png"
fi
pebble kill >/dev/null 2>&1
echo "fertig: $OUT"
