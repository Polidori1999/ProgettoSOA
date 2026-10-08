#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTL="$ROOT/build/syscall_throttle_ctl"
DEMO="$ROOT/build/demo_prog"
LOG="/tmp/syscall_throttle_demo4.log"

PID=""

cleanup() {
    if [[ -n "$PID" ]] && kill -0 "$PID" 2>/dev/null; then
        kill "$PID" 2>/dev/null || true
        wait "$PID" 2>/dev/null || true
    fi

    sudo "$CTL" monitor-off >/dev/null 2>&1 || true
    sudo "$CTL" syscall-remove 39 >/dev/null 2>&1 || true
    sudo "$CTL" program-remove demo_prog >/dev/null 2>&1 || true

    rm -f "$LOG"
}

trap cleanup EXIT

sudo -v

echo "===== DEMO 4: MONITOR-OFF SU WAITER ====="

gcc -Wall -Wextra -Wpedantic -O2 \
    "$ROOT/demos/demo_prog.c" \
    -o "$DEMO"

echo
echo "===== RESET MODULO ====="

make -C "$ROOT" reload

echo
echo "===== CONFIGURAZIONE ====="

sudo "$CTL" set-max 1
sudo "$CTL" program-add demo_prog
sudo "$CTL" syscall-add 39
sudo "$CTL" monitor-on

echo
echo "===== AVVIO PROGRAMMA ====="

"$DEMO" >"$LOG" 2>&1 &
PID=$!

echo "Attendo che almeno un thread risulti realmente bloccato..."

WAITER_FOUND=0
STATS=""

for _ in $(seq 1 300); do
    STATS="$("$CTL" stats)"

    BLOCKED="$(
        printf '%s\n' "$STATS" |
        awk -F': ' '/Thread attualmente bloccati:/ {print $2}'
    )"

    if [[ "${BLOCKED:-0}" =~ ^[0-9]+$ ]] && (( BLOCKED > 0 )); then
        WAITER_FOUND=1
        break
    fi

    if ! kill -0 "$PID" 2>/dev/null; then
        break
    fi

    sleep 0.01
done

if (( WAITER_FOUND == 0 )); then
    echo
    echo "ERRORE: nessun waiter bloccato osservato."
    echo
    cat "$LOG" || true
    exit 1
fi

echo
echo "===== WAITER OSSERVATO ====="
printf '%s\n' "$STATS"

echo
echo "Almeno un thread è realmente bloccato."
echo "Disattivo ora il monitor..."

START_NS="$(date +%s%N)"

sudo "$CTL" monitor-off

wait "$PID"
PID=""

END_NS="$(date +%s%N)"
RELEASE_MS=$(( (END_NS - START_NS) / 1000000 ))

echo
echo "===== OUTPUT PROGRAMMA ====="
cat "$LOG"

echo
echo "===== RISULTATO ====="
echo "Tempo tra monitor-off e completamento: ${RELEASE_MS} ms"
echo
echo "Il waiter è stato rivalidato e rilasciato senza"
echo "attendere la normale apertura della finestra successiva."

echo
echo "===== DEMO 4 COMPLETATA ====="