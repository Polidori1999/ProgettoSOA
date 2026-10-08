#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTL="$ROOT/build/syscall_throttle_ctl"
DEMO="$ROOT/build/demo_block_io"

cleanup() {
    sudo "$CTL" monitor-off >/dev/null 2>&1 || true
    sudo "$CTL" syscall-remove 0 >/dev/null 2>&1 || true
    sudo "$CTL" program-remove soa-read-demo >/dev/null 2>&1 || true
}

if [[ ! -x "$CTL" ]]; then
    echo "Prima esegui make dalla cartella del progetto." >&2
    exit 1
fi

sudo -v
trap cleanup EXIT

echo "===== DEMO 6: SYSCALL BLOCCANTE (READ SU PIPE) ====="

gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -pthread \
    -I"$ROOT/include" "$ROOT/demos/demo_blocking.c" -o "$DEMO"

make -C "$ROOT" reload

sudo "$CTL" set-max 1
sudo "$CTL" program-add soa-read-demo
sudo "$CTL" syscall-add 0

echo
echo "===== MONITOR OFF: SOLO ATTESA DEI DATI ====="
timeout 10s "$DEMO"

echo
echo "===== MONITOR ON: ATTESA DEI DATI + THROTTLING ====="
sudo "$CTL" monitor-on
timeout 10s "$DEMO"

sudo "$CTL" monitor-off

echo
echo "===== STATISTICHE DEL SOLO RITARDO DEL MONITOR ====="
"$CTL" stats

echo
echo "===== RISULTATI ATTESI ====="
echo "OFF: prima read circa 200 ms; seconda read immediata."
echo "ON: prima read circa 200 ms; seconda fino alla nuova finestra."
echo "Il byte B e' gia' disponibile prima della seconda read."
echo "Il monitor misura la propria attesa, non l'attesa dei dati."
