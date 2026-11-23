# adf_event_hub Test App

This ESP-IDF project runs the same 116 Unity cases on hardware and in EMU.
The cases live in `common/test_*.c` and exercise argument validation,
delivery and payload ownership, synchronous cleanup, concurrency, statistics,
and high-subscriber workloads.

## Hardware

```bash
cd components/adf_event_hub/test_apps
idf.py set-target esp32s3
idf.py -p <serial-port> flash monitor
```

Select all Unity cases by entering `*`. The firmware prints
`<<< ADF_EVENT_HUB_UT_DONE >>>` after the Unity summary.

## EMU

From the repository root, build and run the same test app with `pytest` and
[`esp-emu`](https://github.com/espressif/esp-emulator):

```bash
idf.py -C components/adf_event_hub/test_apps -B components/adf_event_hub/test_apps/build_esp32s3_default \
  -D IDF_TARGET=esp32s3 build
pytest components/adf_event_hub/test_apps --embedded-services idf,espemu --target esp32s3
```

Add new tests to `common/`, include `test_port.h`, and use
`TEST_CASE("description", TEST_TAG)`. The component automatically includes
`test_*.c`.
