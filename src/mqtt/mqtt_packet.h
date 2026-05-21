#ifndef PSVITA_MQTT_PACKET_H
#define PSVITA_MQTT_PACKET_H

#include <stddef.h>
#include <stdint.h>

/* All builders return total bytes written into `out`, or -1 on overflow.
 * Caller owns `out`. No allocation. */

int mqtt_build_connect(uint8_t *out, size_t cap,
                       const char *client_id,
                       const char *username, const char *password,
                       const char *will_topic, const char *will_payload,
                       uint16_t keepalive_sec);

int mqtt_build_publish(uint8_t *out, size_t cap,
                       const char *topic,
                       const uint8_t *payload, size_t payload_len,
                       int retain);

int mqtt_build_pingreq(uint8_t *out, size_t cap);

int mqtt_build_disconnect(uint8_t *out, size_t cap);

#endif
