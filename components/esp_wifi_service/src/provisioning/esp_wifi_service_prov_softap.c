/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_prov_softap.h"

#define MAX_SOFTAP_CONN_NUM  1

#define SOFTAP_DNS_UDP_PORT    53
#define SOFTAP_DNS_MAX_PKT     512
#define SOFTAP_DNS_TASK_STACK  4096
#define SOFTAP_DNS_TASK_PRIO   5

#define SOFTAP_DEFAULT_SSID_PREFIX          "ESP_SVC_"
#define SOFTAP_DEFAULT_SSID_SUFFIX_BYTES    3
#define SOFTAP_DEFAULT_SSID_SUFFIX_HEX_LEN  (SOFTAP_DEFAULT_SSID_SUFFIX_BYTES * 2)
#define SOFTAP_MAC_ADDR_LEN                 6

static const char *TAG = "WIFI_PROV_SOFTAP";

/**
 * @brief  SoftAP context
 */
typedef struct {
    esp_netif_t       *ap_netif;      /*!< SoftAP network interface */
    wifi_mode_t        saved_mode;    /*!< Saved mode */
    TaskHandle_t       dns_task;      /*!< DNS task handle */
    volatile bool      dns_running;   /*!< True while DNS task should run */
    SemaphoreHandle_t  dns_done_sem;  /*!< DNS done semaphore */
} wifi_prov_softap_ctx_t;

static wifi_prov_softap_ctx_t *s_prov_softap_ctx;

static esp_err_t dns_skip_name(const uint8_t *pkt, size_t len, size_t *inout_off)
{
    size_t pos = *inout_off;
    unsigned guard = 0;
    while (pos < len && guard++ < 128) {
        uint8_t lab = pkt[pos];
        if (lab == 0) {
            pos++;
            break;
        }
        if ((lab & 0xC0) == 0xC0) {
            if (pos + 2 > len) {
                return ESP_ERR_INVALID_SIZE;
            }
            pos += 2;
            break;
        }
        if (lab > 63 || pos + 1 + lab > len) {
            return ESP_ERR_INVALID_SIZE;
        }
        pos += 1 + lab;
    }
    if (guard >= 128) {
        return ESP_ERR_INVALID_STATE;
    }
    *inout_off = pos;
    return ESP_OK;
}

static esp_err_t dns_question_section_end(const uint8_t *pkt, size_t len, uint16_t qdcount, size_t *qsec_len_out,
                                          uint16_t *first_qtype_out, uint16_t *first_qclass_out)
{
    if (len < 12 || qsec_len_out == NULL || first_qtype_out == NULL || first_qclass_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t pos = 12;
    bool have_first = false;
    for (uint16_t qi = 0; qi < qdcount; ++qi) {
        ESP_RETURN_ON_ERROR(dns_skip_name(pkt, len, &pos), TAG, "Question parse failed");
        if (pos + 4 > len) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (!have_first) {
            *first_qtype_out = (uint16_t)((pkt[pos] << 8) | pkt[pos + 1]);
            *first_qclass_out = (uint16_t)((pkt[pos + 2] << 8) | pkt[pos + 3]);
            have_first = true;
        }
        pos += 4;
    }
    *qsec_len_out = pos - 12;
    return ESP_OK;
}

static ssize_t softap_dns_build_reply(const uint8_t *req, size_t req_len, const esp_ip4_addr_t *ap_ip, uint8_t *rsp, size_t rsp_cap)
{
    if (req_len < 12 || rsp_cap < 12) {
        return -1;
    }

    uint16_t id = (uint16_t)((req[0] << 8) | req[1]);
    uint16_t flags_in = (uint16_t)((req[2] << 8) | req[3]);
    uint16_t qdcount = (uint16_t)((req[4] << 8) | req[5]);

    if (qdcount == 0) {
        return -1;
    }

    size_t qsec_len = 0;
    uint16_t qtype = 0;
    uint16_t qclass = 0;
    if (dns_question_section_end(req, req_len, qdcount, &qsec_len, &qtype, &qclass) != ESP_OK) {
        return -1;
    }
    if (12 + qsec_len > req_len) {
        return -1;
    }
    if (qclass != 1) {
        return -1;
    }

    const uint16_t type_a = 1;
    const uint16_t type_any = 255;

    if (qtype != type_a && qtype != type_any) {
        if (12 + qsec_len > rsp_cap) {
            return -1;
        }
        memcpy(rsp, req, 12 + qsec_len);
        rsp[0] = (uint8_t)(id >> 8);
        rsp[1] = (uint8_t)(id & 0xFF);
        uint16_t rsp_flags = (uint16_t)(0x8000u | 0x0400u | (flags_in & 0x0100u));
        rsp[2] = (uint8_t)(rsp_flags >> 8);
        rsp[3] = (uint8_t)(rsp_flags & 0xFF);
        rsp[6] = 0;
        rsp[7] = 0;
        rsp[8] = 0;
        rsp[9] = 0;
        rsp[10] = 0;
        rsp[11] = 0;
        return (ssize_t)(12 + qsec_len);
    }

    const size_t ans_len = 2 + 2 + 2 + 4 + 2 + 4;
    const size_t total = 12 + qsec_len + ans_len;
    if (total > rsp_cap) {
        return -1;
    }

    memcpy(rsp, req, 12 + qsec_len);
    rsp[0] = (uint8_t)(id >> 8);
    rsp[1] = (uint8_t)(id & 0xFF);
    uint16_t rsp_flags = (uint16_t)(0x8000u | 0x0400u | (flags_in & 0x0100u));
    rsp[2] = (uint8_t)(rsp_flags >> 8);
    rsp[3] = (uint8_t)(rsp_flags & 0xFF);
    rsp[4] = req[4];
    rsp[5] = req[5];
    rsp[6] = 0;
    rsp[7] = 1;
    rsp[8] = 0;
    rsp[9] = 0;
    rsp[10] = 0;
    rsp[11] = 0;

    size_t o = 12 + qsec_len;
    rsp[o++] = 0xC0;
    rsp[o++] = 0x0C;
    rsp[o++] = 0;
    rsp[o++] = 1;
    rsp[o++] = 0;
    rsp[o++] = 1;
    rsp[o++] = 0;
    rsp[o++] = 0;
    rsp[o++] = 0;
    rsp[o++] = 60;
    rsp[o++] = 0;
    rsp[o++] = 4;
    rsp[o++] = (uint8_t)esp_ip4_addr1(ap_ip);
    rsp[o++] = (uint8_t)esp_ip4_addr2(ap_ip);
    rsp[o++] = (uint8_t)esp_ip4_addr3(ap_ip);
    rsp[o++] = (uint8_t)esp_ip4_addr4(ap_ip);

    return (ssize_t)total;
}

static void softap_dns_task(void *arg)
{
    wifi_prov_softap_ctx_t *ctx = (wifi_prov_softap_ctx_t *)arg;
    int dns_sock = -1;
    if (!ctx || !ctx->ap_netif) {
        ESP_LOGE(TAG, "DNS task failed: invalid context");
        goto done;
    }
    dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (dns_sock < 0) {
        ESP_LOGE(TAG, "Socket failed: errno=%d", errno);
        goto done;
    }

    int reuse = 1;
    setsockopt(dns_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SOFTAP_DNS_UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(dns_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "Bind failed: errno=%d", errno);
        close(dns_sock);
        dns_sock = -1;
        goto done;
    }

    uint8_t rx[SOFTAP_DNS_MAX_PKT];
    uint8_t tx[SOFTAP_DNS_MAX_PKT];

    while (ctx->dns_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(dns_sock, &read_fds);
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 200 * 1000,
        };
        int ready = select(dns_sock + 1, &read_fds, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready == 0) {
            continue;
        }
        if (!ctx->dns_running) {
            break;
        }

        struct sockaddr_in from = {0};
        socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(dns_sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &fromlen);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n < 12 || (rx[2] & 0x80) != 0) {
            continue;
        }
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_get_ip_info(ctx->ap_netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
            continue;
        }
        ssize_t m = softap_dns_build_reply(rx, (size_t)n, &ip_info.ip, tx, sizeof(tx));
        if (m > 0) {
            sendto(dns_sock, tx, (size_t)m, 0, (struct sockaddr *)&from, fromlen);
        }
    }

    if (dns_sock >= 0) {
        close(dns_sock);
    }

done:
    if (ctx) {
        if (ctx->dns_done_sem) {
            xSemaphoreGive(ctx->dns_done_sem);
        }
    }
    ESP_LOGI(TAG, "DNS task exited");
    vTaskDeleteWithCaps(NULL);
}

static esp_err_t wifi_prov_softap_dns_configure_dhcp(esp_netif_t *ap_netif)
{
    ESP_RETURN_ON_FALSE(ap_netif, ESP_ERR_INVALID_ARG, TAG, "DHCP config failed: ap_netif is NULL");

    esp_netif_ip_info_t ip_info = {0};
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(ap_netif, &ip_info), TAG, "DHCP config failed: get IP");
    ESP_RETURN_ON_FALSE(ip_info.ip.addr != 0, ESP_ERR_INVALID_STATE, TAG, "DHCP config failed: AP has no IP");

    esp_netif_dns_info_t dns = {
        .ip = {
            .type = ESP_IPADDR_TYPE_V4,
            .u_addr = {
                .ip4 = {
                    .addr = ip_info.ip.addr,
                },
            },
        },
    };
    ESP_RETURN_ON_ERROR(esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns), TAG,
                        "DHCP config failed: set DNS info");

    uint8_t offer_dns = 1;
    esp_err_t ret = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns, sizeof(offer_dns));
    if (ret == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_RETURN_ON_ERROR(esp_netif_dhcps_stop(ap_netif), TAG, "DHCP config failed: stop DHCP server");
        ESP_RETURN_ON_ERROR(esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns, sizeof(offer_dns)),
                            TAG, "DHCP config failed: set DNS option");
        ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(ap_netif), TAG, "DHCP config failed: restart DHCP server");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DHCP config failed: set DNS option err=0x%x", (unsigned)ret);
        return ret;
    }
    return ESP_OK;
}

static esp_err_t wifi_prov_softap_dns_start(wifi_prov_softap_ctx_t *ctx)
{
    ESP_RETURN_ON_FALSE(ctx && ctx->ap_netif, ESP_ERR_INVALID_ARG, TAG, "DNS start failed: ap_netif is NULL");
    if (ctx->dns_task) {
        return ESP_OK;
    }
    if (!ctx->dns_done_sem) {
        ctx->dns_done_sem = xSemaphoreCreateBinaryWithCaps(ESP_WIFI_SERVICE_CAPS);
        ESP_RETURN_ON_FALSE(ctx->dns_done_sem, ESP_ERR_NO_MEM, TAG, "DNS start failed: no memory");
    }
    ctx->dns_running = true;
    BaseType_t ok = xTaskCreateWithCaps(softap_dns_task, "wifi_prov_softap_dns", SOFTAP_DNS_TASK_STACK, ctx,
                                        SOFTAP_DNS_TASK_PRIO, &ctx->dns_task, ESP_WIFI_SERVICE_TASK_STACK_CAPS);
    if (ok != pdPASS) {
        ctx->dns_running = false;
        ESP_LOGE(TAG, "DNS start failed: create task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void wifi_prov_softap_dns_stop(wifi_prov_softap_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    TaskHandle_t dns_task = ctx->dns_task;
    ctx->dns_running = false;
    if (dns_task && ctx->dns_done_sem) {
        if (xSemaphoreTake(ctx->dns_done_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
            ESP_LOGW(TAG, "DNS stop: task timeout");
            vTaskDeleteWithCaps(dns_task);
            ctx->dns_task = NULL;
        }
    }
}

static bool softap_password_valid(const char *pw)
{
    if (!pw || pw[0] == '\0') {
        return false;
    }
    size_t len = strlen(pw);
    if (len < 8 || len > 63) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if ((unsigned char)pw[i] < 0x20 || (unsigned char)pw[i] > 0x7E) {
            return false;
        }
    }
    return true;
}

static bool softap_ssid_valid(const char *ssid)
{
    if (!ssid || ssid[0] == '\0') {
        return false;
    }
    size_t len = strlen(ssid);
    if (len > sizeof(((wifi_ap_config_t *)0)->ssid)) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if ((unsigned char)ssid[i] < 0x20 || (unsigned char)ssid[i] > 0x7E) {
            return false;
        }
    }
    return true;
}

static esp_err_t softap_set_ssid(wifi_ap_config_t *ap_cfg, const char *ssid)
{
    ESP_RETURN_ON_FALSE(ap_cfg && ssid, ESP_ERR_INVALID_ARG, TAG, "SoftAP set SSID failed: invalid args");

    size_t ssid_len = strlen(ssid);
    memset(ap_cfg->ssid, 0, sizeof(ap_cfg->ssid));
    memcpy(ap_cfg->ssid, ssid, ssid_len);
    ap_cfg->ssid_len = ssid_len;
    return ESP_OK;
}

static esp_err_t softap_build_default_ssid(char *ssid, size_t ssid_size)
{
    ESP_RETURN_ON_FALSE(ssid && ssid_size > 0, ESP_ERR_INVALID_ARG, TAG, "SoftAP default SSID failed: invalid args");

    uint8_t mac[SOFTAP_MAC_ADDR_LEN] = {0};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG, "SoftAP default SSID failed: read MAC");

    int len = snprintf(ssid, ssid_size, SOFTAP_DEFAULT_SSID_PREFIX "%02X%02X%02X",
                       mac[SOFTAP_MAC_ADDR_LEN - SOFTAP_DEFAULT_SSID_SUFFIX_BYTES],
                       mac[SOFTAP_MAC_ADDR_LEN - SOFTAP_DEFAULT_SSID_SUFFIX_BYTES + 1],
                       mac[SOFTAP_MAC_ADDR_LEN - SOFTAP_DEFAULT_SSID_SUFFIX_BYTES + 2]);
    ESP_RETURN_ON_FALSE(len > 0 && (size_t)len < ssid_size, ESP_ERR_INVALID_SIZE, TAG,
                        "SoftAP default SSID failed: buffer too small");
    return ESP_OK;
}

static esp_err_t softap_apply_config(const esp_wifi_service_prov_softap_config_t *softap_cfg)
{
    wifi_config_t cfg = {0};
    ESP_RETURN_ON_ERROR(esp_wifi_get_config(WIFI_IF_AP, &cfg), TAG, "SoftAP apply config failed: get AP config");

    if (softap_cfg && softap_ssid_valid(softap_cfg->ssid)) {
        ESP_RETURN_ON_ERROR(softap_set_ssid(&cfg.ap, softap_cfg->ssid), TAG, "SoftAP apply config failed: set SSID");
    } else {
        char default_ssid[sizeof(SOFTAP_DEFAULT_SSID_PREFIX) + SOFTAP_DEFAULT_SSID_SUFFIX_HEX_LEN] = {0};
        ESP_RETURN_ON_ERROR(softap_build_default_ssid(default_ssid, sizeof(default_ssid)), TAG,
                            "SoftAP apply config failed: build default SSID");
        ESP_RETURN_ON_ERROR(softap_set_ssid(&cfg.ap, default_ssid), TAG, "SoftAP apply config failed: set default SSID");
    }
    if (softap_cfg && softap_cfg->channel > 0 && softap_cfg->channel <= 14) {
        cfg.ap.channel = softap_cfg->channel;
    }
    if (softap_cfg && softap_cfg->max_connection > 0) {
        cfg.ap.max_connection = softap_cfg->max_connection > MAX_SOFTAP_CONN_NUM ? MAX_SOFTAP_CONN_NUM : softap_cfg->max_connection;
    }

    const char *pw = softap_cfg ? softap_cfg->password : NULL;
    if (pw && softap_password_valid(pw)) {
        strlcpy((char *)cfg.ap.password, pw, sizeof(cfg.ap.password));
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else if (pw) {
        memset(cfg.ap.password, 0, sizeof(cfg.ap.password));
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &cfg), TAG, "SoftAP apply config failed: set AP config");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "SoftAP apply config failed: set power save");
    return ESP_OK;
}

static void softap_teardown(wifi_prov_softap_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->saved_mode != WIFI_MODE_MAX) {
        esp_wifi_set_mode(ctx->saved_mode);
        ctx->saved_mode = WIFI_MODE_MAX;
    }
    if (ctx->ap_netif) {
        esp_err_t ret = esp_netif_dhcps_stop(ctx->ap_netif);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "SoftAP teardown: stop dhcps failed, err=0x%x", (unsigned)ret);
        }
        esp_netif_destroy_default_wifi(ctx->ap_netif);
        ctx->ap_netif = NULL;
    }
}

static esp_err_t softap_setup(wifi_prov_softap_ctx_t *ctx, const esp_wifi_service_prov_softap_config_t *softap_cfg)
{
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_INVALID_ARG, TAG, "SoftAP setup failed: ctx is NULL");
    ctx->ap_netif = NULL;
    ctx->saved_mode = WIFI_MODE_MAX;
    wifi_mode_t current = WIFI_MODE_NULL;
    ESP_RETURN_ON_ERROR(esp_wifi_get_mode(&current), TAG, "SoftAP setup failed: get mode");
    esp_netif_t *created_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(created_netif, ESP_FAIL, TAG, "SoftAP setup failed: create AP netif");

    wifi_mode_t target = current;
    switch (current) {
        case WIFI_MODE_NULL:
            target = WIFI_MODE_AP;
            break;
        case WIFI_MODE_STA:
            target = WIFI_MODE_APSTA;
            break;
        default:
            break;
    }
    if (target != current) {
        esp_err_t ret = esp_wifi_set_mode(target);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SoftAP setup failed: set mode, err=0x%x", (unsigned)ret);
            esp_netif_destroy_default_wifi(created_netif);
            return ret;
        }
    }
    ctx->saved_mode = current;
    ctx->ap_netif = created_netif;
    esp_err_t ret = softap_apply_config(softap_cfg);
    if (ret != ESP_OK) {
        softap_teardown(ctx);
        return ret;
    }
    return ESP_OK;
}

static void softap_ctx_free(wifi_prov_softap_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->dns_done_sem) {
        vSemaphoreDeleteWithCaps(ctx->dns_done_sem);
        ctx->dns_done_sem = NULL;
    }
    heap_caps_free(ctx);
}

esp_err_t wifi_prov_softap_start(const esp_wifi_service_prov_softap_config_t *softap_cfg)
{
    if (s_prov_softap_ctx) {
        return ESP_OK;
    }

    wifi_prov_softap_ctx_t *ctx = heap_caps_calloc_prefer(1, sizeof(*ctx), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "SoftAP start failed: no memory");
    ctx->saved_mode = WIFI_MODE_MAX;

    esp_err_t ret = softap_setup(ctx, softap_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SoftAP start failed: setup, err=0x%x", (unsigned)ret);
        softap_ctx_free(ctx);
        return ret;
    }
    ret = wifi_prov_softap_dns_configure_dhcp(ctx->ap_netif);
    if (ret != ESP_OK) {
        softap_teardown(ctx);
        softap_ctx_free(ctx);
        return ret;
    }
    ret = wifi_prov_softap_dns_start(ctx);
    if (ret != ESP_OK) {
        softap_teardown(ctx);
        softap_ctx_free(ctx);
        return ret;
    }
    s_prov_softap_ctx = ctx;
    return ESP_OK;
}

void wifi_prov_softap_stop(void)
{
    wifi_prov_softap_ctx_t *ctx = s_prov_softap_ctx;
    if (!ctx) {
        return;
    }
    s_prov_softap_ctx = NULL;
    wifi_prov_softap_dns_stop(ctx);
    softap_teardown(ctx);
    softap_ctx_free(ctx);
}
