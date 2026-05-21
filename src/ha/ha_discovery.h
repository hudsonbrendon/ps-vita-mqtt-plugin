#ifndef PSVITA_MQTT_HA_DISCOVERY_H
#define PSVITA_MQTT_HA_DISCOVERY_H

#include <stddef.h>

typedef struct {
    const char *discovery_prefix;   /* e.g. "homeassistant"      */
    const char *topic_prefix;       /* e.g. "psvita"             */
    const char *client_id;          /* e.g. "psvita-livingroom"  */
    const char *device_name;        /* e.g. "PS Vita"            */
} ha_ctx;

/* Builds the discovery config JSON for a sensor.
 * `kind` is "sensor", "binary_sensor", etc.
 * Returns bytes written (excluding NUL), or -1 on overflow.
 * The discovery TOPIC is built by ha_discovery_topic(). */
int ha_discovery_payload(char *out, size_t cap,
                         const ha_ctx *ctx,
                         const char *kind,
                         const char *object_id,
                         const char *friendly_name,
                         const char *state_topic_suffix,
                         const char *unit_of_measurement,
                         const char *device_class);

/* Builds: "<discovery_prefix>/<kind>/<client_id>_<object_id>/config" */
int ha_discovery_topic(char *out, size_t cap,
                       const ha_ctx *ctx,
                       const char *kind,
                       const char *object_id);

/* Builds: "<topic_prefix>/<client_id>/<state_topic_suffix>" */
int ha_state_topic(char *out, size_t cap,
                   const ha_ctx *ctx,
                   const char *state_topic_suffix);

#endif
