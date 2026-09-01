/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Scheduler thread names for esp_player_service
 *
 *         Tune stack / priority / core through `esp_service_scheduler_set_cb()`.
 *         Requests use `cfg.name` (`ESP_PLAYER_SERVICE_DEFAULT_NAME` is `"player"`;
 *         the audio / video subclasses use `"audio_player"` / `"video_player"`).
 *         Audio and video subclasses share these names; they do not define aliases.
 *
 *         Playback task flow (data moves left to right). Video threads exist only
 *         when a video output is attached.
 *
 *         URL (container):
 *
 *             file/http --> extractor --+--> aud_dec --> aud_render --> ps_mixer --> speaker
 *                                       +--> vid_dec --> vid_render --> ps_render --> LCD
 *
 *         Feed (app `write_frame`; no extractor):
 *
 *             write_frame --+--> aud_dec --> aud_render --> ps_mixer --> speaker
 *                           +--> vid_dec --> vid_render --> ps_render --> LCD
 *
 *         Link (zero-copy from SRC; readers are service-owned):
 *
 *             SRC --+--> ps_reader --> aud_dec --> aud_render --> ps_mixer --> speaker
 *                   +--> ps_vid_rd --> vid_dec --> vid_render --> ps_render --> LCD
 *
 *         Audio-only: omit `vid_dec` / `vid_render` / `ps_vid_rd` / `ps_render`.
 *         Multi-stream: each stream has its own extractor / decoders / readers;
 *         audio always mixes in one `ps_mixer`. One LCD per instance; extra
 *         streams on the same mixer are still equal (session A/V comes from
 *         URL / feed / link).
 *
 *         Name mapping:
 *           ps_defer   - deferred work: playlist advance, mix preempt (one per instance)
 *           ps_mixer   - audio mixer / render process (one per instance)
 *           ps_reader  - linked-source audio reader (link only)
 *           ps_vid_rd  - linked-source video reader (link + video output)
 *           ps_render  - video compose / display (video output attached)
 *           extractor  - esp_player demux (URL only)
 *           aud_dec    - esp_player audio decoder
 *           aud_render - esp_player audio render (into mixer)
 *           vid_dec    - esp_player video decoder
 *           vid_render - esp_player video render (into video_render)
 */

/**
 * Deferred work thread. Runs playlist auto-advance and mix preemption off the
 * player state-machine thread; lives from create to destroy
 */
#define ESP_PLAYER_SERVICE_DEFER_TASK_NAME  "ps_defer"

/** Mixer / render process thread (esp_service_scheduler) */
#define ESP_PLAYER_SERVICE_MIXER_TASK_NAME  "ps_mixer"

/** Linked-source audio reader thread */
#define ESP_PLAYER_SERVICE_READER_TASK_NAME  "ps_reader"

/** Linked-source video reader thread (link + video output) */
#define ESP_PLAYER_SERVICE_VIDEO_READER_TASK_NAME  "ps_vid_rd"

/** Video render compose / display thread (created only when video output is attached) */
#define ESP_PLAYER_SERVICE_VIDEO_RENDER_TASK_NAME  "ps_render"

/** Internal esp_player extractor thread */
#define ESP_PLAYER_SERVICE_PLAYER_EXTRACTOR_TASK_NAME      "extractor"
/** Internal esp_player audio decoder thread */
#define ESP_PLAYER_SERVICE_PLAYER_AUDIO_DECODER_TASK_NAME  "aud_dec"
/** Internal esp_player audio render thread */
#define ESP_PLAYER_SERVICE_PLAYER_AUDIO_RENDER_TASK_NAME   "aud_render"
/** Internal esp_player video decoder thread */
#define ESP_PLAYER_SERVICE_PLAYER_VIDEO_DECODER_TASK_NAME  "vid_dec"
/** Internal esp_player video render thread */
#define ESP_PLAYER_SERVICE_PLAYER_VIDEO_RENDER_TASK_NAME   "vid_render"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
