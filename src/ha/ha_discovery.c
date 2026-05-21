#include "ha_discovery.h"
#include <stdio.h>
#include <string.h>

/* Minimal JSON-string escaper: writes a quoted, escaped copy of `s` into
 * `out` (which must have room for 2 + 2*strlen(s) bytes). Returns chars
 * written on success or -1 on overflow. */
static int json_escape(char *out, size_t cap, const char *s) {
    if (!out || !s || cap < 3) return -1;
    size_t pos = 0;
    out[pos++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (pos + 6 >= cap) return -1;
        unsigned char c = *p;
        if (c == '"' || c == '\\')      { out[pos++] = '\\'; out[pos++] = c; }
        else if (c == '\n')             { out[pos++] = '\\'; out[pos++] = 'n'; }
        else if (c == '\r')             { out[pos++] = '\\'; out[pos++] = 'r'; }
        else if (c == '\t')             { out[pos++] = '\\'; out[pos++] = 't'; }
        else if (c < 0x20)              { pos += snprintf(out + pos, cap - pos, "\\u%04x", c); }
        else                            { out[pos++] = c; }
    }
    if (pos + 2 >= cap) return -1;
    out[pos++] = '"';
    out[pos]   = '\0';
    return (int)pos;
}

int ha_discovery_topic(char *out, size_t cap, const ha_ctx *ctx,
                       const char *kind, const char *object_id) {
    int n = snprintf(out, cap, "%s/%s/%s_%s/config",
                     ctx->discovery_prefix, kind,
                     ctx->client_id, object_id);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}

int ha_state_topic(char *out, size_t cap, const ha_ctx *ctx,
                   const char *state_topic_suffix) {
    int n = snprintf(out, cap, "%s/%s/%s",
                     ctx->topic_prefix, ctx->client_id, state_topic_suffix);
    return (n > 0 && (size_t)n < cap) ? n : -1;
}

int ha_discovery_payload(char *out, size_t cap, const ha_ctx *ctx,
                         const char *kind, const char *object_id,
                         const char *friendly_name,
                         const char *state_topic_suffix,
                         const char *unit, const char *device_class) {
    char state_topic[160];
    if (ha_state_topic(state_topic, sizeof state_topic, ctx,
                       state_topic_suffix) < 0) return -1;

    /* Note: kind is part of the topic, not the payload — silences -Wunused. */
    (void)kind;

    char avail_topic[160];
    int an = snprintf(avail_topic, sizeof avail_topic, "%s/%s/availability",
                      ctx->topic_prefix, ctx->client_id);
    if (an < 0 || an >= (int)sizeof avail_topic) return -1;

    /* Escape user-controlled strings (friendly_name, device_name) so a
     * quote or backslash in config.json can't break the JSON document. */
    char esc_name[160];
    if (json_escape(esc_name, sizeof esc_name, friendly_name) < 0) return -1;

    int pos = snprintf(out, cap,
        "{"
          "\"name\":%s,"
          "\"unique_id\":\"%s_%s\","
          "\"state_topic\":\"%s\","
          "\"availability_topic\":\"%s\","
          "\"payload_available\":\"online\","
          "\"payload_not_available\":\"offline\"",
        esc_name, ctx->client_id, object_id,
        state_topic, avail_topic);
    if (pos < 0 || (size_t)pos >= cap) return -1;

    if (unit) {
        int n = snprintf(out + pos, cap - pos,
            ",\"unit_of_measurement\":\"%s\"", unit);
        if (n < 0 || (size_t)(pos + n) >= cap) return -1;
        pos += n;
    }
    if (device_class) {
        int n = snprintf(out + pos, cap - pos,
            ",\"device_class\":\"%s\"", device_class);
        if (n < 0 || (size_t)(pos + n) >= cap) return -1;
        pos += n;
    }

    char esc_dev[160];
    if (json_escape(esc_dev, sizeof esc_dev, ctx->device_name) < 0) return -1;

    int n = snprintf(out + pos, cap - pos,
        ",\"device\":{"
          "\"identifiers\":[\"%s\"],"
          "\"name\":%s,"
          "\"manufacturer\":\"Sony\","
          "\"model\":\"PlayStation Vita\""
        "}}",
        ctx->client_id, esc_dev);
    if (n < 0 || (size_t)(pos + n) >= cap) return -1;
    pos += n;

    return pos;
}
