#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CTL="$ROOT_DIR/build/syscall_throttle_ctl"
DEMO="$ROOT_DIR/build/demo_conc"
PROCESS_DEMO="$ROOT_DIR/build/demo_prog"
PROCESS_LOG_DIR=""
PROCESS_A_PID=""
PROCESS_B_PID=""

cleanup() {
    echo
    echo "===== CLEANUP ====="

    sudo "$CTL" monitor-off >/dev/null 2>&1 || true
    # OFF libera eventuali chiamate in attesa prima di attendere i processi.
    if [[ -n "$PROCESS_A_PID" ]]; then
        wait "$PROCESS_A_PID" 2>/dev/null || true
    fi
    if [[ -n "$PROCESS_B_PID" ]]; then
        wait "$PROCESS_B_PID" 2>/dev/null || true
    fi
    sudo "$CTL" syscall-remove 39 >/dev/null 2>&1 || true
    sudo "$CTL" program-remove demo_conc >/dev/null 2>&1 || true
    sudo "$CTL" program-remove demo_prog >/dev/null 2>&1 || true
    if [[ -n "$PROCESS_LOG_DIR" ]]; then
        rm -rf -- "$PROCESS_LOG_DIR"
    fi
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

gcc -Wall -Wextra -Wpedantic -O2 \
    "$ROOT_DIR/demos/demo_prog.c" \
    -o "$PROCESS_DEMO"

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
echo "===== VERIFICA AGGIUNTIVA: QUOTA CONDIVISA FRA DUE PROCESSI ====="

# Riparte con statistiche nuove, separate dalla prova dei tre worker.
make -C "$ROOT_DIR" reload
sudo "$CTL" set-max 1
sudo "$CTL" program-add demo_prog
sudo "$CTL" syscall-add 39
PROCESS_LOG_DIR="$(mktemp -d)"

sudo "$CTL" monitor-on

# Due PID distinti, quattro getpid ciascuno: otto ammissioni complessive.
# Timeout evita attese indefinite se il modulo non si comporta come previsto.
timeout 15s "$PROCESS_DEMO" >"$PROCESS_LOG_DIR/process-a.log" 2>&1 &
PROCESS_A_PID=$!
timeout 15s "$PROCESS_DEMO" >"$PROCESS_LOG_DIR/process-b.log" 2>&1 &
PROCESS_B_PID=$!

wait "$PROCESS_A_PID"
PROCESS_A_PID=""
wait "$PROCESS_B_PID"
PROCESS_B_PID=""

sudo "$CTL" monitor-off

echo
echo "--- PROCESSO A ---"
cat "$PROCESS_LOG_DIR/process-a.log"
echo
echo "--- PROCESSO B ---"
cat "$PROCESS_LOG_DIR/process-b.log"
echo
echo "Atteso: PID diversi e una sola ammissione complessiva per finestra."
echo "Otto chiamate richiedono circa sette secondi dalla prima ammissione."
echo "L'ordine fra processi non e' garantito; i tempi sono relativi al singolo avvio."
echo
"$CTL" stats

echo
echo "===== DEMO COMPLETATA ====="
