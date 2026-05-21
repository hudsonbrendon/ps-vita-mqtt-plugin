#include "mqtt_socket.h"
#include <psp2/net/net.h>
#include <stdlib.h>
#include <string.h>

/* SO_RCVTIMEO with a small timeout — used right before close so the
 * drain loop cannot block module_stop indefinitely on a stale link. */
static void set_short_recv_timeout(int fd) {
    struct {
        unsigned int sec;
        unsigned int usec;
    } tv = { 0, 200 * 1000 };
    sceNetSetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &tv, sizeof tv);
}

struct mqtt_socket { int fd; };

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    int fd = sceNetSocket("mqtt", SCE_NET_AF_INET,
                          SCE_NET_SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    SceNetSockaddrIn addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port   = sceNetHtons(port);
    sceNetInetPton(SCE_NET_AF_INET, host, &addr.sin_addr);

    if (sceNetConnect(fd, (SceNetSockaddr *)&addr, sizeof addr) < 0) {
        sceNetSocketClose(fd);
        return NULL;
    }

    mqtt_socket *s = malloc(sizeof *s);
    if (!s) { sceNetSocketClose(fd); return NULL; }
    s->fd = fd;
    return s;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return sceNetSend(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return sceNetRecv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s) return;
    set_short_recv_timeout(s->fd);
    uint8_t drain[64];
    while (sceNetRecv(s->fd, drain, sizeof drain, 0) > 0) {}
    sceNetSocketClose(s->fd);
    free(s);
}
