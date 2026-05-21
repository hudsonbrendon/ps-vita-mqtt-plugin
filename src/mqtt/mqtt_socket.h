#ifndef PSVITA_MQTT_SOCKET_H
#define PSVITA_MQTT_SOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef struct mqtt_socket mqtt_socket;

/* Opens a TCP connection to host:port. Returns NULL on failure.
 * Caller frees with mqtt_socket_close. */
mqtt_socket *mqtt_socket_open(const char *host, uint16_t port);

/* Returns bytes sent or -1. Best-effort one-shot send (no partial loop). */
int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len);

/* Returns bytes read or -1. Returns 0 on clean close. */
int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap);

/* Drains any remaining bytes (best-effort) and closes the socket. */
void mqtt_socket_close(mqtt_socket *s);

#endif
