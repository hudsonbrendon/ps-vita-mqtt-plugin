/* minunit — tiny single-header unit-test framework (public domain).
 * Macros:
 *   mu_assert(message, test)         — fail with `message` if `test` is 0
 *   mu_run_test(test)                — run `test()`, propagate failures
 * Each test function returns NULL on success or a char* error message. */
#ifndef MINUNIT_H
#define MINUNIT_H
#include <stdio.h>
extern int mu_tests_run;
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { \
    const char *message = test(); mu_tests_run++; \
    if (message) return message; \
} while (0)
#endif
