/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * BLE OTA source implementing the official ESP BLE OTA APP protocol natively.
 *
 * The on-air format matches the upstream NimBLE reference (service 0x8018,
 * characteristics 0x8020 / 0x8022, 4 KB sectors with 20-byte indication ACKs
 * and CRC16-CCITT), so the official Android / iOS APP and the PC AT-bridge
 * driver can talk to this device without modification.
 *
 * Reference: https://github.com/EspressifApps/esp-ble-ota-android
 */

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_att.h"
#include "host/ble_uuid.h"
#include "host/ble_store.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

extern void ble_store_config_init(void);

#include "adf_mem.h"
#include "esp_ota_service_source_ble.h"

#if CONFIG_OTA_ENABLE_BLE_SOURCE

#define BLE_URI_PREFIX  "ble://"
#define BLE_URI_PLEN    6

/* Official ESP BLE OTA APP protocol: 16-bit UUIDs */
#define BLE_GATTS_OTA_SVC_UUID               0x8018
#define ALERT_SERVICE_UUID                   0x1811  /* advertised together with 0x8018 (legacy) */
#define RECV_FW_UUID                         0x8020
#define ESP_OTA_SERVICE_SOURCE_BLE_BAR_UUID  0x8021
#define COMMAND_UUID                         0x8022
#define CUSTOMER_UUID                        0x8023

/* COMMAND opcodes (byte 0..1 of write payload, little-endian u16) */
#define ESP_OTA_SERVICE_SOURCE_BLE_CMD_OP_START    0x01
#define ESP_OTA_SERVICE_SOURCE_BLE_CMD_OP_STOP     0x02
#define ESP_OTA_SERVICE_SOURCE_BLE_CMD_ACK_PREFIX  0x03

/* Sector size used by the upstream APP. The APP buffers data into 4 KB
 * sectors and ACKs per sector. */
#define ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE  4096

/* 20-byte ACK frame: 18 bytes payload + 2 bytes CRC16-CCITT. */
#define ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN        20
/* COMMAND write frame used by the official APP: 18-byte payload + 2-byte CRC. */
#define ESP_OTA_SERVICE_SOURCE_BLE_CMD_FRAME_LEN  20

#define ESP_OTA_SERVICE_SOURCE_BLE_DEV_NAME  "OTA_BLE"
#define INVALID_CONN_HANDLE                  0xFFFF

#define ADV_DEFER_MS     200
#define ADV_DEFER_STACK  2560

/* Bytes already returned by esp_ota_service_source_ble_read() are mirrored into a small replay
 * buffer so they can be re-served after a close+open cycle. The OTA service
 * opens the source twice per session (once for the app-desc checker, once
 * for the writer); for streaming sources like BLE we cannot rewind to
 * offset 0, so the writer's first reads must come from this cache before
 * we fall through to the live stream buffer. 1 KiB easily covers the
 * 288-byte app-desc header that the checker consumes. */
#define ESP_OTA_SERVICE_SOURCE_BLE_REPLAY_CAPACITY  1024

/* How long the GATT callback is willing to wait for the consumer to drain
 * the stream buffer before giving up and reporting INSUFFICIENT_RES on the
 * sector ACK. 500 ms is far longer than a typical flash-write sector, but
 * still bounded so the NimBLE host task is not stalled forever on a stuck
 * reader. */
#define STREAM_BUF_SEND_TIMEOUT_MS  500

typedef struct {
    StreamBufferHandle_t  stream_buf;
    SemaphoreHandle_t     start_sem;
    int64_t               total_size;
    volatile bool         started;
    volatile bool         eof;
    volatile bool         abort;
    int                   ring_buf_size;
    int                   wait_start_ms;
    const char           *dev_name;
    bool                  opened;

    /* Sector reassembly state (touched only inside the GATT callbacks while
     * a session is active). */
    uint8_t  *fw_buf;
    uint32_t  fw_buf_offset;
    uint32_t  cur_sector;
    uint8_t   cur_packet;

    /* GATT handles + subscription state, populated by NimBLE callbacks. */
    uint16_t  conn_handle;
    uint16_t  recv_fw_val_handle;
    uint16_t  command_val_handle;
    bool      recv_fw_ind_enabled;
    bool      command_ind_enabled;

    /* Replay buffer state for surviving the checker→writer close/open cycle. */
    uint8_t *replay_buf;
    size_t   replay_capacity;
    size_t   replay_size;
    size_t   replay_pos;
    bool     caching;
    bool     replaying;
} esp_ota_service_source_ble_priv_t;

static const char *TAG = "OTA_SRC_BLE";

/* Active session used by GATT callbacks (set in open(), cleared in destroy()). */
static esp_ota_service_source_ble_priv_t *s_esp_ota_service_ble_priv;

/* Own-address type inferred at host sync. Defaults to PUBLIC for the case where
 * sync runs after a previous session left it at the value below; on_hs_sync()
 * overwrites it with the value returned by ble_hs_id_infer_auto(). */
static uint8_t s_own_addr_type = BLE_OWN_ADDR_PUBLIC;

/* Storage for val_handles assigned by the GATT stack. */
static uint16_t s_recv_fw_val;
static uint16_t s_ota_bar_val;
static uint16_t s_command_val;
static uint16_t s_customer_val;

/* NimBLE stack lifetime tracking; paired init/deinit across source
 * create/destroy cycles so a second create() can re-bring-up the host. */
static bool s_nimble_inited;

static int esp_ota_service_source_ble_recv_fw_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg);
static int esp_ota_service_source_ble_command_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg);
static int esp_ota_service_source_ble_passive_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg);
static void esp_ota_service_source_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
static int on_gap_event(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

static const struct ble_gatt_svc_def esp_ota_service_source_ble_gatt_svc[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = BLE_UUID16_DECLARE(BLE_GATTS_OTA_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = BLE_UUID16_DECLARE(RECV_FW_UUID),
                .access_cb  = esp_ota_service_source_ble_recv_fw_access,
                .val_handle = &s_recv_fw_val,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_INDICATE,
            },
            {
                .uuid       = BLE_UUID16_DECLARE(ESP_OTA_SERVICE_SOURCE_BLE_BAR_UUID),
                .access_cb  = esp_ota_service_source_ble_passive_access,
                .val_handle = &s_ota_bar_val,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
            },
            {
                .uuid       = BLE_UUID16_DECLARE(COMMAND_UUID),
                .access_cb  = esp_ota_service_source_ble_command_access,
                .val_handle = &s_command_val,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_INDICATE,
            },
            {
                .uuid       = BLE_UUID16_DECLARE(CUSTOMER_UUID),
                .access_cb  = esp_ota_service_source_ble_passive_access,
                .val_handle = &s_customer_val,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_INDICATE,
            },
            {0},
        },
    },
    {0},
};

/* CRC16-CCITT, poly 0x1021, init 0x0000. Matches the upstream
 * nimble_ota.c implementation expected by the Android/iOS APP. */
static uint16_t esp_ota_service_source_ble_crc16_ccitt(const uint8_t *buf, int len)
{
    uint16_t crc = 0;
    while (len--) {
        crc ^= (uint16_t)(*buf++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void esp_ota_service_source_ble_fill_ack_crc(uint8_t ack[ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN])
{
    uint16_t crc = esp_ota_service_source_ble_crc16_ccitt(ack, 18);
    ack[18] = (uint8_t)(crc & 0xFF);
    ack[19] = (uint8_t)((crc >> 8) & 0xFF);
}

static void esp_ota_service_source_ble_send_indication(uint16_t val_handle, const uint8_t *data, size_t len)
{
    if (s_esp_ota_service_ble_priv == NULL || s_esp_ota_service_ble_priv->conn_handle == INVALID_CONN_HANDLE) {
        return;
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGW(TAG, "Indicate failed: mbuf alloc failed");
        return;
    }
    int rc = ble_gatts_indicate_custom(s_esp_ota_service_ble_priv->conn_handle, val_handle, om);
    if (rc != 0) {
        ESP_LOGD(TAG, "Indicate: ble_gatts_indicate_custom rc=%d (val_handle=%u)", rc, val_handle);
    }
}

static void esp_ota_service_source_ble_send_recv_fw_ack(const esp_ota_service_source_ble_priv_t *priv,
                                                        const uint8_t header[3],
                                                        uint8_t status,
                                                        uint16_t next_sector)
{
    if (priv == NULL) {
        return;
    }
    uint8_t ack[ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN] = {0};
    ack[0] = header[0];
    ack[1] = header[1];
    ack[2] = status;
    ack[3] = 0x00;
    ack[4] = (uint8_t)(next_sector & 0xFF);
    ack[5] = (uint8_t)((next_sector >> 8) & 0xFF);
    esp_ota_service_source_ble_fill_ack_crc(ack);
    esp_ota_service_source_ble_send_indication(priv->recv_fw_val_handle, ack, ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN);
}

static int esp_ota_service_source_ble_passive_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    /* OTA_BAR / CUSTOMER are declared for compatibility with the upstream
     * GATT layout. Reads return empty, writes are accepted but ignored. */
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    return 0;
}

static int esp_ota_service_source_ble_command_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR || s_esp_ota_service_ble_priv == NULL) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    esp_ota_service_source_ble_priv_t *priv = s_esp_ota_service_ble_priv;
    uint8_t buf[ESP_OTA_SERVICE_SOURCE_BLE_CMD_FRAME_LEN] = {0};
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied);
    if (rc != 0 || copied < 2) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    /* Accept both:
     *  - legacy short command writes (2 or 6 bytes)
     *  - official APP 20-byte command frame with CRC16 over first 18 bytes
     */
    if (copied != 2 && copied != 6 && copied != ESP_OTA_SERVICE_SOURCE_BLE_CMD_FRAME_LEN) {
        ESP_LOGW(TAG, "Invalid COMMAND length: %u", copied);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (copied == ESP_OTA_SERVICE_SOURCE_BLE_CMD_FRAME_LEN) {
        uint16_t rx_crc = (uint16_t)buf[18] | ((uint16_t)buf[19] << 8);
        uint16_t calc_crc = esp_ota_service_source_ble_crc16_ccitt(buf, 18);
        if (rx_crc != calc_crc) {
            ESP_LOGW(TAG, "COMMAND CRC invalid: rx=0x%04x calc=0x%04x", rx_crc, calc_crc);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
    }

    uint16_t cmd_id = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

    uint8_t ack[ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN] = {0};
    ack[0] = ESP_OTA_SERVICE_SOURCE_BLE_CMD_ACK_PREFIX;
    ack[1] = 0x00;

    /* Refresh the active connection handle from this write so the indication
     * callback can route the ACK back without depending on GAP timing. */
    priv->conn_handle = conn_handle;

    if (cmd_id == ESP_OTA_SERVICE_SOURCE_BLE_CMD_OP_START) {
        if (copied >= 6) {
            priv->total_size = (int64_t)((uint32_t)buf[2]
                                         | ((uint32_t)buf[3] << 8)
                                         | ((uint32_t)buf[4] << 16)
                                         | ((uint32_t)buf[5] << 24));
        } else {
            priv->total_size = -1;
        }
        priv->cur_sector = 0;
        priv->cur_packet = 0;
        priv->fw_buf_offset = 0;
        priv->started = true;
        priv->eof = false;
        priv->abort = false;
        xSemaphoreGive(priv->start_sem);
        ESP_LOGI(TAG, "CMD START, fw_length=%" PRId64, priv->total_size);

        ack[2] = 0x01;  /* start ack */
        ack[3] = 0x00;
        esp_ota_service_source_ble_fill_ack_crc(ack);
        esp_ota_service_source_ble_send_indication(priv->command_val_handle, ack, ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN);
        return 0;
    }

    if (cmd_id == ESP_OTA_SERVICE_SOURCE_BLE_CMD_OP_STOP) {
        priv->eof = true;
        ESP_LOGI(TAG, "CMD STOP");

        ack[2] = 0x02;  /* stop ack */
        ack[3] = 0x00;
        esp_ota_service_source_ble_fill_ack_crc(ack);
        esp_ota_service_source_ble_send_indication(priv->command_val_handle, ack, ESP_OTA_SERVICE_SOURCE_BLE_ACK_LEN);
        return 0;
    }

    ESP_LOGW(TAG, "Unknown COMMAND id=0x%04x", cmd_id);
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
}

static int esp_ota_service_source_ble_recv_fw_access(uint16_t conn_handle, uint16_t attr_handle,
                                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR || s_esp_ota_service_ble_priv == NULL) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    esp_ota_service_source_ble_priv_t *priv = s_esp_ota_service_ble_priv;
    if (!priv->started) {
        ESP_LOGW(TAG, "RECV_FW write before CMD START - dropping");
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    if (priv->fw_buf == NULL) {
        ESP_LOGE(TAG, "Recv FW failed: fw_buf not allocated");
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    uint16_t total_len = OS_MBUF_PKTLEN(ctxt->om);
    if (total_len < 3) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t header[3] = {0};
    /* Do not use ble_hs_mbuf_to_flat() for partial header extraction:
     * it returns BLE_HS_EMSGSIZE when om length > max_len (which is true for
     * normal firmware packets), and Android maps our fallback ATT error to
     * GATT status=14. Copy the first 3 bytes directly instead. */
    if (os_mbuf_copydata(ctxt->om, 0, sizeof(header), header) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint16_t recv_sector = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    uint8_t recv_packet = header[2];
    uint16_t payload_len = total_len - 3;
    uint16_t data_len = payload_len;
    uint16_t rx_sector_crc = 0;
    bool has_sector_crc = false;

    priv->conn_handle = conn_handle;

    /* Sector index check. 0xFFFF means "last sector" and is always accepted. */
    if (recv_sector != (uint16_t)priv->cur_sector
        && !(header[0] == 0xFF && header[1] == 0xFF)) {
        ESP_LOGE(TAG, "Sector index error, cur=%" PRIu32 " recv=%u",
                 priv->cur_sector, recv_sector);
        esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x02, (uint16_t)priv->cur_sector);
        return 0;
    }

    /* Packet seq mismatch is logged but the payload is still buffered, since
     * the peer relies on the per-sector ACK to detect and retry drops. */
    if (recv_packet != priv->cur_packet && recv_packet != 0xFF) {
        ESP_LOGW(TAG, "Packet seq mismatch: cur=%u recv=%u (sector=%" PRIu32 ")",
                 priv->cur_packet, recv_packet, priv->cur_sector);
    }

    if (recv_packet == 0xFF) {
        /* The last packet carries: data + CRC16-CCITT(whole sector). */
        if (payload_len < 2) {
            ESP_LOGE(TAG, "Last packet too short for sector CRC: payload=%u", payload_len);
            esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x03, (uint16_t)priv->cur_sector);
            return 0;
        }
        data_len = payload_len - 2;
        uint8_t crc_le[2] = {0};
        if (os_mbuf_copydata(ctxt->om, 3 + data_len, sizeof(crc_le), crc_le) != 0) {
            ESP_LOGE(TAG, "Recv FW failed: read sector CRC failed");
            return BLE_ATT_ERR_UNLIKELY;
        }
        rx_sector_crc = (uint16_t)crc_le[0] | ((uint16_t)crc_le[1] << 8);
        has_sector_crc = true;
    }

    if (data_len > 0) {
        if (priv->fw_buf_offset + data_len > ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE) {
            ESP_LOGE(TAG, "Fw_buf overflow: off=%" PRIu32 " add=%u",
                     priv->fw_buf_offset, data_len);
            esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x03, (uint16_t)priv->cur_sector);
            return 0;
        }
        if (os_mbuf_copydata(ctxt->om, 3, data_len,
                             priv->fw_buf + priv->fw_buf_offset) != 0) {
            ESP_LOGE(TAG, "Recv FW failed: os_mbuf_copydata failed");
            return BLE_ATT_ERR_UNLIKELY;
        }
        priv->fw_buf_offset += data_len;
    }

    if (recv_packet == 0xFF) {
        if (!has_sector_crc) {
            esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x03, (uint16_t)priv->cur_sector);
            return 0;
        }

        if (priv->total_size > 0) {
            uint32_t expected_len = ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE;
            uint64_t sector_base = (uint64_t)priv->cur_sector * (uint64_t)ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE;
            if ((uint64_t)priv->total_size > sector_base) {
                uint64_t remain = (uint64_t)priv->total_size - sector_base;
                if (remain < ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE) {
                    expected_len = (uint32_t)remain;
                }
            }
            if (priv->fw_buf_offset != expected_len) {
                ESP_LOGE(TAG, "Sector length mismatch: sector=%" PRIu32 " got=%" PRIu32 " expect=%" PRIu32,
                         priv->cur_sector, priv->fw_buf_offset, expected_len);
                esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x03, (uint16_t)priv->cur_sector);
                return 0;
            }
        }

        uint16_t calc_sector_crc = esp_ota_service_source_ble_crc16_ccitt(priv->fw_buf, (int)priv->fw_buf_offset);
        if (calc_sector_crc != rx_sector_crc) {
            ESP_LOGE(TAG, "Sector CRC mismatch: sector=%" PRIu32 " rx=0x%04x calc=0x%04x",
                     priv->cur_sector, rx_sector_crc, calc_sector_crc);
            esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x01, (uint16_t)priv->cur_sector);
            return 0;
        }

        /* End of sector: push to stream buffer and ACK. */
        size_t sent = 0;
        if (priv->fw_buf_offset > 0) {
            sent = xStreamBufferSend(priv->stream_buf,
                                     priv->fw_buf,
                                     priv->fw_buf_offset,
                                     pdMS_TO_TICKS(STREAM_BUF_SEND_TIMEOUT_MS));
            if (sent != priv->fw_buf_offset) {
                ESP_LOGE(TAG, "Stream buffer send short: %u/%" PRIu32
                              " (consumer too slow)",
                         (unsigned)sent, priv->fw_buf_offset);
                priv->abort = true;
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }

        esp_ota_service_source_ble_send_recv_fw_ack(priv, header, 0x00, (uint16_t)(priv->cur_sector + 1));

        priv->cur_sector++;
        priv->cur_packet = 0;
        priv->fw_buf_offset = 0;
    } else {
        priv->cur_packet++;
    }

    return 0;
}

static void adv_defer_task(void *pv)
{
    (void)pv;
    vTaskDelay(pdMS_TO_TICKS(ADV_DEFER_MS));
    start_advertising();
    vTaskDelete(NULL);
}

static void schedule_adv_start(void)
{
    BaseType_t ok = xTaskCreate(adv_defer_task, "esp_ota_svc_adv",
                                ADV_DEFER_STACK, NULL,
                                tskIDLE_PRIORITY + 1, NULL);
    if (ok != pdPASS) {
        ESP_LOGW(TAG, "Schedule adv start: xTaskCreate(adv_defer) failed, starting now");
        start_advertising();
    }
}

static void start_advertising(void)
{
    /* Legacy advertising PDUs cap AdvData / scan-response at 31 bytes each.
     * Put the service UUIDs (6 bytes, always needed for the scanner filter)
     * in AdvData, and the potentially-long device name in scan response. */
    const char *name = ble_svc_gap_device_name();
    static const ble_uuid16_t adv_uuids[] = {
        BLE_UUID16_INIT(BLE_GATTS_OTA_SVC_UUID),
        BLE_UUID16_INIT(ALERT_SERVICE_UUID),
    };

    struct ble_hs_adv_fields adv_fields = {0};
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.uuids16 = (ble_uuid16_t *)adv_uuids;
    adv_fields.num_uuids16 = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    adv_fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start advertising failed: ble_gap_adv_set_fields returned %d (AdvData too large?)", rc);
        return;
    }

    if (name && name[0]) {
        /* Legacy scan-response PDU is 31 bytes; the local-name AD field uses
         * 2 bytes of overhead (length + AD type), leaving 29 bytes for the
         * name itself. Clamp up front so an over-long (or, in the worst case,
         * 256+-byte) configured name cannot silently wrap when narrowed to
         * the uint8_t name_len field. */
        const size_t MAX_RSP_NAME_LEN = 29;  // 31-byte PDU - 2-byte AD header
        size_t slen = strlen(name);
        struct ble_hs_adv_fields rsp_fields = {0};
        rsp_fields.name = (const uint8_t *)name;
        if (slen > MAX_RSP_NAME_LEN) {
            ESP_LOGW(TAG, "Set scan-rsp name: len=%u exceeds %u, truncating",
                     (unsigned)slen, (unsigned)MAX_RSP_NAME_LEN);
            rsp_fields.name_len = (uint8_t)MAX_RSP_NAME_LEN;
            rsp_fields.name_is_complete = 0;
        } else {
            rsp_fields.name_len = (uint8_t)slen;
            rsp_fields.name_is_complete = 1;
        }
        rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
        if (rc != 0) {
            ESP_LOGE(TAG, "Set scan-rsp fields failed: rc=%d, name_len=%u",
                     rc, (unsigned)rsp_fields.name_len);
        }
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x20;
    adv_params.itvl_max = 0x40;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, on_gap_event, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "Advertising started (name='%s', svc=0x%04x)",
                 name ? name : "?", BLE_GATTS_OTA_SVC_UUID);
    } else {
        ESP_LOGW(TAG, "Start advertising: ble_gap_adv_start returned %d", rc);
    }
}

static int on_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event == NULL || s_esp_ota_service_ble_priv == NULL) {
        return 0;
    }
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_esp_ota_service_ble_priv->conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "BLE connected, handle=%u", s_esp_ota_service_ble_priv->conn_handle);
                /* Request the shortest allowed connection interval so each GATT
                 * write PDU is delivered as fast as the central will allow. */
                struct ble_gap_upd_params p = {
                    .itvl_min = 6,
                    .itvl_max = 6,  /* 7.5ms */
                    .latency = 0,
                    .supervision_timeout = 500,
                    .min_ce_len = 0,
                    .max_ce_len = 0,
                };
                (void)ble_gap_update_params(s_esp_ota_service_ble_priv->conn_handle, &p);
            } else {
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnected, reason=%d", event->disconnect.reason);
            s_esp_ota_service_ble_priv->conn_handle = INVALID_CONN_HANDLE;
            s_esp_ota_service_ble_priv->recv_fw_ind_enabled = false;
            s_esp_ota_service_ble_priv->command_ind_enabled = false;
            start_advertising();
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            if (event->conn_update.status == 0) {
                struct ble_gap_conn_desc desc;
                if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
                    ESP_LOGI(TAG, "Conn interval updated: %.1f ms (itvl=%u)",
                             desc.conn_itvl * 1.25f, desc.conn_itvl);
                }
            } else {
                ESP_LOGW(TAG, "Conn update rejected (status=%d)",
                         event->conn_update.status);
            }
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            /* NimBLE documents the typical reasons as: 0 = terminated due to
             * connection, BLE_HS_ETIMEOUT = duration expired,
             * BLE_HS_EPREEMPTED = host preempted to set local identity.
             * For a single-connection OTA service, do not relaunch advertising
             * while a connection is active - the BLE_GAP_EVENT_DISCONNECT
             * handler is responsible for restarting it after disconnect. */
            if (event->adv_complete.reason == 0
                || s_esp_ota_service_ble_priv->conn_handle != INVALID_CONN_HANDLE) {
                ESP_LOGI(TAG, "ADV_COMPLETE: connection active, not restarting (reason=%d)",
                         event->adv_complete.reason);
            } else {
                schedule_adv_start();
            }
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_esp_ota_service_ble_priv->recv_fw_val_handle) {
                s_esp_ota_service_ble_priv->recv_fw_ind_enabled = event->subscribe.cur_indicate;
                ESP_LOGI(TAG, "RECV_FW indicate=%d", event->subscribe.cur_indicate);
            } else if (event->subscribe.attr_handle == s_esp_ota_service_ble_priv->command_val_handle) {
                s_esp_ota_service_ble_priv->command_ind_enabled = event->subscribe.cur_indicate;
                ESP_LOGI(TAG, "COMMAND indicate=%d", event->subscribe.cur_indicate);
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated, mtu=%d", event->mtu.value);
            break;

        default:
            break;
    }
    return 0;
}

static void on_hs_sync(void)
{
    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGW(TAG, "Infer own addr type: rc=%d, falling back to PUBLIC", rc);
        own_addr_type = BLE_OWN_ADDR_PUBLIC;
    }
    s_own_addr_type = own_addr_type;
    ESP_LOGI(TAG, "Infer own addr type: type=%u", (unsigned)s_own_addr_type);
    /* Defer first start so we are not inside sync when stack may emit
     * ADV_COMPLETE (stop). */
    schedule_adv_start();
}

static void esp_ota_service_source_ble_gatt_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    (void)arg;
    if (ctxt->op == BLE_GATT_REGISTER_OP_CHR && s_esp_ota_service_ble_priv != NULL) {
        /* Capture the val_handles assigned to RECV_FW and COMMAND so the
         * indication path can target the right characteristic. */
        s_esp_ota_service_ble_priv->recv_fw_val_handle = s_recv_fw_val;
        s_esp_ota_service_ble_priv->command_val_handle = s_command_val;
    }
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

static esp_err_t esp_ota_service_source_ble_ensure_stack_init(const char *dev_name)
{
    if (s_nimble_inited) {
        return ESP_OK;
    }

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ensure stack init failed: nimble_port_init returned %s", esp_err_to_name(ret));
        return ret;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(dev_name);

    int rc = ble_gatts_count_cfg(esp_ota_service_source_ble_gatt_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Ensure stack init failed: ble_gatts_count_cfg returned %d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(esp_ota_service_source_ble_gatt_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Ensure stack init failed: ble_gatts_add_svcs returned %d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    ble_hs_cfg.sync_cb = on_hs_sync;
    ble_hs_cfg.gatts_register_cb = esp_ota_service_source_ble_gatt_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_store_config_init();

    /* Prefer large MTU so each GATT write can carry up to 512 bytes. */
    int mtu_rc = ble_att_set_preferred_mtu(512);
    if (mtu_rc == 0) {
        ESP_LOGI(TAG, "Preferred ATT MTU set to 512");
    } else {
        ESP_LOGW(TAG, "Ensure stack init: ble_att_set_preferred_mtu(512) returned %d, using default MTU", mtu_rc);
    }

    nimble_port_freertos_init(nimble_host_task);
    s_nimble_inited = true;
    ESP_LOGI(TAG, "NimBLE BLE OTA (APP protocol) server started, name='%s'", dev_name);
    return ESP_OK;
}

static esp_err_t esp_ota_service_source_ble_open(esp_ota_service_source_t *self, const char *uri)
{
    esp_ota_service_source_ble_priv_t *priv = (esp_ota_service_source_ble_priv_t *)self->priv;

    if (strncmp(uri, BLE_URI_PREFIX, BLE_URI_PLEN) != 0) {
        ESP_LOGE(TAG, "URI must start with " BLE_URI_PREFIX " - got: %s", uri);
        return ESP_ERR_INVALID_ARG;
    }

    /* Reopen-within-same-session path: the checker already received CMD_START,
     * read the app-desc header, then closed us. The writer is now opening us
     * again; do NOT wait for another CMD_START (the host will not send one),
     * just rebind and switch the read path into "replay-then-live" mode so the
     * writer sees the bytes the checker drained from the stream buffer. */
    if (priv->started) {
        priv->opened = true;
        s_esp_ota_service_ble_priv = priv;
        priv->caching = false;
        priv->replay_pos = 0;
        priv->replaying = (priv->replay_size > 0);
        ESP_LOGI(TAG, "BLE OTA reopened (session continues), replay=%u bytes",
                 (unsigned)priv->replay_size);
        return ESP_OK;
    }

    /* Bind the active session before initialising the host so any GAP event
     * that arrives between sync and the first sleep below is routed to
     * on_gap_event correctly. */
    s_esp_ota_service_ble_priv = priv;

    const char *dev_name = priv->dev_name ? priv->dev_name : ESP_OTA_SERVICE_SOURCE_BLE_DEV_NAME;
    esp_err_t ret = esp_ota_service_source_ble_ensure_stack_init(dev_name);
    if (ret != ESP_OK) {
        s_esp_ota_service_ble_priv = NULL;
        return ret;
    }

    priv->total_size = -1;
    priv->started = false;
    priv->eof = false;
    priv->abort = false;
    priv->opened = true;
    priv->cur_sector = 0;
    priv->cur_packet = 0;
    priv->fw_buf_offset = 0;
    if (priv->stream_buf != NULL) {
        (void)xStreamBufferReset(priv->stream_buf);
    }
    priv->replay_size = 0;
    priv->replay_pos = 0;
    priv->replaying = false;
    priv->caching = (priv->replay_buf != NULL);

    int wait_ms = priv->wait_start_ms > 0 ? priv->wait_start_ms : 300000;
    if (xSemaphoreTake(priv->start_sem, pdMS_TO_TICKS(wait_ms)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout waiting for CMD_START from APP");
        s_esp_ota_service_ble_priv = NULL;
        priv->opened = false;
        priv->caching = false;
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "BLE OTA opened, size=%" PRId64, priv->total_size);
    return ESP_OK;
}

static int esp_ota_service_source_ble_read(esp_ota_service_source_t *self, uint8_t *buf, int len)
{
    esp_ota_service_source_ble_priv_t *priv = (esp_ota_service_source_ble_priv_t *)self->priv;
    if (!priv->opened || priv->abort || buf == NULL || len <= 0) {
        ESP_LOGE(TAG, "Read failed: session not open or aborted");
        return -1;
    }

    /* Replay path: serve cached bytes (recorded during the checker phase)
     * before touching the live stream buffer. */
    if (priv->replaying && priv->replay_pos < priv->replay_size) {
        size_t avail = priv->replay_size - priv->replay_pos;
        size_t to_copy = (avail < (size_t)len) ? avail : (size_t)len;
        memcpy(buf, priv->replay_buf + priv->replay_pos, to_copy);
        priv->replay_pos += to_copy;
        if (priv->replay_pos >= priv->replay_size) {
            priv->replaying = false;
        }
        return (int)to_copy;
    }

    /* Poll the stream buffer in short slices instead of one 30-s blocking
     * receive. This lets EOF (CMD_STOP) and abort wake the reader within at
     * most one poll interval, instead of after the full 30-s deadline. */
    const TickType_t poll_ticks = pdMS_TO_TICKS(500);
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(30000);

    while (true) {
        size_t received = xStreamBufferReceive(priv->stream_buf, buf, (size_t)len, poll_ticks);
        if (received > 0) {
            if (priv->caching && priv->replay_buf != NULL
                && priv->replay_size < priv->replay_capacity) {
                size_t cap_left = priv->replay_capacity - priv->replay_size;
                size_t to_cache = (received < cap_left) ? received : cap_left;
                memcpy(priv->replay_buf + priv->replay_size, buf, to_cache);
                priv->replay_size += to_cache;
            }
            return (int)received;
        }

        if (priv->abort) {
            ESP_LOGE(TAG, "Read failed: aborted while waiting for data");
            return -1;
        }
        if (priv->eof && xStreamBufferBytesAvailable(priv->stream_buf) == 0) {
            return 0;
        }
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            ESP_LOGE(TAG, "Read failed: timeout waiting for data");
            return -1;
        }
    }
}

static int64_t esp_ota_service_source_ble_get_size(esp_ota_service_source_t *self)
{
    return ((esp_ota_service_source_ble_priv_t *)self->priv)->total_size;
}

static esp_err_t esp_ota_service_source_ble_close(esp_ota_service_source_t *self)
{
    esp_ota_service_source_ble_priv_t *priv = (esp_ota_service_source_ble_priv_t *)self->priv;
    /* Soft close: keep started/eof so the writer phase can resume after the
     * checker phase. The real teardown happens in esp_ota_service_source_ble_destroy(). Caching
     * is frozen here so the replay buffer holds exactly what the previous
     * reader consumed. */
    priv->opened = false;
    priv->caching = false;
    return ESP_OK;
}

static void esp_ota_service_source_ble_destroy(esp_ota_service_source_t *self)
{
    if (self == NULL) {
        return;
    }
    esp_ota_service_source_ble_priv_t *priv = (esp_ota_service_source_ble_priv_t *)self->priv;
    s_esp_ota_service_ble_priv = NULL;

    /* Tear the NimBLE host down so a subsequent create+open is allowed to
     * re-register GATT services and reuse the stack. Disconnect peers first
     * so no GATT/HCI events fire into freed memory during deinit. */
    if (s_nimble_inited) {
        if (priv && priv->conn_handle != INVALID_CONN_HANDLE) {
            (void)ble_gap_terminate(priv->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            /* Give the host task time to process the disconnect event and
             * drain pending HCI num-completed-pkts notifications. */
            vTaskDelay(pdMS_TO_TICKS(300));
            priv->conn_handle = INVALID_CONN_HANDLE;
        }
        (void)ble_gap_adv_stop();
        vTaskDelay(pdMS_TO_TICKS(50));
        int rc = nimble_port_stop();
        if (rc == 0) {
            nimble_port_deinit();
            s_nimble_inited = false;
        } else {
            ESP_LOGW(TAG, "Destroy: nimble_port_stop returned %d - skipping deinit", rc);
        }
    }

    if (priv) {
        priv->opened = false;
        priv->started = false;
        priv->eof = true;
        if (priv->stream_buf != NULL) {
            vStreamBufferDelete(priv->stream_buf);
        }
        if (priv->start_sem != NULL) {
            vSemaphoreDelete(priv->start_sem);
        }
        if (priv->fw_buf != NULL) {
            adf_free(priv->fw_buf);
        }
        if (priv->replay_buf != NULL) {
            adf_free(priv->replay_buf);
        }
        adf_free(priv);
    }
    adf_free(self);
}

esp_err_t esp_ota_service_source_ble_create(const esp_ota_service_source_ble_cfg_t *cfg, esp_ota_service_source_t **out_src)
{
    if (out_src == NULL) {
        ESP_LOGE(TAG, "Create failed: out_src is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_src = NULL;

    esp_ota_service_source_t *src = adf_calloc(1, sizeof(esp_ota_service_source_t));
    if (src == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for source object");
        return ESP_ERR_NO_MEM;
    }
    esp_ota_service_source_ble_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_source_ble_priv_t));
    if (priv == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for source private data");
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }

    int ring_size = (cfg && cfg->ring_buf_size > 0) ? cfg->ring_buf_size : CONFIG_OTA_BLE_RING_BUF_SIZE;
    int wait_ms = (cfg && cfg->wait_start_timeout_ms > 0) ? cfg->wait_start_timeout_ms : CONFIG_OTA_BLE_WAIT_START_MS;
    /* Need room for at least one in-flight sector while another is draining. */
    if (ring_size < ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE * 2) {
        ring_size = ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE * 2;
    }

    priv->stream_buf = xStreamBufferCreate((size_t)ring_size, 1);
    if (priv->stream_buf == NULL) {
        ESP_LOGE(TAG, "Create failed: stream buffer (size=%d)", ring_size);
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }
    priv->start_sem = xSemaphoreCreateBinary();
    if (priv->start_sem == NULL) {
        ESP_LOGE(TAG, "Create failed: start semaphore");
        vStreamBufferDelete(priv->stream_buf);
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }
    priv->fw_buf = adf_malloc(ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE);
    if (priv->fw_buf == NULL) {
        ESP_LOGE(TAG, "Create failed: fw_buf (%d bytes)", ESP_OTA_SERVICE_SOURCE_BLE_SECTOR_SIZE);
        vStreamBufferDelete(priv->stream_buf);
        vSemaphoreDelete(priv->start_sem);
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }

    priv->ring_buf_size = ring_size;
    priv->wait_start_ms = wait_ms;
    priv->dev_name = (cfg && cfg->dev_name) ? cfg->dev_name : NULL;
    priv->total_size = -1;
    priv->conn_handle = INVALID_CONN_HANDLE;

    /* Replay cache lets us survive the checker→writer close+open cycle with
     * a streaming source. Failure to allocate is non-fatal: without it the
     * source still works for sources whose consumer opens only once. */
    priv->replay_capacity = ESP_OTA_SERVICE_SOURCE_BLE_REPLAY_CAPACITY;
    priv->replay_buf = adf_malloc(priv->replay_capacity);
    if (priv->replay_buf == NULL) {
        ESP_LOGW(TAG, "Replay cache (%u bytes) not allocated; reopen will fail",
                 (unsigned)priv->replay_capacity);
        priv->replay_capacity = 0;
    }
    priv->replay_size = 0;
    priv->replay_pos = 0;
    priv->caching = false;
    priv->replaying = false;

    src->open = esp_ota_service_source_ble_open;
    src->read = esp_ota_service_source_ble_read;
    src->get_size = esp_ota_service_source_ble_get_size;
    src->seek = NULL;
    src->close = esp_ota_service_source_ble_close;
    src->destroy = esp_ota_service_source_ble_destroy;
    src->priv = priv;

    *out_src = src;
    return ESP_OK;
}

#endif  /* CONFIG_OTA_ENABLE_BLE_SOURCE */
