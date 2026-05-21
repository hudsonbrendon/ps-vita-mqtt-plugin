#include "../third_party/minunit/minunit.h"
#include "../src/config.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_parse_full(void) {
    const char *json =
      "{\"broker_host\":\"10.0.0.5\",\"broker_port\":1883,"
      "\"username\":\"u\",\"password\":\"p\","
      "\"client_id\":\"psvita-1\",\"device_name\":\"PS Vita\","
      "\"topic_prefix\":\"psvita\",\"discovery_prefix\":\"homeassistant\","
      "\"poll_interval_sec\":7}";
    mqtt_config c = {0};
    mu_assert("parse ok", config_parse_json(json, &c) == 0);
    mu_assert("host", strcmp(c.broker_host, "10.0.0.5") == 0);
    mu_assert("port", c.broker_port == 1883);
    mu_assert("client", strcmp(c.client_id, "psvita-1") == 0);
    mu_assert("interval", c.poll_interval_sec == 7);
    return NULL;
}

static const char *test_defaults_when_missing(void) {
    mqtt_config c = {0};
    mu_assert("parse ok",
        config_parse_json("{\"broker_host\":\"x\"}", &c) == 0);
    mu_assert("default port 1883", c.broker_port == 1883);
    mu_assert("default prefix",
        strcmp(c.topic_prefix, "psvita") == 0);
    mu_assert("default discovery",
        strcmp(c.discovery_prefix, "homeassistant") == 0);
    mu_assert("default interval 5",
        c.poll_interval_sec == 5);
    return NULL;
}

static const char *test_garbage_input_returns_error(void) {
    mqtt_config c = {0};
    mu_assert("malformed json should fail",
        config_parse_json("not json at all", &c) == -1);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_parse_full);
    mu_run_test(test_defaults_when_missing);
    mu_run_test(test_garbage_input_returns_error);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
