#include "collectors.h"
#include <string.h>

int collector_network(network_state *out) {
    out->link_up = 1;
    strcpy(out->ip, "1.2.3.4");
    strcpy(out->ssid, "TestWiFi");
    out->rssi_dbm = -55;
    return 0;
}
