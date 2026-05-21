#include "../third_party/minunit/minunit.h"
#include "../src/mqtt/mqtt_packet.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_pingreq_is_two_bytes(void) {
    uint8_t buf[8];
    int n = mqtt_build_pingreq(buf, sizeof buf);
    mu_assert("PINGREQ should be exactly 2 bytes", n == 2);
    mu_assert("PINGREQ fixed header type should be 0xC0", buf[0] == 0xC0);
    mu_assert("PINGREQ remaining length should be 0", buf[1] == 0x00);
    return NULL;
}

static const char *test_disconnect_is_two_bytes(void) {
    uint8_t buf[8];
    int n = mqtt_build_disconnect(buf, sizeof buf);
    mu_assert("DISCONNECT should be exactly 2 bytes", n == 2);
    mu_assert("DISCONNECT type byte should be 0xE0", buf[0] == 0xE0);
    return NULL;
}

static const char *test_publish_retain_flag_set(void) {
    uint8_t buf[64];
    int n = mqtt_build_publish(buf, sizeof buf, "a/b",
                               (const uint8_t *)"x", 1, /*retain*/1);
    mu_assert("PUBLISH should succeed", n > 0);
    /* PUBLISH fixed header: 0x30 | (retain ? 0x01 : 0) */
    mu_assert("retain bit should be set", (buf[0] & 0x01) == 0x01);
    mu_assert("type should be PUBLISH (0x3)", (buf[0] >> 4) == 0x03);
    return NULL;
}

static const char *test_publish_topic_and_payload_present(void) {
    uint8_t buf[64];
    int n = mqtt_build_publish(buf, sizeof buf, "ha/x",
                               (const uint8_t *)"42", 2, /*retain*/0);
    mu_assert("PUBLISH should succeed", n > 0);
    mu_assert("topic 'ha/x' should appear in output",
              memmem(buf, n, "ha/x", 4) != NULL);
    mu_assert("payload '42' should appear in output",
              memmem(buf, n, "42", 2) != NULL);
    return NULL;
}

static const char *test_connect_carries_client_id(void) {
    uint8_t buf[256];
    int n = mqtt_build_connect(buf, sizeof buf, "psvita-test",
                               NULL, NULL, NULL, NULL, 30);
    mu_assert("CONNECT should succeed", n > 0);
    mu_assert("client id should appear in output",
              memmem(buf, n, "psvita-test", 11) != NULL);
    mu_assert("protocol name 'MQTT' should appear",
              memmem(buf, n, "MQTT", 4) != NULL);
    return NULL;
}

static const char *test_connect_with_lwt_carries_will_topic(void) {
    uint8_t buf[256];
    int n = mqtt_build_connect(buf, sizeof buf, "psvita-test",
                               NULL, NULL,
                               "psvita/availability", "offline", 30);
    mu_assert("CONNECT should succeed", n > 0);
    mu_assert("will topic should appear",
              memmem(buf, n, "psvita/availability", 19) != NULL);
    mu_assert("will payload should appear",
              memmem(buf, n, "offline", 7) != NULL);
    return NULL;
}

static const char *test_overflow_returns_negative(void) {
    uint8_t buf[4];
    int n = mqtt_build_publish(buf, sizeof buf,
                               "this/topic/is/way/too/long",
                               (const uint8_t *)"x", 1, 0);
    mu_assert("overflow should return -1", n < 0);
    return NULL;
}

static const char *all_tests(void) {
    mu_run_test(test_pingreq_is_two_bytes);
    mu_run_test(test_disconnect_is_two_bytes);
    mu_run_test(test_publish_retain_flag_set);
    mu_run_test(test_publish_topic_and_payload_present);
    mu_run_test(test_connect_carries_client_id);
    mu_run_test(test_connect_with_lwt_carries_will_topic);
    mu_run_test(test_overflow_returns_negative);
    return NULL;
}

int main(void) {
    const char *r = all_tests();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
