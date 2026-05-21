#include "config.h"
#include "../third_party/cJSON/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_str(char *dst, size_t cap, const char *src, const char *fallback) {
    const char *use = (src && *src) ? src : fallback;
    strncpy(dst, use ? use : "", cap - 1);
    dst[cap - 1] = '\0';
}

int config_parse_json(const char *json, mqtt_config *out) {
    if (!json || !out) return -1;
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    cJSON *j;
    j = cJSON_GetObjectItem(root, "broker_host");
    copy_str(out->broker_host, sizeof out->broker_host,
             cJSON_IsString(j) ? j->valuestring : NULL, "");
    j = cJSON_GetObjectItem(root, "broker_port");
    out->broker_port = (uint16_t)(cJSON_IsNumber(j) ? j->valueint : 1883);
    j = cJSON_GetObjectItem(root, "username");
    copy_str(out->username, sizeof out->username,
             cJSON_IsString(j) ? j->valuestring : NULL, "");
    j = cJSON_GetObjectItem(root, "password");
    copy_str(out->password, sizeof out->password,
             cJSON_IsString(j) ? j->valuestring : NULL, "");
    j = cJSON_GetObjectItem(root, "client_id");
    copy_str(out->client_id, sizeof out->client_id,
             cJSON_IsString(j) ? j->valuestring : NULL, "psvita");
    j = cJSON_GetObjectItem(root, "device_name");
    copy_str(out->device_name, sizeof out->device_name,
             cJSON_IsString(j) ? j->valuestring : NULL, "PS Vita");
    j = cJSON_GetObjectItem(root, "topic_prefix");
    copy_str(out->topic_prefix, sizeof out->topic_prefix,
             cJSON_IsString(j) ? j->valuestring : NULL, "psvita");
    j = cJSON_GetObjectItem(root, "discovery_prefix");
    copy_str(out->discovery_prefix, sizeof out->discovery_prefix,
             cJSON_IsString(j) ? j->valuestring : NULL, "homeassistant");
    j = cJSON_GetObjectItem(root, "poll_interval_sec");
    out->poll_interval_sec = (uint32_t)(cJSON_IsNumber(j) ? j->valueint : 5);

    cJSON_Delete(root);
    return 0;
}

/* Host implementation of config_load_from_path. The Vita variant lives
 * in src/config_vita.c (added in Task 15 when module_start needs it). */
#ifndef PSVITA_BUILD
int config_load_from_path(const char *path, mqtt_config *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 16 * 1024) { fclose(f); return -1; }
    char *buf = malloc(n + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, n, f);
    fclose(f);
    buf[n] = '\0';
    int rc = config_parse_json(buf, out);
    free(buf);
    return rc;
}
#endif
