#include "publisher.h"
#include "mqtt/mqtt_client.h"
#include "ha/ha_discovery.h"
#include "collectors/collectors.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* Sleep abstraction — Vita uses ksceKernelDelayThread, host uses sleep(). */
#ifdef PSVITA_KERNEL_BUILD
  #include <psp2kern/kernel/threadmgr.h>
  static void sleep_seconds(unsigned s) { ksceKernelDelayThread(s * 1000000); }
#else
  #include <unistd.h>
  static void sleep_seconds(unsigned s) { sleep(s); }
#endif

/* --- discovery setup -------------------------------------------------- */

typedef struct {
    const char *kind;
    const char *object_id;
    const char *friendly;
    const char *state_suffix;
    const char *unit;
    const char *device_class;
} sensor_def;

static const sensor_def SENSORS[] = {
    { "sensor",        "battery_level",   "Battery Level",      "battery/level",    "%",   "battery"     },
    { "binary_sensor", "battery_charging","Battery Charging",   "battery/charging", NULL,  "battery_charging" },
    { "sensor",        "battery_temp",    "Battery Temperature","battery/temp",     "\xc2\xb0""C", "temperature" },
    { "sensor",        "battery_minutes", "Battery Remaining",  "battery/minutes",  "min", NULL          },
    { "sensor",        "firmware",        "Firmware",           "system/firmware",  NULL,  NULL          },
    { "sensor",        "model",           "Model",              "system/model",     NULL,  NULL          },
    { "sensor",        "uptime",          "Uptime",             "system/uptime",    "s",   NULL          },
    { "sensor",        "plugin_uptime",   "Plugin Uptime",      "plugin/uptime",    "s",   NULL          },
    { "binary_sensor", "in_game",         "In Game",            "app/in_game",      NULL,  NULL          },
    { "sensor",        "title_id",        "Title ID",           "app/title_id",     NULL,  NULL          },
    { "sensor",        "game_name",       "Game",               "app/game_name",    NULL,  NULL          },
    { "binary_sensor", "link",            "Network Link",       "net/link",         NULL,  "connectivity"},
    { "sensor",        "ip",              "IP Address",         "net/ip",           NULL,  NULL          },
    { "sensor",        "ssid",            "SSID",               "net/ssid",         NULL,  NULL          },
    { "sensor",        "rssi",            "WiFi RSSI",          "net/rssi",         "dBm", "signal_strength" },
};
#define N_SENSORS (sizeof SENSORS / sizeof SENSORS[0])

static int publish_discovery(mqtt_client *cli, const ha_ctx *ctx) {
    for (size_t i = 0; i < N_SENSORS; i++) {
        const sensor_def *s = &SENSORS[i];
        char topic[160], payload[1024];
        if (ha_discovery_topic(topic, sizeof topic, ctx, s->kind, s->object_id) < 0) return -1;
        if (ha_discovery_payload(payload, sizeof payload, ctx,
                                 s->kind, s->object_id, s->friendly,
                                 s->state_suffix, s->unit, s->device_class) < 0) return -1;
        if (mqtt_client_publish(cli, topic, (const uint8_t *)payload,
                                strlen(payload), /*retain*/1) < 0) return -1;
    }
    return 0;
}

static int publish_state(mqtt_client *cli, const ha_ctx *ctx,
                         uint64_t plugin_started_at) {
    battery_state b; system_state s; app_state a; network_state n;
    collector_battery(&b);
    collector_system(&s, plugin_started_at);
    collector_app(&a);
    collector_network(&n);

    #define PUB(suffix, fmt, ...) do { \
        char t[160], v[128]; \
        ha_state_topic(t, sizeof t, ctx, suffix); \
        int vn = snprintf(v, sizeof v, fmt, __VA_ARGS__); \
        if (vn < 0) return -1; \
        if (mqtt_client_publish(cli, t, (const uint8_t *)v, vn, 1) < 0) return -1; \
    } while (0)

    PUB("battery/level",    "%d", b.level_pct);
    PUB("battery/charging", "%s", b.charging > 0 ? "ON" : "OFF");
    PUB("battery/temp",     "%d.%d", b.temp_celsius_x10/10, b.temp_celsius_x10 % 10);
    PUB("battery/minutes",  "%d", b.remaining_minutes);
    PUB("system/firmware",  "%s", s.firmware);
    PUB("system/model",     "%s", s.model);
    PUB("system/uptime",    "%llu", (unsigned long long)s.system_uptime_sec);
    PUB("plugin/uptime",    "%llu", (unsigned long long)s.plugin_uptime_sec);
    PUB("app/in_game",      "%s", a.in_game ? "ON" : "OFF");
    PUB("app/title_id",     "%s", a.title_id);
    PUB("app/game_name",    "%s", a.game_name);
    PUB("net/link",         "%s", n.link_up ? "ON" : "OFF");
    PUB("net/ip",           "%s", n.ip);
    PUB("net/ssid",         "%s", n.ssid);
    PUB("net/rssi",         "%d", n.rssi_dbm);

    /* Availability comes last so HA flips entities to "available" only
     * after a complete state burst. Retained, online. */
    char avail[160];
    snprintf(avail, sizeof avail, "%s/%s/availability", ctx->topic_prefix, ctx->client_id);
    return mqtt_client_publish(cli, avail, (const uint8_t *)"online", 6, 1);
}

int publisher_publish_once(const mqtt_config *cfg) {
    ha_ctx ctx = {
        .discovery_prefix = cfg->discovery_prefix,
        .topic_prefix     = cfg->topic_prefix,
        .client_id        = cfg->client_id,
        .device_name      = cfg->device_name,
    };
    char avail_topic[160];
    snprintf(avail_topic, sizeof avail_topic, "%s/%s/availability",
             cfg->topic_prefix, cfg->client_id);

    mqtt_client_config mc = {
        .host          = cfg->broker_host,
        .port          = cfg->broker_port,
        .client_id     = cfg->client_id,
        .username      = cfg->username[0] ? cfg->username : NULL,
        .password      = cfg->password[0] ? cfg->password : NULL,
        .will_topic    = avail_topic,
        .will_payload  = "offline",
        .keepalive_sec = 60,
    };

    mqtt_client *cli = mqtt_client_open(&mc);
    if (!cli) return -1;
    if (publish_discovery(cli, &ctx) < 0) { mqtt_client_close(cli); return -1; }
    if (publish_state(cli, &ctx, 0)    < 0) { mqtt_client_close(cli); return -1; }
    mqtt_client_close(cli);
    return 0;
}

void publisher_run(const mqtt_config *cfg, volatile int *stop_flag) {
    ha_ctx ctx = {
        .discovery_prefix = cfg->discovery_prefix,
        .topic_prefix     = cfg->topic_prefix,
        .client_id        = cfg->client_id,
        .device_name      = cfg->device_name,
    };
    char avail_topic[160];
    snprintf(avail_topic, sizeof avail_topic, "%s/%s/availability",
             cfg->topic_prefix, cfg->client_id);

    unsigned backoff = 1;
    uint64_t plugin_started_at = 0; /* fill from system uptime later */

    while (!*stop_flag) {
        mqtt_client_config mc = {
            .host = cfg->broker_host, .port = cfg->broker_port,
            .client_id = cfg->client_id,
            .username = cfg->username[0] ? cfg->username : NULL,
            .password = cfg->password[0] ? cfg->password : NULL,
            .will_topic = avail_topic,
            .will_payload = "offline",
            .keepalive_sec = 60,
        };
        mqtt_client *cli = mqtt_client_open(&mc);
        if (!cli) {
            LOGE("connect failed, retry in %us", backoff);
            sleep_seconds(backoff);
            if (backoff < 60) backoff *= 2;
            continue;
        }
        backoff = 1;
        if (publish_discovery(cli, &ctx) < 0) {
            mqtt_client_close(cli);
            continue;
        }
        while (!*stop_flag) {
            if (publish_state(cli, &ctx, plugin_started_at) < 0) break;
            sleep_seconds(cfg->poll_interval_sec);
            if (mqtt_client_ping(cli) < 0) break;
        }
        mqtt_client_close(cli);
    }
}
