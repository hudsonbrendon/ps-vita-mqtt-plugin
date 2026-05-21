#ifndef PSVITA_MQTT_COLLECTORS_H
#define PSVITA_MQTT_COLLECTORS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int      level_pct;          /* 0..100, -1 unknown */
    int      charging;           /* 0/1, -1 unknown    */
    int      temp_celsius_x10;   /* e.g. 312 = 31.2 °C */
    int      remaining_minutes;
} battery_state;

typedef struct {
    char     firmware[16];       /* "3.74"   */
    char     model[16];          /* "PCH-2000" or "PSTV" */
    uint64_t system_uptime_sec;
    uint64_t plugin_uptime_sec;
} system_state;

typedef struct {
    int      in_game;            /* 0/1 */
    char     title_id[16];       /* "PCSE00001" or "" */
    char     game_name[64];      /* looked up from titles.txt; may be "" */
} app_state;

typedef struct {
    int      link_up;            /* 0/1 */
    char     ip[16];             /* "192.168.1.42" or "" */
    char     ssid[34];           /* may be "" */
    int      rssi_dbm;
} network_state;

int collector_battery(battery_state *out);
int collector_system(system_state *out, uint64_t plugin_started_at_sec);
int collector_app(app_state *out);
int collector_network(network_state *out);

#endif
