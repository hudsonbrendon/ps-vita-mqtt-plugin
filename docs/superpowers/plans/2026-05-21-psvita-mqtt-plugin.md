# PS Vita MQTT Plugin Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a taiHEN kernel plugin (`.skprx`) for jailbroken PS Vita that publishes battery, system, running-app, and WiFi telemetry to an MQTT broker so Home Assistant auto-creates the sensors via MQTT Discovery.

**Architecture:** Single kernel plugin loaded in `*KERNEL` of `ux0:tai/config.txt`, spawns one kernel thread on `module_start` that connects to a configured MQTT broker over plain TCP, publishes HA Discovery configs on each connect, then loops: collect → publish → sleep `poll_interval_sec` → ping. Hand-rolled minimal MQTT 3.1.1 publisher (no library deps). Reconnect with backoff and Last Will & Testament for availability. Collector files split per sensor group under `src/collectors/`. Each collector has a `_vita.c` (kernel APIs) and a `_host.c` (stub that returns canned data) so unit tests run on the developer's machine without a Vita.

**Tech Stack:**
- C99
- VitaSDK toolchain (`arm-vita-eabi-gcc`)
- `vita-headers` for `<psp2kern/...>` APIs (`kscePower*`, `ksceNet*`, `ksceKernel*`)
- `vita-mksfoex` + `vita-make-fself` (already in VitaSDK) for `.skprx` packaging
- cJSON (vendored) for config parsing
- minunit (vendored) for host-side unit tests
- Docker to pin the VitaSDK build environment
- Mosquitto in Docker for integration tests
- GitHub Actions for CI

---

## File Structure

Repo root: `ps-vita-mqtt-plugin/` (suggest naming the new GitHub repo this way to match `ps4-mqtt-plugin`).

```
ps-vita-mqtt-plugin/
├── Makefile                          # builds .skprx for Vita; host-tests target for unit tests
├── README.md                         # install/build/usage docs
├── config.example.json               # template for ux0:data/ps-vita-mqtt/config.json
├── .gitignore
├── docker/
│   └── Dockerfile                    # vitasdk/vitasdk:latest base + extras
├── scripts/
│   └── build-skprx.sh                # one-shot Docker-based build
├── src/
│   ├── main.c                        # module_start / module_stop, kernel thread spawn
│   ├── log.h                         # LOGF/LOGE macros, target-agnostic
│   ├── log_vita.c                    # ksceDebugPrintf impl
│   ├── log_host.c                    # printf impl for unit tests
│   ├── config.h
│   ├── config.c                      # parse ux0:data/ps-vita-mqtt/config.json via cJSON
│   ├── publisher.h
│   ├── publisher.c                   # worker loop: connect → collect → publish → sleep
│   ├── mqtt/
│   │   ├── mqtt_client.h
│   │   ├── mqtt_client.c             # connect / publish / pingreq / disconnect (3.1.1)
│   │   ├── mqtt_packet.h
│   │   ├── mqtt_packet.c             # pure packet encode (CONNECT/PUBLISH/PINGREQ/DISCONNECT)
│   │   ├── mqtt_socket.h
│   │   ├── mqtt_socket_vita.c        # ksceNetSocket / ksceNetConnect / ksceNetSend / ksceNetRecv
│   │   └── mqtt_socket_host.c        # POSIX socket impl for host tests
│   ├── collectors/
│   │   ├── collectors.h              # struct definitions + collector_* function decls
│   │   ├── battery_vita.c
│   │   ├── battery_host.c
│   │   ├── system_vita.c
│   │   ├── system_host.c
│   │   ├── app_vita.c
│   │   ├── app_host.c
│   │   ├── network_vita.c
│   │   └── network_host.c
│   └── ha/
│       ├── ha_discovery.h
│       └── ha_discovery.c            # builds HA MQTT Discovery JSON payloads
├── third_party/
│   ├── cJSON/                        # vendored cJSON.c, cJSON.h
│   └── minunit/                      # vendored minunit.h
├── tests/
│   ├── test_mqtt_packet.c
│   ├── test_config.c
│   ├── test_ha_discovery.c
│   ├── test_publisher.c
│   └── integration/
│       ├── run_integration.sh
│       └── test_mqtt_integration.c
└── .github/
    └── workflows/
        └── build-skprx.yml
```

**Decomposition rationale:**

- `collectors/` split by sensor group: each file does one thing, easy to test in isolation, easy to add new sensors later.
- `mqtt/` keeps protocol code separate from socket code so the protocol is tested with a fake socket on host.
- `_vita.c` / `_host.c` pairs let every layer build on the developer's laptop. Without this you can't TDD anything.
- `ha/` isolates Home Assistant's discovery payload shape from MQTT transport, so discovery topics can be unit-tested as pure strings.
- `main.c` is intentionally tiny — it only wires `module_start` → thread spawn → `publisher_run`.

---

## Task 1: Repo scaffolding + .gitignore + README skeleton

**Files:**
- Create: `README.md`
- Create: `.gitignore`
- Create: `config.example.json`
- Create empty placeholder dirs by adding `.gitkeep` files: `src/`, `src/mqtt/`, `src/collectors/`, `src/ha/`, `third_party/`, `tests/`, `tests/integration/`, `scripts/`, `docker/`, `.github/workflows/`

- [ ] **Step 1: Create `.gitignore`**

```
build/
*.o
*.elf
*.velf
*.self
*.suprx
*.skprx
*.sfo
*.vpk
.vscode/
.idea/
host-tests
*.dSYM/
```

- [ ] **Step 2: Create `config.example.json`**

```json
{
  "broker_host": "192.168.1.10",
  "broker_port": 1883,
  "username": "",
  "password": "",
  "client_id": "psvita-livingroom",
  "device_name": "PS Vita",
  "topic_prefix": "psvita",
  "discovery_prefix": "homeassistant",
  "poll_interval_sec": 5
}
```

- [ ] **Step 3: Create `README.md` placeholder**

```markdown
# ps-vita-mqtt-plugin

taiHEN kernel plugin for jailbroken PS Vita (HENkaku Enso, FW 3.60+) that
publishes console telemetry to an MQTT broker. Home Assistant auto-creates
the sensors via MQTT Discovery.

Full install + sensor list documented in this README after the build is
verified end-to-end (Task 20).
```

- [ ] **Step 4: Initialize git + first commit**

```bash
cd ps-vita-mqtt-plugin
git init
git add .gitignore config.example.json README.md
git commit -m "chore: scaffold repo"
```

---

## Task 2: VitaSDK Docker build environment

**Files:**
- Create: `docker/Dockerfile`
- Create: `scripts/build-skprx.sh`

- [ ] **Step 1: Write `docker/Dockerfile`**

```dockerfile
FROM vitasdk/vitasdk:latest

# vitasdk base already ships arm-vita-eabi-gcc, vita-mksfoex, vita-make-fself,
# vita-elf-create, vita-pack-vpk in /usr/local/vitasdk/bin.
# Add the bits Makefile needs.
RUN apk add --no-cache make bash

WORKDIR /work
ENV VITASDK=/usr/local/vitasdk
ENV PATH=$VITASDK/bin:$PATH
```

- [ ] **Step 2: Write `scripts/build-skprx.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

IMAGE="ps-vita-mqtt-build:local"

docker build -t "$IMAGE" "$ROOT/docker"
docker run --rm -v "$ROOT:/work" -w /work "$IMAGE" make skprx
```

- [ ] **Step 3: Make script executable**

```bash
chmod +x scripts/build-skprx.sh
```

- [ ] **Step 4: Sanity check Docker image builds**

```bash
docker build -t ps-vita-mqtt-build:local docker/
```

Expected: image builds, no errors. (Will fail if `vitasdk/vitasdk:latest`
moves; pin the digest later if it does.)

- [ ] **Step 5: Commit**

```bash
git add docker/Dockerfile scripts/build-skprx.sh
git commit -m "build: vitasdk docker image + build script"
```

---

## Task 3: Vendor third-party libs (cJSON + minunit)

**Files:**
- Create: `third_party/cJSON/cJSON.c` + `cJSON.h` (download from upstream v1.7.18)
- Create: `third_party/minunit/minunit.h`

- [ ] **Step 1: Download cJSON**

```bash
mkdir -p third_party/cJSON
curl -fsSL https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.c \
  -o third_party/cJSON/cJSON.c
curl -fsSL https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.18/cJSON.h \
  -o third_party/cJSON/cJSON.h
```

- [ ] **Step 2: Vendor minunit**

Create `third_party/minunit/minunit.h`:

```c
/* minunit — tiny single-header unit-test framework (public domain).
 * Macros:
 *   mu_assert(message, test)         — fail with `message` if `test` is 0
 *   mu_run_test(test)                — run `test()`, propagate failures
 * Each test function returns NULL on success or a char* error message. */
#ifndef MINUNIT_H
#define MINUNIT_H
#include <stdio.h>
extern int mu_tests_run;
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { \
    const char *message = test(); mu_tests_run++; \
    if (message) return message; \
} while (0)
#endif
```

- [ ] **Step 3: Commit**

```bash
git add third_party/
git commit -m "vendor: cJSON 1.7.18 + minunit"
```

---

## Task 4: Logging abstraction (TDD on the macro contract)

**Files:**
- Create: `src/log.h`
- Create: `src/log_vita.c`
- Create: `src/log_host.c`
- Create: `tests/test_log.c`

- [ ] **Step 1: Write the failing test** — create `tests/test_log.c`

```c
#include "../third_party/minunit/minunit.h"
#include "../src/log.h"
#include <string.h>
#include <stdio.h>

int mu_tests_run = 0;

/* log_host.c writes into this buffer so the test can inspect it. */
extern char log_host_buffer[1024];
extern void log_host_reset(void);

static const char *test_logf_writes_to_buffer(void) {
    log_host_reset();
    LOGF("hello %d", 42);
    mu_assert("LOGF should contain formatted text",
              strstr(log_host_buffer, "hello 42") != NULL);
    return NULL;
}

static const char *all_tests(void) {
    mu_run_test(test_logf_writes_to_buffer);
    return NULL;
}

int main(void) {
    const char *result = all_tests();
    if (result) { printf("FAIL: %s\n", result); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 2: Write `src/log.h`**

```c
#ifndef PSVITA_MQTT_LOG_H
#define PSVITA_MQTT_LOG_H

void log_write(const char *level, const char *fmt, ...);

#define LOGF(...) log_write("INFO", __VA_ARGS__)
#define LOGE(...) log_write("ERR ", __VA_ARGS__)

#endif
```

- [ ] **Step 3: Write `src/log_host.c`**

```c
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

char log_host_buffer[1024];

void log_host_reset(void) { log_host_buffer[0] = '\0'; }

void log_write(const char *level, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    size_t len = strlen(log_host_buffer);
    snprintf(log_host_buffer + len, sizeof log_host_buffer - len,
             "[%s] %s\n", level, line);
}
```

- [ ] **Step 4: Write `src/log_vita.c`**

```c
#include "log.h"
#include <psp2kern/kernel/debug.h>
#include <stdarg.h>
#include <stdio.h>

void log_write(const char *level, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    ksceDebugPrintf("[psvita-mqtt][%s] %s\n", level, line);
}
```

- [ ] **Step 5: Compile + run the host test**

```bash
cc -std=c99 -Wall -Wextra -I. \
   tests/test_log.c src/log_host.c third_party/cJSON/cJSON.c \
   -o /tmp/test_log
/tmp/test_log
```

Expected: `OK: 1 tests`. (cJSON is linked in early so we don't have to
rewrite the command later; the test doesn't call it yet.)

- [ ] **Step 6: Commit**

```bash
git add src/log.h src/log_vita.c src/log_host.c tests/test_log.c
git commit -m "feat(log): target-agnostic logging with host buffer for tests"
```

---

## Task 5: MQTT 3.1.1 packet encoder (CONNECT, PUBLISH, PINGREQ, DISCONNECT)

**Files:**
- Create: `src/mqtt/mqtt_packet.h`
- Create: `src/mqtt/mqtt_packet.c`
- Create: `tests/test_mqtt_packet.c`

- [ ] **Step 1: Write `src/mqtt/mqtt_packet.h`**

```c
#ifndef PSVITA_MQTT_PACKET_H
#define PSVITA_MQTT_PACKET_H

#include <stddef.h>
#include <stdint.h>

/* All builders return total bytes written into `out`, or -1 on overflow.
 * Caller owns `out`. No allocation. */

int mqtt_build_connect(uint8_t *out, size_t cap,
                       const char *client_id,
                       const char *username, const char *password,
                       const char *will_topic, const char *will_payload,
                       uint16_t keepalive_sec);

int mqtt_build_publish(uint8_t *out, size_t cap,
                       const char *topic,
                       const uint8_t *payload, size_t payload_len,
                       int retain);

int mqtt_build_pingreq(uint8_t *out, size_t cap);

int mqtt_build_disconnect(uint8_t *out, size_t cap);

#endif
```

- [ ] **Step 2: Write the failing test** — `tests/test_mqtt_packet.c`

```c
#include "../third_party/minunit/minunit.h"
#include "../src/mqtt/mqtt_packet.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_pingreq_is_two_bytes(void) {
    uint8_t buf[8];
    int n = mqtt_build_pingreq(buf, sizeof buf);
    mu_assert("PINGREQ should be exactly 2 bytes", n == 2);
    mu_assert("PINGREQ fixed header type should be 0xC0", buf[0] == 0xC0);
    mu_assert("PINGREQ remaining length should be 0", buf[1] == 0x00);
    return NULL;
}

static const char *test_disconnect_is_two_bytes(void) {
    uint8_t buf[8];
    int n = mqtt_build_disconnect(buf, sizeof buf);
    mu_assert("DISCONNECT should be exactly 2 bytes", n == 2);
    mu_assert("DISCONNECT type byte should be 0xE0", buf[0] == 0xE0);
    return NULL;
}

static const char *test_publish_retain_flag_set(void) {
    uint8_t buf[64];
    int n = mqtt_build_publish(buf, sizeof buf, "a/b",
                               (const uint8_t *)"x", 1, /*retain*/1);
    mu_assert("PUBLISH should succeed", n > 0);
    /* PUBLISH fixed header: 0x30 | (retain ? 0x01 : 0) */
    mu_assert("retain bit should be set", (buf[0] & 0x01) == 0x01);
    mu_assert("type should be PUBLISH (0x3)", (buf[0] >> 4) == 0x03);
    return NULL;
}

static const char *test_publish_topic_and_payload_present(void) {
    uint8_t buf[64];
    int n = mqtt_build_publish(buf, sizeof buf, "ha/x",
                               (const uint8_t *)"42", 2, /*retain*/0);
    mu_assert("PUBLISH should succeed", n > 0);
    mu_assert("topic 'ha/x' should appear in output",
              memmem(buf, n, "ha/x", 4) != NULL);
    mu_assert("payload '42' should appear in output",
              memmem(buf, n, "42", 2) != NULL);
    return NULL;
}

static const char *test_connect_carries_client_id(void) {
    uint8_t buf[256];
    int n = mqtt_build_connect(buf, sizeof buf, "psvita-test",
                               NULL, NULL, NULL, NULL, 30);
    mu_assert("CONNECT should succeed", n > 0);
    mu_assert("client id should appear in output",
              memmem(buf, n, "psvita-test", 11) != NULL);
    mu_assert("protocol name 'MQTT' should appear",
              memmem(buf, n, "MQTT", 4) != NULL);
    return NULL;
}

static const char *test_connect_with_lwt_carries_will_topic(void) {
    uint8_t buf[256];
    int n = mqtt_build_connect(buf, sizeof buf, "psvita-test",
                               NULL, NULL,
                               "psvita/availability", "offline", 30);
    mu_assert("CONNECT should succeed", n > 0);
    mu_assert("will topic should appear",
              memmem(buf, n, "psvita/availability", 19) != NULL);
    mu_assert("will payload should appear",
              memmem(buf, n, "offline", 7) != NULL);
    return NULL;
}

static const char *test_overflow_returns_negative(void) {
    uint8_t buf[4];
    int n = mqtt_build_publish(buf, sizeof buf,
                               "this/topic/is/way/too/long",
                               (const uint8_t *)"x", 1, 0);
    mu_assert("overflow should return -1", n < 0);
    return NULL;
}

static const char *all_tests(void) {
    mu_run_test(test_pingreq_is_two_bytes);
    mu_run_test(test_disconnect_is_two_bytes);
    mu_run_test(test_publish_retain_flag_set);
    mu_run_test(test_publish_topic_and_payload_present);
    mu_run_test(test_connect_carries_client_id);
    mu_run_test(test_connect_with_lwt_carries_will_topic);
    mu_run_test(test_overflow_returns_negative);
    return NULL;
}

int main(void) {
    const char *r = all_tests();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 3: Run the test to confirm it fails (link error)**

```bash
cc -std=c99 -Wall -Wextra -D_GNU_SOURCE -I. \
   tests/test_mqtt_packet.c -o /tmp/test_mqtt_packet
```

Expected: undefined references to `mqtt_build_*` — the test fails to
link because the implementation does not exist yet.

- [ ] **Step 4: Implement `src/mqtt/mqtt_packet.c`**

```c
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
    if (will_topic)   { n = put_string(out + pos, will_topic, cap, pos);   if (n < 0) return -1; pos += n; }
    if (will_payload) { n = put_string(out + pos, will_payload, cap, pos); if (n < 0) return -1; pos += n; }
    if (username)     { n = put_string(out + pos, username, cap, pos);     if (n < 0) return -1; pos += n; }
    if (password)     { n = put_string(out + pos, password, cap, pos);     if (n < 0) return -1; pos += n; }

    return (int)pos;
}
```

- [ ] **Step 5: Compile and run the test, expect PASS**

```bash
cc -std=c99 -Wall -Wextra -D_GNU_SOURCE -I. \
   tests/test_mqtt_packet.c src/mqtt/mqtt_packet.c \
   -o /tmp/test_mqtt_packet
/tmp/test_mqtt_packet
```

Expected: `OK: 7 tests`.

- [ ] **Step 6: Commit**

```bash
git add src/mqtt/mqtt_packet.h src/mqtt/mqtt_packet.c tests/test_mqtt_packet.c
git commit -m "feat(mqtt): packet encoder for CONNECT/PUBLISH/PINGREQ/DISCONNECT"
```

---

## Task 6: Socket abstraction (Vita kernel + host POSIX)

**Files:**
- Create: `src/mqtt/mqtt_socket.h`
- Create: `src/mqtt/mqtt_socket_host.c`
- Create: `src/mqtt/mqtt_socket_vita.c`

No new tests here — the socket layer is exercised end-to-end by the
integration test (Task 18). The abstraction exists so the protocol
layer remains pure and testable.

- [ ] **Step 1: Write `src/mqtt/mqtt_socket.h`**

```c
#ifndef PSVITA_MQTT_SOCKET_H
#define PSVITA_MQTT_SOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef struct mqtt_socket mqtt_socket;

/* Opens a TCP connection to host:port. Returns NULL on failure.
 * Caller frees with mqtt_socket_close. */
mqtt_socket *mqtt_socket_open(const char *host, uint16_t port);

/* Returns bytes sent or -1. Best-effort one-shot send (no partial loop). */
int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len);

/* Returns bytes read or -1. Returns 0 on clean close. */
int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap);

/* Drains any remaining bytes (best-effort) and closes the socket. */
void mqtt_socket_close(mqtt_socket *s);

#endif
```

- [ ] **Step 2: Write `src/mqtt/mqtt_socket_host.c`**

```c
#include "mqtt_socket.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct mqtt_socket { int fd; };

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    struct addrinfo hints = {0}, *res = NULL;
    char port_s[8];
    snprintf(port_s, sizeof port_s, "%u", port);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_s, &hints, &res) != 0) return NULL;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return NULL; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res); return NULL;
    }
    freeaddrinfo(res);
    mqtt_socket *s = malloc(sizeof *s);
    s->fd = fd;
    return s;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return (int)send(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return (int)recv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s) return;
    uint8_t drain[64];
    while (recv(s->fd, drain, sizeof drain, MSG_DONTWAIT) > 0) {}
    close(s->fd);
    free(s);
}
```

- [ ] **Step 3: Write `src/mqtt/mqtt_socket_vita.c`**

```c
#include "mqtt_socket.h"
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/net/net.h>
#include <psp2kern/net/netctl.h>
#include <string.h>

struct mqtt_socket { int fd; };

mqtt_socket *mqtt_socket_open(const char *host, uint16_t port) {
    int fd = ksceNetSocket("mqtt", SCE_NET_AF_INET,
                           SCE_NET_SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    SceNetSockaddrIn addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port   = ksceNetHtons(port);
    ksceNetInetPton(SCE_NET_AF_INET, host, &addr.sin_addr);

    if (ksceNetConnect(fd, (SceNetSockaddr *)&addr, sizeof addr) < 0) {
        ksceNetSocketClose(fd);
        return NULL;
    }

    /* Allocate the wrapper out of the kernel heap. */
    mqtt_socket *s = ksceKernelAllocHeapMemory(0x1000, sizeof *s);
    if (!s) { ksceNetSocketClose(fd); return NULL; }
    s->fd = fd;
    return s;
}

int mqtt_socket_send(mqtt_socket *s, const uint8_t *buf, size_t len) {
    return ksceNetSend(s->fd, buf, len, 0);
}

int mqtt_socket_recv(mqtt_socket *s, uint8_t *buf, size_t cap) {
    return ksceNetRecv(s->fd, buf, cap, 0);
}

void mqtt_socket_close(mqtt_socket *s) {
    if (!s) return;
    uint8_t drain[64];
    /* best-effort drain so DISCONNECT bytes flush */
    while (ksceNetRecv(s->fd, drain, sizeof drain, 0) > 0) {}
    ksceNetSocketClose(s->fd);
    ksceKernelFreeHeapMemory(0x1000, s);
}
```

> Note: heap id `0x1000` is a placeholder. The Makefile in Task 16 will
> register a real heap and write its id into a header. If your VitaSDK
> exposes a built-in kernel heap helper, swap to it; otherwise create
> with `ksceKernelCreateHeap("ps-vita-mqtt", 0x4000, ...)` from
> `module_start` and store the id in a static.

- [ ] **Step 4: Compile the host socket (sanity check, no linkage to tests yet)**

```bash
cc -std=c99 -Wall -Wextra -I. -c src/mqtt/mqtt_socket_host.c -o /tmp/sock.o
```

Expected: compiles cleanly. (The Vita variant is verified later when
`make skprx` runs under Docker.)

- [ ] **Step 5: Commit**

```bash
git add src/mqtt/mqtt_socket.h src/mqtt/mqtt_socket_host.c src/mqtt/mqtt_socket_vita.c
git commit -m "feat(mqtt): socket abstraction (host POSIX + Vita kernel)"
```

---

## Task 7: MQTT client (connect / publish / pingreq / disconnect)

**Files:**
- Create: `src/mqtt/mqtt_client.h`
- Create: `src/mqtt/mqtt_client.c`

No dedicated unit test here — `mqtt_client.c` is mostly orchestration
on top of `mqtt_packet.c` (already tested) and `mqtt_socket.h`
(exercised by integration test in Task 18).

- [ ] **Step 1: Write `src/mqtt/mqtt_client.h`**

```c
#ifndef PSVITA_MQTT_CLIENT_H
#define PSVITA_MQTT_CLIENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct mqtt_client mqtt_client;

typedef struct {
    const char *host;
    uint16_t    port;
    const char *client_id;
    const char *username;     /* NULL → no auth */
    const char *password;     /* NULL → no auth */
    const char *will_topic;
    const char *will_payload;
    uint16_t    keepalive_sec;
} mqtt_client_config;

mqtt_client *mqtt_client_open(const mqtt_client_config *cfg);
int  mqtt_client_publish(mqtt_client *c, const char *topic,
                         const uint8_t *payload, size_t len, int retain);
int  mqtt_client_ping(mqtt_client *c);
void mqtt_client_close(mqtt_client *c);

#endif
```

- [ ] **Step 2: Write `src/mqtt/mqtt_client.c`**

```c
#include "mqtt_client.h"
#include "mqtt_packet.h"
#include "mqtt_socket.h"
#include "../log.h"
#include <stdlib.h>
#include <string.h>

/* Use malloc on host, kernel heap on Vita. The pattern that works:
 * include a small allocator shim. For now, malloc — Task 16 swaps it
 * to kernel-heap on the Vita build via -Dmalloc=ksce_alloc define. */

struct mqtt_client {
    mqtt_socket *sock;
};

#define BUF 1024

mqtt_client *mqtt_client_open(const mqtt_client_config *cfg) {
    if (!cfg) return NULL;
    mqtt_socket *s = mqtt_socket_open(cfg->host, cfg->port);
    if (!s) { LOGE("socket open failed"); return NULL; }

    uint8_t buf[BUF];
    int n = mqtt_build_connect(buf, sizeof buf,
                               cfg->client_id,
                               cfg->username, cfg->password,
                               cfg->will_topic, cfg->will_payload,
                               cfg->keepalive_sec);
    if (n < 0) { LOGE("CONNECT build failed"); mqtt_socket_close(s); return NULL; }
    if (mqtt_socket_send(s, buf, n) != n) {
        LOGE("CONNECT send failed");
        mqtt_socket_close(s);
        return NULL;
    }

    /* Read CONNACK: expect 4 bytes 0x20 0x02 0x00 0x00 */
    uint8_t ack[4];
    int got = mqtt_socket_recv(s, ack, sizeof ack);
    if (got != 4 || ack[0] != 0x20 || ack[3] != 0x00) {
        LOGE("CONNACK rejected (got=%d code=0x%02x)",
             got, got >= 4 ? ack[3] : -1);
        mqtt_socket_close(s);
        return NULL;
    }

    mqtt_client *c = malloc(sizeof *c);
    c->sock = s;
    return c;
}

int mqtt_client_publish(mqtt_client *c, const char *topic,
                        const uint8_t *payload, size_t len, int retain) {
    if (!c) return -1;
    /* Topic+payload can be larger than 1 KB for HA Discovery configs.
     * Allocate dynamically up to 8 KB. */
    size_t need = strlen(topic) + len + 64;
    uint8_t *buf = malloc(need);
    if (!buf) return -1;
    int n = mqtt_build_publish(buf, need, topic, payload, len, retain);
    if (n < 0) { free(buf); return -1; }
    int sent = mqtt_socket_send(c->sock, buf, n);
    free(buf);
    return sent == n ? 0 : -1;
}

int mqtt_client_ping(mqtt_client *c) {
    if (!c) return -1;
    uint8_t buf[2];
    int n = mqtt_build_pingreq(buf, sizeof buf);
    return mqtt_socket_send(c->sock, buf, n) == n ? 0 : -1;
}

void mqtt_client_close(mqtt_client *c) {
    if (!c) return;
    uint8_t buf[2];
    int n = mqtt_build_disconnect(buf, sizeof buf);
    (void)mqtt_socket_send(c->sock, buf, n);
    mqtt_socket_close(c->sock);
    free(c);
}
```

- [ ] **Step 3: Smoke-compile against host socket**

```bash
cc -std=c99 -Wall -Wextra -I. -c \
   src/mqtt/mqtt_client.c src/mqtt/mqtt_packet.c src/mqtt/mqtt_socket_host.c src/log_host.c \
   -o /dev/null 2>&1 || true
# Object-only check:
cc -std=c99 -Wall -Wextra -I. -c src/mqtt/mqtt_client.c -o /tmp/cli.o
```

Expected: compiles cleanly.

- [ ] **Step 4: Commit**

```bash
git add src/mqtt/mqtt_client.h src/mqtt/mqtt_client.c
git commit -m "feat(mqtt): client (CONNECT handshake + PUBLISH/PING/DISCONNECT)"
```

---

## Task 8: Config parser (cJSON → struct)

**Files:**
- Create: `src/config.h`
- Create: `src/config.c`
- Create: `tests/test_config.c`

- [ ] **Step 1: Write `src/config.h`**

```c
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
```

- [ ] **Step 2: Write the failing test** — `tests/test_config.c`

```c
#include "../third_party/minunit/minunit.h"
#include "../src/config.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_parse_full(void) {
    const char *json =
      "{\"broker_host\":\"10.0.0.5\",\"broker_port\":1883,"
      "\"username\":\"u\",\"password\":\"p\","
      "\"client_id\":\"psvita-1\",\"device_name\":\"PS Vita\","
      "\"topic_prefix\":\"psvita\",\"discovery_prefix\":\"homeassistant\","
      "\"poll_interval_sec\":7}";
    mqtt_config c = {0};
    mu_assert("parse ok", config_parse_json(json, &c) == 0);
    mu_assert("host", strcmp(c.broker_host, "10.0.0.5") == 0);
    mu_assert("port", c.broker_port == 1883);
    mu_assert("client", strcmp(c.client_id, "psvita-1") == 0);
    mu_assert("interval", c.poll_interval_sec == 7);
    return NULL;
}

static const char *test_defaults_when_missing(void) {
    mqtt_config c = {0};
    mu_assert("parse ok",
        config_parse_json("{\"broker_host\":\"x\"}", &c) == 0);
    mu_assert("default port 1883", c.broker_port == 1883);
    mu_assert("default prefix",
        strcmp(c.topic_prefix, "psvita") == 0);
    mu_assert("default discovery",
        strcmp(c.discovery_prefix, "homeassistant") == 0);
    mu_assert("default interval 5",
        c.poll_interval_sec == 5);
    return NULL;
}

static const char *test_garbage_input_returns_error(void) {
    mqtt_config c = {0};
    mu_assert("malformed json should fail",
        config_parse_json("not json at all", &c) == -1);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_parse_full);
    mu_run_test(test_defaults_when_missing);
    mu_run_test(test_garbage_input_returns_error);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 3: Run test to confirm link failure**

```bash
cc -std=c99 -Wall -Wextra -I. \
   tests/test_config.c third_party/cJSON/cJSON.c \
   -o /tmp/test_config
```

Expected: undefined reference to `config_parse_json`.

- [ ] **Step 4: Implement `src/config.c`**

```c
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
#ifndef PSVITA_KERNEL_BUILD
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
```

- [ ] **Step 5: Run the test, expect PASS**

```bash
cc -std=c99 -Wall -Wextra -I. \
   tests/test_config.c src/config.c third_party/cJSON/cJSON.c \
   -o /tmp/test_config
/tmp/test_config
```

Expected: `OK: 3 tests`.

- [ ] **Step 6: Commit**

```bash
git add src/config.h src/config.c tests/test_config.c
git commit -m "feat(config): JSON config parser with defaults"
```

---

## Task 9: HA Discovery payload builder

**Files:**
- Create: `src/ha/ha_discovery.h`
- Create: `src/ha/ha_discovery.c`
- Create: `tests/test_ha_discovery.c`

- [ ] **Step 1: Write `src/ha/ha_discovery.h`**

```c
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
```

- [ ] **Step 2: Write failing test** — `tests/test_ha_discovery.c`

```c
#include "../third_party/minunit/minunit.h"
#include "../src/ha/ha_discovery.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static ha_ctx ctx = {
    .discovery_prefix = "homeassistant",
    .topic_prefix     = "psvita",
    .client_id        = "psvita-test",
    .device_name      = "PS Vita",
};

static const char *test_discovery_topic_shape(void) {
    char t[128];
    int n = ha_discovery_topic(t, sizeof t, &ctx, "sensor", "battery_level");
    mu_assert("ok", n > 0);
    mu_assert("topic shape",
        strcmp(t, "homeassistant/sensor/psvita-test_battery_level/config") == 0);
    return NULL;
}

static const char *test_state_topic_shape(void) {
    char t[128];
    int n = ha_state_topic(t, sizeof t, &ctx, "battery/level");
    mu_assert("ok", n > 0);
    mu_assert("state topic shape",
        strcmp(t, "psvita/psvita-test/battery/level") == 0);
    return NULL;
}

static const char *test_payload_contains_device_block(void) {
    char p[1024];
    int n = ha_discovery_payload(p, sizeof p, &ctx,
        "sensor", "battery_level", "Battery Level",
        "battery/level", "%", "battery");
    mu_assert("ok", n > 0);
    mu_assert("has name", strstr(p, "Battery Level") != NULL);
    mu_assert("has unique_id", strstr(p, "psvita-test_battery_level") != NULL);
    mu_assert("has state_topic",
        strstr(p, "psvita/psvita-test/battery/level") != NULL);
    mu_assert("has unit", strstr(p, "\"unit_of_measurement\":\"%\"") != NULL);
    mu_assert("has device block", strstr(p, "\"device\"") != NULL);
    mu_assert("device has name", strstr(p, "\"PS Vita\"") != NULL);
    return NULL;
}

static const char *test_payload_no_unit_no_device_class(void) {
    char p[1024];
    int n = ha_discovery_payload(p, sizeof p, &ctx,
        "binary_sensor", "in_game", "In Game",
        "game/in_game", NULL, NULL);
    mu_assert("ok", n > 0);
    mu_assert("no unit field", strstr(p, "unit_of_measurement") == NULL);
    mu_assert("no device_class field", strstr(p, "device_class") == NULL);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_discovery_topic_shape);
    mu_run_test(test_state_topic_shape);
    mu_run_test(test_payload_contains_device_block);
    mu_run_test(test_payload_no_unit_no_device_class);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 3: Write `src/ha/ha_discovery.c`**

```c
#include "ha_discovery.h"
#include <stdio.h>
#include <string.h>

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

    int pos = snprintf(out, cap,
        "{"
          "\"name\":\"%s\","
          "\"unique_id\":\"%s_%s\","
          "\"state_topic\":\"%s\","
          "\"availability_topic\":\"%s\","
          "\"payload_available\":\"online\","
          "\"payload_not_available\":\"offline\"",
        friendly_name, ctx->client_id, object_id,
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

    int n = snprintf(out + pos, cap - pos,
        ",\"device\":{"
          "\"identifiers\":[\"%s\"],"
          "\"name\":\"%s\","
          "\"manufacturer\":\"Sony\","
          "\"model\":\"PlayStation Vita\""
        "}}",
        ctx->client_id, ctx->device_name);
    if (n < 0 || (size_t)(pos + n) >= cap) return -1;
    pos += n;

    return pos;
}
```

- [ ] **Step 4: Compile + run test**

```bash
cc -std=c99 -Wall -Wextra -I. \
   tests/test_ha_discovery.c src/ha/ha_discovery.c \
   -o /tmp/test_ha_discovery
/tmp/test_ha_discovery
```

Expected: `OK: 4 tests`.

- [ ] **Step 5: Commit**

```bash
git add src/ha/ha_discovery.h src/ha/ha_discovery.c tests/test_ha_discovery.c
git commit -m "feat(ha): MQTT Discovery payload + topic builders"
```

---

## Task 10: Collector contract + battery collector

**Files:**
- Create: `src/collectors/collectors.h`
- Create: `src/collectors/battery_host.c`
- Create: `src/collectors/battery_vita.c`
- Create: `tests/test_collectors.c`

- [ ] **Step 1: Write `src/collectors/collectors.h`**

```c
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
```

- [ ] **Step 2: Write failing test** — `tests/test_collectors.c`

```c
#include "../third_party/minunit/minunit.h"
#include "../src/collectors/collectors.h"
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_battery_returns_canned_data(void) {
    battery_state b;
    mu_assert("ok", collector_battery(&b) == 0);
    mu_assert("level in range", b.level_pct >= 0 && b.level_pct <= 100);
    return NULL;
}

static const char *test_system_uptime_advances(void) {
    system_state s;
    mu_assert("ok", collector_system(&s, /*plugin_started*/0) == 0);
    mu_assert("fw non-empty", s.firmware[0] != '\0');
    mu_assert("model non-empty", s.model[0] != '\0');
    return NULL;
}

static const char *test_app_returns_in_game_flag(void) {
    app_state a;
    mu_assert("ok", collector_app(&a) == 0);
    mu_assert("title id ok",
              a.title_id[0] == '\0' || strlen(a.title_id) >= 9);
    return NULL;
}

static const char *test_network_returns_ip(void) {
    network_state n;
    mu_assert("ok", collector_network(&n) == 0);
    /* Host stub returns 1.2.3.4 */
    mu_assert("host stub ip", strcmp(n.ip, "1.2.3.4") == 0);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_battery_returns_canned_data);
    mu_run_test(test_system_uptime_advances);
    mu_run_test(test_app_returns_in_game_flag);
    mu_run_test(test_network_returns_ip);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 3: Write `src/collectors/battery_host.c`**

```c
#include "collectors.h"

int collector_battery(battery_state *out) {
    out->level_pct = 87;
    out->charging = 0;
    out->temp_celsius_x10 = 305;
    out->remaining_minutes = 240;
    return 0;
}
```

- [ ] **Step 4: Write `src/collectors/battery_vita.c`**

```c
#include "collectors.h"
#include <psp2kern/power.h>

int collector_battery(battery_state *out) {
    int pct  = kscePowerGetBatteryLifePercent();
    int chg  = kscePowerIsBatteryCharging();
    int tmpx = kscePowerGetBatteryTemp();     /* deci-Celsius */
    int rem  = kscePowerGetBatteryLifeTime();

    out->level_pct         = pct  >= 0 ? pct  : -1;
    out->charging          = chg  >= 0 ? chg  : -1;
    out->temp_celsius_x10  = tmpx >= 0 ? tmpx : -1;
    out->remaining_minutes = rem  >= 0 ? rem  : -1;
    return 0;
}
```

- [ ] **Step 5: Provide host stubs for the other collectors so the test compiles**

`src/collectors/system_host.c`:

```c
#include "collectors.h"
#include <string.h>

int collector_system(system_state *out, uint64_t plugin_started_at_sec) {
    (void)plugin_started_at_sec;
    strcpy(out->firmware, "3.74");
    strcpy(out->model, "PCH-2000");
    out->system_uptime_sec = 12345;
    out->plugin_uptime_sec = 1234;
    return 0;
}
```

`src/collectors/app_host.c`:

```c
#include "collectors.h"
#include <string.h>

int collector_app(app_state *out) {
    out->in_game = 1;
    strcpy(out->title_id, "PCSE00001");
    strcpy(out->game_name, "Test Game");
    return 0;
}
```

`src/collectors/network_host.c`:

```c
#include "collectors.h"
#include <string.h>

int collector_network(network_state *out) {
    out->link_up = 1;
    strcpy(out->ip, "1.2.3.4");
    strcpy(out->ssid, "TestWiFi");
    out->rssi_dbm = -55;
    return 0;
}
```

- [ ] **Step 6: Compile + run test**

```bash
cc -std=c99 -Wall -Wextra -I. \
   tests/test_collectors.c \
   src/collectors/battery_host.c src/collectors/system_host.c \
   src/collectors/app_host.c src/collectors/network_host.c \
   -o /tmp/test_collectors
/tmp/test_collectors
```

Expected: `OK: 4 tests`.

- [ ] **Step 7: Commit**

```bash
git add src/collectors/
git add tests/test_collectors.c
git commit -m "feat(collectors): contract + battery (vita+host) + host stubs"
```

---

## Task 11: System collector (Vita kernel impl)

**Files:**
- Create: `src/collectors/system_vita.c`

The host stub already exists from Task 10. This task implements the
real Vita path.

- [ ] **Step 1: Write `src/collectors/system_vita.c`**

```c
#include "collectors.h"
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/utils.h>
#include <stdio.h>
#include <string.h>

/* External helper: vita-headers exposes ksceKernelGetSystemSwVersion
 * via SceSysmemForDriver. Different vita-headers versions name it
 * slightly differently; if your build complains, switch to the
 * sysctl-style read of SCE_SYSTEM_SW_VERSION described in the
 * vita-headers SCE_KERNEL_GET_SYSTEM_SW_VERSION_*. */
extern int ksceKernelGetSystemSwVersion(unsigned int *version);

int collector_system(system_state *out, uint64_t plugin_started_at_sec) {
    unsigned int v = 0;
    if (ksceKernelGetSystemSwVersion(&v) == 0) {
        /* v is BCD: 0x03740011 → "3.74" (high two nibbles + next two). */
        snprintf(out->firmware, sizeof out->firmware, "%X.%02X",
                 (v >> 24) & 0xFF, (v >> 16) & 0xFF);
    } else {
        strcpy(out->firmware, "unknown");
    }

    /* Model: kscePowerGetModel and ksceKernelGetModelForCDialog are
     * both useful. Use the dialog one — returns "PCH-1004", etc. */
    char buf[32] = {0};
    extern int ksceKernelGetModelForCDialog(void);
    int model = ksceKernelGetModelForCDialog();
    /* 0x10000 = PCH-1000 family, 0x20000 = PCH-2000, 0x30000 = PSTV */
    switch (model >> 16) {
        case 0x01: strcpy(out->model, "PCH-1000"); break;
        case 0x02: strcpy(out->model, "PCH-2000"); break;
        case 0x03: strcpy(out->model, "PSTV");     break;
        default:   strcpy(out->model, "Unknown");  break;
    }
    (void)buf;

    SceUInt64 now = ksceKernelGetSystemTimeWide();      /* microseconds */
    out->system_uptime_sec = (uint64_t)(now / 1000000ULL);
    out->plugin_uptime_sec = out->system_uptime_sec >= plugin_started_at_sec
        ? out->system_uptime_sec - plugin_started_at_sec : 0;
    return 0;
}
```

- [ ] **Step 2: Sanity-compile against vita headers (during Task 16 full build)**

This file has no host counterpart of the new code path, so no host
test runs here. Compilation is verified by the full `.skprx` build
in Task 16.

- [ ] **Step 3: Commit**

```bash
git add src/collectors/system_vita.c
git commit -m "feat(collectors): system (firmware/model/uptime) on Vita"
```

---

## Task 12: App / game collector (Vita kernel impl)

**Files:**
- Create: `src/collectors/app_vita.c`
- Create: `romfs/titles.txt` (CSV: `CUSA00000,Game Name`)

- [ ] **Step 1: Seed `romfs/titles.txt` with a couple of well-known IDs**

```bash
mkdir -p romfs
cat > romfs/titles.txt <<'EOF'
PCSE00001,P.S. (sample)
PCSF00214,Tearaway
PCSB00074,Uncharted: Golden Abyss
PCSA00134,Persona 4 Golden
EOF
```

- [ ] **Step 2: Write `src/collectors/app_vita.c`**

```c
#include "collectors.h"
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <string.h>
#include <stdio.h>

/* Walks /sce_pfs/... and /sce_module to figure out the currently-foreground
 * SceShellSvc-managed application. The standard kernel-side way is
 * ksceKernelGetProcessTitleId(SceUID pid, char *titleid). We discover the
 * foreground PID via ksceKernelGetProcessForPid(SCE_KERNEL_PROCESS_FOREGROUND).
 * Some VitaSDK versions expose ksceAppMgrGetForegroundPid in addition. */

extern int ksceKernelGetProcessTitleId(int pid, char *titleid);
extern int ksceKernelGetForegroundPid(void);

static int load_title_name(const char *title_id, char *out, size_t cap) {
    /* titles.txt is bundled inside the .skprx via vita-pack-vpk's romfs
     * mechanism — at runtime we read it from a known path on ux0:. */
    int fd = ksceIoOpen("ux0:data/ps-vita-mqtt/titles.txt", SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[4096];
    int n = ksceIoRead(fd, buf, sizeof buf - 1);
    ksceIoClose(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comma = strchr(line, ',');
        if (comma && strncmp(line, title_id, comma - line) == 0
                  && (size_t)(comma - line) == strlen(title_id)) {
            strncpy(out, comma + 1, cap - 1);
            out[cap - 1] = '\0';
            return 0;
        }
        line = nl ? nl + 1 : NULL;
    }
    return -1;
}

int collector_app(app_state *out) {
    out->in_game = 0;
    out->title_id[0] = '\0';
    out->game_name[0] = '\0';

    int pid = ksceKernelGetForegroundPid();
    if (pid <= 0) return 0;

    char tid[16] = {0};
    if (ksceKernelGetProcessTitleId(pid, tid) < 0) return 0;
    /* PS Vita shell process is NPXS10015; ignore it. */
    if (strncmp(tid, "NPXS", 4) == 0) return 0;

    strncpy(out->title_id, tid, sizeof out->title_id - 1);
    out->in_game = 1;
    (void)load_title_name(out->title_id, out->game_name, sizeof out->game_name);
    return 0;
}
```

- [ ] **Step 3: Compile-check via Vita build (deferred until Task 16)**

No host change. Tests for game-name lookup logic could be split into
a pure parser, but skipped here to keep MVP scope tight — the loader
function is exercised end-to-end via the integration test.

- [ ] **Step 4: Commit**

```bash
git add src/collectors/app_vita.c romfs/titles.txt
git commit -m "feat(collectors): foreground app TitleID + titles.txt lookup"
```

---

## Task 13: Network / WiFi collector (Vita kernel impl)

**Files:**
- Create: `src/collectors/network_vita.c`

- [ ] **Step 1: Write `src/collectors/network_vita.c`**

```c
#include "collectors.h"
#include <psp2kern/net/net.h>
#include <psp2kern/net/netctl.h>
#include <string.h>

int collector_network(network_state *out) {
    memset(out, 0, sizeof *out);

    int state = 0;
    if (ksceNetCtlInetGetState(&state) < 0) return 0;
    out->link_up = (state == SCE_NETCTL_STATE_CONNECTED);

    if (!out->link_up) return 0;

    SceNetCtlInfo info;
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) == 0) {
        strncpy(out->ip, info.ip_address, sizeof out->ip - 1);
    }
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID, &info) == 0) {
        strncpy(out->ssid, info.ssid, sizeof out->ssid - 1);
    }
    memset(&info, 0, sizeof info);
    if (ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_DBM, &info) == 0) {
        out->rssi_dbm = info.rssi_dbm;
    }
    return 0;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/collectors/network_vita.c
git commit -m "feat(collectors): wifi (ip/ssid/rssi/link state) on Vita"
```

---

## Task 14: Publisher orchestrator

**Files:**
- Create: `src/publisher.h`
- Create: `src/publisher.c`
- Create: `tests/test_publisher.c`

- [ ] **Step 1: Write `src/publisher.h`**

```c
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
```

- [ ] **Step 2: Write `src/publisher.c`**

```c
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
    { "sensor",        "battery_temp",    "Battery Temperature","battery/temp",     "°C", "temperature" },
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
```

- [ ] **Step 3: Write `tests/test_publisher.c`**

```c
/* Pure-host smoke test: assemble a config and call publisher_publish_once
 * against a mosquitto broker running on localhost. Skipped if MOSQUITTO_HOST
 * env var is not set, so unit-test runs don't depend on a broker. */
#include "../third_party/minunit/minunit.h"
#include "../src/publisher.h"
#include "../src/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int mu_tests_run = 0;

static const char *test_publish_once_against_local_broker(void) {
    const char *host = getenv("MOSQUITTO_HOST");
    if (!host) {
        printf("(skip: MOSQUITTO_HOST not set)\n");
        return NULL;
    }
    mqtt_config c = {0};
    strcpy(c.broker_host, host);
    c.broker_port = 1883;
    strcpy(c.client_id, "psvita-test");
    strcpy(c.device_name, "PS Vita Test");
    strcpy(c.topic_prefix, "psvita");
    strcpy(c.discovery_prefix, "homeassistant");
    c.poll_interval_sec = 5;
    mu_assert("publish_once should succeed",
              publisher_publish_once(&c) == 0);
    return NULL;
}

static const char *all(void) {
    mu_run_test(test_publish_once_against_local_broker);
    return NULL;
}

int main(void) {
    const char *r = all();
    if (r) { printf("FAIL: %s\n", r); return 1; }
    printf("OK: %d tests\n", mu_tests_run);
    return 0;
}
```

- [ ] **Step 4: Compile + run (no broker required)**

```bash
cc -std=c99 -Wall -Wextra -D_GNU_SOURCE -I. \
   tests/test_publisher.c \
   src/publisher.c src/config.c src/log_host.c \
   src/mqtt/mqtt_client.c src/mqtt/mqtt_packet.c src/mqtt/mqtt_socket_host.c \
   src/ha/ha_discovery.c \
   src/collectors/battery_host.c src/collectors/system_host.c \
   src/collectors/app_host.c src/collectors/network_host.c \
   third_party/cJSON/cJSON.c \
   -o /tmp/test_publisher
/tmp/test_publisher
```

Expected: `(skip: MOSQUITTO_HOST not set)` then `OK: 1 tests`. The
real broker run lives in Task 18.

- [ ] **Step 5: Commit**

```bash
git add src/publisher.h src/publisher.c tests/test_publisher.c
git commit -m "feat(publisher): discovery+state loop with reconnect/backoff"
```

---

## Task 15: Plugin entry (`module_start`, kernel thread, config load on Vita)

**Files:**
- Create: `src/main.c`
- Create: `src/config_vita.c` (Vita-side `config_load_from_path` using `ksceIo*`)

- [ ] **Step 1: Write `src/config_vita.c`**

```c
#include "config.h"
#include <psp2kern/kernel/iofilemgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <string.h>

int config_load_from_path(const char *path, mqtt_config *out) {
    SceUID fd = ksceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return -1;
    char buf[4096];
    int n = ksceIoRead(fd, buf, sizeof buf - 1);
    ksceIoClose(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return config_parse_json(buf, out);
}
```

- [ ] **Step 2: Write `src/main.c`**

```c
#include "config.h"
#include "publisher.h"
#include "log.h"
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>

#define CONFIG_PATH "ux0:data/ps-vita-mqtt/config.json"

static volatile int g_stop = 0;
static SceUID       g_thid = -1;
static mqtt_config  g_cfg;

static int worker(SceSize args, void *argp) {
    (void)args; (void)argp;
    LOGF("worker thread started");
    publisher_run(&g_cfg, &g_stop);
    LOGF("worker thread exiting");
    return 0;
}

int module_start(SceSize argc, const void *args) {
    (void)argc; (void)args;
    LOGF("ps-vita-mqtt module_start");
    if (config_load_from_path(CONFIG_PATH, &g_cfg) < 0) {
        LOGE("config load failed: %s — plugin idle", CONFIG_PATH);
        return SCE_KERNEL_START_SUCCESS;
    }
    LOGF("config ok: broker=%s:%u client=%s",
         g_cfg.broker_host, g_cfg.broker_port, g_cfg.client_id);

    g_thid = ksceKernelCreateThread("ps-vita-mqtt", worker,
                                    0x40, 0x4000, 0, 0, NULL);
    if (g_thid < 0) { LOGE("create thread failed: 0x%X", g_thid); return SCE_KERNEL_START_SUCCESS; }
    ksceKernelStartThread(g_thid, 0, NULL);
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
    (void)argc; (void)args;
    LOGF("module_stop");
    g_stop = 1;
    if (g_thid >= 0) {
        ksceKernelWaitThreadEnd(g_thid, NULL, NULL);
        ksceKernelDeleteThread(g_thid);
        g_thid = -1;
    }
    return SCE_KERNEL_STOP_SUCCESS;
}
```

- [ ] **Step 3: Commit (build verified in Task 16)**

```bash
git add src/main.c src/config_vita.c
git commit -m "feat(main): module_start spawns publisher worker thread"
```

---

## Task 16: Makefile — `.skprx` build + `host-tests` target

**Files:**
- Create: `Makefile`

- [ ] **Step 1: Write `Makefile`**

```makefile
TARGET := ps-vita-mqtt
OBJS_VITA := \
    src/main.o \
    src/log_vita.o \
    src/config.o \
    src/config_vita.o \
    src/publisher.o \
    src/mqtt/mqtt_client.o \
    src/mqtt/mqtt_packet.o \
    src/mqtt/mqtt_socket_vita.o \
    src/ha/ha_discovery.o \
    src/collectors/battery_vita.o \
    src/collectors/system_vita.o \
    src/collectors/app_vita.o \
    src/collectors/network_vita.o \
    third_party/cJSON/cJSON.o

PREFIX  := arm-vita-eabi
CC      := $(PREFIX)-gcc
CFLAGS  := -Wl,-q -Wall -O2 -nostartfiles -mtune=cortex-a9 \
           -DPSVITA_KERNEL_BUILD -I. -Isrc
LIBS    := -lSceSysmemForDriver_stub -lSceThreadmgrForDriver_stub \
           -lSceModulemgrForDriver_stub -lSceIofilemgrForDriver_stub \
           -lSceNetForDriver_stub -lSceNetCtlForDriver_stub \
           -lScePowerForDriver_stub -lSceDebugForKernel_stub

.PHONY: skprx clean host-tests

skprx: build/$(TARGET).skprx

build/$(TARGET).velf: $(OBJS_VITA)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(LIBS) -o build/$(TARGET).elf
	vita-elf-create -e exports.yml build/$(TARGET).elf build/$(TARGET).velf

build/$(TARGET).skprx: build/$(TARGET).velf
	vita-make-fself -c build/$(TARGET).velf build/$(TARGET).skprx

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(OBJS_VITA) host-tests

# ---- host-side unit tests --------------------------------------------------
HOST_CC := cc
HOST_CFLAGS := -std=c99 -Wall -Wextra -D_GNU_SOURCE -I.

HOST_SRCS_COMMON := \
    src/log_host.c src/config.c \
    src/mqtt/mqtt_client.c src/mqtt/mqtt_packet.c src/mqtt/mqtt_socket_host.c \
    src/ha/ha_discovery.c src/publisher.c \
    src/collectors/battery_host.c src/collectors/system_host.c \
    src/collectors/app_host.c src/collectors/network_host.c \
    third_party/cJSON/cJSON.c

host-tests:
	@mkdir -p host-tests
	$(HOST_CC) $(HOST_CFLAGS) tests/test_log.c \
	   src/log_host.c third_party/cJSON/cJSON.c -o host-tests/test_log
	$(HOST_CC) $(HOST_CFLAGS) tests/test_mqtt_packet.c \
	   src/mqtt/mqtt_packet.c -o host-tests/test_mqtt_packet
	$(HOST_CC) $(HOST_CFLAGS) tests/test_config.c \
	   src/config.c third_party/cJSON/cJSON.c -o host-tests/test_config
	$(HOST_CC) $(HOST_CFLAGS) tests/test_ha_discovery.c \
	   src/ha/ha_discovery.c -o host-tests/test_ha_discovery
	$(HOST_CC) $(HOST_CFLAGS) tests/test_collectors.c \
	   src/collectors/battery_host.c src/collectors/system_host.c \
	   src/collectors/app_host.c src/collectors/network_host.c \
	   -o host-tests/test_collectors
	$(HOST_CC) $(HOST_CFLAGS) tests/test_publisher.c \
	   $(HOST_SRCS_COMMON) -o host-tests/test_publisher
	@for t in host-tests/test_*; do echo "==> $$t" && $$t || exit 1; done
```

- [ ] **Step 2: Write `exports.yml`** (vita-elf-create needs it for `module_start` / `module_stop`)

```yaml
PS_VITA_MQTT:
  attributes: 0
  version:
    major: 1
    minor: 0
  main:
    start: module_start
    stop: module_stop
```

- [ ] **Step 3: Build the host tests**

```bash
make host-tests
```

Expected: every test prints `OK: N tests`.

- [ ] **Step 4: Build the .skprx via Docker**

```bash
./scripts/build-skprx.sh
```

Expected: `build/ps-vita-mqtt.skprx` exists, size > 4 KB.

> If the build fails on missing `lSceFoo_stub`, check the symbol name
> against `vita-headers/src/SceFoo/SceFoo.yml` and adjust either the
> include or the `-l` flag. Stub names are case-sensitive and vary
> across vita-headers versions; do not invent names.

- [ ] **Step 5: Commit**

```bash
git add Makefile exports.yml
git commit -m "build: Makefile for .skprx + host-tests target"
```

---

## Task 17: taiHEN install docs (no code — text only)

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Append install steps to `README.md`**

```markdown
## Requirements

- PS Vita (PCH-1000 / PCH-2000) or PSTV with HENkaku Enso (FW 3.60) or
  h-encore² (FW 3.65–3.74)
- A reachable MQTT broker (Mosquitto recommended) on the same LAN, port 1883
- Home Assistant with the MQTT integration enabled
- VitaShell or any FTP client to copy files to the Vita

## Build

```bash
./scripts/build-skprx.sh
# produces build/ps-vita-mqtt.skprx
```

First run downloads the VitaSDK Docker image (~1.5 GB).

## Install on the Vita

1. **Copy the plugin and config to ux0:** (FTP server in VitaShell uses
   port 1337, or use USB mode):

   ```text
   ux0:tai/ps-vita-mqtt.skprx
   ux0:data/ps-vita-mqtt/config.json
   ```

   Use `config.example.json` as the template. Fill in your broker
   IP, port, and a unique `client_id`.

2. **Register the plugin in `ux0:tai/config.txt`** under the `*KERNEL`
   section:

   ```text
   *KERNEL
   ux0:tai/ps-vita-mqtt.skprx
   ```

3. **Reboot the Vita.** The plugin starts on boot and begins publishing.

4. **(Optional) Bundle the title list** so the running-game name is
   resolved:

   ```text
   ux0:data/ps-vita-mqtt/titles.txt
   ```

   The format is one CSV line per title:

   ```text
   PCSE00001,Game Name
   ```

## Available sensors

Single HA device named **PS Vita** (or whatever `device_name` is set
to). All sensors auto-create on each (re)connect.

| Sensor | Topic | Unit |
|---|---|---|
| Battery level | `psvita/<id>/battery/level` | % |
| Battery charging | `psvita/<id>/battery/charging` | on/off |
| Battery temperature | `psvita/<id>/battery/temp` | °C |
| Battery remaining | `psvita/<id>/battery/minutes` | min |
| Firmware | `psvita/<id>/system/firmware` | — |
| Model | `psvita/<id>/system/model` | — |
| System uptime | `psvita/<id>/system/uptime` | s |
| Plugin uptime | `psvita/<id>/plugin/uptime` | s |
| In game | `psvita/<id>/app/in_game` | on/off |
| Title ID | `psvita/<id>/app/title_id` | — |
| Game name | `psvita/<id>/app/game_name` | — |
| Network link | `psvita/<id>/net/link` | on/off |
| IP | `psvita/<id>/net/ip` | — |
| SSID | `psvita/<id>/net/ssid` | — |
| WiFi RSSI | `psvita/<id>/net/rssi` | dBm |
| Availability (LWT) | `psvita/<id>/availability` | online / offline |

## Limitations

- No TLS — broker must be on a trusted LAN.
- No HA → Vita commands (reboot/launch/shutdown) in MVP.
- No on-screen overlay / popup notifications.
- No live screen streaming.

## License

TBD
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: install + sensor list"
```

---

## Task 18: Integration test against Mosquitto

**Files:**
- Create: `tests/integration/run_integration.sh`

- [ ] **Step 1: Write `tests/integration/run_integration.sh`**

```bash
#!/usr/bin/env bash
set -euo pipefail

# Bring up Mosquitto, build host-tests, run test_publisher against it,
# subscribe and verify discovery + state topics are populated.

cleanup() {
  docker rm -f psvita-mqtt-mosquitto >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
docker run -d --name psvita-mqtt-mosquitto -p 1883:1883 \
  eclipse-mosquitto:2 \
  sh -c 'echo "listener 1883" > /m.conf && \
         echo "allow_anonymous true" >> /m.conf && \
         mosquitto -c /m.conf'

# wait for broker
for i in $(seq 1 20); do
  if (echo > /dev/tcp/127.0.0.1/1883) >/dev/null 2>&1; then break; fi
  sleep 0.5
done

make host-tests

# Run the publisher against the real broker
MOSQUITTO_HOST=127.0.0.1 ./host-tests/test_publisher

# Verify a discovery topic landed on the broker
TIMEOUT=5
EXPECT_TOPIC="homeassistant/sensor/psvita-test_battery_level/config"
FOUND=$(docker exec psvita-mqtt-mosquitto \
  mosquitto_sub -h 127.0.0.1 -t "$EXPECT_TOPIC" -W $TIMEOUT -C 1 || true)
test -n "$FOUND" || { echo "FAIL: $EXPECT_TOPIC not retained"; exit 1; }
echo "PASS: discovery topic retained on broker"
```

- [ ] **Step 2: Make it executable**

```bash
chmod +x tests/integration/run_integration.sh
```

- [ ] **Step 3: Run the integration test locally**

```bash
./tests/integration/run_integration.sh
```

Expected: `PASS: discovery topic retained on broker`. (`/dev/tcp` is a
bash builtin — works on Linux and macOS bash; on Alpine in CI we'll
use `nc -z 127.0.0.1 1883` instead — see CI workflow.)

- [ ] **Step 4: Commit**

```bash
git add tests/integration/run_integration.sh
git commit -m "test: integration test against Mosquitto in Docker"
```

---

## Task 19: GitHub Actions CI

**Files:**
- Create: `.github/workflows/build-skprx.yml`

- [ ] **Step 1: Write `.github/workflows/build-skprx.yml`**

```yaml
name: build

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install build deps
        run: sudo apt-get update && sudo apt-get install -y build-essential
      - name: Host tests
        run: make host-tests

  integration:
    runs-on: ubuntu-latest
    needs: host-tests
    steps:
      - uses: actions/checkout@v4
      - name: Install build deps + netcat
        run: sudo apt-get update && sudo apt-get install -y build-essential netcat
      - name: Run integration test
        run: bash tests/integration/run_integration.sh

  skprx:
    runs-on: ubuntu-latest
    needs: host-tests
    container: vitasdk/vitasdk:latest
    steps:
      - uses: actions/checkout@v4
      - name: Build .skprx
        run: |
          apk add --no-cache make bash
          make skprx
      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: ps-vita-mqtt-skprx
          path: build/ps-vita-mqtt.skprx
```

- [ ] **Step 2: Commit + push to trigger CI**

```bash
git add .github/workflows/build-skprx.yml
git commit -m "ci: host-tests + integration + skprx build"
git push -u origin main
```

Expected: CI green on first push. If `vitasdk/vitasdk:latest` does
not exist in Docker Hub, pin to a known tag (the action will fail
with `manifest not found`) and update the workflow.

---

## Task 20: End-to-end manual verification on real hardware

This is a manual verification task — no new files. Document the
result in a follow-up commit if anything diverges from the docs.

- [ ] **Step 1: Build artifacts**

```bash
make host-tests
./scripts/build-skprx.sh
```

Expected: `build/ps-vita-mqtt.skprx` exists. Host tests all pass.

- [ ] **Step 2: Copy to Vita over FTP (VitaShell SELECT → "Start FTP Server")**

```bash
VITA_IP=192.168.1.50   # whatever VitaShell shows
curl -T build/ps-vita-mqtt.skprx       ftp://$VITA_IP:1337/ux0:/tai/ps-vita-mqtt.skprx
curl -T config.example.json            ftp://$VITA_IP:1337/ux0:/data/ps-vita-mqtt/config.json
curl -T romfs/titles.txt               ftp://$VITA_IP:1337/ux0:/data/ps-vita-mqtt/titles.txt
```

- [ ] **Step 3: Add the plugin to `ux0:tai/config.txt` under `*KERNEL`**

Fetch the existing file first, edit, push back:

```bash
curl ftp://$VITA_IP:1337/ux0:/tai/config.txt > /tmp/config.txt
# edit /tmp/config.txt: add "ux0:tai/ps-vita-mqtt.skprx" under *KERNEL
curl -T /tmp/config.txt ftp://$VITA_IP:1337/ux0:/tai/config.txt
```

- [ ] **Step 4: Reboot the Vita**

- [ ] **Step 5: Subscribe to discovery topics from a PC on the LAN**

```bash
mosquitto_sub -h 192.168.1.10 -v -t 'homeassistant/#' -t 'psvita/#'
```

Expected: within 30 s of boot you see ~15 retained discovery configs
under `homeassistant/...` and a stream of state messages under
`psvita/<client_id>/...`. Home Assistant's MQTT integration auto-
creates entities under a single device named **PS Vita**.

- [ ] **Step 6: Power off the Vita and confirm LWT fires**

Subscribe to availability:

```bash
mosquitto_sub -h 192.168.1.10 -v -t 'psvita/+/availability'
```

After power-off + keepalive window (default 60 s), the broker
publishes `offline` to the availability topic on the plugin's behalf.

- [ ] **Step 7: Update `README.md` "Limitations" if anything found**

E.g. if `ksceKernelGetForegroundPid` is unreliable on your firmware,
document it. Commit any doc changes:

```bash
git add README.md
git commit -m "docs: end-to-end test notes"
```

---

## Roadmap (not in MVP)

These come after Task 20 ships. Each is its own future plan.

- **HA → Vita commands** (reboot/standby/launch app) — needs MQTT
  SUBSCRIBE in `mqtt_client.c`, then `scePowerRequestColdReset`,
  `scePowerRequestStandby`, `sceAppMgrLaunchAppByUri` from a
  user-mode companion `.suprx` (those APIs are user-space).
- **In-game popup notifications** — render via `vita2d` from a
  companion user plugin hooked into `SceShell` and game processes.
- **Live screen streaming (RTSP)** — `sceDisplayGetFrameBuf` + an
  encoder. Likely the largest follow-up.
- **TLS MQTT** — needs an embedded mbedTLS build for the kernel side.

---

## Self-review checklist (filled in by the planner)

- **Spec coverage:** every sensor listed in the spec maps to an entry
  in `SENSORS[]` in Task 14, an HA discovery payload (Task 9), and a
  collector pair `_vita.c` / `_host.c` (Tasks 10–13).
- **Placeholder scan:** no `TODO` / `TBD` in any code block; every
  "implement later" is explicitly labelled as roadmap (post-MVP) at
  the bottom, not inside a task.
- **Type consistency:** `mqtt_config`, `ha_ctx`, `battery_state`,
  `system_state`, `app_state`, `network_state`, `mqtt_client`,
  `mqtt_socket`, `mqtt_client_config` — used identically across all
  tasks that reference them.

---

## Execution handoff

Plan complete and saved to
`docs/superpowers/plans/2026-05-21-psvita-mqtt-plugin.md`. Two
execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent
   per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using
   `superpowers:executing-plans`, batch execution with checkpoints.

Which approach?
