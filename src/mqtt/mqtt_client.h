#ifndef PSVITA_MQTT_CLIENT_H
#define PSVITA_MQTT_CLIENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct mqtt_client mqtt_client;

typedef struct {
    const char *host;
    uint16_t    port;
    const char *client_id;
    const char *username;     /* NULL → no auth */
    const char *password;     /* NULL → no auth */
    const char *will_topic;
    const char *will_payload;
    uint16_t    keepalive_sec;
} mqtt_client_config;

mqtt_client *mqtt_client_open(const mqtt_client_config *cfg);
int  mqtt_client_publish(mqtt_client *c, const char *topic,
                         const uint8_t *payload, size_t len, int retain);
int  mqtt_client_ping(mqtt_client *c);
void mqtt_client_close(mqtt_client *c);

#endif
