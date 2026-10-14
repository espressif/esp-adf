/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_media_track_mngr.h"

#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#ifndef CONFIG_ESP_LLM_SERVICE_UPLINK_STOP_TIMEOUT_MS
#define CONFIG_ESP_LLM_SERVICE_UPLINK_STOP_TIMEOUT_MS  (5000)  /*!< Uplink join timeout in milliseconds */
#endif
#ifndef CONFIG_ESP_LLM_SERVICE_UPLINK_FRAME_TIMEOUT_MS
#define CONFIG_ESP_LLM_SERVICE_UPLINK_FRAME_TIMEOUT_MS  (100)   /*!< Provider acquire_frame timeout in milliseconds */
#endif

/* Uplink consume-task defaults; override per task through esp_service_scheduler_set_cb().
   Keep core >= 0: PSRAM-stack tasks from esp_gmf_oal_thread_create() cannot use tskNO_AFFINITY. */
#define ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_STACK  (8192)  /*!< Uplink consume-task stack size in bytes */
#define ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_PRIO   (5)     /*!< Uplink consume-task priority */
#define ESP_LLM_SERVICE_DEFAULT_UPLINK_TASK_CORE   (0)     /*!< Uplink consume-task core affinity */

#define ESP_LLM_AUDIO_TTS_TRACK_ID       (1U)          /*!< Downlink audio TTS track identifier */
#define ESP_LLM_VIDEO_DOWNLINK_TRACK_ID  (0U)          /*!< Downlink video track identifier */
#define ESP_LLM_AUDIO_DN_CACHE_SIZE      (16 * 1024)   /*!< Audio downlink track-manager cache size */
#define ESP_LLM_VIDEO_DN_CACHE_SIZE      (128 * 1024)  /*!< Video downlink track-manager cache size */
#define ESP_LLM_AUDIO_DN_ADDR_ALIGN      (16U)         /*!< Audio frame payload alignment */
#define ESP_LLM_VIDEO_DN_ADDR_ALIGN      (128U)        /*!< Video frame payload alignment */
#define ESP_LLM_MEDIA_WRITE_TIMEOUT_MS   (100U)        /*!< Downlink track-manager write timeout */

_Static_assert(ESP_LLM_MEDIA_STREAM_AUDIO_UP == 0, "AUDIO_UP must be paths[0]");
_Static_assert(ESP_LLM_MEDIA_STREAM_VIDEO_UP == 1, "VIDEO_UP must be paths[1]");
_Static_assert(ESP_LLM_MEDIA_STREAM_AUDIO_DN == 2, "AUDIO_DN must be paths[2]");
_Static_assert(ESP_LLM_MEDIA_STREAM_VIDEO_DN == 3, "VIDEO_DN must be paths[3]");
_Static_assert(ESP_LLM_MEDIA_STREAM_COUNT == 4, "paths[] covers the four LLM streams");

/**
 * @brief  Opaque internal audio front-end helper
 *
 *         Allocated by esp_llm_audio_ai_attach() and released by
 *         esp_llm_audio_ai_detach().
 */
typedef struct esp_llm_audio_ai esp_llm_audio_ai_t;

/**
 * @brief  Uplink consume state for one capture path
 */
typedef struct {
    struct esp_llm_service  *service;   /*!< Owning service; set before consume-task create */
    esp_media_provider_t     provider;  /*!< Copied capture provider after link */
    uint16_t                 track_id;  /*!< Selected provider track */
    bool                     running;   /*!< True after consume-task create until join */
    atomic_bool              stop;      /*!< Consume stop flag */
    SemaphoreHandle_t        done;      /*!< Join semaphore given just before self-delete */
} esp_llm_uplink_slot_t;

/**
 * @brief  Shared downlink track manager for player-route streams
 */
typedef struct {
    esp_media_track_mngr_t  *mngr;               /*!< Owned track manager; one audio and/or video track */
    esp_media_provider_t     provider;           /*!< Exported provider for get_provider */
    uint32_t                 audio_cache_size;   /*!< Audio track cache; 0 uses ESP_LLM_AUDIO_DN_CACHE_SIZE */
    uint32_t                 video_cache_size;   /*!< Video track cache; 0 uses ESP_LLM_VIDEO_DN_CACHE_SIZE */
    bool                     provider_exported;  /*!< True after get_provider copies the shared provider */
} esp_llm_downlink_t;

/**
 * @brief  One of the four LLM media paths
 *
 *         Index in service->paths[] equals esp_llm_media_stream_t. up is used
 *         only for AUDIO_UP and VIDEO_UP.
 */
typedef struct {
    esp_media_track_type_t  type;     /*!< AUDIO or VIDEO */
    bool                    enabled;  /*!< True when the adapter enables this stream */
    esp_media_track_info_t  info;     /*!< Adapter-resolved stream format */
    esp_llm_uplink_slot_t   up;       /*!< Uplink consume slot */
} esp_llm_media_path_t;

/**
 * @brief  Internal LLM service object
 *
 *         This header is not part of the public API. First member is
 *         esp_media_service_t so ESP_SERVICE_BASE() works. Lifecycle is
 *         esp_service.state; derived fields describe resource ownership only.
 *         Control-plane helpers follow the same single-thread contract as
 *         esp_llm_service.h and are not ISR-safe. service is non-NULL when a
 *         helper is called from the LLM service lifecycle.
 */
struct esp_llm_service {
    esp_media_service_t            media;                   /*!< Media service base, must stay first */
    esp_llm_service_cfg_t          cfg;                     /*!< Copied service configuration */
    const esp_llm_adapter_desc_t  *adapter_desc;            /*!< Borrowed adapter descriptor */
    void                          *adapter_ctx;             /*!< Adapter context */
    atomic_bool                    audio_uplink_enabled;    /*!< Cross-task audio uplink gate */
    atomic_bool                    video_uplink_enabled;    /*!< Cross-task video uplink gate */
    atomic_bool                    uplink_send_allowed;     /*!< Closed on stop; opened after adapter start */
    atomic_bool                    wakeup_cancel_response;  /*!< Cancel current response on wake-up start */
    esp_llm_capability_t           effective_capabilities;  /*!< Static caps intersected with current wiring */
    esp_llm_media_path_t           paths[ESP_LLM_MEDIA_STREAM_COUNT];  /*!< Indexed by esp_llm_media_stream_t */
    esp_llm_downlink_t             downlink;                /*!< Shared player-route track manager */
    esp_llm_audio_ai_t            *audio_ai;                /*!< Internal AFE helper; NULL if unused */
};

/**
 * @brief  Get the effective capability bitmask
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - Capability bitmask stored on the service
 */
static inline esp_llm_capability_t esp_llm_service_caps(const esp_llm_service_t *service)
{
    return service->effective_capabilities;
}

/**
 * @brief  Get the adapter name used in logs and events
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - Adapter name string
 *       - "unknown" if service or adapter name is unavailable
 */
static inline const char *esp_llm_adapter_name(const esp_llm_service_t *service)
{
    if (service == NULL || service->adapter_desc == NULL || service->adapter_desc->name == NULL) {
        return "unknown";
    }
    return service->adapter_desc->name;
}

/**
 * @brief  Get the adapter operation table
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - Adapter ops pointer
 *       - NULL if service or adapter descriptor is unavailable
 */
static inline const esp_llm_adapter_ops_t *esp_llm_adapter_ops(const esp_llm_service_t *service)
{
    if (service == NULL || service->adapter_desc == NULL) {
        return NULL;
    }
    return service->adapter_desc->ops;
}

/**
 * @brief  Whether consume / send_frame may forward this track type
 *
 * @param[in]  service  LLM service handle
 * @param[in]  type     Audio or video track type
 *
 * @return
 *       - true   Frames of this type are sent
 *       - false  Frames of this type are discarded
 */
static inline bool esp_llm_uplink_type_enabled(const esp_llm_service_t *service,
                                               esp_media_track_type_t type)
{
    if (service == NULL) {
        return false;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return atomic_load(&service->audio_uplink_enabled);
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return atomic_load(&service->video_uplink_enabled);
    }
    return true;
}

/**
 * @brief  Keep the first non-ESP_OK status while continuing cleanup
 *
 * @param[in,out]  result     Accumulated status; updated only when currently ESP_OK
 * @param[in]      candidate  Latest status to record
 */
static inline void record_first_error(esp_err_t *result, esp_err_t candidate)
{
    if (*result == ESP_OK && candidate != ESP_OK) {
        *result = candidate;
    }
}

/**
 * @brief  Check whether any uplink consume task is still running
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - true   Audio or video uplink consume task is running
 *       - false  No uplink consume task is running
 */
static inline bool esp_llm_uplink_running(const esp_llm_service_t *service)
{
    return service->paths[ESP_LLM_MEDIA_STREAM_AUDIO_UP].up.running ||
           service->paths[ESP_LLM_MEDIA_STREAM_VIDEO_UP].up.running;
}

/**
 * @brief  Get a string name for an esp_service state
 *
 * @param[in]  state  Service state value
 *
 * @return
 *       - Static state name
 *       - "UNKNOWN" if state is not recognized
 */
static inline const char *esp_llm_service_state_name(esp_service_state_t state)
{
    switch (state) {
        case ESP_SERVICE_STATE_UNINITIALIZED:
            return "UNINITIALIZED";
        case ESP_SERVICE_STATE_INITIALIZED:
            return "INITIALIZED";
        case ESP_SERVICE_STATE_RUNNING:
            return "RUNNING";
        case ESP_SERVICE_STATE_PAUSED:
            return "PAUSED";
        case ESP_SERVICE_STATE_STOPPING:
            return "STOPPING";
        case ESP_SERVICE_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief  Dispatch one event to the application through the service event hub
 *
 *         The envelope is copied so payload and string fields only need to
 *         remain valid during this call.
 *
 * @param[in]  service  LLM service handle
 * @param[in]  event    Event envelope to copy and deliver
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   If service or event is invalid
 *       - ESP_ERR_INVALID_SIZE  If the copied envelope would overflow
 *       - ESP_ERR_NO_MEM        If event copy allocation failed
 *       - Others                Error returned by esp_service_publish_event()
 */
esp_err_t esp_llm_event_dispatch(esp_llm_service_t *service, const esp_llm_event_t *event);

/**
 * @brief  Get a string name for a published event-hub ID
 *
 * @param[in]  event_id  Common event ID or ESP_LLM_EVENT_BACKEND_RAW
 *
 * @return
 *       - Static event name
 *       - NULL if event_id is unknown
 */
const char *esp_llm_event_to_name(uint16_t event_id);

/**
 * @brief  Get the LLM media-service virtual operations
 *
 * @return
 *       - Pointer to the static media ops table
 */
const esp_media_service_ops_t *esp_llm_media_ops(void);

/**
 * @brief  Resolve the four media paths from service stream configuration
 *
 *         Disabled streams, or streams the adapter does not advertise, stay
 *         off. Enabling a stream the adapter cannot advertise fails create.
 *
 * @param[in]  service  LLM service handle with cfg.streams[] already copied
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or a configured track type is invalid
 *       - ESP_ERR_NOT_SUPPORTED  If an enabled stream is outside adapter capabilities
 */
esp_err_t esp_llm_media_resolve_paths(esp_llm_service_t *service);

/**
 * @brief  Create the shared downlink track manager for enabled DN streams
 *
 *         `downlink_cb` is a tap and does not skip track-manager creation.
 *         On failure, created downlink resources are destroyed.
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK    On success
 *       - Others    Error returned while creating a downlink path
 */
esp_err_t esp_llm_media_create_downlink(esp_llm_service_t *service);

/**
 * @brief  Destroy the shared downlink track manager
 *
 * @param[in]  service  LLM service handle
 */
void esp_llm_media_destroy_downlink(esp_llm_service_t *service);

/**
 * @brief  Clear abort before adapter start
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK    On success
 *       - Others    Error returned by esp_media_track_clear_abort()
 */
esp_err_t esp_llm_media_prepare(esp_llm_service_t *service);

/**
 * @brief  Start uplink consume tasks for linked capture providers
 *
 *         On failure, already started uplink tasks are stopped.
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK    On success
 *       - Others    Error returned while creating an uplink consume task
 */
esp_err_t esp_llm_media_start_uplink(esp_llm_service_t *service);

/**
 * @brief  Stop uplink consume tasks and wait for them to exit
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK           On success
 *       - ESP_ERR_TIMEOUT  If a consume task did not exit in time
 */
esp_err_t esp_llm_media_stop_uplink(esp_llm_service_t *service);

/**
 * @brief  Abort in-flight downlink writes
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK    On success
 *       - Others    Error returned by esp_media_track_write_abort()
 */
esp_err_t esp_llm_media_stop_downlink(esp_llm_service_t *service);

/**
 * @brief  Write one downlink frame to a callback or PLAYER-route track manager
 *
 *         Adapters must fill frame->pts. If DTS is not independent, set dts to
 *         PTS or ESP_LLM_PTS_ABSENT so this function copies PTS. A zero dts is
 *         a real timestamp. EOS frames may omit payload data.
 *
 * @param[in]  service  LLM service handle
 * @param[in]  frame    Adapter downlink frame
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If a non-EOS frame has no payload, or PTS is absent
 *       - ESP_ERR_NOT_SUPPORTED  If the matching downlink path is disabled
 *       - ESP_ERR_NOT_FOUND      If no callback or track manager is available
 *       - Others                 Error returned by the downlink sink
 */
esp_err_t esp_llm_media_write(esp_llm_service_t *service, const esp_media_frame_t *frame);

/**
 * @brief  Check whether audio_ai configuration requests AFE features
 *
 * @param[in]  cfg  Audio AI configuration, or NULL
 *
 * @return
 *       - true   Feature mask is enabled
 *       - false  cfg is NULL or AI audio is unused
 */
bool esp_llm_audio_ai_requested(const esp_llm_audio_ai_cfg_t *cfg);

/**
 * @brief  Attach AFE features and callbacks to the borrowed capture
 *
 *         Capture must not be running. No-op when audio_ai is unused or already
 *         attached. On failure, callbacks and features are rolled back.
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK                 On success, including when AI audio is unused
 *       - ESP_ERR_INVALID_ARG    If service is NULL or capture is missing
 *       - ESP_ERR_INVALID_STATE  If capture is already running
 *       - ESP_ERR_NO_MEM         If helper allocation failed
 *       - Others                 Error returned while attaching AFE features
 */
esp_err_t esp_llm_audio_ai_attach(esp_llm_service_t *service);

/**
 * @brief  Detach AFE features and callbacks from the borrowed capture
 *
 *         No-op when no helper is attached. Capture should already be stopped.
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK                 On success, including when nothing is attached
 *       - ESP_ERR_INVALID_STATE  If capture AI state could not be cleared
 *       - Others                 First error returned while detaching AFE
 */
esp_err_t esp_llm_audio_ai_detach(esp_llm_service_t *service);

/**
 * @brief  Manually enter the AFE wakeup state on the borrowed capture
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If audio AI is not attached
 *       - Others                 Error returned by capture trigger_wakeup
 */
esp_err_t esp_llm_audio_ai_trigger_wakeup(esp_llm_service_t *service);

/**
 * @brief  Manually leave the AFE wakeup state on the borrowed capture
 *
 * @param[in]  service  LLM service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If audio AI is not attached
 *       - Others                 Error returned by capture trigger_sleep
 */
esp_err_t esp_llm_audio_ai_trigger_sleep(esp_llm_service_t *service);

/**
 * @brief  Keep the AFE in wakeup state without automatic timeout
 *
 * @param[in]  service  LLM service handle
 * @param[in]  enable   True to stay awake, false to allow wakeup timeout
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If audio AI is not attached
 *       - Others                 Error returned by capture keep_awake
 */
esp_err_t esp_llm_audio_ai_keep_awake(esp_llm_service_t *service, bool enable);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
