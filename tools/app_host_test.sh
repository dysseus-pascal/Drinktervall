#!/bin/sh
# Die App als Ganzes (drinktervall.c, Erinnerungen, Kaffee- und Trink-Fenster)
# mit dem C-Compiler des Rechners pruefen - Fenster, Tasten, Start, Glance und
# AppMessage kommen als Attrappe aus tools/host. Unter drei Zeitzonen.
#
#   sh tools/app_host_test.sh
#
# main() aus drinktervall.c heisst hier dt_main: der Test hat sein eigenes.
# Die Nachrichtenschluessel entstehen wie in phone_host_test.sh aus
# package.json.
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
node -e "require(\"$DIR/package.json\").pebble.messageKeys.forEach((k, i) =>
  console.log(\"#define MESSAGE_KEY_\" + k + \" \" + (10000 + i)))" > "$OUT/message_keys.auto.h" || exit 1
FLAGS="-std=gnu99 -Wall -Wextra -Wno-unused-parameter -DATTRAPPE_GRAFIK -DATTRAPPE_APP
       -I $DIR/tools/host -I $DIR/src/c -include $OUT/message_keys.auto.h"
cc $FLAGS -Dmain=dt_main -c "$DIR/src/c/drinktervall.c" -o "$OUT/drinktervall.o" || exit 1
cc $FLAGS "$DIR/tools/app_host_test.c" "$OUT/drinktervall.o" \
   "$DIR/src/c/reminder_window.c" "$DIR/src/c/coffee_window.c" "$DIR/src/c/drink_window.c" \
   "$DIR/src/c/glass_fx.c" "$DIR/src/c/schedule.c" "$DIR/src/c/coffee.c" "$DIR/src/c/phone.c" \
   "$DIR/src/c/strings.c" "$DIR/tools/host/attrappe.c" "$DIR/tools/host/attrappe_persist.c" \
   "$DIR/tools/host/attrappe_grafik.c" "$DIR/tools/host/attrappe_app.c" \
   -lm -o "$OUT/app_test" || exit 1
R=0
for ZONE in Europe/Zurich Europe/London America/New_York; do
  TZ=$ZONE "$OUT/app_test" || R=1
done
exit $R
