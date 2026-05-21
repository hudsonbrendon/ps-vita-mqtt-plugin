/* Pure-host smoke test: assemble a config and call publisher_publish_once
 * against a mosquitto broker running on localhost. Skipped if MOSQUITTO_HOST
 * env var is not set, so unit-test runs don't depend on a broker. */
#include "../third_party/minunit/minunit.h"
#include "../src/publisher.h"
#include "../src/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_publish_once_against_local_broker(void) {
    const char *host = getenv("MOSQUITTO_HOST");
    if (!host) {
        printf("(skip: MOSQUITTO_HOST not set)\n");
        return NULL;
    }
    mqtt_config c = {0};
    strcpy(c.broker_host, host);
    c.broker_port = 1883;
    strcpy(c.client_id, "psvita-test");
    strcpy(c.device_name, "PS Vita Test");
    strcpy(c.topic_prefix, "psvita");
    strcpy(c.discovery_prefix, "homeassistant");
    c.poll_interval_sec = 5;
    mu_assert("publish_once should succeed",
              publisher_publish_once(&c) == 0);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_publish_once_against_local_broker);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
