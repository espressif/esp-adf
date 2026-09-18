# Changelog

## v0.5.3

### Bug Fixes

- Return USER-cache frames on write abort and provider abort before wakeup, avoiding owner-pool leaks after queue reset
- Claim in-flight read/write nodes with atomic take before release to avoid double-free when abort races with frame release
- Abort waits for the consumer to release a held read frame; that release drains pending USER frames, then abort wakes queues
- Make write/provider abort single-flight so concurrent stop and task exit do not double-free USER frames

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
