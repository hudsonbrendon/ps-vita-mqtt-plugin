#ifndef PSVITA_MQTT_CONFIG_H
#define PSVITA_MQTT_CONFIG_H

#include <stdint.h>

typedef struct {
    char     broker_host[64];
    uint16_t broker_port;
    char     username[64];
    char     password[64];
    char     client_id[64];
    char     device_name[64];
    char     topic_prefix[64];
    char     discovery_prefix[64];
    uint32_t poll_interval_sec;
} mqtt_config;

/* Returns 0 on success, -1 on parse failure. */
int config_parse_json(const char *json, mqtt_config *out);

/* Convenience: read entire file and call config_parse_json.
 * Returns 0 / -1. Implemented on host using fopen, on Vita using
 * ksceIoOpen/ksceIoRead. */
int config_load_from_path(const char *path, mqtt_config *out);

#endif
