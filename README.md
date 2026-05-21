# ps-vita-mqtt-plugin

taiHEN user plugin for jailbroken PS Vita (HENkaku Enso on FW 3.60, or
h-encore²/Enso on FW 3.65–3.74) that publishes console telemetry to an
MQTT broker. Home Assistant auto-creates the sensors via MQTT
Discovery.

The plugin is loaded into the **SceShell** process under taiHEN's
`*main` section. It runs whenever you are in the LiveArea, pauses while
a game is in foreground, and resumes when you return to the menu.

## Requirements

- PS Vita (PCH-1000 / PCH-2000) or PSTV with HENkaku Enso (FW 3.60) or
  h-encore² (FW 3.65–3.74)
- A reachable MQTT broker (Mosquitto recommended) on the same LAN, port 1883
- Home Assistant with the MQTT integration enabled
- VitaShell or any FTP client to copy files to the Vita

## Build

```bash
./scripts/build-suprx.sh
# produces build/ps-vita-mqtt.suprx
```

First run downloads the VitaSDK Docker image (`vitasdk/vitasdk:latest`,
linux/amd64). Subsequent builds are fast.

## Install on the Vita

1. **Copy the plugin and config to `ux0:`** (VitaShell's FTP server is
   on port 1337; USB mode also works):

   ```text
   ux0:tai/ps-vita-mqtt.suprx
   ux0:data/ps-vita-mqtt/config.json
   ```

   Use `config.example.json` as the template. Fill in your broker
   IP, port, and a unique `client_id`.

2. **Register the plugin in `ux0:tai/config.txt`** under the `*main`
   section (so it loads into SceShell):

   ```text
   *main
   ux0:tai/ps-vita-mqtt.suprx
   ```

3. **Reload taiHEN** (in HENkaku Settings) **or reboot the Vita**. The
   plugin starts when SceShell launches and begins publishing.

## Available sensors

A single Home Assistant device named **PS Vita** (or whatever
`device_name` is set to) is auto-created via MQTT Discovery on each
(re)connect.

| Sensor | Topic | Unit |
|---|---|---|
| Battery level | `psvita/<id>/battery/level` | % |
| Battery charging | `psvita/<id>/battery/charging` | on/off |
| Battery temperature | `psvita/<id>/battery/temp` | °C |
| Battery remaining | `psvita/<id>/battery/minutes` | min |
| Firmware | `psvita/<id>/system/firmware` | — |
| Model | `psvita/<id>/system/model` | — |
| System uptime (process) | `psvita/<id>/system/uptime` | s |
| Plugin uptime | `psvita/<id>/plugin/uptime` | s |
| In game (any app running) | `psvita/<id>/app/in_game` | on/off |
| Title ID | `psvita/<id>/app/title_id` | — (best-effort) |
| Game name | `psvita/<id>/app/game_name` | — (best-effort) |
| Network link | `psvita/<id>/net/link` | on/off |
| IP | `psvita/<id>/net/ip` | — |
| SSID | `psvita/<id>/net/ssid` | — |
| WiFi RSSI | `psvita/<id>/net/rssi` | dBm |
| Availability (LWT) | `psvita/<id>/availability` | online / offline |

`<id>` is `client_id` from `config.json`.

The optional title-name lookup reads
`ux0:data/ps-vita-mqtt/titles.txt`. Format is one CSV line per app:

```text
PCSE00001,Game Name
```

## Limitations

- No TLS — broker must be on a trusted LAN.
- No HA → Vita commands (reboot/launch/shutdown). Publish-only.
- No on-screen overlay / popup notifications.
- No live screen streaming.
- The plugin lives inside SceShell, so sensors do **not** update while
  a game is in foreground — they refresh as soon as you return to
  LiveArea. The `offline` LWT fires after the keepalive window
  (default 60 s).
- `app/title_id` and `app/game_name` are **best-effort**. Determining
  the foreground app's TitleID from the SceShell user-mode context
  requires taiHEN-side hooks that are out of MVP scope. Today only
  the `in_game` boolean is reliably populated.
- `system/uptime` reports SceShell process time, not absolute system
  uptime (resolving that needs `sceRtcGetCurrentTick` + boot-time
  delta; deferred).

## Architecture

- C99, single user thread
- Custom minimal MQTT 3.1.1 publisher (CONNECT, PUBLISH, PINGREQ,
  DISCONNECT — no SUBSCRIBE, QoS 0, no TLS)
- HA Discovery configs published once on every (re)connect, retained
- Last Will & Testament keeps HA availability accurate when the plugin
  is paused, the Vita sleeps, or the network drops

## Layout

```
src/
├── main.c                       # module_start / module_stop
├── log.h, log_vita.c, log_host.c
├── config.h, config.c, config_vita.c
├── publisher.h, publisher.c     # worker loop (connect → discovery → state → sleep → ping)
├── mqtt/mqtt_packet.c, mqtt_client.c, mqtt_socket_vita.c, mqtt_socket_host.c
├── ha/ha_discovery.c            # HA MQTT Discovery payloads
└── collectors/                  # per-sensor-group, _vita.c + _host.c pairs
```

Every Vita-targeted module ships with a host stub used by the unit
tests, so the publisher, packet encoder, config parser and Discovery
payload builders all run on the developer's laptop without a Vita.

## Host tests

```bash
make host-tests
```

Compiles and runs `tests/test_*` against the host stubs. The
integration test against Mosquitto in Docker lives at
`tests/integration/run_integration.sh`.

## License

TBD
