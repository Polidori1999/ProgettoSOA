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

    if ! cleanup_test_environment; then
        exit_code=1
    fi

    exit "$exit_code"
}

controller_timed()
{
    run_as_root timeout 10s "$CONTROLLER" "$@"
}

main()
{
    local command_name

    for command_name in \
        make \
        gcc \
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

    build_project

    # L'eventuale modulo residuo viene rimosso prima
    # di fissare il punto iniziale di dmesg.
    unload_module
    capture_dmesg_mark

    log "CARICAMENTO MODULO"

    load_module

    module_loaded ||
        fail "il modulo non risulta caricato"

    pass "modulo caricato"

    log "CONTROLLO DEVICE E CONFIGURAZIONE"

    controller_timed ping
    controller_timed get-max
    controller_timed monitor-status

    pass "controller e device raggiungibili"

    log "CONTROLLO MONITOR"

    controller_timed monitor-off
    controller_timed monitor-status

    controller_timed monitor-on
    controller_timed monitor-status

    controller_timed monitor-off
    controller_timed monitor-status

    pass "transizioni monitor off/on/off completate"

    log "SCARICAMENTO MODULO"

    unload_module

    if module_loaded; then
        fail "il modulo risulta ancora caricato"
    fi

    pass "modulo scaricato"

    log "CONTROLLO DMESG"

    check_new_dmesg_errors

    log "RISULTATO"

    pass "smoke test completato"
}

trap cleanup EXIT

main "$@"
