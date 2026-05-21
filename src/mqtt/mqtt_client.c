#include "mqtt_client.h"
#include "mqtt_packet.h"
#include "mqtt_socket.h"
#include "../log.h"
#include <stdlib.h>
#include <string.h>

struct mqtt_client {
    mqtt_socket *sock;
};

#define BUF 1024

mqtt_client *mqtt_client_open(const mqtt_client_config *cfg) {
    if (!cfg) return NULL;
    mqtt_socket *s = mqtt_socket_open(cfg->host, cfg->port);
    if (!s) { LOGE("socket open failed"); return NULL; }

    uint8_t buf[BUF];
    int n = mqtt_build_connect(buf, sizeof buf,
                               cfg->client_id,
                               cfg->username, cfg->password,
                               cfg->will_topic, cfg->will_payload,
                               cfg->keepalive_sec);
    if (n < 0) { LOGE("CONNECT build failed"); mqtt_socket_close(s); return NULL; }
    if (mqtt_socket_send(s, buf, n) != n) {
        LOGE("CONNECT send failed");
        mqtt_socket_close(s);
        return NULL;
    }

    /* Read CONNACK: expect exactly 4 bytes:
     *   ack[0] = 0x20 (CONNACK type),
     *   ack[1] = 0x02 (remaining length),
     *   ack[2] = session-present byte, only bit 0 may be set,
     *   ack[3] = return code, 0x00 = accepted. */
    uint8_t ack[4];
    int got = mqtt_socket_recv(s, ack, sizeof ack);
    if (got != 4 || ack[0] != 0x20 || ack[1] != 0x02
                 || (ack[2] & 0xFE) != 0x00 || ack[3] != 0x00) {
        LOGE("CONNACK rejected (got=%d code=0x%02x)",
             got, got >= 4 ? ack[3] : -1);
        mqtt_socket_close(s);
        return NULL;
    }

    mqtt_client *c = malloc(sizeof *c);
    c->sock = s;
    return c;
}

int mqtt_client_publish(mqtt_client *c, const char *topic,
                        const uint8_t *payload, size_t len, int retain) {
    if (!c) return -1;
    /* Topic+payload can be larger than 1 KB for HA Discovery configs.
     * Allocate dynamically up to 8 KB. */
    size_t need = strlen(topic) + len + 64;
    uint8_t *buf = malloc(need);
    if (!buf) return -1;
    int n = mqtt_build_publish(buf, need, topic, payload, len, retain);
    if (n < 0) { free(buf); return -1; }
    int sent = mqtt_socket_send(c->sock, buf, n);
    free(buf);
    return sent == n ? 0 : -1;
}

int mqtt_client_ping(mqtt_client *c) {
    if (!c) return -1;
    uint8_t buf[2];
    int n = mqtt_build_pingreq(buf, sizeof buf);
    return mqtt_socket_send(c->sock, buf, n) == n ? 0 : -1;
}

void mqtt_client_close(mqtt_client *c) {
    if (!c) return;
    uint8_t buf[2];
    int n = mqtt_build_disconnect(buf, sizeof buf);
    (void)mqtt_socket_send(c->sock, buf, n);
    mqtt_socket_close(c->sock);
    free(c);
}
