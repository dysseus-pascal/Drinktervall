#!/bin/sh
# Den Weg zum Telefon (phone.c) und die eigenen Getraenke (coffee.c) mit dem
# C-Compiler des Rechners pruefen - pebble.h und AppMessage kommen als
# Attrappe aus tools/host.
#
#   sh tools/phone_host_test.sh
#
# Die Nachrichtenschluessel stehen nur in package.json; die SDK erzeugt daraus
# message_keys.auto.h. Hier geschieht dasselbe, damit der Test keine eigene
# Liste fuehrt, die auseinanderlaufen koennte.
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
node -e "require(\"$DIR/package.json\").pebble.messageKeys.forEach((k, i) =>
  console.log(\"#define MESSAGE_KEY_\" + k + \" \" + (10000 + i)))" > "$OUT/message_keys.auto.h" || exit 1
QUELLEN="$DIR/tools/phone_host_test.c $DIR/src/c/phone.c $DIR/src/c/schedule.c $DIR/src/c/coffee.c
         $DIR/src/c/strings.c $DIR/tools/host/attrappe.c $DIR/tools/host/attrappe_persist.c"
FLAGS="-std=gnu99 -Wall -Wextra -Wno-unused-parameter -I $DIR/tools/host -I $DIR/src/c -include $OUT/message_keys.auto.h"
cc $FLAGS $QUELLEN -o "$OUT/phone_test" || exit 1
# Zweiter Bau: der Text der eigenen Getraenke passt nicht (Puffer 60 Byte).
cc $FLAGS -Wno-unused-function -DTEXT_ZU_KLEIN -DCUSTOM_TEXT_MAX=60 $QUELLEN -o "$OUT/phone_test_klein" || exit 1
TZ=UTC "$OUT/phone_test"; R1=$?
TZ=UTC "$OUT/phone_test_klein"; R2=$?
[ $R1 -eq 0 ] && [ $R2 -eq 0 ]
