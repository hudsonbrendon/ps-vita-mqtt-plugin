#include "../third_party/minunit/minunit.h"
#include "../src/ha/ha_discovery.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static ha_ctx ctx = {
    .discovery_prefix = "homeassistant",
    .topic_prefix     = "psvita",
    .client_id        = "psvita-test",
    .device_name      = "PS Vita",
};

static const char *test_discovery_topic_shape(void) {
    char t[128];
    int n = ha_discovery_topic(t, sizeof t, &ctx, "sensor", "battery_level");
    mu_assert("ok", n > 0);
    mu_assert("topic shape",
        strcmp(t, "homeassistant/sensor/psvita-test_battery_level/config") == 0);
    return NULL;
}

static const char *test_state_topic_shape(void) {
    char t[128];
    int n = ha_state_topic(t, sizeof t, &ctx, "battery/level");
    mu_assert("ok", n > 0);
    mu_assert("state topic shape",
        strcmp(t, "psvita/psvita-test/battery/level") == 0);
    return NULL;
}

static const char *test_payload_contains_device_block(void) {
    char p[1024];
    int n = ha_discovery_payload(p, sizeof p, &ctx,
        "sensor", "battery_level", "Battery Level",
        "battery/level", "%", "battery");
    mu_assert("ok", n > 0);
    mu_assert("has name", strstr(p, "Battery Level") != NULL);
    mu_assert("has unique_id", strstr(p, "psvita-test_battery_level") != NULL);
    mu_assert("has state_topic",
        strstr(p, "psvita/psvita-test/battery/level") != NULL);
    mu_assert("has unit", strstr(p, "\"unit_of_measurement\":\"%\"") != NULL);
    mu_assert("has device block", strstr(p, "\"device\"") != NULL);
    mu_assert("device has name", strstr(p, "\"PS Vita\"") != NULL);
    return NULL;
}

static const char *test_payload_no_unit_no_device_class(void) {
    char p[1024];
    int n = ha_discovery_payload(p, sizeof p, &ctx,
        "binary_sensor", "in_game", "In Game",
        "game/in_game", NULL, NULL);
    mu_assert("ok", n > 0);
    mu_assert("no unit field", strstr(p, "unit_of_measurement") == NULL);
    mu_assert("no device_class field", strstr(p, "device_class") == NULL);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_discovery_topic_shape);
    mu_run_test(test_state_topic_shape);
    mu_run_test(test_payload_contains_device_block);
    mu_run_test(test_payload_no_unit_no_device_class);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
