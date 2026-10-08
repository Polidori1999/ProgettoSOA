#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CTL="$ROOT_DIR/build/syscall_throttle_ctl"
DEMO="$ROOT_DIR/build/demo_conc"

cleanup() {
    echo
    echo "===== CLEANUP ====="

    sudo "$CTL" monitor-off >/dev/null 2>&1 || true
    sudo "$CTL" syscall-remove 39 >/dev/null 2>&1 || true
    sudo "$CTL" program-remove demo_conc >/dev/null 2>&1 || true
}

trap cleanup EXIT

echo "===== DEMO 3: CONCORRENZA + STATISTICHE ====="
echo

# Autentica sudo una sola volta all'inizio.
sudo -v

echo "===== COMPILAZIONE DEMO ====="

gcc -Wall -Wextra -Wpedantic -O2 -pthread \
    "$ROOT_DIR/demos/demo_concurrency.c" \
    -o "$DEMO"

echo
echo "===== RESET MODULO ====="

make -C "$ROOT_DIR" reload

echo
echo "===== CONFIGURAZIONE ====="

sudo "$CTL" set-max 1
sudo "$CTL" program-add demo_conc
sudo "$CTL" syscall-add 39

echo
"$CTL" get-max
"$CTL" program-list
"$CTL" syscall-list

echo
echo "===== ATTIVAZIONE MONITOR ====="

sudo "$CTL" monitor-on

echo
echo "===== ESECUZIONE CONCORRENTE ====="
echo

"$DEMO"

echo
echo "===== DISATTIVAZIONE MONITOR ====="

sudo "$CTL" monitor-off

echo
echo "===== STATISTICHE ====="
echo

"$CTL" stats

echo
echo "===== DEMO COMPLETATA ====="