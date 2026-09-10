#!/bin/sh
# Quellen nach ~/aquatakt spiegeln und bauen (waf vertraegt keine Pfade mit
# Leerzeichen). Aufruf: sync_aqua.sh [<Quellordner>]; ohne Argument wird
# $AQUATAKT_SRC verwendet, z.B. /mnt/c/Users/<name>/Dokumente/AquaTakt.
export PATH=$HOME/.local/bin:$PATH
SRC="${1:-$AQUATAKT_SRC}"
[ -d "$SRC" ] || { echo "Quellordner fehlt: '$SRC' (Argument oder AQUATAKT_SRC setzen)"; exit 1; }
DST=$HOME/aquatakt
mkdir -p "$DST"
rm -rf "$DST/src" "$DST/resources" "$DST/build"
cp -r "$SRC/src" "$SRC/resources" "$SRC/package.json" "$SRC/wscript" "$DST/"
cd "$DST" || exit 1
echo "Dateien in src/c: $(ls src/c | wc -l)"
pebble build 2>&1 | grep -iE 'error|warning: \.\./src|APP MEMORY|footprint in RAM|finished successfully|Build failed|Traceback'
