#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>

#include "controller.h"

#include <syscall_throttle_ioctl.h>

#define DEVICE_PATH "/dev/syscall_throttle"

/* Descrive ogni comando una sola volta: nome, ioctl e argomento. */
enum argument_kind {
    ARG_NONE,
    ARG_MAX,
    ARG_UID,
    ARG_PROGRAM,
    ARG_SYSCALL
};

struct controller_command {
    const char *name;
    unsigned long request;
    enum argument_kind argument;
    const char *error_message;
};

static const struct controller_command commands[] = {
    {"ping", SYSCALL_THROTTLE_IOC_PING, ARG_NONE, "ioctl PING fallito"},
    {"get-max", SYSCALL_THROTTLE_IOC_GET_MAX, ARG_NONE, "ioctl GET_MAX fallito"},
    {"set-max", SYSCALL_THROTTLE_IOC_SET_MAX, ARG_MAX, "ioctl SET_MAX fallito"},
    {"monitor-on", SYSCALL_THROTTLE_IOC_ENABLE_MONITOR, ARG_NONE,
     "ioctl ENABLE_MONITOR fallito"},
    {"monitor-off", SYSCALL_THROTTLE_IOC_DISABLE_MONITOR, ARG_NONE,
     "ioctl DISABLE_MONITOR fallito"},
    {"monitor-status", SYSCALL_THROTTLE_IOC_GET_MONITOR, ARG_NONE,
     "ioctl GET_MONITOR fallito"},
    {"uid-add", SYSCALL_THROTTLE_IOC_REGISTER_UID, ARG_UID,
     "ioctl REGISTER_UID fallito"},
    {"uid-remove", SYSCALL_THROTTLE_IOC_UNREGISTER_UID, ARG_UID,
     "ioctl UNREGISTER_UID fallito"},
    {"uid-list", SYSCALL_THROTTLE_IOC_GET_UIDS, ARG_NONE, "ioctl GET_UIDS fallito"},
    {"program-add", SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM, ARG_PROGRAM,
     "ioctl REGISTER_PROGRAM fallito"},
    {"program-remove", SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM, ARG_PROGRAM,
     "ioctl UNREGISTER_PROGRAM fallito"},
    {"program-list", SYSCALL_THROTTLE_IOC_GET_PROGRAMS, ARG_NONE,
     "ioctl GET_PROGRAMS fallito"},
    {"syscall-add", SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL, ARG_SYSCALL,
     "ioctl REGISTER_SYSCALL fallito"},
    {"syscall-remove", SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL, ARG_SYSCALL,
     "ioctl UNREGISTER_SYSCALL fallito"},
    {"syscall-list", SYSCALL_THROTTLE_IOC_GET_SYSCALLS, ARG_NONE,
     "ioctl GET_SYSCALLS fallito"},
    {"stats", SYSCALL_THROTTLE_IOC_GET_STATS, ARG_NONE, "ioctl GET_STATS fallito"}
};

/* La stessa invocazione utilizza un solo tipo di payload. */
union controller_data {
    __u32 value;
    struct syscall_throttle_program program;
    struct syscall_throttle_uid_list uids;
    struct syscall_throttle_program_list programs;
    struct syscall_throttle_syscall_list syscalls;
    struct syscall_throttle_statistics statistics;
};

static const struct controller_command *find_command(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (strcmp(name, commands[i].name) == 0)
            return &commands[i];
    }

    return NULL;
}

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Uso:\n");

    fprintf(stderr, "  %s ping\n", program_name);
    fprintf(stderr, "  %s get-max\n", program_name);
    fprintf(stderr, "  %s set-max NUMERO\n", program_name);

    fprintf(stderr, "  %s monitor-on\n", program_name);
    fprintf(stderr, "  %s monitor-off\n", program_name);
    fprintf(stderr, "  %s monitor-status\n", program_name);

    fprintf(stderr, "  %s uid-add UID\n", program_name);
    fprintf(stderr, "  %s uid-remove UID\n", program_name);
    fprintf(stderr, "  %s uid-list\n", program_name);

    fprintf(stderr, "  %s program-add NOME\n", program_name);
    fprintf(stderr, "  %s program-remove NOME\n", program_name);
    fprintf(stderr, "  %s program-list\n", program_name);

    fprintf(stderr, "  %s syscall-add NUMERO\n", program_name);
    fprintf(stderr, "  %s syscall-remove NUMERO\n", program_name);
    fprintf(stderr, "  %s syscall-list\n", program_name);

    fprintf(stderr, "  %s stats\n", program_name);
}

static int parse_u32(
    const char *text,
    __u32 minimum,
    __u32 *value) {
    unsigned long parsed;
    char *end;

    errno = 0;
    end = NULL;

    parsed = strtoul(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed < minimum ||
        parsed > UINT_MAX) {
        return -1;
    }

    *value = (__u32) parsed;
    return 0;
}

static int parse_program_name(
    const char *text,
    struct syscall_throttle_program *program) {
    size_t length;

    length = strlen(text);

    if (length == 0 ||
        length >= SYSCALL_THROTTLE_PROGRAM_NAME_LEN) {
        return -1;
    }

    /*
     * Azzera tutta la struttura, garantendo la presenza
     * del terminatore '\0' dopo il nome copiato.
     */
    memset(program, 0, sizeof(*program));
    memcpy(program->name, text, length);

    return 0;
}

static void print_statistics(
    const struct syscall_throttle_statistics *stats)
{
    double average_blocked_threads;

    average_blocked_threads = 0.0;

    if (stats->monitor_enabled_time_ns != 0) {
        average_blocked_threads =
            (double)stats->weighted_blocking_time_ns /
            (double)stats->monitor_enabled_time_ns;
    }

    printf(
        "Thread attualmente bloccati: %u\n",
        stats->current_blocked_threads
    );

    printf(
        "Picco thread bloccati: %u\n",
        stats->peak_blocked_threads
    );

    printf(
        "Media thread bloccati: %.3f\n",
        average_blocked_threads
    );

    printf(
        "Tempo totale monitor attivo: %" PRIu64 " ns\n",
        (uint64_t)stats->monitor_enabled_time_ns
    );

    printf(
        "Tempo pesato di blocking: %" PRIu64 " ns\n",
        (uint64_t)stats->weighted_blocking_time_ns
    );

    if (stats->peak_delay_valid == 0) {
        printf("Ritardo massimo: non disponibile\n");
        return;
    }

    printf(
        "Ritardo massimo: %" PRIu64 " ns (%.3f ms)\n",
        (uint64_t)stats->peak_delay_ns,
        (double)stats->peak_delay_ns / 1000000.0
    );

    printf(
        "UID associato al ritardo massimo: %u\n",
        stats->peak_delay_uid
    );

    printf(
        "Programma associato al ritardo massimo: %s\n",
        stats->peak_delay_program
    );
}

static int validate_arguments(const struct controller_command *command,
                              int argc, char *argv[],
                              union controller_data *data)
{
    const char *error_message;

    if (command->argument == ARG_NONE) {
        if (argc == 2)
            return 0;
        print_usage(argv[0]);
        return -1;
    }

    if (command->argument == ARG_PROGRAM) {
        if (argc == 3 && parse_program_name(argv[2], &data->program) == 0)
            return 0;
        error_message = "Errore: il nome deve contenere da 1 a 15 caratteri.";
    } else {
        __u32 minimum = command->argument == ARG_MAX ? 1U : 0U;

        if (argc == 3 && parse_u32(argv[2], minimum, &data->value) == 0)
            return 0;

        switch (command->argument) {
        case ARG_MAX:
            error_message = "Errore: MAX deve essere un intero positivo.";
            break;
        case ARG_UID:
            error_message = "Errore: UID non valido.";
            break;
        default:
            error_message = "Errore: numero di syscall non valido.";
            break;
        }
    }

    fprintf(stderr, "%s\n", error_message);
    return -1;
}

static void *ioctl_argument(unsigned long request, union controller_data *data)
{
    switch (request) {
    case SYSCALL_THROTTLE_IOC_PING:
    case SYSCALL_THROTTLE_IOC_ENABLE_MONITOR:
    case SYSCALL_THROTTLE_IOC_DISABLE_MONITOR:
        return NULL;
    case SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM:
    case SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM:
        return &data->program;
    case SYSCALL_THROTTLE_IOC_GET_UIDS:
        return &data->uids;
    case SYSCALL_THROTTLE_IOC_GET_PROGRAMS:
        return &data->programs;
    case SYSCALL_THROTTLE_IOC_GET_SYSCALLS:
        return &data->syscalls;
    case SYSCALL_THROTTLE_IOC_GET_STATS:
        return &data->statistics;
    default:
        return &data->value;
    }
}

static void print_result(unsigned long request, const union controller_data *data)
{
    __u32 i;

    switch (request) {
    case SYSCALL_THROTTLE_IOC_PING:
        printf("PING completato correttamente.\n");
        break;
    case SYSCALL_THROTTLE_IOC_GET_MAX:
        printf("MAX corrente: %u\n", data->value);
        break;
    case SYSCALL_THROTTLE_IOC_SET_MAX:
        printf("MAX impostato a %u.\n", data->value);
        break;
    case SYSCALL_THROTTLE_IOC_ENABLE_MONITOR:
        printf("Monitor attivato.\n");
        break;
    case SYSCALL_THROTTLE_IOC_DISABLE_MONITOR:
        printf("Monitor disattivato.\n");
        break;
    case SYSCALL_THROTTLE_IOC_GET_MONITOR:
        printf("Monitor: %s\n", data->value ? "attivo" : "disattivo");
        break;
    case SYSCALL_THROTTLE_IOC_REGISTER_UID:
    case SYSCALL_THROTTLE_IOC_UNREGISTER_UID:
        printf("UID %u %s.\n", data->value,
               request == SYSCALL_THROTTLE_IOC_REGISTER_UID ?
               "registrato" : "deregistrato");
        break;
    case SYSCALL_THROTTLE_IOC_GET_UIDS:
        printf("UID registrati: %u\n", data->uids.count);
        if (data->uids.count == 0)
            printf("  nessuno\n");
        for (i = 0; i < data->uids.count; ++i)
            printf("  %u\n", data->uids.uids[i]);
        break;
    case SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM:
    case SYSCALL_THROTTLE_IOC_UNREGISTER_PROGRAM:
        printf("Programma '%s' %s.\n", data->program.name,
               request == SYSCALL_THROTTLE_IOC_REGISTER_PROGRAM ?
               "registrato" : "deregistrato");
        break;
    case SYSCALL_THROTTLE_IOC_GET_PROGRAMS:
        printf("Programmi registrati: %u\n", data->programs.count);
        if (data->programs.count == 0)
            printf("  nessuno\n");
        for (i = 0; i < data->programs.count; ++i)
            printf("  %s\n", data->programs.programs[i].name);
        break;
    case SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL:
    case SYSCALL_THROTTLE_IOC_UNREGISTER_SYSCALL:
        printf("Syscall %u %s.\n", data->value,
               request == SYSCALL_THROTTLE_IOC_REGISTER_SYSCALL ?
               "registrata" : "deregistrata");
        break;
    case SYSCALL_THROTTLE_IOC_GET_SYSCALLS:
        printf("Syscall registrate: %u\n", data->syscalls.count);
        if (data->syscalls.count == 0)
            printf("  nessuna\n");
        for (i = 0; i < data->syscalls.count; ++i)
            printf("  %u\n", data->syscalls.numbers[i]);
        break;
    case SYSCALL_THROTTLE_IOC_GET_STATS:
        print_statistics(&data->statistics);
        break;
    }
}

int syscall_throttle_controller_run(int argc, char *argv[])
{
    const struct controller_command *command;
    union controller_data data = {0};
    int fd;
    int status = 0;

    if (argc < 2 || (command = find_command(argv[1])) == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    if (validate_arguments(command, argc, argv, &data) != 0)
        return 1;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd == -1) {
        perror("Impossibile aprire " DEVICE_PATH);
        return 1;
    }

    if (ioctl(fd, command->request, ioctl_argument(command->request, &data)) == -1) {
        perror(command->error_message);
        status = 1;
    } else {
        print_result(command->request, &data);
    }

    if (close(fd) == -1) {
        perror("Chiusura del device fallita");
        status = 1;
    }

    return status;
}
