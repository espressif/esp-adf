# ESP Media Service Agent Guide

Use this when implementing or reviewing services built on `esp_media_service`.

## What It Is

`esp_media_service` is the media layer above `esp_service`. Services expose streams, pass `esp_media_provider_t` handles at link time, and exchange frames through provider read APIs and track-manager write APIs.

Core files:

- `include/esp_media_service.h` — roles, streams, media ops, link/unlink
- `include/esp_media_service_types.h` — frames, tracks, provider ops
- `include/esp_media_track_mngr.h` — track manager lifecycle
- `include/esp_media_track.h` — write/abort/query frame APIs
- `include/esp_media_provider.h` — provider read APIs

## Service Model

One service object is one instance. Embed `esp_media_service_t` as the first member:

```c
typedef struct {
    esp_media_service_t     media;
    esp_media_track_mngr_t *mngr;
    esp_media_provider_t    provider;
} my_service_t;
```

Implement only the ops the service supports:

- `get_role` — returns `ESP_MEDIA_ROLE_SRC`, `ESP_MEDIA_ROLE_SINK`, or `ESP_MEDIA_ROLE_SRC_SINK`
- `get_provider` — source op; returns a provider for a stream
- `set_provider` — sink op; stores or clears an upstream provider
- `get_request` — sink op; declares link-time needs
- `set_request` — source op; applies those needs before `get_provider`

`esp_media_stream_id_t` is a `uint16_t` stream index. Use `ESP_MEDIA_DEFAULT_STREAM` for stream 0.

## Link Flow

`esp_media_service_link(src, src_stream, sink, sink_stream)`:

1. Checks source role if `get_role` exists
2. Checks sink role if `get_role` exists
3. Calls `sink.get_request()`
4. If request succeeds, calls `src.set_request()`
5. Calls `src.get_provider()`
6. Calls `sink.set_provider(provider)`

`ESP_ERR_NOT_SUPPORTED` from request ops is normal. Other errors stop linking.

`esp_media_service_unlink()` checks roles, then calls `sink.set_provider(NULL)`.

## Provider Read Side

Sinks consume frames through `esp_media_provider_t`:

- `esp_media_provider_get_track_num`, `get_track_info`
- `esp_media_provider_set_event_cb`
- `esp_media_provider_acquire_frame`, `read_frame`, `release_frame`
- `esp_media_provider_abort`

Always release frames from `esp_media_provider_acquire_frame()`. Do not use `frame.data` after release.

## Source Write Side

A source exposes an `esp_media_provider_t` through `get_provider()`. Sinks read from that handle; the source is responsible for filling it with frames. Choose one of two patterns:

### Custom provider ops

Use when the source already owns its frame pipeline — for example, hardware buffers, a codec output queue, or custom track rules.

Implement `esp_media_provider_ops_t` and return the handle from `get_provider()`:

```c
static const esp_media_provider_ops_t s_my_provider_ops = {
    .get_track_num = my_get_track_num,
    .get_track_info = my_get_track_info,
    .set_event_cb = my_set_event_cb,
    .acquire_frame = my_acquire_frame,
    .read_frame = my_read_frame,
    .release_frame = my_release_frame,
    .abort = my_abort,
};

// get_provider op:
out_provider->ops = &s_my_provider_ops;
out_provider->ctx = svc;
```

Your producer task pushes frames into internal storage. The provider ops expose that storage to linked sinks. You do not need `esp_media_track_mngr_t`.

### Track manager (recommended default)

Use when the source needs an external frame queue — for example, decoupling producer and consumer tasks, multi-track caching, or RTMP-like interleaved delivery.

Create a track manager, write frames through `esp_media_track_*`, and export its provider:

```c
esp_media_track_mngr_cfg_t cfg = {.max_track_num = 2};
esp_media_track_mngr_create(&cfg, &mngr);
esp_media_track_mngr_add_track(mngr, &track_cfg);
esp_media_track_mngr_get_provider(mngr, &provider);

esp_media_track_write_frame(mngr, &frame, timeout_ms);
// get_provider op: *out_provider = provider;
```

Cache modes (`cache_cfg.cache_type`):

- `ESP_MEDIA_TRACK_CACHE_INTERNAL` — manager owns payload bytes; `esp_media_track_acquire_frame()` is supported
- `ESP_MEDIA_TRACK_CACHE_USER` — manager queues metadata only; payload returned via `frame_release`

Global cache: enable with `cfg.use_global_cache` or `esp_media_track_mngr_set_global_cache()` before adding tracks. Useful for RTMP-like interleaved transports.

State helpers:

- `esp_media_track_mngr_query()` — queued block count and byte size
- `esp_media_track_clear_abort()` — clear abort and reset queues, keep tracks
- `esp_media_track_mngr_reset()` — remove tracks and reset queues
- `esp_media_track_mngr_destroy()` — reset and free all storage

## Tracks And Events

Frames must carry matching `track_id` and `type`.

Dynamic track changes queue control frames:

- `esp_media_track_mngr_update_track()` — `ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED`
- `esp_media_track_mngr_remove_track()` — `ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED`

Provider events (`esp_media_provider_event_t`):

- `ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED` — metadata changed
- `ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED` — track ended
- `ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT` — all tracks aborted; `info == NULL`

Register callbacks in `set_provider()`. Clear before replacing a provider.

## Abort And Stop

Read and write share one abort flag on the track manager.

- Source stop: call `esp_media_track_write_abort(mngr)` to notify downstream with `TRACKS_ABORT`
- Sink stop: set stop flag, call `esp_media_provider_abort()`, wait for tasks, release frames, unlink
- Unlink before reset/destroy when a link is active
- Do not reset/destroy a track manager while another task holds an acquired frame

## Common Patterns

Source:

```c
esp_media_track_mngr_create(&cfg, &svc->mngr);
esp_media_track_mngr_add_track(svc->mngr, &audio_track);
esp_media_track_mngr_get_provider(svc->mngr, &svc->provider);
// in get_provider op: *out = svc->provider;
```

Sink read loop:

```c
while (!svc->stop) {
    esp_media_frame_t frame = {0};
    if (esp_media_provider_acquire_frame(&svc->provider, &frame, timeout_ms) == ESP_OK) {
        process_frame(&frame);
        esp_media_provider_release_frame(&svc->provider, &frame);
    }
}
```

RTMP-like global cache request:

```c
request->need_global_cache = true;
// source set_request:
esp_media_track_mngr_set_global_cache(mngr, true, cache_size);
```

## Review Checklist

- Role ops match how the service is linked
- `set_request()` applied before `get_provider()`
- Global cache enabled before tracks are added
- Frame `track_id` and `type` match configured tracks
- Acquired provider frames are always released
- User-cache tracks have a valid `frame_release` callback
- Sinks handle `TRACKS_ABORT`
- Unlink before reset/destroy when linked
- `set_provider(NULL)` clears callbacks and local provider state
