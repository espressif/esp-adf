# ADF Event Hub

[English](./README.md)

**ADF Event Hub** 是面向 ADF / GMF 组件的轻量级按域（domain）发布-订阅机制。每个 hub 句柄对应一个事件源域（如 `"wifi"` 或 `"ota"`）；发布者向该域投递 `adf_event_t`，订阅者可通过 **队列模式** 或 **回调模式** 接收匹配事件。模块对启动顺序无要求：订阅者可以先于发布者注册。

## 主要特性

- **按域发布-订阅** — 每个事件源域对应一个 hub 句柄；订阅者按 `(domain, event_id)` 过滤，`ADF_EVENT_ANY_ID` 作为通配符
- **两种投递模式** — 队列模式（非阻塞 `xQueueSend`）或同步回调模式，按订阅者粒度选择
- **容忍任意启动顺序** — `adf_event_hub_subscribe()` 可在目标域尚未创建时调用，目标域会被自动创建
- **带引用计数的 envelope 投递** — 可选的 `release_cb` 在每次成功 publish 后被精确调用一次：若无队列模式订阅者则立即调用，否则在最后一次 `adf_event_hub_delivery_done()` 调用后触发；对所有代码路径下的堆上 payload 均安全
- **尽力而为的队列投递** — 某个订阅者队列满只影响该订阅者的单次投递，不影响其他订阅者
- **线程安全** — 所有公共 API 由单个内部互斥锁保护，可从任意任务安全调用（不支持 ISR）
- **可观测** — `adf_event_hub_get_stats()` 返回每个域的订阅者数量与 envelope 池使用情况；`adf_event_hub_dump()` 输出完整状态日志

## 工作原理

```
发布者任务                       adf_event_hub                 订阅者
     │                                │                             │
     │── adf_event_hub_publish ──────▶│                             │
     │                                │── 回调 handler（同步）─────▶│（回调模式）
     │                                │                             │
     │                                │── xQueueSend（非阻塞）─────▶│（队列模式）
     │                                │                             │── adf_event_hub_delivery_done ──▶
     │                                │                             │
     │◀──────── release_cb（在最后一次 delivery_done 之后）─────────│
```

事件被浅拷贝到带引用计数的 envelope 槽中。回调模式订阅者在发布者任务里同步执行；队列模式订阅者收到 `adf_event_delivery_t` 项后，必须对每次投递调用恰好一次 `adf_event_hub_delivery_done()` 以释放引用。

## 配置说明

`adf_event_hub` 没有模块级配置结构；状态以 hub 为粒度，在 `create` / `subscribe` 时设定。

### adf_event_subscribe_info_t

| 字段            | 类型                   | 说明                                                    | 默认值                   |
|-----------------|------------------------|---------------------------------------------------------|--------------------------|
| `event_domain`  | `const char *`         | 要订阅的域；`NULL` 表示订阅者自身 hub 所属的域          | `NULL`                   |
| `event_id`      | `uint16_t`             | 事件 ID 过滤；`ADF_EVENT_ANY_ID` 匹配全部               | `ADF_EVENT_ANY_ID`       |
| `target_queue`  | `QueueHandle_t`        | 队列模式收件箱；非 NULL 即选择队列模式                  | `NULL`                   |
| `handler`       | `adf_event_handler_t`  | 回调模式处理函数；当 `target_queue` 为 `NULL` 时生效    | `NULL`                   |
| `handler_ctx`   | `void *`               | 透传给 `handler` 的不透明上下文                         | `NULL`                   |

请用 `ADF_EVENT_SUBSCRIBE_INFO_DEFAULT()` 初始化，然后按需覆盖字段。若 `target_queue` 和 `handler` 同时设置，以队列模式为准，`handler` 被忽略。

## 快速入门

```c
#include "adf_event_hub.h"

#define WIFI_EVT_CONNECTED  1

static void on_wifi_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    printf("wifi event_id=%u payload_len=%u\n", event->event_id, event->payload_len);
}

void app_main(void)
{
    /* 1. 域的所有者创建自己的 hub */
    adf_event_hub_t wifi_hub = NULL;
    ESP_ERROR_CHECK(adf_event_hub_create("wifi", &wifi_hub));

    /* 2. 订阅者创建自己的 hub（用于标识），并订阅其他域。
     *    target_queue 为 NULL 表示使用回调模式。 */
    adf_event_hub_t app_hub = NULL;
    ESP_ERROR_CHECK(adf_event_hub_create("app", &app_hub));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "wifi";
    info.event_id     = WIFI_EVT_CONNECTED;
    info.handler      = on_wifi_event;
    ESP_ERROR_CHECK(adf_event_hub_subscribe(app_hub, &info));

    /* 3. 发布者发布事件 */
    adf_event_t ev = {
        .domain      = "wifi",
        .event_id    = WIFI_EVT_CONNECTED,
        .payload     = NULL,
        .payload_len = 0,
    };
    ESP_ERROR_CHECK(adf_event_hub_publish(wifi_hub, &ev, NULL, NULL));
}
```

### 队列模式订阅者

```c
QueueHandle_t inbox = xQueueCreate(8, sizeof(adf_event_delivery_t));

adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
info.event_domain = "wifi";
info.target_queue = inbox;
adf_event_hub_subscribe(app_hub, &info);

adf_event_delivery_t delivery;
if (xQueueReceive(inbox, &delivery, portMAX_DELAY) == pdTRUE) {
    /* ... 处理 delivery.event ... */
    adf_event_hub_delivery_done(app_hub, &delivery);  /* 释放 envelope 引用 */
}
```

### 堆上 payload 与 release 回调

```c
static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);  /* 在最后一次 delivery_done 之后被精确调用一次 */
}

char *msg = strdup("hello");
adf_event_t ev = { .domain = "wifi", .event_id = 7, .payload = msg, .payload_len = strlen(msg) + 1 };
adf_event_hub_publish(wifi_hub, &ev, release_payload, NULL);
```

## 典型应用场景

- **服务解耦** — 每个服务（`wifi` / `ota` / `player` …）向自己的域发布事件；消费者只订阅，不直接依赖服务
- **后绑定订阅者** — UI / 日志任务可在启动期订阅，即使发布方尚未完成初始化
- **批量聚合消费** — 事件聚合器使用队列模式，由专门工作任务排空队列
- **细粒度回调** — 对延迟敏感、可在发布者任务中运行的处理逻辑使用回调模式

## 示例工程

演示多个服务通过事件中心互动的完整示例位于：

```
components/adf_event_hub/examples/
```

同一份服务源码在两端都能编译：

```bash
# PC 主机（FreeRTOS POSIX 模拟器）
cd components/adf_event_hub/examples/host
cmake -S . -B build -G Ninja
cmake --build build
./build/event_hub_example

# ESP-IDF 目标（烧录到芯片）
cd components/adf_event_hub/examples
idf.py set-target esp32s3
idf.py build flash monitor
```

## 测试

单元测试位于 [`test_apps/`](test_apps/)，共享同一套源码，同时在 PC 主机和 ESP 目标上编译运行。构建、运行、Sanitizer 和覆盖率用法详见 [`test_apps/README.md`](test_apps/README.md)。

