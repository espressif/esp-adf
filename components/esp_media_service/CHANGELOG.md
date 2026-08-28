# Changelog


## v0.5.2

### Features

- Allow `esp_media_track_mngr_set_global_cache()` after tracks exist (re-link)
- Extended `esp_media_track_clear_abort()` to keep tracks while draining queues (no wakeup)
- Recreate global/track queues destroy-first to limit peak memory; set abort if recreate fails

### Bug Fixes

- Clear abort and drain queues when global cache is reconfigured with the same mode
- Skip `esp_media_track_mngr_update_track()` when typical codec/layout fields are unchanged
- Fixed `dummy_src` H264 start code length changes for SPS-PPS and NAL
- Fixed release frame failed to find frame in global_cache case

## v0.5.1

### Features

- Added `dummy_src`, `dummy_sink` for debug support

## v0.5.0

### Features

- Initial version of `esp_media_service`
