#include "test_common.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>

#define TEST_PROGRAM_NAME "soa-throttle"
#define TEST_MAX 1U

#define MINIMUM_EXPECTED_DELAY_NS 400000000ULL
#define MAXIMUM_ATTEMPTS 3U

static uint64_t measure_second_invocation(
    struct test_fixture *fixture)
{
    uint64_t start_ns;

    test_fixture_disable_monitor(fixture);
    test_fixture_enable_monitor(fixture);

    test_invoke_syscall(fixture->syscall_number);

    start_ns = test_monotonic_time_ns();

    test_invoke_syscall(fixture->syscall_number);

    return test_monotonic_time_ns() - start_ns;
}

static void verify_throttling_delay(
    struct test_fixture *fixture)
{
    uint64_t delay_ns;
    uint64_t delay_ms;
    unsigned int attempt;

    for (attempt = 1;
         attempt <= MAXIMUM_ATTEMPTS;
         ++attempt) {
        delay_ns = measure_second_invocation(fixture);
        delay_ms = delay_ns / 1000000ULL;

        printf(
            "INFO: tentativo %u, ritardo = "
            "%" PRIu64 " ms\n",
            attempt,
            delay_ms
        );

        if (delay_ns >= MINIMUM_EXPECTED_DELAY_NS) {
            test_pass(
                "seconda syscall bloccata fino "
                "alla finestra successiva"
            );
            return;
        }
    }

    test_fail(
        "ritardo insufficiente dopo %u tentativi: "
        "%" PRIu64 " ms",
        MAXIMUM_ATTEMPTS,
        delay_ms
    );
}

int main(void)
{
    struct test_fixture fixture;

    test_fixture_setup(
        &fixture,
        TEST_PROGRAM_NAME,
        TEST_MAX,
        (__u32)SYS_getpid
    );

    verify_throttling_delay(&fixture);

    test_fixture_cleanup(&fixture);

    test_pass("test del throttling completato");

    return EXIT_SUCCESS;
}
