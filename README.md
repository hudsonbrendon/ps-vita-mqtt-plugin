<p align="center">
  <a href="https://www.home-assistant.io/integrations/mqtt/"><img alt="Home Assistant" src="https://img.shields.io/badge/Home%20Assistant-MQTT-41bdf5?style=for-the-badge"></a>
  <a href="https://henkaku.xyz/"><img alt="PS Vita" src="https://img.shields.io/badge/PS%20Vita-HENkaku-003791?style=for-the-badge"></a>
</p>

# ps-vita-mqtt-plugin

taiHEN user plugin (`.suprx`) for a jailbroken PS Vita that publishes
console telemetry to an MQTT broker. Home Assistant auto-creates the
sensors via MQTT Discovery.

Verified working on **FW 3.74 / HENkaku** (PCH-1000), publishing 15
sensors as a single Home Assistant device.

Tested live: battery level/charging state/temperature, WiFi SSID/RSSI/
IP, firmware/model, plugin uptime, network link, availability with LWT.

## How it works

The plugin loads into the **SceShell** process via taiHEN's `*main`
section. On boot, `module_start` returns immediately, spawning a worker
thread that waits 5 seconds (so SceShell can finish its own startup)
and then:

1. Loads `ux0:/data/ps-vita-mqtt/config.json`.
2. Publishes Home Assistant MQTT Discovery configs for every sensor
   (retained).
3. Loops: collect → publish → sleep → ping.

Plain TCP, no TLS, QoS 0. Hand-rolled minimal MQTT 3.1.1 client — no
library dependencies, no libc heap. Last Will & Testament keeps the HA
availability accurate when the Vita sleeps or the plugin stops.

## Requirements

- PS Vita (PCH-1000 / PCH-2000) or PSTV
- HENkaku Enso (FW 3.60) or h-encore² (FW 3.65 – 3.74)
- Reachable MQTT broker on the same LAN, port 1883 (Mosquitto recommended)
- Home Assistant with the MQTT integration enabled
- VitaShell on the Vita with the FTP server enabled (`SELECT`)

## Build

```bash
./scripts/build-suprx.sh
# produces build/ps-vita-mqtt.suprx
```

First run downloads the VitaSDK Docker image (~1.5 GB,
`vitasdk/vitasdk:latest`, linux/amd64 — works on Apple Silicon via QEMU).
Subsequent builds are fast.

## Install on the Vita

Pre-built binaries are attached to every GitHub release. To install:

1. **Copy the plugin and config to the Vita** over FTP (VitaShell uses
   port 1337):

   ```text
   ur0:tai/ps-vita-mqtt.suprx
   ux0:data/ps-vita-mqtt/config.json
   ```

   Use [`config.example.json`](./config.example.json) as the template.
   Fill in your broker IP, port, credentials, and a unique `client_id`.

   Example:

   ```bash
   VITA_IP=192.168.1.50

   curl -T ps-vita-mqtt.suprx \
     ftp://$VITA_IP:1337/ur0:/tai/ps-vita-mqtt.suprx
   curl -T config.json \
     ftp://$VITA_IP:1337/ux0:/data/ps-vita-mqtt/config.json
   ```

2. **Register the plugin in taiHEN config**. The plugin must be listed
   under the `*main` section so it loads into SceShell. Open
   `ux0:tai/config.txt` (or `ur0:tai/config.txt` if `ux0:` doesn't exist)
   and add:

   ```text
   *main
   ur0:tai/ps-vita-mqtt.suprx
   ```

3. **Reboot the Vita** (HENkaku Settings → *Reload taiHEN* is not enough
   for user plugins — SceShell needs to be restarted).

4. Within ~5 s of boot, the broker receives ~15 retained discovery
   topics and the HA UI gets a new device named **PS Vita**.

## Available sensors

All sensors appear under a single HA device. `<id>` is `client_id` from
`config.json`.

| Sensor | Topic | Unit |
|---|---|---|
| Battery level | `psvita/<id>/battery/level` | % |
| Battery charging | `psvita/<id>/battery/charging` | ON / OFF |
| Battery temperature | `psvita/<id>/battery/temp` | °C |
| Battery remaining | `psvita/<id>/battery/minutes` | min |
| Firmware | `psvita/<id>/system/firmware` | — |
| Model | `psvita/<id>/system/model` | — |
| System uptime (process) | `psvita/<id>/system/uptime` | s |
| Plugin uptime | `psvita/<id>/plugin/uptime` | s |
| In game | `psvita/<id>/app/in_game` | ON / OFF |
| Title ID | `psvita/<id>/app/title_id` | — *(stub, see Limitations)* |
| Game name | `psvita/<id>/app/game_name` | — *(stub)* |
| Network link | `psvita/<id>/net/link` | ON / OFF |
| IP | `psvita/<id>/net/ip` | — |
| SSID | `psvita/<id>/net/ssid` | — |
| WiFi RSSI | `psvita/<id>/net/rssi` | dBm |
| Availability (LWT) | `psvita/<id>/availability` | online / offline |

## Limitations

- **No TLS** — broker must be on a trusted LAN.
- **No HA → Vita commands** (reboot/launch/shutdown). Publish-only.
- **No on-screen overlay** notifications.
- **No screen streaming**.
- The plugin lives inside **SceShell**, so sensors do not refresh while
  a game is in the foreground. They resume when you return to LiveArea.
  The `offline` LWT fires after the broker's keepalive window
  (60 s).
- `app/title_id` and `app/game_name` show `-` by default. Resolving the
  foreground game's TitleID from SceShell context is non-trivial:
  taiHEN's `*ALL` and `*<TITLEID>` plugin-injection sections do **not**
  load user plugins into signed commercial Vita game processes on the
  firmware we tested. See [issue #1](../../issues/1) for the experiment
  log and the planned `sceAppMgrLaunchAppByUri` hook approach.
- `in_game` only flips while a process is detected (via the optional
  `src/companion/` plugin, disabled by default — see "Optional companion
  plugin" below). With the default config, `in_game` stays `OFF`.
- `system/uptime` reports the SceShell process time, not absolute system
  uptime.

## Recovery — if a bad build bricks the boot

If a buggy plugin makes SceShell crash on boot and the Vita gets stuck
on the Sony logo:

1. Power-off the Vita (hold power 30 s).
2. Remove the SD2VITA / microSD card and put it in a PC.
3. Open `tai/config.txt` on the card (i.e. `ux0:/tai/config.txt`).
   - If the file does not exist, create it with just the recovery
     baseline (see [`recovery/config.txt`](./recovery/config.txt) in
     this repo).
   - If it exists, **delete** the line `ur0:tai/ps-vita-mqtt.suprx`.
4. Put the card back, boot the Vita normally. `ux0:/tai/config.txt`
   takes precedence over `ur0:/tai/config.txt`, so taiHEN will skip the
   bad plugin.

## Architecture

- C99, one user thread
- Custom minimal MQTT 3.1.1 publisher (CONNECT, PUBLISH, PINGREQ,
  DISCONNECT — no SUBSCRIBE, QoS 0, no TLS)
- HA Discovery configs published once on every (re)connect, retained
- LWT keeps HA availability accurate when the plugin pauses

### Why a user plugin, not a kernel plugin

The original plan was a kernel `.skprx`, but VitaSDK's kernel headers
expose so few of the user-facing APIs (no `ksceNetCtl*`, no foreground
PID, no TitleID lookup) that a kernel-only build would mean a battery-
only MVP. The user `.suprx` route gives full `scePower*`, `sceNet*`,
`sceNetCtl*`, and `sceAppMgr*` access at the cost of pausing during
games.

### Why weak SCE stubs

Most plugins link against `-lScexxx_stub`. We link against
`-lScexxx_stub_weak` instead. With strong stubs, the entire plugin is
rejected at load time if even one imported NID cannot be resolved in
the SceShell context — silently, with no log. Weak stubs defer
resolution to first use, so the plugin loads even if a few exotic
imports stay null, and we get diagnostic logs from the symbols we
actually call.

### Why no SceLibc

Plugins loaded with `-nostartfiles` (which we must use — there is no
`main`, only `module_start`) never run newlib's heap initialization.
Linking `-lSceLibc_stub` causes a load-time crash because the libc
heap stays uninitialized. We replace libc with:

- `sceClibSnprintf` / `sceClibVsnprintf` (in `SceLibKernel`) instead of
  `snprintf` / `vsnprintf`
- Static buffers + a fixed-size bump pool for `cJSON_InitHooks`
- A no-op stub for `_free_vita_newlib` (the only newlib symbol that
  pulls in the missing SceLibc heap path)

## Layout

```
src/
├── main.c                       # module_start spawns the worker thread
├── log.h, log_vita.c, log_host.c
├── config.h, config.c, config_vita.c
├── publisher.h, publisher.c     # connect → discovery → state → sleep → ping
├── mqtt/mqtt_packet.c, mqtt_client.c, mqtt_socket_vita.c, mqtt_socket_host.c
├── ha/ha_discovery.c            # MQTT Discovery payloads
├── collectors/                  # per-sensor-group, _vita.c + _host.c pairs
├── sce_libc_shim.h              # redirects snprintf to sceClibSnprintf on the Vita build
└── vita_newlib_stubs.c          # stubs the newlib heap path we don't use
```

Every Vita-targeted module ships with a host stub used by the unit
tests, so the publisher, packet encoder, config parser, and Discovery
payload builders all run on the developer's laptop without a Vita.

## Host tests

```bash
make host-tests
```

Compiles and runs `tests/test_*` against the host stubs. The
integration test against Mosquitto in Docker lives at
`tests/integration/run_integration.sh`.

## Optional companion plugin (experimental)

A second tiny `.suprx`, `ps-vita-mqtt-tag.suprx`, lives under
`src/companion/`. When loaded into any user process, it reads the
current process TitleID and writes it to
`ux0:/data/ps-vita-mqtt/current_title.txt`. The main plugin reads that
file each poll cycle.

It works fine in homebrew apps (VitaShell, etc.) but **does not load
into commercial Vita game processes** on FW 3.74 — taiHEN's `*ALL`
section and explicit `*<TITLEID>` sections both fail silently for
signed games. See [issue #1](../../issues/1) for the experiment.

If you still want to enable it (works for homebrew / non-signed apps):

```text
# add to ux0:/tai/config.txt (or ur0:/tai/config.txt)
*ALL
ur0:tai/ps-vita-mqtt-tag.suprx
```

And copy `build/ps-vita-mqtt-tag.suprx` to `ur0:tai/`.

## Roadmap

- Foreground TitleID detection (taiHEN hook on `sceShellUtilLaunchAppByUri`
  or similar)
- HA → Vita commands (reboot / standby) via MQTT SUBSCRIBE
- In-game popup notifications via a companion `.suprx` injected with `*ALL`
- TLS support (mbedTLS port)

## License

MIT — see [`LICENSE`](./LICENSE).
