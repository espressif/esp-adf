/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * USB Mass Storage backend for the ota_fs example (esp_ota_service).
 *
 * Installs the USB Host Library + managed `usb_host_msc` driver, waits for
 * the first flash drive to be enumerated, and mounts its FAT volume under
 * `CONFIG_OTA_FS_EXAMPLE_USB_MOUNT_POINT` (default `/usb`).
 *
 * The OTA run logic in `esp_ota_service_fs_example.c` is unchanged — it just opens
 * `file://<mount>/firmware.bin` through `esp_ota_service_source_fs_create()`.
 *
 * Supported targets: ESP32-S2, ESP32-S3, ESP32-P4 (any SoC with a USB host
 * capable peripheral).
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdkconfig.h"

#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"

#include "esp_ota_service_fs_example.h"

#ifdef CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC

#define USB_MOUNT_POINT    CONFIG_OTA_FS_EXAMPLE_USB_MOUNT_POINT
#define USB_CONNECT_TO_MS  CONFIG_OTA_FS_EXAMPLE_USB_CONNECT_TIMEOUT_MS

#define EVT_DEVICE_READY  (BIT0)
#define EVT_HOST_EXITED   (BIT1)

static const char *TAG = "OTA_FS_USB";

static TaskHandle_t s_usb_task;
static QueueHandle_t s_usb_queue;
static EventGroupHandle_t s_usb_events;
static volatile bool s_usb_quit;

static msc_host_device_handle_t s_msc_device;
static msc_host_vfs_handle_t s_msc_vfs;

typedef struct {
    enum {
        USB_MSG_QUIT,
        USB_MSG_DEVICE_CONNECTED,
        USB_MSG_DEVICE_DISCONNECTED,
    } id;
    union {
        uint8_t                   new_dev_address;
        msc_host_device_handle_t  device_handle;
    } data;
} usb_msg_t;

static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    (void)arg;
    if (event == NULL) {
        return;
    }
    usb_msg_t msg = {0};
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected (usb_addr=%d)", event->device.address);
        msg.id = USB_MSG_DEVICE_CONNECTED;
        msg.data.new_dev_address = event->device.address;
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        ESP_LOGW(TAG, "MSC device disconnected");
        msg.id = USB_MSG_DEVICE_DISCONNECTED;
        msg.data.device_handle = event->device.handle;
    } else {
        ESP_LOGW(TAG, "Unsupported MSC event: %d", event->event);
        return;
    }
    if (s_usb_queue) {
        xQueueSend(s_usb_queue, &msg, 0);
    }
}

static void usb_host_task(void *args)
{
    (void)args;
    ESP_LOGI(TAG, "USB host task started");

    while (!s_usb_quit) {
        uint32_t event_flags = 0;
        esp_err_t err = usb_host_lib_handle_events(pdMS_TO_TICKS(200), &event_flags);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Usb host events: usb_host_lib_handle_events returned %s", esp_err_to_name(err));
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            (void)usb_host_device_free_all();
        }
    }

    /* Final drain — give the stack a chance to release pending devices/clients. */
    for (int i = 0; i < 20; ++i) {
        uint32_t event_flags = 0;
        (void)usb_host_lib_handle_events(pdMS_TO_TICKS(50), &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            break;
        }
    }

    xEventGroupSetBits(s_usb_events, EVT_HOST_EXITED);
    ESP_LOGI(TAG, "USB host task exiting");
    vTaskDelete(NULL);
}

static esp_err_t install_and_mount_device(uint8_t addr)
{
    esp_err_t err = msc_host_install_device(addr, &s_msc_device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Install MSC device failed: msc_host_install_device returned %s", esp_err_to_name(err));
        return err;
    }

    const esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 8192,
    };

    err = msc_host_vfs_register(s_msc_device, USB_MOUNT_POINT, &mount_cfg, &s_msc_vfs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Mount MSC failed: msc_host_vfs_register('%s') returned %s", USB_MOUNT_POINT, esp_err_to_name(err));
        (void)msc_host_uninstall_device(s_msc_device);
        s_msc_device = NULL;
        return err;
    }

    ESP_LOGI(TAG, "USB flash drive mounted at %s", USB_MOUNT_POINT);
    xEventGroupSetBits(s_usb_events, EVT_DEVICE_READY);
    return ESP_OK;
}

esp_err_t esp_ota_service_fs_example_init_usb(void)
{
    if (s_usb_queue || s_usb_events || s_usb_task) {
        ESP_LOGW(TAG, "Init_usb: already initialized");
        return ESP_OK;
    }

    s_usb_queue = xQueueCreate(4, sizeof(usb_msg_t));
    s_usb_events = xEventGroupCreate();
    if (!s_usb_queue || !s_usb_events) {
        ESP_LOGE(TAG, "Init_usb failed: FreeRTOS primitive create returned NULL");
        goto fail_free_primitives;
    }

    const usb_host_config_t host_cfg = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&host_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init_usb failed: usb_host_install returned %s", esp_err_to_name(err));
        goto fail_free_primitives;
    }

    s_usb_quit = false;
    BaseType_t ok = xTaskCreate(usb_host_task, "esp_ota_svc_usb", 4096, NULL, 5, &s_usb_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Init_usb failed: xTaskCreate(usb_host_task) returned %d", (int)ok);
        (void)usb_host_uninstall();
        goto fail_free_primitives;
    }

    const msc_host_driver_config_t msc_cfg = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .callback = msc_event_cb,
    };
    err = msc_host_install(&msc_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init_usb failed: msc_host_install returned %s", esp_err_to_name(err));
        s_usb_quit = true;
        (void)xEventGroupWaitBits(s_usb_events, EVT_HOST_EXITED, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        (void)usb_host_uninstall();
        goto fail_free_primitives;
    }

    ESP_LOGI(TAG, "Waiting up to %d ms for a USB flash drive on %s ...", USB_CONNECT_TO_MS, USB_MOUNT_POINT);

    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(USB_CONNECT_TO_MS);
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            break;
        }
        usb_msg_t msg;
        TickType_t remain = deadline - now;
        if (xQueueReceive(s_usb_queue, &msg, remain) != pdTRUE) {
            break;
        }
        if (msg.id == USB_MSG_DEVICE_CONNECTED) {
            esp_err_t mount_err = install_and_mount_device(msg.data.new_dev_address);
            if (mount_err == ESP_OK) {
                return ESP_OK;
            }
            ESP_LOGW(TAG, "Init_usb: mount failed (%s) — keep waiting for another device",
                     esp_err_to_name(mount_err));
        }
    }

    ESP_LOGE(TAG, "Init_usb failed: no USB flash drive enumerated within %d ms", USB_CONNECT_TO_MS);
    (void)esp_ota_service_fs_example_deinit_usb();
    return ESP_OTA_SERVICE_FS_EX_ERR_USB_TIMEOUT;

fail_free_primitives:
    if (s_usb_queue) {
        vQueueDelete(s_usb_queue);
        s_usb_queue = NULL;
    }
    if (s_usb_events) {
        vEventGroupDelete(s_usb_events);
        s_usb_events = NULL;
    }
    return ESP_OTA_SERVICE_FS_EX_ERR_USB_INSTALL;
}

esp_err_t esp_ota_service_fs_example_deinit_usb(void)
{
    esp_err_t first_err = ESP_OK;

    if (s_msc_vfs) {
        esp_err_t err = msc_host_vfs_unregister(s_msc_vfs);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Deinit_usb: msc_host_vfs_unregister returned %s", esp_err_to_name(err));
            first_err = err;
        }
        s_msc_vfs = NULL;
    }
    if (s_msc_device) {
        esp_err_t err = msc_host_uninstall_device(s_msc_device);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Deinit_usb: msc_host_uninstall_device returned %s", esp_err_to_name(err));
            if (first_err == ESP_OK) {
                first_err = err;
            }
        }
        s_msc_device = NULL;
    }

    esp_err_t err = msc_host_uninstall();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Deinit_usb: msc_host_uninstall returned %s", esp_err_to_name(err));
        if (first_err == ESP_OK) {
            first_err = err;
        }
    }

    /* Signal the host task to wind down, then wait for it to exit. */
    s_usb_quit = true;
    if (s_usb_events) {
        (void)xEventGroupWaitBits(s_usb_events, EVT_HOST_EXITED, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
    }
    s_usb_task = NULL;

    err = usb_host_uninstall();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Deinit_usb: usb_host_uninstall returned %s", esp_err_to_name(err));
        if (first_err == ESP_OK) {
            first_err = err;
        }
    }

    if (s_usb_queue) {
        vQueueDelete(s_usb_queue);
        s_usb_queue = NULL;
    }
    if (s_usb_events) {
        vEventGroupDelete(s_usb_events);
        s_usb_events = NULL;
    }

    return first_err;
}

#endif  /* CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC */
