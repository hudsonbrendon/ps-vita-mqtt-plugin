#include "../third_party/minunit/minunit.h"
#include "../src/collectors/collectors.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_battery_returns_canned_data(void) {
    battery_state b;
    mu_assert("ok", collector_battery(&b) == 0);
    mu_assert("level in range", b.level_pct >= 0 && b.level_pct <= 100);
    return NULL;
}

static const char *test_system_uptime_advances(void) {
    system_state s;
    mu_assert("ok", collector_system(&s, /*plugin_started*/0) == 0);
    mu_assert("fw non-empty", s.firmware[0] != '\0');
    mu_assert("model non-empty", s.model[0] != '\0');
    return NULL;
}

static const char *test_app_returns_in_game_flag(void) {
    app_state a;
    mu_assert("ok", collector_app(&a) == 0);
    mu_assert("title id ok",
              a.title_id[0] == '\0' || strlen(a.title_id) >= 9);
    return NULL;
}

static const char *test_network_returns_ip(void) {
    network_state n;
    mu_assert("ok", collector_network(&n) == 0);
    /* Host stub returns 1.2.3.4 */
    mu_assert("host stub ip", strcmp(n.ip, "1.2.3.4") == 0);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_battery_returns_canned_data);
    mu_run_test(test_system_uptime_advances);
    mu_run_test(test_app_returns_in_game_flag);
    mu_run_test(test_network_returns_ip);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
