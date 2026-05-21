#include "mqtt_packet.h"
#include <string.h>

/* MQTT 3.1.1 variable-length remaining-length encoder.
 * Writes 1–4 bytes starting at *p. Returns bytes written. */
static int encode_remaining_length(uint8_t *p, size_t len) {
    int written = 0;
    do {
        uint8_t b = len & 0x7F;
        len >>= 7;
        if (len) b |= 0x80;
        *p++ = b;
        written++;
    } while (len && written < 4);
    return written;
}

static int put_u16_be(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
    return 2;
}

static int put_string(uint8_t *p, const char *s, size_t cap, size_t used) {
    size_t len = strlen(s);
    if (used + 2 + len > cap) return -1;
    put_u16_be(p, (uint16_t)len);
    memcpy(p + 2, s, len);
    return (int)(2 + len);
}

int mqtt_build_pingreq(uint8_t *out, size_t cap) {
    if (cap < 2) return -1;
    out[0] = 0xC0;
    out[1] = 0x00;
    return 2;
}

int mqtt_build_disconnect(uint8_t *out, size_t cap) {
    if (cap < 2) return -1;
    out[0] = 0xE0;
    out[1] = 0x00;
    return 2;
}

int mqtt_build_publish(uint8_t *out, size_t cap,
                       const char *topic,
                       const uint8_t *payload, size_t payload_len,
                       int retain) {
    if (!out || !topic) return -1;
    size_t topic_len = strlen(topic);
    /* variable header = 2 + topic_len  (QoS 0 → no packet id)
     * payload        = payload_len
     * remaining_len  = variable header + payload */
    size_t remaining = 2 + topic_len + payload_len;
    if (remaining > 0x0FFFFFFF) return -1;

    uint8_t header[5];
    header[0] = 0x30 | (retain ? 0x01 : 0x00);
    int rl = encode_remaining_length(header + 1, remaining);
    size_t total = 1 + rl + remaining;
    if (total > cap) return -1;

    size_t pos = 0;
    memcpy(out + pos, header, 1 + rl); pos += 1 + rl;
    put_u16_be(out + pos, (uint16_t)topic_len); pos += 2;
    memcpy(out + pos, topic, topic_len); pos += topic_len;
    if (payload_len) {
        memcpy(out + pos, payload, payload_len);
        pos += payload_len;
    }
    return (int)pos;
}

int mqtt_build_connect(uint8_t *out, size_t cap,
                       const char *client_id,
                       const char *username, const char *password,
                       const char *will_topic, const char *will_payload,
                       uint16_t keepalive_sec) {
    if (!out || !client_id) return -1;

    uint8_t flags = 0x02; /* clean session */
    if (username) flags |= 0x80;
    if (password) flags |= 0x40;
    if (will_topic && will_payload) flags |= 0x04 | 0x20; /* will + retain */

    /* variable header: proto name "MQTT" (2+4) + level (1) + flags (1) + keepalive (2) = 10 */
    size_t varhdr = 10;
    size_t payload = 2 + strlen(client_id);
    if (will_topic)   payload += 2 + strlen(will_topic);
    if (will_payload) payload += 2 + strlen(will_payload);
    if (username)     payload += 2 + strlen(username);
    if (password)     payload += 2 + strlen(password);

    size_t remaining = varhdr + payload;
    uint8_t header[5];
    header[0] = 0x10;
    int rl = encode_remaining_length(header + 1, remaining);
    size_t total = 1 + rl + remaining;
    if (total > cap) return -1;

    size_t pos = 0;
    memcpy(out + pos, header, 1 + rl); pos += 1 + rl;
    put_u16_be(out + pos, 4); pos += 2;
    memcpy(out + pos, "MQTT", 4); pos += 4;
    out[pos++] = 0x04;          /* protocol level 4 (MQTT 3.1.1) */
    out[pos++] = flags;
    put_u16_be(out + pos, keepalive_sec); pos += 2;

    int n;
    n = put_string(out + pos, client_id, cap, pos); if (n < 0) return -1; pos += n;
    /* Will topic and payload must be written together — partial fields
     * would misalign the payload section against the Will flag. */
    if (will_topic && will_payload) {
        n = put_string(out + pos, will_topic,   cap, pos); if (n < 0) return -1; pos += n;
        n = put_string(out + pos, will_payload, cap, pos); if (n < 0) return -1; pos += n;
    }
    if (username)     { n = put_string(out + pos, username, cap, pos);     if (n < 0) return -1; pos += n; }
    if (password)     { n = put_string(out + pos, password, cap, pos);     if (n < 0) return -1; pos += n; }

    return (int)pos;
}
