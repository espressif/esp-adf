/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_service.h"
#include "esp_media_service_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Default media service configuration */
#define ESP_MEDIA_SERVICE_CONFIG_DEFAULT() {  \
    .name        = NULL,                      \
    .user_data   = NULL,                      \
    .service_ops = NULL,                      \
    .media_ops   = NULL,                      \
}

/**
 * @brief  Media service data direction
 */
typedef enum {
    ESP_MEDIA_ROLE_NONE     = 0,
    ESP_MEDIA_ROLE_SRC      = (1 << 0),                                    /*!< Service produces media frames */
    ESP_MEDIA_ROLE_SINK     = (1 << 1),                                    /*!< Service consumes media frames */
    ESP_MEDIA_ROLE_SRC_SINK = (ESP_MEDIA_ROLE_SRC | ESP_MEDIA_ROLE_SINK),  /*!< Service produces and consumes frames */
} esp_media_role_t;

/**
 * @brief  Instance-aware media stream address
 */
typedef uint16_t esp_media_stream_id_t;

/** Default media stream ID */
#define ESP_MEDIA_DEFAULT_STREAM  ((esp_media_stream_id_t)0)

/**
 * @brief  Media-service link request/capability hints
 *
 *         The structure is intentionally extensible so future services can add
 *         link-time requests without changing the media op shape
 */
typedef struct {
    bool  need_global_cache;  /*!< Sink requests source frames in one arrival-order cache */
} esp_media_service_request_t;

/**
 * @brief  Media-service virtual operations
 */
typedef struct {
    esp_err_t (*get_role)(esp_service_t *service, esp_media_role_t *out_role);                                                   /*!< Query source/sink role */
    esp_err_t (*get_provider)(esp_service_t *service, esp_media_stream_id_t stream, esp_media_provider_t *out_provider);         /*!< Get provider exported by a source stream */
    esp_err_t (*set_provider)(esp_service_t *service, esp_media_stream_id_t stream, const esp_media_provider_t *provider);       /*!< Set provider consumed by a sink stream */
    esp_err_t (*get_request)(esp_service_t *service, esp_media_stream_id_t stream, esp_media_service_request_t *request);        /*!< Get sink link requests */
    esp_err_t (*set_request)(esp_service_t *service, esp_media_stream_id_t stream, const esp_media_service_request_t *request);  /*!< Apply sink requests to a source */
} esp_media_service_ops_t;

/**
 * @brief  Media service configuration
 */
typedef struct {
    const char                    *name;         /*!< Service instance name */
    void                          *user_data;    /*!< User data passed to esp_service */
    const esp_service_ops_t       *service_ops;  /*!< Optional esp_service lifecycle ops */
    const esp_media_service_ops_t *media_ops;    /*!< Optional media ops */
} esp_media_service_config_t;

/**
 * @brief  Base media service structure
 *         Derived services embed this as the first member
 */
typedef struct esp_media_service {
    esp_service_t                  base;       /*!< Base service, must stay first */
    const esp_media_service_ops_t *media_ops;  /*!< Media virtual operations */
} esp_media_service_t;

/**
 * @brief  Initialize a media service base object
 *
 *         Derived services call this after allocating or embedding
 *         esp_media_service_t, before exposing the base esp_service_t
 *
 * @param[in,out]  service  Media service object to initialize
 * @param[in]      config   Media service configuration
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service or config is NULL
 *       - Others               Error returned by esp_service_init()
 */
esp_err_t esp_media_service_init(esp_media_service_t *service, const esp_media_service_config_t *config);

/**
 * @brief  Deinitialize a media service base object
 *
 * @param[in]  service  Base service pointer returned by ESP_SERVICE_BASE()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service is NULL
 *       - Others               Error returned by esp_service_deinit()
 */
esp_err_t esp_media_service_deinit(esp_service_t *service);

/**
 * @brief  Query the media role of a service
 *
 * @param[in]   service   Base media service handle
 * @param[out]  out_role  Output media role
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or out_role is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the service
 *       - Others                 Error returned by the service implementation
 */
esp_err_t esp_media_service_get_role(esp_service_t *service, esp_media_role_t *out_role);

/**
 * @brief  Get a provider from a source stream
 *
 * @param[in]   service       Base media service handle
 * @param[in]   stream        Source stream ID
 * @param[out]  out_provider  Output provider
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Service or provider is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the service
 *       - Others                 Error returned by the service implementation
 */
esp_err_t esp_media_service_get_provider(esp_service_t *service, esp_media_stream_id_t stream,
                                         esp_media_provider_t *out_provider);

/**
 * @brief  Set a provider on a sink stream
 *
 *         Pass NULL provider to disconnect the sink stream
 *
 * @param[in]  service   Base media service handle
 * @param[in]  stream    Sink stream ID
 * @param[in]  provider  Provider handle to consume, or NULL to clear
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL, or provider has no ops
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the service
 *       - Others                 Error returned by the service implementation
 */
esp_err_t esp_media_service_set_provider(esp_service_t *service, esp_media_stream_id_t stream,
                                         const esp_media_provider_t *provider);

/**
 * @brief  Get link request hints for a media stream
 *
 * @param[in]   service      Base media service handle
 * @param[in]   stream       Stream ID
 * @param[out]  out_request  Output request hints
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or out_request is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the service
 *       - Others                 Error returned by the service implementation
 */
esp_err_t esp_media_service_get_request(esp_service_t *service, esp_media_stream_id_t stream,
                                        esp_media_service_request_t *out_request);

/**
 * @brief  Set link request hints for a media stream
 *
 * @param[in]  service  Base media service handle
 * @param[in]  stream   Stream ID
 * @param[in]  request  Request hints to apply
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or request is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the service
 *       - Others                 Error returned by the service implementation
 */
esp_err_t esp_media_service_set_request(esp_service_t *service, esp_media_stream_id_t stream,
                                        const esp_media_service_request_t *request);

/**
 * @brief  Link a source stream to a sink stream
 *
 *         The sink request hints are applied to the source when supported, then
 *         the source provider is passed to the sink
 *
 * @param[in]  src_service   Source media service handle
 * @param[in]  src_stream    Source stream ID
 * @param[in]  sink_service  Sink media service handle
 * @param[in]  sink_stream   Sink stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    src_service or sink_service is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Role check failed or required operation is missing
 *       - Others                 Error returned by source or sink implementation
 */
esp_err_t esp_media_service_link(esp_service_t *src_service, esp_media_stream_id_t src_stream,
                                 esp_service_t *sink_service, esp_media_stream_id_t sink_stream);

/**
 * @brief  Unlink a source stream from a sink stream
 *
 *         Clears the provider currently set on the sink stream
 *
 * @param[in]  src_service   Source media service handle
 * @param[in]  src_stream    Source stream ID
 * @param[in]  sink_service  Sink media service handle
 * @param[in]  sink_stream   Sink stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    src_service or sink_service is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Role check failed or sink operation is missing
 *       - Others                 Error returned by source or sink implementation
 */
esp_err_t esp_media_service_unlink(esp_service_t *src_service, esp_media_stream_id_t src_stream,
                                   esp_service_t *sink_service, esp_media_stream_id_t sink_stream);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
