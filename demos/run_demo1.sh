#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTL="$ROOT/build/syscall_throttle_ctl"
DEMO="$ROOT/build/demo_prog"

cleanup() {
    sudo "$CTL" monitor-off >/dev/null 2>&1 || true
    sudo "$CTL" syscall-remove 39 >/dev/null 2>&1 || true
    sudo "$CTL" program-remove demo_prog >/dev/null 2>&1 || true
}

trap cleanup EXIT
sudo -v

echo "===== DEMO 1: PROGRAM NAME THROTTLING ====="

gcc -Wall -Wextra -Wpedantic -O2 \
    "$ROOT/demos/demo_prog.c" \
    -o "$DEMO"

make -C "$ROOT" reload

echo
echo "===== CONTROLLO SENZA THROTTLING ====="
"$DEMO"

echo
echo "===== CONFIGURAZIONE ====="

sudo "$CTL" set-max 1
sudo "$CTL" program-add demo_prog
sudo "$CTL" syscall-add 39

"$CTL" get-max
"$CTL" program-list
"$CTL" syscall-list

echo
echo "===== MONITOR ON ====="

sudo "$CTL" monitor-on

echo
echo "===== ESECUZIONE ====="

"$DEMO"

echo
echo "===== DEMO 1 COMPLETATA ====="