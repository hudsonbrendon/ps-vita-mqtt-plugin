#include "collectors.h"
#include <psp2kern/net/net.h>
#include <psp2kern/net/netctl.h>
#include <string.h>

int collector_network(network_state *out) {
    memset(out, 0, sizeof *out);

    int state = 0;
    if (ksceNetCtlInetGetState(&state) < 0) return 0;
    out->link_up = (state == SCE_NETCTL_STATE_CONNECTED);

    if (!out->link_up) return 0;

    SceNetCtlInfo info;
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) == 0) {
        strncpy(out->ip, info.ip_address, sizeof out->ip - 1);
    }
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID, &info) == 0) {
        strncpy(out->ssid, info.ssid, sizeof out->ssid - 1);
    }
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_DBM, &info) == 0) {
        out->rssi_dbm = info.rssi_dbm;
    }
    return 0;
}
