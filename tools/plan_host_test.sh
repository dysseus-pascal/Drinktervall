#!/bin/sh
# Wecker, Plan-Liste und Pins (schedule.c, coffee.c) an Sommerzeit-
# Umstellungen und nach erreichtem Tagesziel - mit dem C-Compiler des
# Rechners, unter drei Zeitzonen mit verschiedenen Umstellungstagen.
#
#   sh tools/plan_host_test.sh
#
# Exitcode 0 = alles bestanden.
DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu99 -Wall -Wextra -Wno-unused-parameter -I "$DIR/tools/host" -I "$DIR/src/c" \
   "$DIR/tools/plan_host_test.c" "$DIR/src/c/schedule.c" "$DIR/src/c/coffee.c" "$DIR/src/c/strings.c" \
   "$DIR/tools/host/attrappe.c" "$DIR/tools/host/attrappe_persist.c" \
   -o "$OUT/plan_test" || exit 1
R=0
for ZONE in Europe/Zurich Europe/London America/New_York; do
  TZ=$ZONE "$OUT/plan_test" || R=1
done
exit $R
