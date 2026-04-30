# Button Service

[English](./README.md)

**Button Service** 是一个轻量级的 `esp_service_t` 封装层，负责将 `esp_board_manager` 按键设备与 ADF 事件系统打通。它自动发现所有类型为 `button` 的板级设备，为选定的事件集注册 `iot_button` 回调，并将按键动作以有类型的服务事件形式转发出去——无需手动维护设备名称列表。

> **版本要求：** 需要 `espressif/esp_board_manager *`。

## 主要特性

- **零配置自动发现** — 遍历 `esp_board_manager` 中注册的所有 `ESP_BOARD_DEVICE_TYPE_BUTTON` 设备，同时支持 GPIO 按键和 ADC 多按键组
- **精细化事件注册** — 通过事件掩码（`event_mask`）精确选择要安装的 `iot_button` 回调，降低 CPU 开销
- **生命周期感知转发** — 仅在服务运行期间发布事件；服务停止/暂停/进入低功耗时自动抑制转发，恢复运行/继续/退出低功耗时自动恢复
- **类型化载荷** — 每个事件均携带包含板级设备标签的 `esp_button_service_payload_t`，便于多按键场景的分发处理
- **标准服务接口** — 继承 `esp_service` 提供的 `esp_service_start`、`esp_service_stop`、`esp_service_event_subscribe` 等统一接口

## 工作原理

```
esp_board_manager              esp_button_service                  应用层
      │                               │                           │
      │── ESP_BOARD_DEVICE_TYPE_BUTTON 设备 ──▶ 发现并初始化      │
      │                               │                           │
  iot_button ──── 回调 ─────────────▶│                           │
                                      │── esp_service_publish_event ▶│
                                      │   (ESP_BUTTON_SERVICE_EVT_*, label)         │
```

`esp_button_service_create()` 扫描板级设备注册表，初始化每个按键设备，并注册所选的 `iot_button` 回调。
调用 `esp_service_start()` 后，每次按键动作都会通过服务事件总线发布，所有订阅者均可响应。

## 配置说明

### esp_button_service_cfg_t

| 字段         | 类型       | 说明                                              | 默认值                  |
|--------------|------------|---------------------------------------------------|-------------------------|
| `name`       | `const char *` | 服务名，用作事件总线域名；`NULL` → `"esp_button_service"` | `"esp_button_service"` |
| `event_mask` | `uint32_t` | `ESP_BUTTON_SERVICE_EVT_MASK_xxx` 常量的按位或，选择要注册的回调 | `ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT`  |

### 事件掩码常量

| 掩码常量                        | 对应事件 ID                     |
|---------------------------------|---------------------------------|
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_DOWN`       | `ESP_BUTTON_SERVICE_EVT_PRESS_DOWN`            |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_UP`         | `ESP_BUTTON_SERVICE_EVT_PRESS_UP`              |
| `ESP_BUTTON_SERVICE_EVT_MASK_SINGLE_CLICK`     | `ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_DOUBLE_CLICK`     | `ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_START` | `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START`      |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_HOLD`  | `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD`       |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_UP`    | `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP`         |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT`     | `ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT_DONE`| `ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE`     |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_END`        | `ESP_BUTTON_SERVICE_EVT_PRESS_END`             |

`ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT` 默认开启：`PRESS_DOWN`、`PRESS_UP`、`SINGLE_CLICK`、`DOUBLE_CLICK`、`LONG_PRESS_START` 和 `LONG_PRESS_UP`。

## 快速入门

```c
#include "esp_board_manager.h"
#include "esp_service.h"
#include "adf_event_hub.h"
#include "esp_button_service.h"

static void on_button_event(const adf_event_t *event, void *ctx)
{
    const esp_button_service_payload_t *pl = (const esp_button_service_payload_t *)event->payload;
    printf("button=%s event_id=%u\n", pl ? pl->label : "?", event->event_id);
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_board_manager_init());

    esp_button_service_cfg_t cfg = {
        .name       = "esp_button_service",
        .event_mask = ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT,
    };
    esp_button_service_t *svc = NULL;
    ESP_ERROR_CHECK(esp_button_service_create(&cfg, &svc));

    esp_service_t *base = (esp_service_t *)svc;

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler  = on_button_event;
    ESP_ERROR_CHECK(esp_service_event_subscribe(base, &sub));

    ESP_ERROR_CHECK(esp_service_start(base));
}
```

## 事件载荷

每个 `ESP_BUTTON_SERVICE_EVT_*` 事件均携带一个堆分配的 `esp_button_service_payload_t`，所有订阅者接收后自动释放。

```c
typedef struct {
    const char *label;  /* 板级设备名，在进程生命周期内有效 */
} esp_button_service_payload_t;
```

对于 ADC 多按键组，`label` 取自板级设备配置中的 `button_labels[]`（`dev_button_config_t`），而非设备名本身。

## 典型应用场景

- **单 GPIO 按键** — 用于简单用户输入的按下/释放检测
- **ADC 多按键组** — 单路 ADC 通道配多个电压挡位按键（如开发板上的音量加/减、播放/暂停）
- **低功耗唤醒** — 系统退出低功耗模式后，服务自动恢复事件转发，无需应用层额外处理

## 示例工程

包含多按键（GPIO + ADC 组）及完整事件订阅流程的示例位于：

```
components/esp_button_service/examples/esp_button_service_example
```
