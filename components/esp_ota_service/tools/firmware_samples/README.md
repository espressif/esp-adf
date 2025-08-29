# firmware_samples

Single source of truth for every firmware image that can be OTA-upgraded in
the examples and unit tests. Every binary here carries a semver suffix so
it's always obvious which example + version a file came from.

## Layout

```
firmware_samples/
├── README.md
├── manifest.json                # latest published set (size / sha256 / url / ...)
├── ota_http_v<major>.<minor>.<patch>.bin
├── ota_fs_v<major>.<minor>.<patch>.bin
├── userdata_v<major>.<minor>.<patch>.bin      # companion of ota_fs
├── ota_ble_v<major>.<minor>.<patch>.bin
└── flash_tone_v<major>.<minor>.<patch>.bin    # optional (external generator)
```

`*.bin` files published by `build_firmware.py` are generated artefacts and
are git-ignored. The only binaries that ship in the repo are a pair of
**reference samples** kept here for manual testing:

- `userdata_v0.0.0.bin` — full 450 KiB userdata partition image with a zero
  semver header; handy when you want to exercise the userdata-write path
  with a realistic-sized payload rather than the 1 KiB stub that
  `build_firmware.py fs` emits.
- `flash_tone_v0.5.0.bin` — upstream `mk_audio_tone.py` output
  (`project_name=ESP_TONE_BIN`, version `0.5.0`); used as a canned
  `flash_tone` partition payload. Regenerate with
  `python tools/audio_tone/mk_audio_tone.py -f out -r resources -v <ver> -F 1 -t flash_tone.bin`
  in the upstream esp-adf tree, then rename with a `_v<ver>` suffix.

Publish a fresh build-output payload with `build_firmware.py` (run from
`components/esp_ota_service/tools/`):

```
./build_firmware.py http -v 1.0.1
./build_firmware.py fs   -v 0.2.0
./build_firmware.py ble  -v 1.0.1
```

`build_firmware.py` writes the requested semver into `examples/ota_<x>/version.txt`,
runs `idf.py build`, copies the output to `firmware_samples/ota_<x>_v<ver>.bin`,
and refreshes `manifest.json`. For `fs` it also generates a matching
`userdata_v<ver>.bin` with the same 4-byte LE u32 semver header that
`esp_ota_service_version_pack_semver()` produces.

## manifest.json

One manifest tracks the most recently published firmware; everyone else
(HTTP server, downstream tooling) reads from this file.

```
{
    "example": "http",
    "project_name": "ota_http",
    "version": "1.0.1",
    "file": "ota_http_v1.0.1.bin",
    "url": "http://10.0.0.42:18070/firmware.bin",
    "size": 648496,
    "sha256": "...",
    "md5": "...",
    "updated_at": "2026-04-21T09:00:00Z"
}
```

Downstream scripts look up binaries by `<project>_v*.bin` and pick the latest
semver automatically, so you rarely need to read the manifest manually.

## Consumers

| Consumer                                         | Uses                                                      |
| ------------------------------------------------ | --------------------------------------------------------- |
| `tools/ota_http_serve.py`                        | Serves the newest `ota_http_v*.bin` at `/firmware.bin`.   |
| `tools/sdcard_writer` + `ota_fs_flash_sdcard.py` | Embeds latest `ota_fs_v*.bin` + `userdata_v*.bin`.        |
| `tools/ota_ble.py upload`                    | Uploads the latest `ota_ble_v*.bin` over BLE.         |
| `tools/ota_fs_test_resume.py`                    | Builds a newer `ota_fs_v*.bin` to drive the resume test.  |
