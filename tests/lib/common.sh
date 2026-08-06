#!/usr/bin/env bash

set -Eeuo pipefail

PROJECT_ROOT="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

MODULE_NAME="syscall_throttle"
MODULE_PATH="$PROJECT_ROOT/kernel/$MODULE_NAME.ko"
CONTROLLER="$PROJECT_ROOT/build/syscall_throttle_ctl"

DMESG_LINE_MARK=0

if (( EUID == 0 )); then
    SUDO=()
else
    SUDO=(sudo)
fi

log()
{
    printf '\n===== %s =====\n' "$1"
}

pass()
{
    printf 'PASS: %s\n' "$1"
}

fail()
{
    printf 'FAIL: %s\n' "$1" >&2
    return 1
}

require_command()
{
    local command_name="$1"

    if ! command -v "$command_name" >/dev/null 2>&1; then
        fail "comando richiesto non disponibile: $command_name"
    fi
}

run_as_root()
{
    "${SUDO[@]}" "$@"
}

module_loaded()
{
    lsmod |
        awk -v module="$MODULE_NAME" '
            $1 == module {
                found = 1
            }

            END {
                exit !found
            }
        '
}

controller()
{
    [[ -x "$CONTROLLER" ]] ||
        fail "controller non disponibile: $CONTROLLER"

    run_as_root "$CONTROLLER" "$@"
}

unload_module()
{
    if ! module_loaded; then
        return 0
    fi

    controller monitor-off >/dev/null 2>&1 || true
    run_as_root rmmod "$MODULE_NAME"

    if module_loaded; then
        fail "il modulo risulta ancora caricato"
    fi
}

load_module()
{
    unload_module

    run_as_root insmod "$MODULE_PATH"

    if ! module_loaded; then
        fail "il modulo non risulta caricato"
    fi
}

capture_dmesg_mark()
{
    DMESG_LINE_MARK="$(
        run_as_root dmesg |
        wc -l
    )"
}

new_dmesg_lines()
{
    run_as_root dmesg |
        tail -n "+$((DMESG_LINE_MARK + 1))"
}

check_new_dmesg_errors()
{
    local errors

    errors="$(
        new_dmesg_lines |
        grep -Ei \
            'BUG:|Oops:|general protection fault|use-after-free|KASAN:|kernel panic|hung task|Unable to handle kernel' \
        || true
    )"

    if [[ -n "$errors" ]]; then
        printf '%s\n' "$errors" >&2
        fail "rilevati errori kernel durante il test"
    fi

    pass "nessun errore critico in dmesg"
}
