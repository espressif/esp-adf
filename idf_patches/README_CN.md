## 概述

- [English Version](./README.md)

存放在 `idf_ patches` 文件夹下的文件主要涉及已经投入使用、但尚未正式纳入 **ESP-IDF** 的功能。为方便使用，这些文件已按功能分类，并打包为补丁。


## 补丁描述

### 1. FreeRTOS

>  FreeRTOS 补丁用于将部分任务的堆栈放入 PSRAM 中。

 **应用场景：** 使用 PSRAM 时，一些默认的任务堆栈将被放入 PSRAM 以节省内存。为避免在编译过程中出现错误，请在 `ESP-IDF` 路径中应用此补丁。

- `idf_v3.3_freertos.patch` # 支持 ESP-IDF release/v3.3
- `idf_v4.1_freertos.patch` # 支持 ESP-IDF release/v4.0
- `idf_v4.1_freertos.patch` # 支持 ESP-IDF release/v4.1
- `idf_v4.2_freertos.patch` # 支持 ESP-IDF release/v4.2
- `idf_v4.3_freertos.patch` # 支持 ESP-IDF release/v4.3
- `idf_v4.4_freertos.patch` # 支持 ESP-IDF release/v4.4

```
如果您的 ESP-IDF 版本不在上述范围内，请根据 `idf_v4.4_freertos.patch` 补丁内容，手动修改对应文件。
```

### 2. esp-http-client

> esp-http-client 补丁用于存放 HTTP 客户端的 errno 错误码。

 **应用场景：** 在多个 HTTP 客户端中，`errno` 都不是线程安全的。为了避免由 errno 异常引起的 HTTP 客户端读写异常，需要合理存放 HTTP 客户端的 errno。

- `idf_v3.3_esp_http_client.patch` # 支持 ESP-IDF release/v3.3
- `idf_v4.1_esp_http_client.patch` # 支持 ESP-IDF release/v4.0
- `idf_v4.1_esp_http_client.patch` # 支持 ESP-IDF release/v4.1
- `idf_v4.2_esp_http_client.patch` # 支持 ESP-IDF release/v4.2
- `idf_v4.3_esp_http_client.patch` # 支持 ESP-IDF release/v4.3

```
如果您的 ESP-IDF 版本不在上述范围内，请根据 `idf_v4.3_esp_http_client.patch` 补丁内容，手动修改对应文件。ESP-IDF release/v4.4 及之后的版本已修复该问题，无需应用此补丁。
```

 ### 3. light_sleep

  > idf_v3.3_light_sleep.path 补丁用于在 ESP32 进入 Light-sleep 模式时启用 I2S 的 MCLK。

 **应用场景：** 当 ESP32 处于 Light-sleep 模式时，I2S 的 MCLK 默认不工作。因此，ESP32 在被唤醒后需重新启动 MCLK，导致出声较慢。`idf_v3.3_light_sleep.path` 补丁可以让 ESP32 处于 Light-sleep 模式时仍然启用 MCLK，如此便可省去 ESP32 被唤醒后启动 MCLK 的时间，保证解码器解码的实时性，更快出声。

- `idf_v3.3_light_sleep.patch` # 支持 ESP-IDF release/v3.3

```
仅适用于 ESP-IDF release/v3.3，之后的版本已修复该问题，无需应用此补丁。
```