#include "../third_party/minunit/minunit.h"
#include "../src/log.h"
#include <string.h>
#include <stdio.h>

int mu_tests_run = 0;

/* log_host.c writes into this buffer so the test can inspect it. */
extern char log_host_buffer[1024];
extern void log_host_reset(void);

static const char *test_logf_writes_to_buffer(void) {
    log_host_reset();
    LOGF("hello %d", 42);
    mu_assert("LOGF should contain formatted text",
              strstr(log_host_buffer, "hello 42") != NULL);
    return NULL;
}

static const char *all_tests(void) {
    mu_run_test(test_logf_writes_to_buffer);
    return NULL;
}

int main(void) {
    const char *result = all_tests();
    if (result) { printf("FAIL: %s\n", result); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
