# adf_event_hub Multi-Service Example

A runnable demo that wires five independent services together through
`adf_event_hub` and prints the resulting event flow.  The same sources compile
on both ESP-IDF and a PC build (FreeRTOS POSIX simulator), so you can iterate
on the integration logic without flashing a board.

## Layout

```
examples/
├── CMakeLists.txt        # ESP-IDF top-level project
├── sdkconfig.defaults    # IDF build defaults
├── main/                 # IDF-only entry
│   ├── CMakeLists.txt    #   Component registration (pulls in ../common/*.c)
│   └── app_main.c        #   IDF entry (spawns example_run from a task)
├── pc/                   # PC-only glue
│   ├── CMakeLists.txt    #   Standalone CMake, fetches FreeRTOS-Kernel
│   ├── FreeRTOSConfig.h  #   POSIX simulator config
│   └── main.c            #   PC entry (starts scheduler, runs example task)
└── common/               # Shared sources (IDF + PC)
    ├── example_run.[ch]  #   Platform-neutral simulation body
    ├── demo_button_service.*      #   Publisher: random button presses (demo, not esp_button_service)
    ├── demo_wifi_service.*      #   Reacts to button, publishes wifi events (demo stub)
    ├── demo_ota_service.*         #   Reacts to wifi, publishes progress (demo, not esp_ota_service)
    ├── demo_player_service.*      #   Reacts to button/wifi/ota (demo stub)
    └── demo_led_service.*         #   Queue-mode consumer of all domains (demo stub)
```

## Build and Run

### PC (FreeRTOS POSIX simulator)

```bash
cd components/adf_event_hub/examples/pc
cmake -S . -B build -G Ninja
cmake --build build
./build/event_hub_example
```

The process exits cleanly after the simulation completes (~10 s).

### ESP-IDF target

```bash
cd components/adf_event_hub/examples
idf.py set-target esp32s3   # or any supported chip
idf.py build flash monitor
```

`app_main()` spawns the same `event_hub_example_run()` function in a dedicated
task; the log output matches the PC run.

## Event Flow

Five domain hubs run side-by-side. The arrows below trace the subscriptions
wired up in the service `_create()` functions:

```
                  ┌─────────────────────────────────────────────┐
                  │   monitor hub  (callback, ANY_ID all 4)     │
                  │   led hub      (queue,    ANY_ID all 4)     │
                  └──────▲─────────▲──────────▲────────▲────────┘
                         │         │          │        │
  button ──PROVISION,MODE┤         │          │        │
  button ──PLAY,VOL_UP,VOL_DOWN────┐          │        │
                         │         │          │        │
                         ▼         ▼          │        │
                       wifi      player       │        │
                         │         ▲          │        │
                         │         │          │        │
                         └GOT_IP─► ota ──COMPLETE──────┘
                                   │
  wifi ──DISCONNECTED─► player ◄───┘
```

Per-service subscription list (see `*_service_create()` for the exact `info`
setup):

| Subscriber | Domain   | Event                                | Mode     |
|------------|----------|--------------------------------------|----------|
| wifi       | button   | `PROVISION`, `MODE`                  | callback |
| ota        | wifi     | `GOT_IP`                             | callback |
| player     | button   | `PLAY`, `VOL_UP`, `VOL_DOWN`         | callback |
| player     | wifi     | `DISCONNECTED`                       | callback |
| player     | ota      | `COMPLETE`                           | callback |
| led        | button / wifi / ota / player | ANY_ID               | queue    |
| monitor    | button / wifi / ota / player | ANY_ID               | callback |

What each publisher emits:

- **button** — `PROVISION`, `PLAY`, `VOL_UP`, `VOL_DOWN`, `MODE` (random presses).
- **wifi** — `CONNECTING`, `CONNECTED`, `DISCONNECTED`, `GOT_IP`, `SCAN_DONE`,
  `RSSI_UPDATE` (`RSSI_UPDATE` carries an `int32_t` payload with refcounted release).
- **ota** — `CHECK_START`, `UPDATE_AVAILABLE`, `PROGRESS` (`uint8_t %`),
  `COMPLETE`, `ERROR` (C-string), `NO_UPDATE`.
- **player** — `STARTED`, `PAUSED`, `STOPPED`, `PROGRESS` (`uint8_t %`),
  `VOLUME_CHANGED` (`int32_t`), `ERROR` (C-string).

For the full publish→react behavior chains (what downstream events each
publisher's event actually produces at runtime, and which state transitions
are guarded), see [`EVENT_FLOW_LOG_REPORT.md`](EVENT_FLOW_LOG_REPORT.md).

## What It Exercises

- **Cross-domain fan-out** — button events drive wifi/player/led reactions.
- **Mixed delivery modes** — callback subscribers for low-latency monitors,
  queue subscribers (LED) for bulk consumption.
- **Payload ownership** — services publish heap payloads with release callbacks
  so you can validate the refcounted lifecycle.
- **Runtime introspection** — calls `adf_event_hub_dump()` and
  `adf_event_hub_get_stats()` before teardown to print subscriber counts and
  envelope pool usage.
