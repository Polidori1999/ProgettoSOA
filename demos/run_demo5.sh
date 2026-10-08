#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTL="$ROOT/build/syscall_throttle_ctl"

ORIGINAL_MAX=""

cleanup() {
    if [[ -n "$ORIGINAL_MAX" ]]; then
        sudo "$CTL" set-max "$ORIGINAL_MAX" >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT
sudo -v

echo "===== DEMO 5: PRIVILEGI ROOT / NON-ROOT ====="

make -C "$ROOT" reload

ORIGINAL_MAX="$("$CTL" get-max | awk '{print $3}')"

echo
echo "===== OPERAZIONI READ-ONLY SENZA SUDO ====="

"$CTL" get-max
"$CTL" monitor-status
"$CTL" stats

echo
echo "===== MODIFICA SENZA SUDO ====="

if "$CTL" set-max 10; then
    echo "ERRORE: set-max è stato accettato senza privilegi."
    exit 1
else
    echo "PASS: modifica rifiutata per utente non privilegiato."
fi

echo
echo "===== MODIFICA CON SUDO ====="

sudo "$CTL" set-max 10

"$CTL" get-max

echo
echo "PASS: modifica accettata con EUID 0."

echo
echo "===== DEMO 5 COMPLETATA ====="