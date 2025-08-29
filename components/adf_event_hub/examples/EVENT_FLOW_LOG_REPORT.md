# Event Flow Reference

This document describes the **runtime behavior** the multi-service example is
designed to produce: for every publisher/event pair, which subscribers react
and what visible effect the log should show.

The static subscription topology (who-subscribes-to-what) is in
[`README.md`](README.md#event-flow). This file focuses on the dynamic chain
reactions you should expect when reading the console output.

## How to Read an Output Line

Every event goes through two "observer" subscribers at the same time:

- **`Monitor`** — callback subscriber that logs `[MONITOR #N] <domain>/<event>` for
  every event on every domain. The `#N` counter is only meaningful inside one
  run and is useful for cross-referencing with service logs.
- **`LED`** — queue subscriber that logs `LED: <domain>/<event>` after draining
  each delivery from its own task.

If you only see a Monitor line with no LED line, the LED queue consumer is
running behind; both lines always eventually appear.

## 1. Sender `button`

Button publishes random presses with no payload. All reactions run in the
subscriber's task context (callback mode) or worker task (LED queue).

### `button/MODE`

- `wifi` logs `will reconnect` and publishes the reconnect sequence:
  `wifi/CONNECTING → wifi/SCAN_DONE → wifi/CONNECTED → wifi/GOT_IP → wifi/RSSI_UPDATE`.
- The resulting `wifi/GOT_IP` indirectly triggers `ota` (see below).
- `player` has no direct subscription; it only reacts to the downstream
  `wifi/DISCONNECTED` (if any) and `ota/COMPLETE`.

### `button/PROVISION`

- `wifi` toggles between connect and disconnect intent and publishes one of:
  - connect path — same sequence as `MODE`;
  - disconnect path — `wifi/DISCONNECTED`.
- Downstream effects (`ota`, `player/PAUSED`) follow as usual.

### `button/PLAY`

- `player` starts playback (`player/STARTED` + periodic `player/PROGRESS`) **as
  long as reboot is not pending**.
- After an `ota/COMPLETE` has been seen, the handler logs
  `ignored (reboot pending)` and does **not** publish `player/STARTED` — this
  is the primary regression guard.

### `button/VOL_UP` / `button/VOL_DOWN`

- `player` bumps its internal volume and publishes `player/VOLUME_CHANGED`
  carrying the new `int32_t` value.
- Still active after reboot-pending (volume changes are not gated).

## 2. Sender `wifi`

WiFi publishes lifecycle events with no payload except `RSSI_UPDATE` which
carries an `int32_t`.

### `wifi/CONNECTING`, `wifi/SCAN_DONE`, `wifi/CONNECTED`, `wifi/RSSI_UPDATE`

Informational in this example. Only `LED` and `Monitor` observe them;
no service reacts.

Key invariant: `wifi/CONNECTED` on its own does **not** trigger OTA — OTA
only subscribes to `GOT_IP`.

### `wifi/GOT_IP`

- `ota` logs `schedule OTA check` and starts a check-in-progress cycle which
  emits one of the sequences:
  - success: `ota/CHECK_START → ota/UPDATE_AVAILABLE → ota/PROGRESS×N → ota/COMPLETE`;
  - failure: `ota/CHECK_START → ota/UPDATE_AVAILABLE → ota/PROGRESS×M → ota/ERROR`;
  - no update: `ota/CHECK_START → ota/NO_UPDATE`.
- After the configured WiFi-triggered attempt limit is reached, the handler
  logs `wifi-triggered OTA limit reached` and skips scheduling a new one.

### `wifi/DISCONNECTED`

- `player` publishes `player/PAUSED` **if** playback is active.
- If the player is already idle (e.g., because reboot is pending) no extra
  `PAUSED` is published.

## 3. Sender `ota`

### `ota/CHECK_START`, `ota/UPDATE_AVAILABLE`, `ota/NO_UPDATE`, `ota/PROGRESS`, `ota/ERROR`

Informational for service subscribers — only `LED` and `Monitor` observe them.
`PROGRESS` carries a `uint8_t` percentage; `ERROR` carries a heap-allocated
C string (with release callback).

### `ota/COMPLETE`

- `player` receives the event and:
  - logs `stop requested for reboot`;
  - sets an internal `reboot_pending` flag;
  - if playback was active, requests stop (producing `player/STOPPED`);
  - blocks later `button/PLAY` and player self-play attempts.
- Subsequent `button/PLAY` presses log `ignored (reboot pending)` in the
  `player` handler.

This is the main cross-service state transition in the example.

## 4. Sender `player`

### `player/STARTED`, `player/PROGRESS`

Produced by self-play cycles or by `button/PLAY`; observed by `LED` and
`Monitor`. `PROGRESS` carries a `uint8_t` percentage.

### `player/PAUSED`, `player/STOPPED`

`PAUSED` is typically driven by `wifi/DISCONNECTED`. `STOPPED` is produced
when a stop is requested — either the self-initiated stop round or the
reboot-pending transition.

### `player/VOLUME_CHANGED`

Carries the new `int32_t` volume. Produced by both external presses
(`button/VOL_UP` / `VOL_DOWN`) and player self-actions.

### `player/ERROR`

Self-generated random error path; observed by `LED` and `Monitor`, no
service subscribes.

## Key Behaviors Verified by One Run

- After the first `ota/COMPLETE`, later `button/PLAY` presses never publish
  `player/STARTED` again.
- Self-play attempts by the player task are also skipped while
  `reboot_pending` is set.
- `wifi/DISCONNECTED` only produces `player/PAUSED` when playback is active;
  an idle player is not perturbed.
- `wifi/GOT_IP` is the sole trigger for OTA; `CONNECTING` / `CONNECTED` /
  `RSSI_UPDATE` never start an OTA check on their own.

## Coverage Gaps That a Single Random Run May Not Exercise

The simulation is randomized, so any particular run may miss one of these
specific chains:

- `button/PLAY → player/STARTED` after reboot became pending (intentionally
  blocked — but you should still see the pre-reboot path in a long run).
- `button/PLAY → player/STOPPED` while actively playing (depends on timing
  between self-play rounds and presses).
- Simultaneous `ota/COMPLETE` and `wifi/DISCONNECTED` landing in the same
  delivery window while playback is active.

If you need to reproduce any of these deterministically, bump the
corresponding `*_ROUNDS` macro in `common/example_run.c` and/or extend
`SIM_DURATION_MS`.
