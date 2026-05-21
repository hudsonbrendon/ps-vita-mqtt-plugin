# PS Vita MQTT Plugin — Design Spec

**Status:** Draft — 2026-05-21
**Author:** Hudson Brendon

## Goal

Build a Home Assistant integration for a jailbroken PS Vita that mirrors
the pattern of `hudsonbrendon/ps4-mqtt-plugin` and
`ErSeraph/switch-assistant`: a low-level plugin that publishes console
telemetry to an MQTT broker, with Home Assistant MQTT Discovery so the
sensors auto-create.

## Target hardware / firmware

- PS Vita (PCH-1000 / PCH-2000 / PSTV) running HENkaku Enso (FW 3.60 or
  3.65–3.74 via h-encore²)
- taiHEN kernel/plugin loader present and writable at `ux0:tai/config.txt`
- Network reachable to an MQTT broker (plain TCP, port 1883)

## Architecture decision

Three plugin shapes were considered:

| Shape | Runs when | Pros | Cons |
|---|---|---|---|
| User plugin in `*main` (SceShell) | In LiveArea only | Simple, full user APIs | Suspended during games — no telemetry while gaming |
| User plugin in `*ALL` | Every process | Always running | Multiple instances; broker connection races |
| Kernel plugin in `*KERNEL` | Boot → shutdown | Single instance, persistent | Kernel-mode APIs only; no user `scePower*` directly |

**Decision:** **Kernel plugin (`.skprx`) loaded in `*KERNEL`**, mirroring
the always-running sysmodule pattern of `switch-assistant`. Uses
`<psp2kern/...>` headers, runs a dedicated kernel thread, opens TCP
sockets via `ksceNet*`, calls `kscePower*` and `ksceNetCtl*` for
telemetry. No user-mode companion `.suprx` in MVP.

Rationale: the user explicitly asked for "always running" behavior
("taiHEN plugin sempre rodando"), which only the kernel plugin shape
delivers cleanly without multi-instance hacks.

## Out of scope (MVP)

- HA → Vita commands (reboot/shutdown/launch app) — publish-only.
- On-screen overlay / popup notifications.
- Live screen streaming (RTSP).
- TLS MQTT (broker must be on trusted LAN).
- Config-time UI on the Vita — config lives at
  `ux0:data/ps-vita-mqtt/config.json`, edited from PC over FTP/USB.

## Sensors (MVP)

All under one HA device named **PS Vita** via MQTT Discovery.

| Group | Sensor | Source API |
|---|---|---|
| Battery | level (%) | `kscePowerGetBatteryLifePercent` |
| Battery | charging | `kscePowerIsBatteryCharging` |
| Battery | temperature (°C) | `kscePowerGetBatteryTemp` |
| Battery | remaining time (min) | `kscePowerGetBatteryLifeTime` |
| System | firmware | `ksceKernelGetSystemSwVersion` |
| System | model | `ksceKernelGetModelForCDialog` / `ksceKernelGetModel` |
| System | uptime (sec) | `ksceKernelGetSystemTimeWide` from boot |
| System | plugin uptime | local tick counter |
| App | in-game (yes/no) | `ksceKernelGetProcessTitleId` of foreground proc |
| App | TitleID | same |
| App | game name | lookup TitleID against bundled `romfs/titles.txt` |
| Network | IP | `ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS)` |
| Network | SSID | `ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SSID)` |
| Network | RSSI | `ksceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_DBM)` |
| Network | link state | `ksceNetCtlInetGetState` |
| Plugin | publish count | local counter |
| Plugin | reconnect count | local counter |
| Plugin | availability (LWT) | `online` / `offline` retained |

## MQTT contract

- Protocol: MQTT 3.1.1 over plain TCP
- Client ID: `psvita-<mac-suffix>`
- Topic base: `psvita/<client_id>/...`
- LWT: `psvita/<client_id>/availability` payload `offline` retained, QoS 0
- All state topics published QoS 0, retained
- HA Discovery prefix: `homeassistant` (configurable)
- Discovery configs published once on each connect, retained

## Config file

`ux0:data/ps-vita-mqtt/config.json`:

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

If config file is missing or unreadable, plugin logs an error to
`ksceDebugPrintf` and exits cleanly (does not crash the kernel).

## Why this mirrors the reference projects

- **From ps4-mqtt-plugin**: minimal hand-rolled MQTT 3.1.1 publisher
  (no library deps), worker thread with reconnect+backoff, HA
  Discovery on every reconnect, LWT for availability, sensors split
  per file under `src/collectors/`, host-build target for unit tests.
- **From switch-assistant**: always-running background process
  (kernel plugin = Vita's analog of Atmosphere sysmodule), config
  file on storage, MQTT Discovery for auto-entity creation, single
  HA device with grouped sensors.

## Repo name

Suggested: `hudsonbrendon/ps-vita-mqtt-plugin` to match the existing
`ps4-mqtt-plugin` naming.
