## Overview

- [中文版本](./README_CN.md)

Files stored in the `idf_ patches` folder mainly relate to the features that are already in use but have not yet been officially incorporated into **ESP-IDF**. These files are categorized by feature and provided in the form of patches for your use.


## Patch Description

### 1. FreeRTOS

>  The function of FreeRTOS patch is to put the stack of some tasks on PSRAM.

 **Application Scenario:** When using PSRAM, some default task stacks will be put into PSRAM to save memory. To avoid errors during compilation, please apply this patch in the `ESP-IDF` path.

- `idf_v3.3_freertos.patch` # for ESP-IDF release/v3.3
- `idf_v4.1_freertos.patch` # for ESP-IDF release/v4.0
- `idf_v4.1_freertos.patch` # for ESP-IDF release/v4.1
- `idf_v4.2_freertos.patch` # for ESP-IDF release/v4.2
- `idf_v4.3_freertos.patch` # for ESP-IDF release/v4.3
- `idf_v4.4_freertos.patch` # for ESP-IDF release/v4.4

```
If your current ESP-IDF version is not included in the list above, please update the files according to the content in `idf_v4.4_freertos.patch`.
```

### 2. esp-http-client

> The function of esp-http-client patch is to save the errno number of HTTP clients.

 **Application Scenario:** `errno` is not thread safe in multiple HTTP clients. To avoid reading and writing exceptions of HTTP clients caused by errno exceptions, it is necessary to save the errno number of HTTP clients.

- `idf_v3.3_esp_http_client.patch` # for ESP-IDF release/v3.3
- `idf_v4.1_esp_http_client.patch` # for ESP-IDF release/v4.0
- `idf_v4.1_esp_http_client.patch` # for ESP-IDF release/v4.1
- `idf_v4.2_esp_http_client.patch` # for ESP-IDF release/v4.2
- `idf_v4.3_esp_http_client.patch` # for ESP-IDF release/v4.3

```
If your current ESP-IDF version is not included in the list above, please update the files according to the content in `idf_v4.3_esp_http_client.patch`. Note that ESP-IDF release/v4.4 and later versions have fixed the error and thus do not need this patch.
```

 ### 3. light_sleep

  > The function of idf_v3.3_light_sleep.path is to enable the MCLK of I2S when ESP32 enters Light-sleep mode.

 **Application Scenario:** When the ESP32 is in Light-sleep mode, the MCLK of the I2S does not work by default. Therefore, the MCLK needs to be restarted when ESP32 is woken up, causing slower sound output. The `idf_v3.3_light_sleep.path` patch keeps MCLK enabled in Light-sleep mode, thus saving the time to start MCLK and ensuring the real-time decoding by the codec and faster sound output.

- `idf_v3.3_light_sleep.patch` # for ESP-IDF release/v3.3

```
This issue is only for ESP-IDF release/v3.3. Later versions have fixed the error and thus do not need this patch.
```