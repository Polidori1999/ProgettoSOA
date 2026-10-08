#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTL="$ROOT/build/syscall_throttle_ctl"
DEMO="$ROOT/build/demo_prog"

DEMO_USER="throttle_demo"
TMP_DEMO="/tmp/demo_euid"

cleanup() {
    sudo "$CTL" monitor-off >/dev/null 2>&1 || true

    if [[ -n "${DEMO_UID:-}" ]]; then
        sudo "$CTL" uid-remove "$DEMO_UID" >/dev/null 2>&1 || true
    fi

    sudo "$CTL" syscall-remove 39 >/dev/null 2>&1 || true
    sudo rm -f "$TMP_DEMO"
}

trap cleanup EXIT
sudo -v

echo "===== DEMO 2: EUID THROTTLING ====="

if ! id "$DEMO_USER" >/dev/null 2>&1; then
    echo "Creo utente dedicato '$DEMO_USER'..."
    sudo useradd -m -s /bin/bash "$DEMO_USER"
fi

DEMO_UID="$(id -u "$DEMO_USER")"

echo "Utente demo: $DEMO_USER"
echo "EUID demo:   $DEMO_UID"

gcc -Wall -Wextra -Wpedantic -O2 \
    "$ROOT/demos/demo_prog.c" \
    -o "$DEMO"

sudo cp "$DEMO" "$TMP_DEMO"
sudo chmod 755 "$TMP_DEMO"

make -C "$ROOT" reload

echo
echo "===== CONTROLLO SENZA THROTTLING ====="

sudo -u "$DEMO_USER" "$TMP_DEMO"

echo
echo "===== CONFIGURAZIONE ====="

sudo "$CTL" set-max 1
sudo "$CTL" uid-add "$DEMO_UID"
sudo "$CTL" syscall-add 39

"$CTL" get-max
"$CTL" uid-list
"$CTL" program-list
"$CTL" syscall-list

echo
echo "===== MONITOR ON ====="

sudo "$CTL" monitor-on

echo
echo "===== EUID REGISTRATO: DEVE ESSERE THROTTLED ====="

sudo -u "$DEMO_USER" "$TMP_DEMO"

echo
echo "===== EUID CORRENTE: NON DEVE ESSERE THROTTLED ====="

echo "EUID corrente: $(id -u)"
"$TMP_DEMO"

echo
echo "===== DEMO 2 COMPLETATA ====="
