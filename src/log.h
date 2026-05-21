#ifndef PSVITA_MQTT_LOG_H
#define PSVITA_MQTT_LOG_H

void log_write(const char *level, const char *fmt, ...);

#define LOGF(...) log_write("INFO", __VA_ARGS__)
#define LOGE(...) log_write("ERR ", __VA_ARGS__)

#endif
