#ifndef PSVITA_MQTT_PUBLISHER_H
#define PSVITA_MQTT_PUBLISHER_H

#include "config.h"

/* Long-running loop:
 *   while (!stop_flag) { ensure_connected; publish_all; sleep; }
 * Returns when stop_flag becomes non-zero. */
void publisher_run(const mqtt_config *cfg, volatile int *stop_flag);

/* Single pass — exposed for testing. Connects to cfg->broker, publishes
 * the full discovery + state set once, then disconnects.
 * Returns 0 on success, -1 on any failure. */
int publisher_publish_once(const mqtt_config *cfg);

#endif
