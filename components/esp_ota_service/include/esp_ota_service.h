/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_service.h"
#include "esp_ota_service_source.h"
#include "esp_ota_service_checker.h"
#include "esp_ota_service_target.h"
#include "esp_ota_service_verifier.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** @brief Event Hub domain name for OTA service */
#define ESP_OTA_SERVICE_DOMAIN  "esp_ota_service"

/** @brief Default OTA worker stack when esp_ota_service_task_cfg_t.stack_size is 0 */
#define ESP_OTA_SERVICE_DEFAULT_WORKER_STACK_SIZE  6144U

/** @brief Default OTA worker priority when esp_ota_service_task_cfg_t.priority is 0 */
#define ESP_OTA_SERVICE_DEFAULT_WORKER_PRIORITY  5

/** @brief Default read/write chunk size when esp_ota_service_cfg_t.write_chunk_size is 0 */
#define ESP_OTA_SERVICE_DEFAULT_WRITE_CHUNK_SIZE  4096U

/** @brief Default esp_service_stop() wait when esp_ota_service_cfg_t.stop_timeout_ms is 0 */
#define ESP_OTA_SERVICE_DEFAULT_STOP_TIMEOUT_MS  10000U

/**
 * @brief  Opaque OTA service handle (definition in esp_ota_service.c)
 *
 *         The concrete object embeds @c esp_service_t as its first member: the handle pointer
 *         has the same address as that base.  ADF entry points take @c esp_service_t *, so use
 *         @c (esp_service_t *)svc only to satisfy the C type system (e.g. esp_service_event_subscribe()).
 */
typedef struct esp_ota_service esp_ota_service_t;

/**
 * @brief  OTA service event identifiers
 *
 *         Enumerators are listed in the typical emission order within one session.
 *         IDs start at 2 (0 is invalid for @c esp_service_publish_event(), 1 is
 *         reserved for @c ESP_SERVICE_EVENT_STATE_CHANGED); use the symbols — numeric
 *         values are not stable across releases.  @c ESP_OTA_SERVICE_EVT_MAX is a non-event
 *         upper-bound sentinel and is never published.
 *
 *         @c ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK fires only when @c checker is non-NULL; read
 *         @c error first, then @c ver_check.upgrade_available:
 *           - @c error != ESP_OK            — check failed; @c ITEM_END follows (no @c ITEM_BEGIN).
 *           - @c error == ESP_OK, not newer — skipped; neither @c ITEM_BEGIN nor @c ITEM_END.
 *           - @c error == ESP_OK, newer     — download proceeds with @c ITEM_BEGIN.
 */
typedef enum {
    ESP_OTA_SERVICE_EVT_SESSION_BEGIN = 2,  /*!< OTA session started (before first item) */
    ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,     /*!< Pre-download checker finished; see @c ver_check and @c error. */
    ESP_OTA_SERVICE_EVT_ITEM_BEGIN,         /*!< Read/write loop about to start (after source/target @c open(),
                                                 optional verifier @c verify_begin(), and internal resume setup).
                                                 No union payload; only @c item_index / @c item_label. */
    ESP_OTA_SERVICE_EVT_ITEM_PROGRESS,      /*!< Progress update for the current item (throttled, ~1s) */
    ESP_OTA_SERVICE_EVT_ITEM_END,           /*!< Single partition upgrade finished. Payload carries @c item_end.status
                                                 (OK / SKIPPED / FAILED), @c item_end.reason (classification when status
                                                 is not OK), and @c error (@c ESP_OK for OK and SKIPPED; pipeline /
                                                 checker / abort codes when @c FAILED). */
    ESP_OTA_SERVICE_EVT_SESSION_END,        /*!< OTA session finished. Payload carries success/failed/skipped counts and
                                                 aborted flag; consumers should handle this single event instead of three
                                                 disjoint outcomes. */

    ESP_OTA_SERVICE_EVT_MAX,  /*!< Sentinel; not a real event. */
} esp_ota_service_event_id_t;

/**
 * @brief  Per-item outcome reported in ESP_OTA_SERVICE_EVT_ITEM_END.
 */
typedef enum {
    ESP_OTA_SERVICE_ITEM_STATUS_OK      = 0,  /*!< Item written and committed successfully. */
    ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED = 1,  /*!< Item was deliberately not applied; see @c esp_ota_service_item_end_reason_t. */
    ESP_OTA_SERVICE_ITEM_STATUS_FAILED  = 2,  /*!< Item failed; @c esp_ota_service_event_t::error carries the reason. */
} esp_ota_service_item_status_t;

/**
 * @brief  Coarse outcome detail for @c ESP_OTA_SERVICE_EVT_ITEM_END.
 *
 *         When @c item_end.status is @c ESP_OTA_SERVICE_ITEM_STATUS_OK, the value is @c ESP_OTA_SERVICE_ITEM_END_REASON_NONE.
 *         For @c ESP_OTA_SERVICE_ITEM_STATUS_FAILED and @c ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED, use @c reason together with
 *         @c esp_ota_service_event_t::error (especially for failures — @c error holds the precise @c esp_err_t).
 */
typedef enum {
    ESP_OTA_SERVICE_ITEM_END_REASON_NONE              = 0,  /*!< No extra classification (success path). */
    ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED = 1,  /*!< SKIPPED: streaming verifier rejected the image at
                                                                 verify_begin() or verify_finish(). */
    ESP_OTA_SERVICE_ITEM_END_REASON_CHECK_FAILED      = 2,  /*!< FAILED: pre-download checker returned an error. */
    ESP_OTA_SERVICE_ITEM_END_REASON_ABORTED           = 3,  /*!< FAILED: session stop / user abort before item finished. */
    ESP_OTA_SERVICE_ITEM_END_REASON_PIPELINE          = 4,  /*!< FAILED: download / write / commit path (@c error). */
} esp_ota_service_item_end_reason_t;

/**
 * @brief  OTA service event payload
 *
 *         Published on the service-bound Event Hub; subscribe with
 *         esp_service_event_subscribe((esp_service_t *)svc, &info).  The handler's
 *         @c adf_event_t::payload is a heap @c esp_ota_service_event_t owned by the hub (including
 *         a deep copy of @c item_label); both stay valid until the hub releases it.
 *
 *         @c item_index == -1 marks a session-scoped event (@c SESSION_BEGIN, and
 *         @c SESSION_END on normal completion); when @c session_end.aborted is true,
 *         @c item_index carries the in-progress list index.
 *
 *         Inspect @c id first, then read the matching union branch (reading the wrong
 *         branch is undefined):
 *
 *          - SESSION_BEGIN  : no union field
 *          - ITEM_VER_CHECK : @c ver_check, @c error (checker outcome)
 *          - ITEM_BEGIN     : no union field
 *          - ITEM_PROGRESS  : @c progress
 *          - ITEM_END       : @c item_end, @c error
 *          - SESSION_END    : @c session_end, @c error
 */
typedef struct {
    esp_ota_service_event_id_t  id;          /*!< Event identifier */
    esp_err_t                   error;       /*!< Semantics depend on @c id (e.g. @c ITEM_VER_CHECK checker result;
                                                 @c ITEM_PROGRESS is @c ESP_OK; @c ITEM_END carries skip / fail / OK). */
    int                         item_index;  /*!< Upgrade-list index; @c -1 when the event is session-scoped
                                                 (@c SESSION_BEGIN, and @c SESSION_END on normal completion). */
    const char                 *item_label;  /*!< Partition label for that item; deep copy in payload. May be @c NULL. */

    /* Event-specific payload. Access only the branch that matches @c id. */
    union {
        /** ESP_OTA_SERVICE_EVT_ITEM_PROGRESS */
        struct {
            uint32_t  bytes_written;  /*!< Bytes written so far for the current item. */
            uint32_t  total_bytes;    /*!< Total bytes for the current item (UINT32_MAX if unknown). */
        } progress;

        /** ESP_OTA_SERVICE_EVT_ITEM_END */
        struct {
            esp_ota_service_item_status_t      status;  /*!< OK / SKIPPED / FAILED */
            esp_ota_service_item_end_reason_t  reason;  /*!< @c ESP_OTA_SERVICE_ITEM_END_REASON_NONE when @c status is OK; otherwise coarse class */
        } item_end;

        /** ESP_OTA_SERVICE_EVT_SESSION_END */
        struct {
            uint16_t  success_count;  /*!< Items with ESP_OTA_SERVICE_ITEM_STATUS_OK */
            uint16_t  failed_count;   /*!< Items with ESP_OTA_SERVICE_ITEM_STATUS_FAILED */
            uint16_t  skipped_count;  /*!< Items with ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED */
            bool      aborted;        /*!< True when stopped by esp_service_stop() */
        } session_end;

        /** @c ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK */
        struct {
            uint32_t  image_size;         /*!< Incoming image size, or UINT32_MAX if unknown. */
            bool      upgrade_available;  /*!< If @c error != ESP_OK: always false (ignore for outcome). If
                                           @c error == ESP_OK: true = will upgrade, false = not newer. */
        } ver_check;
    };
} esp_ota_service_event_t;

/**
 * @brief  Descriptor for a single partition upgrade
 *
 *         @c uri and @c partition_label are deep-copied, so stack-allocated items
 *         are safe.  Ownership of @c source / @c target / @c verifier / @c checker
 *         transfers to the service when @c esp_ota_service_set_upgrade_list() is entered:
 *         their @c destroy() runs when the list is replaced, the service is destroyed,
 *         or validation fails (set @c destroy = NULL to opt out).
 *
 *         When @c checker is non-NULL, the worker runs a header-only version check
 *         before downloading and emits @c ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK.  If the check
 *         reports @c !upgrade_available with @c error == ESP_OK, the item skips
 *         @c ITEM_BEGIN / @c ITEM_END.  The worker never loops on a single item.
 */
typedef struct esp_ota_upgrade_item {
    const char                 *uri;              /*!< Passed to source->open(). Deep-copied. Required. */
    const char                 *partition_label;  /*!< Passed to target->open(). Deep-copied.
                                                   *   NULL selects the next OTA app slot. For @c esp_ota_service_target_data_create(),
                                                   *   a typical image prefixes the payload with a 4-byte little-endian
                                                   *   header: @c esp_ota_service_version_pack_semver() of the same semver
                                                   *   string as @c esp_app_desc_t::version (see @c examples/ota_fs). */
    esp_ota_service_source_t   *source;           /*!< Data stream source for download. Required. */
    esp_ota_service_target_t   *target;           /*!< Write target. Required. */
    esp_ota_service_verifier_t *verifier;         /*!< Optional streaming integrity verifier (SHA256, signature, etc.).
                                                   *   NULL = no integrity verification during download. */
    esp_ota_service_checker_t  *checker;          /*!< Optional pre-download update checker. NULL = skip version check.
                                                   *   Use @c esp_ota_service_checker_app_create() for app images,
                                                   *   @c esp_ota_service_checker_data_version_create() for data partitions,
                                                   *   @c esp_ota_service_checker_manifest_create() for JSON manifests,
                                                   *   or implement @c esp_ota_service_checker_t for custom formats. */
    bool                        skip_on_fail;     /*!< If true, continue to the next item on failure.
                                                   *   If false (default), abort the entire session. */
    bool                        resumable;        /*!< Enable NVS-based resume on failure/reboot.
                                                   *   Requires @c source->seek and @c target->set_write_offset
                                                   *   to be implemented; otherwise @c esp_ota_service_set_upgrade_list()
                                                   *   returns ESP_ERR_NOT_SUPPORTED. Data-partition and
                                                   *   bootloader targets erase the whole partition on open()
                                                   *   and therefore cannot be resumed: only the app target
                                                   *   and sources with random-access seek currently qualify. */
} esp_ota_upgrade_item_t;

/**
 * @brief  Worker task configuration for the OTA download task
 *
 *         This task is distinct from the ADF service command-processing task.
 *         It performs the actual source read, verifier update, and target write loop.
 */
typedef struct {
    uint32_t  stack_size;  /*!< Worker task stack size in bytes.
                            *   0 = use ESP_OTA_SERVICE_DEFAULT_WORKER_STACK_SIZE. */
    uint8_t   priority;    /*!< Worker task FreeRTOS priority.
                            *   0 = use ESP_OTA_SERVICE_DEFAULT_WORKER_PRIORITY. */
    int8_t    core_id;     /*!< Core affinity. -1 = any core. */
} esp_ota_service_task_cfg_t;

/**
 * @brief  OTA service configuration
 */
typedef struct {
    const char                 *name;              /*!< ADF service / Event Hub domain name; NULL = ESP_OTA_SERVICE_DOMAIN */
    esp_ota_service_task_cfg_t  worker_task;       /*!< OTA download worker task config. */
    uint32_t                    write_chunk_size;  /*!< Per-chunk read/write buffer size in bytes.
                                                        0 = ESP_OTA_SERVICE_DEFAULT_WRITE_CHUNK_SIZE.
                                                        Valid range when non-zero: 512-65536. */
    uint32_t                    stop_timeout_ms;   /*!< Max wait in ms for worker to exit in esp_service_stop().
                                                        0 = ESP_OTA_SERVICE_DEFAULT_STOP_TIMEOUT_MS.
                                                        Minimum when non-zero: 1000. */
} esp_ota_service_cfg_t;

/** @brief Convenience initialiser for esp_ota_service_cfg_t */
/* clang-format off */
#define ESP_OTA_SERVICE_CFG_DEFAULT() \
    { \
        .name = NULL, \
        .worker_task = { \
            .stack_size = 0, \
            .priority = 0, \
            .core_id = -1, \
        }, \
        .write_chunk_size = 0, \
        .stop_timeout_ms = 0, \
    }
/* clang-format on */

/**
 * @brief  Information about available firmware update
 *
 *         Same layout as @c esp_ota_service_check_result_t.
 */
typedef esp_ota_service_check_result_t esp_ota_service_update_info_t;

/**
 * @brief  Create and initialise an OTA service instance
 *
 *         Allocates the service object, initialises the ADF service base, and
 *         creates the internal stop semaphore.  No network or flash operations occur.
 *
 * @param[in]   cfg      Service configuration
 * @param[out]  out_svc  Receives the created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       On allocation failure (service object or stop semaphore)
 *       - ESP_ERR_INVALID_ARG  If cfg or out_svc is NULL, or write_chunk_size / stop_timeout_ms in cfg is invalid
 *       - Other                Error code returned by esp_service_init() on ADF base initialisation failure
 */
esp_err_t esp_ota_service_create(const esp_ota_service_cfg_t *cfg, esp_ota_service_t **out_svc);

/**
 * @brief  Destroy an OTA service instance and free all resources
 *
 *         Calls esp_service_stop() to abort any in-progress upgrade, destroys the
 *         source/target/verifier/checker instances in the current upgrade list, deinits the
 *         ADF service base, and frees the service object.
 *
 * @param[in]  svc  Service handle obtained from esp_ota_service_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If svc is NULL
 */
esp_err_t esp_ota_service_destroy(esp_ota_service_t *svc);

/**
 * @brief  Set (or replace) the upgrade item list
 *
 *         The service makes a shallow copy of the list array and deep-copies the
 *         @c uri and @c partition_label strings. The @c source / @c target /
 *         @c verifier / @c checker pointers inside each item are owned by the
 *         service from the moment this function is ENTERED — the caller must not
 *         destroy them afterwards, even if the call fails. If validation or
 *         allocation fails, the service destroys every item's source/target/
 *         verifier/checker before returning.
 *
 *         If a previous list exists, its source/target/verifier/checker destroy()
 *         functions are called before the list is replaced. Any saved NVS resume
 *         records are cleared at this point, because the new list may not match
 *         the previous item layout.
 *
 *         Allowed only while the service is in INITIALIZED or PAUSED. Not allowed
 *         in RUNNING while the worker is downloading.
 *
 * @param[in]  svc    Service instance
 * @param[in]  list   Array of upgrade item descriptors
 * @param[in]  count  Number of items in the array (must be > 0)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If svc/list is NULL, count <= 0, item uri/source/target is NULL, or a uri
 *                                exceeds the resume store's capacity (255 chars)
 *       - ESP_ERR_INVALID_STATE  If called while the service is neither INITIALIZED nor PAUSED
 *       - ESP_ERR_NOT_SUPPORTED  If item.resumable=true but the source has no seek() or the target has
 *                                no set_write_offset() (data / bootloader targets cannot resume)
 *       - ESP_ERR_NO_MEM         If the list or deep-copied strings cannot be allocated
 */
esp_err_t esp_ota_service_set_upgrade_list(esp_ota_service_t *svc, const esp_ota_upgrade_item_t *list, int count);

/**
 * @brief  Check if a newer firmware is available without performing OTA
 *
 *         Calls @c checker->check() on the item at @a item_index. No flash write.
 *         Requires @c esp_ota_service_set_upgrade_list() first, and the item must have a
 *         non-NULL @c checker.
 *
 * @param[in]   svc         Service instance
 * @param[in]   item_index  Index into the upgrade list (0-based)
 * @param[out]  out_info    Receives update information
 *
 * @return
 *       - ESP_OK               On success (inspect @c out_info->upgrade_available)
 *       - ESP_ERR_INVALID_ARG  If svc or out_info is NULL, item_index is invalid,
 *                              upgrade list is unset, or item's @c checker is NULL
 *       - Other                Error from @c checker->check()
 */
esp_err_t esp_ota_service_check_update(esp_ota_service_t *svc, int item_index, esp_ota_service_update_info_t *out_info);

/**
 * @brief  Query current download progress
 *
 *         Values reflect the item currently being processed.
 *
 * @param[in]   svc          Service instance
 * @param[out]  out_written  Bytes written so far for the current item; NULL to omit
 * @param[out]  out_total    Total bytes for the current item (UINT32_MAX if unknown); NULL to omit
 * @param[out]  out_percent  Approximate progress 0–100 for the current item from
 *                           @a out_written / @a out_total; -1 if total size is unknown;
 *                           NULL to omit
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If svc is NULL
 */
esp_err_t esp_ota_service_get_progress(const esp_ota_service_t *svc, uint32_t *out_written, uint32_t *out_total,
                                       int32_t *out_percent);

/**
 * @brief  Confirm that the currently running OTA image is valid
 *
 *         Call this after verifying that the new firmware works correctly.
 *         Cancels the automatic rollback that would occur on next reboot.
 *         Operates only on the currently running application partition (no slot
 *         or item index is passed); IDF must report ESP_OTA_IMG_PENDING_VERIFY there.
 *         Only effective when CONFIG_OTA_ENABLE_ROLLBACK is enabled.
 *
 * @return
 *       - ESP_OK                 On success (rollback cancelled)
 *       - ESP_ERR_INVALID_STATE  If the running image is not pending verification
 *       - ESP_ERR_NOT_SUPPORTED  If CONFIG_OTA_ENABLE_ROLLBACK is disabled
 *       - Other                  Error code returned by esp_ota_mark_app_valid_cancel_rollback() on failure
 */
esp_err_t esp_ota_service_confirm_update(void);

/**
 * @brief  Mark current image as invalid and reboot to the previous firmware
 *
 * @note  Does not return on success — the device reboots.
 *
 * @return
 *       - ESP_ERR_NOT_SUPPORTED  If CONFIG_OTA_ENABLE_ROLLBACK is disabled
 *       - Other                  Error code returned by esp_ota_mark_app_invalid_rollback_and_reboot() on failure
 */
esp_err_t esp_ota_service_rollback(void);

/**
 * @brief  Check whether the currently running image is in ESP_OTA_IMG_PENDING_VERIFY
 *
 *         Safe to call at any time from any task.  Does not depend on an OTA service handle
 *         and does not allocate.  Intended to be called early in @c app_main() so the
 *         application can run its self-test and resolve the state with
 *         @c esp_ota_service_confirm_update() or @c esp_ota_service_rollback() before any
 *         subsequent reset would otherwise trigger bootloader auto-rollback.
 *
 * @param[out]  out_pending  Receives @c true when the running image needs confirmation.
 *
 * @return
 *       - ESP_OK                 On success (inspect @c *out_pending)
 *       - ESP_ERR_INVALID_ARG    If @a out_pending is NULL
 *       - ESP_ERR_INVALID_STATE  If esp_ota_get_running_partition() returned NULL
 *       - ESP_ERR_NOT_SUPPORTED  If CONFIG_OTA_ENABLE_ROLLBACK is disabled; @c *out_pending is
 *                                set to @c false for caller convenience
 *       - Other                  Error returned by esp_ota_get_state_partition() on failure
 */
esp_err_t esp_ota_service_is_pending_verify(bool *out_pending);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
