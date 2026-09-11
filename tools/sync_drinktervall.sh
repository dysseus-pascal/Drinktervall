#!/bin/sh
# Quellen nach ~/drinktervall spiegeln und bauen (waf vertraegt keine Pfade mit
# Leerzeichen). Aufruf: sync_drinktervall.sh [<Quellordner>]; ohne Argument wird
# $DRINKTERVALL_SRC verwendet, z.B. /mnt/c/Users/<name>/Dokumente/Drinktervall.
export PATH=$HOME/.local/bin:$PATH
SRC="${1:-$DRINKTERVALL_SRC}"
[ -d "$SRC" ] || { echo "Quellordner fehlt: '$SRC' (Argument oder DRINKTERVALL_SRC setzen)"; exit 1; }
DST=$HOME/drinktervall
mkdir -p "$DST"
rm -rf "$DST/src" "$DST/resources" "$DST/build"
cp -r "$SRC/src" "$SRC/resources" "$SRC/package.json" "$SRC/wscript" "$DST/"
cd "$DST" || exit 1
echo "Dateien in src/c: $(ls src/c | wc -l)"
pebble build 2>&1 | grep -iE 'error|warning: \.\./src|APP MEMORY|footprint in RAM|finished successfully|Build failed|Traceback'
