#include "mqtt_socket.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct mqtt_socket { int fd; };

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    struct addrinfo hints = {0}, *res = NULL;
    char port_s[8];
    snprintf(port_s, sizeof port_s, "%u", port);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_s, &hints, &res) != 0) return NULL;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return NULL; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res); return NULL;
    }
    freeaddrinfo(res);
    mqtt_socket *s = malloc(sizeof *s);
    if (!s) { close(fd); return NULL; }
    s->fd = fd;
    return s;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return (int)send(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return (int)recv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s) return;
    uint8_t drain[64];
    while (recv(s->fd, drain, sizeof drain, MSG_DONTWAIT) > 0) {}
    close(s->fd);
    free(s);
}
