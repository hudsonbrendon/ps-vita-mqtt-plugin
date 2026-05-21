#include "mqtt_socket.h"
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/net/net.h>
#include <psp2kern/net/netctl.h>
#include <string.h>

struct mqtt_socket { int fd; };

/* Heap ID is allocated in module_start (see src/main.c) and exposed here. */
extern SceUID g_mqtt_heap;

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    int fd = ksceNetSocket("mqtt", SCE_NET_AF_INET,
                           SCE_NET_SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    SceNetSockaddrIn addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port   = ksceNetHtons(port);
    ksceNetInetPton(SCE_NET_AF_INET, host, &addr.sin_addr);

    if (ksceNetConnect(fd, (SceNetSockaddr *)&addr, sizeof addr) < 0) {
        ksceNetSocketClose(fd);
        return NULL;
    }

    mqtt_socket *s = ksceKernelAllocHeapMemory(g_mqtt_heap, sizeof *s);
    if (!s) { ksceNetSocketClose(fd); return NULL; }
    s->fd = fd;
    return s;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return ksceNetSend(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return ksceNetRecv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s) return;
    uint8_t drain[64];
    while (ksceNetRecv(s->fd, drain, sizeof drain, 0) > 0) {}
    ksceNetSocketClose(s->fd);
    ksceKernelFreeHeapMemory(g_mqtt_heap, s);
}
