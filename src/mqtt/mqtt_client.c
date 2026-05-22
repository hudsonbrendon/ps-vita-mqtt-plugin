#include "mqtt_client.h"
#include "mqtt_packet.h"
#include "mqtt_socket.h"
#include "../log.h"
#include <string.h>

#ifdef PSVITA_BUILD
  /* Static buffers — plugin has 1 client at a time, no heap. */
  struct mqtt_client { mqtt_socket *sock; int in_use; };
  static struct mqtt_client g_client = { 0, 0 };
  static uint8_t g_pub_buf[4096];
  static inline struct mqtt_client *client_alloc(void) {
      if (g_client.in_use) return 0;
      g_client.in_use = 1;
      return &g_client;
  }
  static inline void client_free(struct mqtt_client *c) {
      if (c) { c->sock = 0; c->in_use = 0; }
  }
  #define PUB_BUF      g_pub_buf
  #define PUB_BUF_CAP  sizeof(g_pub_buf)
#else
  #include <stdlib.h>
  struct mqtt_client { mqtt_socket *sock; };
  static inline struct mqtt_client *client_alloc(void) {
      return (struct mqtt_client *)malloc(sizeof(struct mqtt_client));
  }
  static inline void client_free(struct mqtt_client *c) { free(c); }
#endif

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

    uint8_t ack[4];
    int got = mqtt_socket_recv(s, ack, sizeof ack);
    if (got != 4 || ack[0] != 0x20 || ack[1] != 0x02
                 || (ack[2] & 0xFE) != 0x00 || ack[3] != 0x00) {
        LOGE("CONNACK rejected (got=%d code=0x%02x)",
             got, got >= 4 ? ack[3] : -1);
        mqtt_socket_close(s);
        return NULL;
    }

    mqtt_client *c = client_alloc();
    if (!c) { mqtt_socket_close(s); return NULL; }
    c->sock = s;
    return c;
}

int mqtt_client_publish(mqtt_client *c, const char *topic,
                        const uint8_t *payload, size_t len, int retain) {
    if (!c) return -1;
#ifdef PSVITA_BUILD
    size_t need = strlen(topic) + len + 64;
    if (need > PUB_BUF_CAP) return -1;
    int n = mqtt_build_publish(PUB_BUF, PUB_BUF_CAP, topic, payload, len, retain);
    if (n < 0) return -1;
    return mqtt_socket_send(c->sock, PUB_BUF, n) == n ? 0 : -1;
#else
    size_t need = strlen(topic) + len + 64;
    uint8_t *buf = (uint8_t *)malloc(need);
    if (!buf) return -1;
    int n = mqtt_build_publish(buf, need, topic, payload, len, retain);
    if (n < 0) { free(buf); return -1; }
    int sent = mqtt_socket_send(c->sock, buf, n);
    free(buf);
    return sent == n ? 0 : -1;
#endif
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
    client_free(c);
}
