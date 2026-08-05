#!/usr/bin/env bash

SCRIPT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")" &&
    pwd
)"

# shellcheck source=lib/common.sh
source "$SCRIPT_DIR/lib/common.sh"

cleanup()
{
    local exit_code=$?

    trap - EXIT

    set +e

    unload_module
    if (( $? != 0 )); then
        exit_code=1
    fi

    set -e

    exit "$exit_code"
}

main()
{
    local test_binary
    local command_name

    if (( $# != 1 )); then
        fail "uso: $0 PERCORSO_TEST"
    fi

    test_binary="$1"

    if [[ ! -x "$test_binary" ]]; then
        fail "test non eseguibile: $test_binary"
    fi

    for command_name in \
        timeout \
        insmod \
        rmmod \
        lsmod \
        awk \
        dmesg \
        grep \
        tail \
        wc
    do
        require_command "$command_name"
    done

    if (( EUID != 0 )); then
        require_command sudo
    fi

    unload_module
    capture_dmesg_mark

    log "CARICAMENTO MODULO"

    load_module
    pass "modulo caricato"

    log "ESECUZIONE $(basename "$test_binary")"

    run_as_root timeout 30s "$test_binary"

    log "SCARICAMENTO MODULO"

    unload_module
    pass "modulo scaricato"

    log "CONTROLLO DMESG"

    check_new_dmesg_errors

    log "RISULTATO"

    pass "$(basename "$test_binary") completato"
}

trap cleanup EXIT

main "$@"
