#include "mqtt_socket.h"
#include <psp2/net/net.h>
#include <psp2/kernel/clib.h>

/* Single static instance — plugin opens at most one socket at a time. */
struct mqtt_socket { int fd; int in_use; };
static struct mqtt_socket g_sock = { -1, 0 };

static void set_short_recv_timeout(int fd) {
    struct {
        unsigned int sec;
        unsigned int usec;
    } tv = { 0, 200 * 1000 };
    sceNetSetsockopt(fd, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &tv, sizeof tv);
}

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    if (g_sock.in_use) return NULL;
    int fd = sceNetSocket("mqtt", SCE_NET_AF_INET,
                          SCE_NET_SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    SceNetSockaddrIn addr;
    sceClibMemset(&addr, 0, sizeof addr);
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port   = sceNetHtons(port);
    sceNetInetPton(SCE_NET_AF_INET, host, &addr.sin_addr);

    if (sceNetConnect(fd, (SceNetSockaddr *)&addr, sizeof addr) < 0) {
        sceNetSocketClose(fd);
        return NULL;
    }

    g_sock.fd = fd;
    g_sock.in_use = 1;
    return &g_sock;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return sceNetSend(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return sceNetRecv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s || !s->in_use) return;
    set_short_recv_timeout(s->fd);
    uint8_t drain[64];
    while (sceNetRecv(s->fd, drain, sizeof drain, 0) > 0) {}
    sceNetSocketClose(s->fd);
    s->fd = -1;
    s->in_use = 0;
}
