# Tuya-Local EKAZA Zigbee Lock Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fork [make-all/tuya-local](https://github.com/make-all/tuya-local), add a new device YAML for the EKAZA `EKAV-T229Z` Zigbee smart lock (category `jtmspro`), install the fork in Home Assistant, validate both household locks (`Porta da cozinha`, `Porta da sala`), and open a PR upstream.

**Architecture:**
- `tuya-local` is a Home Assistant custom component that maps Tuya devices to HA entities via per-device YAML files in `custom_components/tuya_local/devices/`. New device support = new YAML + entry in test config. No Python required.
- The EKAZA `EKAV-T229Z` is a Zigbee subdevice (`sub: true`, no own IP). It is reachable through a Tuya Zigbee gateway already on the LAN and is already detected by `tuya-local` — the integration just lacks a YAML whose `products[].id` matches the device's Tuya product ID and whose DPs match the device's datapoints.
- DP **names** are visible via the Tuya Cloud MCP (`tuya_get_device_status`), but YAML requires the **numeric** DP IDs. These are obtained from the Tuya IoT Developer Portal "Things Data Model" view (preferred) and cross-checked against the `tuya_local` debug logs in Home Assistant.

**Tech Stack:** Python 3.12+, `uv` (package manager), `ruff` (lint/format), `yamllint`, `pytest`, Home Assistant 2024.x+, HACS (custom repository install).

**Reference device captured via Tuya Cloud MCP:**

```text
id:           eb60b9814dedfcd7a1pieo
name:         Porta da cozinha
category:     jtmspro
product_name: EKAZA Door locker Zigbee
model:        EKAV-T229Z
sub:          true
ip:           ""    # zigbee subdevice — gateway-routed
```

DP names observed in `tuya_get_device_status` response (numeric IDs TBD in Task 4):

```text
unlock_fingerprint, unlock_password, unlock_temporary, unlock_dynamic, unlock_card,
open_close (bool), alarm_lock (enum: e.g. "wrong_finger"), residual_electricity (int, %),
reverse_lock (bool), doorbell (bool), doorbell_song (enum: "ding_0"),
remote_result (bool), hijack (bool), unlock_remote (int seconds),
closed_opened (enum: "closed"/"opened"), remote_no_pd_setkey (base64),
remote_no_dp_key (base64), unlock_method_create (string), unlock_method_delete (string),
lock_motor_state (bool), unlock_voice_remote (int), unlock_offline_pd (string)
```

**Reference files to study before starting** (in upstream repo, not local):
- `custom_components/tuya_local/devices/intelbras_IFR7000_lock.yaml` — closest functional analogue (Brazilian lock, similar DP names)
- `custom_components/tuya_local/devices/novadigital_sl06_lock.yaml` — has `alarm` bitfield + battery sensor + hijack patterns
- `AGENTS.md` — coding standards (UV, ruff, yamllint, naming)
- `DEVICE_DETAILS.md` — DP discovery instructions

**File structure created/modified:**

```text
~/Github/tuya-local/                                        (new clone of fork)
├── custom_components/tuya_local/devices/
│   └── ekaza_ekavt229z_lock.yaml                            (NEW — primary deliverable)
└── (no other files modified — existing test_device_config.py auto-discovers new yamls)
```

---

### Task 1: Fork upstream and clone into a working directory

**Files:**
- Create: `~/Github/tuya-local/` (new clone)

- [ ] **Step 1: Fork the upstream repo on GitHub**

Run from any directory:

```bash
gh repo fork make-all/tuya-local --clone=false --remote=false
```

Expected output: `✓ Created fork hudsonbrendon/tuya-local`

If `gh` is not authenticated:

```bash
gh auth login
```

- [ ] **Step 2: Clone the fork into ~/Github/tuya-local**

```bash
cd ~/Github && git clone git@github.com:hudsonbrendon/tuya-local.git
cd ~/Github/tuya-local
```

Expected: directory `~/Github/tuya-local` exists with `custom_components/tuya_local/devices/` inside.

- [ ] **Step 3: Add upstream remote for syncing**

```bash
cd ~/Github/tuya-local
git remote add upstream https://github.com/make-all/tuya-local.git
git fetch upstream
git remote -v
```

Expected: `origin` points to `hudsonbrendon/tuya-local`, `upstream` points to `make-all/tuya-local`.

- [ ] **Step 4: Create feature branch**

```bash
cd ~/Github/tuya-local
git checkout -b feat/ekaza-ekavt229z-lock upstream/main
```

Expected: switched to a new branch tracking the latest upstream main.

- [ ] **Step 5: Commit the branch creation as a marker (optional, no-op)**

Skip — no changes yet. Proceed to Task 2.

---

### Task 2: Bootstrap the development environment

**Files:** (none modified; tooling install only)

- [ ] **Step 1: Verify `uv` is installed**

```bash
uv --version
```

Expected: `uv 0.x.y` (any version ≥ 0.4).

If missing:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

- [ ] **Step 2: Install project dependencies into a uv-managed virtualenv**

```bash
cd ~/Github/tuya-local
uv sync
```

Expected: `Resolved N packages` then `Installed N packages`. A `.venv` directory appears.

- [ ] **Step 3: Install pre-commit hooks (if `.pre-commit-config.yaml` present)**

```bash
cd ~/Github/tuya-local
uv run pre-commit install
```

Expected: `pre-commit installed at .git/hooks/pre-commit`.

- [ ] **Step 4: Run the full test suite once to confirm a green baseline**

```bash
cd ~/Github/tuya-local
uv run pytest -q
```

Expected: all tests pass. Note pass/fail counts for comparison after the YAML is added.

If tests fail on a fresh clone, stop and investigate — do not proceed with a broken baseline.

- [ ] **Step 5: Commit nothing — environment is local-only**

No commit. Move to Task 3.

---

### Task 3: Capture the EKAZA Tuya product ID

The `products[].id` field in the YAML must match the Tuya **product ID** (16 lowercase alphanumeric chars, e.g. `a6nttc41` for the Intelbras IFR7000). This is **not** the device ID. It identifies the SKU and is the join key against which `tuya-local` matches the YAML.

**Files:** (read-only research)

- [ ] **Step 1: Log into the Tuya IoT Developer Portal**

Open https://iot.tuya.com in a browser. Authenticate with the same account that owns the EKAZA lock.

If you do not yet have a cloud project linked to this account, follow https://github.com/make-all/tuya-local/blob/main/DEVICE_DETAILS.md to create one and link your app account.

- [ ] **Step 2: Locate the device**

Navigate: Cloud → Development → (your project) → Devices → Linked Devices.

Find the row for `Porta da cozinha` (device ID `eb60b9814dedfcd7a1pieo`).

- [ ] **Step 3: Record the Product ID**

In the device row, the column labelled **Product ID** (or **PID**) holds the value you need. Record it. Expected format: 16 lowercase alphanumeric characters.

Write the value into a scratch note for use in Task 5:

```text
EKAZA_PRODUCT_ID = <value-from-portal>
```

- [ ] **Step 4: If the second lock (`Porta da sala`) is linked, capture its Product ID too**

If `Porta da sala` is not yet visible in the portal, it is not yet paired with the Tuya account — pair it via the Tuya/EKAZA app first, then re-check. If it shares a Product ID with the kitchen lock, one YAML covers both. If it has a different Product ID, add a second entry to `products:` in Task 5 with the same `manufacturer`/`model` block.

- [ ] **Step 5: No commit — research only**

---

### Task 4: Capture numeric DP IDs from the Tuya Things Data Model

YAML DPs need numeric `id:` fields. The Cloud MCP only returns DP names. Numeric IDs come from the Things Data Model.

**Files:** (read-only research)

- [ ] **Step 1: Open the Things Data Model for the device**

In the Tuya IoT Portal device view for `Porta da cozinha`, click the device → tab **Device Debugging** → sub-tab **Things Data Model** (or use Cloud → API Explorer → "Query Things Data Model").

- [ ] **Step 2: Record each DP's numeric code, name, and type**

Tabulate every DP listed. Expected approximate set (names already observed via `tuya_get_device_status`):

| DP code (id) | name | type | notes |
|---|---|---|---|
| ? | unlock_fingerprint | integer (raw / value) | user index that unlocked |
| ? | unlock_password | integer | user index |
| ? | unlock_temporary | integer | user index |
| ? | unlock_dynamic | integer | user index |
| ? | unlock_card | integer | user index |
| ? | open_close | boolean | momentary open command |
| ? | alarm_lock | enum (string) | wrong_finger, wrong_password, ... |
| ? | residual_electricity | integer | battery % |
| ? | reverse_lock | boolean | privacy bolt |
| ? | doorbell | boolean | doorbell pressed |
| ? | doorbell_song | enum | ding_0..N |
| ? | remote_result | boolean | remote unlock result |
| ? | hijack | boolean | duress alarm |
| ? | unlock_remote | integer | remote unlock window seconds |
| ? | closed_opened | enum | closed / opened |
| ? | remote_no_pd_setkey | string (base64) | provisioning key |
| ? | remote_no_dp_key | string (base64) | rolling key |
| ? | unlock_method_create | string | user enrolment events |
| ? | unlock_method_delete | string | user removal events |
| ? | lock_motor_state | boolean | bolt position true=locked |
| ? | unlock_voice_remote | integer | voice unlock |
| ? | unlock_offline_pd | string | offline password event |

Save the filled table in a scratch file `~/Github/tuya-local/.notes/ekaza_dps.md` (gitignored — do not commit) for reference in Tasks 5–10.

```bash
mkdir -p ~/Github/tuya-local/.notes
echo ".notes/" >> ~/Github/tuya-local/.gitignore   # only if .gitignore lacks it
```

- [ ] **Step 3: Cross-check at least one DP value live**

Use the MCP `tuya_send_command` against a non-destructive DP (e.g. read-only ones are already returning values via `tuya_get_device_status` — confirm the same names appear). This sanity-checks that the Things Data Model snapshot matches runtime.

- [ ] **Step 4: Commit nothing — this is research, the file is gitignored**

---

### Task 5: Create the YAML skeleton (`name`, `products`, empty `entities`)

**Files:**
- Create: `~/Github/tuya-local/custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Write the file with header and product mapping**

Create `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`:

```yaml
name: Door lock
products:
  - id: <EKAZA_PRODUCT_ID_FROM_TASK_3>
    manufacturer: EKAZA
    model: EKAV-T229Z
entities: []
```

Replace `<EKAZA_PRODUCT_ID_FROM_TASK_3>` with the literal 16-char ID. If `Porta da sala` has a distinct product ID, add a second entry under `products:` with the same manufacturer/model.

Per `AGENTS.md` naming rules: `name:` is unbranded (`Door lock`, not `EKAZA Door lock`); filename uses the brand prefix.

- [ ] **Step 2: Run yamllint to confirm syntax is valid**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
```

Expected: no output (success).

- [ ] **Step 3: Run the device-config test (will fail — `entities` is empty)**

```bash
cd ~/Github/tuya-local
uv run pytest tests/test_device_config.py -q
```

Expected: failure with a message about `ekaza_ekavt229z_lock.yaml` having no entities, or similar. This is the failing test that drives Task 6.

- [ ] **Step 4: Commit the skeleton**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): add EKAZA EKAV-T229Z lock skeleton"
```

---

### Task 6: Add the `lock` entity with unlock-method DPs and lock state

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Replace `entities: []` with the lock entity**

Substitute the `?` placeholders below with the numeric DP IDs from Task 4. The `lock_motor_state` mapping inverts the boolean because `tuya-local`'s `lock_state` semantics are `true = locked`, and the EKAZA datapoint already reports `true = locked` — keep direct unless Task 4 notes otherwise (Intelbras inverts because its DP reports the opposite).

```yaml
name: Door lock
products:
  - id: <EKAZA_PRODUCT_ID_FROM_TASK_3>
    manufacturer: EKAZA
    model: EKAV-T229Z
entities:
  - entity: lock
    dps:
      - id: ?   # unlock_fingerprint
        type: integer
        name: unlock_fingerprint
        optional: true
        persist: false
      - id: ?   # unlock_password
        type: integer
        name: unlock_password
        optional: true
        persist: false
      - id: ?   # unlock_temporary
        type: integer
        name: unlock_temp_pwd
        optional: true
        persist: false
      - id: ?   # unlock_dynamic
        type: integer
        name: unlock_dynamic_pwd
        optional: true
        persist: false
      - id: ?   # unlock_card
        type: integer
        name: unlock_card
        optional: true
        persist: false
      - id: ?   # lock_motor_state (true = locked)
        type: boolean
        name: lock_state
      - id: ?   # alarm_lock (enum)
        type: string
        name: alarm
        optional: true
      - id: ?   # remote_result
        type: boolean
        name: remote_result
        optional: true
      - id: ?   # unlock_voice_remote
        type: integer
        name: unlock_voice
        optional: true
        persist: false
```

Names like `unlock_temp_pwd`, `unlock_dynamic_pwd`, `lock_state`, `alarm`, `remote_result`, `unlock_voice` are the **canonical names** tuya-local's lock entity recognises (per the `devices/README.md` schema). Map the EKAZA DP names to those canonical names via the `name:` field.

- [ ] **Step 2: Validate YAML syntax**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
```

Expected: no output.

- [ ] **Step 3: Run device-config tests**

```bash
cd ~/Github/tuya-local
uv run pytest tests/test_device_config.py -q
```

Expected: pass. (The test discovers all device yamls automatically and validates schema/entity types.)

- [ ] **Step 4: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — lock entity"
```

---

### Task 7: Add battery sensor (`residual_electricity`)

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append battery sensor entity**

Append to the `entities:` list:

```yaml
  - entity: sensor
    class: battery
    category: diagnostic
    dps:
      - id: ?   # residual_electricity
        type: integer
        name: sensor
        unit: "%"
```

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: yamllint silent; pytest passes.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — battery sensor"
```

---

### Task 8: Add door open/closed binary sensor (`closed_opened`)

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append the binary_sensor entity**

```yaml
  - entity: binary_sensor
    class: door
    category: diagnostic
    dps:
      - id: ?   # closed_opened
        type: string
        name: sensor
        mapping:
          - dps_val: opened
            value: true
          - dps_val: closed
            value: false
```

If Task 4 confirms `closed_opened` is a boolean rather than an enum, use:

```yaml
      - id: ?
        type: boolean
        name: sensor
```

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: pass.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — door open/closed sensor"
```

---

### Task 9: Add hijack (duress) event

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append the event entity**

```yaml
  - entity: event
    name: Hijack
    class: safety
    category: diagnostic
    dps:
      - id: ?   # hijack
        type: boolean
        name: event
        optional: true
        mapping:
          - dps_val: true
            value: hijack
          - dps_val: false
            value: clear
```

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: pass.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — hijack event"
```

---

### Task 10: Add doorbell event + chime select

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append doorbell event + chime selector**

Confirm via Task 4 the exact enum values for `doorbell_song` (the observed `"ding_0"` suggests `ding_0..ding_N`). Substitute below.

```yaml
  - entity: event
    class: doorbell
    category: diagnostic
    dps:
      - id: ?   # doorbell
        type: boolean
        name: event
        optional: true
        mapping:
          - dps_val: true
            value: ring
          - dps_val: false
            value: null
  - entity: select
    name: Doorbell chime
    icon: "mdi:music"
    category: config
    dps:
      - id: ?   # doorbell_song
        type: string
        name: option
        optional: true
        mapping:
          - dps_val: ding_0
            value: Chime 1
          - dps_val: ding_1
            value: Chime 2
          - dps_val: ding_2
            value: Chime 3
```

Extend the chime mapping with every enum value listed in the Things Data Model. Do not invent values not in the schema.

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: pass.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — doorbell event + chime select"
```

---

### Task 11: Add reverse_lock switch (privacy bolt)

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append the switch entity**

```yaml
  - entity: switch
    name: Reverse lock
    icon: "mdi:lock-alert"
    category: config
    dps:
      - id: ?   # reverse_lock
        type: boolean
        name: switch
        optional: true
```

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: pass.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — reverse lock switch"
```

---

### Task 12: Add remote unlock timer + button

**Files:**
- Modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Append `request_unlock` integer + `open_close` button**

`unlock_remote` is the remote unlock approval window (seconds); `open_close` triggers an unlock pulse.

```yaml
  - entity: number
    name: Remote unlock window
    icon: "mdi:timer-lock-open"
    category: config
    dps:
      - id: ?   # unlock_remote
        type: integer
        name: value
        optional: true
        unit: s
        range:
          min: 0
          max: 3600
  - entity: button
    name: Open door
    icon: "mdi:door-open"
    dps:
      - id: ?   # open_close
        type: boolean
        name: button
        optional: true
```

If Task 4 reveals that `open_close` is read-only (status only), drop the `button` entity and add a `binary_sensor` instead. Decide based on the Things Data Model "Permission" column (`rw` vs `ro`).

- [ ] **Step 2: Validate and test**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
```

Expected: pass.

- [ ] **Step 3: Commit**

```bash
cd ~/Github/tuya-local
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — remote unlock window + open button"
```

---

### Task 13: Run the full repo lint + test gate

**Files:** (none modified)

- [ ] **Step 1: Lint Python (must pass even though no Python changed)**

```bash
cd ~/Github/tuya-local
uv run ruff check .
uv run ruff check --select -I .
uv run ruff format --check .
```

Expected: all silent / `All checks passed`.

- [ ] **Step 2: Lint all device YAMLs**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices
```

Expected: silent.

- [ ] **Step 3: Run the entire pytest suite**

```bash
cd ~/Github/tuya-local
uv run pytest -q
```

Expected: same pass/fail counts as the Task 2 baseline plus the new YAML passing under `test_device_config.py`. Zero failures.

- [ ] **Step 4: No commit — gate only**

---

### Task 14: Install the fork into Home Assistant via HACS

**Files:** (none in this repo)

- [ ] **Step 1: Push the branch to your fork**

```bash
cd ~/Github/tuya-local
git push -u origin feat/ekaza-ekavt229z-lock
```

Expected: branch published to `hudsonbrendon/tuya-local`.

- [ ] **Step 2: Open Home Assistant → HACS → Integrations → 3-dot menu → Custom Repositories**

Add:

- Repository: `https://github.com/hudsonbrendon/tuya-local`
- Category: `Integration`

Click **Add**.

- [ ] **Step 3: Remove any prior `tuya-local` install first**

If the upstream `make-all/tuya-local` is already installed via HACS, remove it (HACS → Integrations → tuya-local → 3-dot → Remove) and **restart Home Assistant** before installing the fork. Both cannot coexist.

- [ ] **Step 4: Install the fork from the new custom repository entry**

In HACS, find "Tuya local" sourced from `hudsonbrendon/tuya-local`, click **Install**, choose the `feat/ekaza-ekavt229z-lock` branch (HACS → Redownload → branch selector), and restart Home Assistant.

- [ ] **Step 5: Confirm the version label in HACS shows the branch / commit hash**

After restart, HACS → tuya-local → Information should display the fork branch / commit, not the upstream release.

- [ ] **Step 6: No commit**

---

### Task 15: Pair `Porta da cozinha` and verify entities

**Files:** (none in this repo)

- [ ] **Step 1: Enable debug logging for tuya_local**

Add to Home Assistant `configuration.yaml`:

```yaml
logger:
  default: warning
  logs:
    custom_components.tuya_local: debug
```

Restart Home Assistant. Tail the log:

```bash
# from a Home Assistant SSH session or `ha core logs` add-on
tail -F /config/home-assistant.log | grep tuya_local
```

- [ ] **Step 2: Add the device via the integration UI**

Home Assistant → Settings → Devices & services → Add integration → "Tuya local". Enter:

- Device ID: `eb60b9814dedfcd7a1pieo`
- Local key: (from Tuya IoT Portal → API Explorer → "Query Device Details in Bulk")
- Host: `Auto` (or the gateway IP)
- Device type: the wizard should auto-match `EKAZA EKAV-T229Z Door lock`.

Expected: the wizard offers the EKAZA EKAV-T229Z lock as a match. If it does not, the `products[].id` in the YAML does not match — revisit Task 3.

- [ ] **Step 3: Confirm every entity defined in Tasks 6–12 appears in HA**

In Settings → Devices → Door lock, verify the presence of:

- `lock.porta_da_cozinha` (lockable)
- `sensor.porta_da_cozinha_battery` (reports % from Cloud MCP showed 100)
- `binary_sensor.porta_da_cozinha_door` (closed/open)
- `event.porta_da_cozinha_hijack`
- `event.porta_da_cozinha_doorbell`
- `select.porta_da_cozinha_doorbell_chime`
- `switch.porta_da_cozinha_reverse_lock`
- `number.porta_da_cozinha_remote_unlock_window`
- `button.porta_da_cozinha_open_door`

- [ ] **Step 4: Trigger each entity end-to-end at least once**

- Press a fingerprint → confirm `lock` state updates and `alarm` clears.
- Press the bell physically → confirm doorbell event fires.
- Toggle `Reverse lock` → confirm the bolt reacts.
- Press `Open door` button → confirm lock briefly unlocks.
- Adjust `Remote unlock window` → confirm value persists.

- [ ] **Step 5: Capture log output**

Save the relevant `tuya_local` debug lines from the log to `~/Github/tuya-local/.notes/ha_runtime_dps.log` (gitignored). This is evidence for the PR.

- [ ] **Step 6: No commit unless YAML adjustments are made (see Task 17)**

---

### Task 16: Pair `Porta da sala` and verify the same product ID

**Files:** (none in this repo)

- [ ] **Step 1: Pair the living-room lock with the Tuya app**

If `Porta da sala` is not yet in the Tuya/EKAZA app, pair it (follow EKAZA's app pairing flow). Once it appears in the app and the IoT portal, it is reachable.

- [ ] **Step 2: Confirm same product ID**

Open the Tuya IoT Portal → device `Porta da sala` → record its Product ID.

If it equals `EKAZA_PRODUCT_ID` from Task 3, the existing YAML covers both. Skip to Step 4.

If different, append a second entry under `products:`:

```yaml
products:
  - id: <kitchen product id>
    manufacturer: EKAZA
    model: EKAV-T229Z
  - id: <living room product id>
    manufacturer: EKAZA
    model: EKAV-T229Z
```

- [ ] **Step 3: If YAML was updated, re-validate and commit**

```bash
cd ~/Github/tuya-local
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "feat(devices): EKAZA EKAV-T229Z — second product variant"
git push
```

Then HACS → Redownload to pull the new commit into HA. Restart HA.

- [ ] **Step 4: Add `Porta da sala` via Settings → Devices & services**

Same wizard flow as Task 15 with the new device ID + local key. Verify all entities appear and behave.

---

### Task 17: Reconcile any divergences observed at runtime

**Files:**
- Possibly modify: `custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml`

- [ ] **Step 1: Compare runtime DPS log to the YAML**

Inspect `~/Github/tuya-local/.notes/ha_runtime_dps.log`. Look for:

- DP IDs that appear in the log but are absent from the YAML → add them as `optional: true` diagnostic sensors.
- DPs declared in the YAML that never appear in the log → keep `optional: true` so absence is non-fatal.
- Enum values that the device emits but the YAML mapping doesn't cover → extend the mapping. **Do not** silently drop unknown values; either map them or remove the entity.
- DPs whose type in the YAML disagrees with the log (e.g. string vs integer) → fix the YAML.

- [ ] **Step 2: For each divergence, edit the YAML and re-test**

For each change:

```bash
cd ~/Github/tuya-local
# edit the yaml
uv run yamllint custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml \
  && uv run pytest tests/test_device_config.py -q
git add custom_components/tuya_local/devices/ekaza_ekavt229z_lock.yaml
git commit -m "fix(devices): EKAZA EKAV-T229Z — <one-line description of the divergence>"
```

Then HACS → Redownload, restart HA, re-verify the affected entity.

- [ ] **Step 3: Stop when the runtime log shows no unexpected DP IDs**

A clean log over a 24-hour observation window (covering at least one of each: fingerprint unlock, password unlock, door open + close cycle, doorbell press, battery report) is the exit criterion.

---

### Task 18: Open the upstream pull request

**Files:** (none in this repo)

- [ ] **Step 1: Sync with upstream once more before opening the PR**

```bash
cd ~/Github/tuya-local
git fetch upstream
git rebase upstream/main
```

If rebase reports conflicts in unrelated files, abort and ask — do not force-resolve.

- [ ] **Step 2: Re-run the full lint + test gate**

```bash
cd ~/Github/tuya-local
uv run ruff check .
uv run ruff check --select -I .
uv run ruff format --check .
uv run yamllint custom_components/tuya_local/devices
uv run pytest -q
```

Expected: all green.

- [ ] **Step 3: Force-push the rebased branch to your fork**

```bash
cd ~/Github/tuya-local
git push --force-with-lease origin feat/ekaza-ekavt229z-lock
```

`--force-with-lease` is used because the branch was rebased; it refuses to overwrite remote work that wasn't in the local view, which `--force` alone does not.

- [ ] **Step 4: Open the PR using `gh`**

```bash
cd ~/Github/tuya-local
gh pr create \
  --repo make-all/tuya-local \
  --base main \
  --head hudsonbrendon:feat/ekaza-ekavt229z-lock \
  --title "Add EKAZA EKAV-T229Z Zigbee door lock" \
  --body "$(cat <<'EOF'
## Summary

Adds a new device config for the EKAZA EKAV-T229Z Zigbee smart lock (category `jtmspro`), used in two Brazilian-market door locks.

## Device

- **Brand/model:** EKAZA EKAV-T229Z (also marketed as "EKAZA Door locker Zigbee")
- **Category:** `jtmspro`
- **Connectivity:** Zigbee subdevice routed through a Tuya Zigbee gateway
- **Product ID(s):** `<insert>` (kitchen unit), `<insert>` (living-room unit, if distinct)

## Entities added

- `lock` — with `unlock_fingerprint`, `unlock_password`, `unlock_temp_pwd`, `unlock_dynamic_pwd`, `unlock_card`, `lock_state`, `alarm`, `remote_result`, `unlock_voice`
- `sensor` (battery, %)
- `binary_sensor` (door open/closed)
- `event` (hijack / duress)
- `event` (doorbell)
- `select` (doorbell chime)
- `switch` (reverse lock / privacy bolt)
- `number` (remote unlock window)
- `button` (open door)

## Validation

- `uv run yamllint custom_components/tuya_local/devices` — clean
- `uv run pytest tests/test_device_config.py` — passes
- `uv run pytest` — full suite passes
- Runtime-tested in Home Assistant against two physical EKAV-T229Z units (kitchen + living room); 24-hour DPS log shows no unmapped datapoints.

## Notes

- DPS IDs were captured from the Tuya IoT Portal Things Data Model and cross-checked against `custom_components.tuya_local` debug logs in Home Assistant.
- Mapping conventions follow `intelbras_IFR7000_lock.yaml` and `novadigital_sl06_lock.yaml`, which share most DP names with this device.
EOF
)"
```

- [ ] **Step 5: Verify CI on the PR**

```bash
cd ~/Github/tuya-local
gh pr checks --repo make-all/tuya-local --watch
```

Expected: every check green. If any fail, fix locally, push, and re-watch.

- [ ] **Step 6: Respond to maintainer review**

The maintainer may rename the YAML (per the project policy on generic-vs-specific names) or trim entities. Apply requested changes with new commits; do not amend the merged history.

---

## Self-Review Checklist

**Spec coverage**

- Fork the upstream project → Task 1.
- Use the Tuya MCP to extract what's needed from a linked device → Tasks 3 + 4 (Product ID + DP list) — research-only steps backed by real Cloud MCP output already captured at plan-writing time.
- Increase the number of integrations in `tuya-local` → Tasks 5–12 add a new device config.
- Specifically for the kitchen and living-room Ziggs/EKAZA locks → Tasks 15 + 16 cover both physical units, including the case where they share or differ in Product ID.
- Install the fork in Home Assistant → Task 14 (HACS custom repository).
- Configure the devices in HA from the fork → Tasks 15 + 16.
- Open a PR upstream → Task 18.

**Placeholder scan**

The DP-id `?` placeholders in Tasks 5–12 are intentional and are explicitly fed by Task 4's research step (Things Data Model lookup). Engineers must substitute them from the real DP table before running tests. No vague "implement appropriate handling" anywhere; every step gives either the exact YAML or the exact command.

**Type consistency**

- The lock entity uses canonical names (`lock_state`, `alarm`, `unlock_temp_pwd`, etc.) consistent with both reference YAMLs (`intelbras_IFR7000_lock.yaml`, `novadigital_sl06_lock.yaml`) and `custom_components/tuya_local/devices/README.md`.
- `lock_state` is declared `boolean` and the EKAZA `lock_motor_state` DP is also boolean per the Cloud MCP status sample (`"lock_motor_state": true`).
- `closed_opened` is mapped as a string-enum because the Cloud MCP status shows `"closed"` (string), with an explicit fallback to boolean if Task 4 contradicts.
- `doorbell_song` mapped as string-enum because the Cloud MCP status shows `"ding_0"` (string).
