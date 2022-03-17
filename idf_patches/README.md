## Abstract

 About the idf_ patches folder, the files stored in it are mainly the features that we have used in advance but have not yet been officially incorporated into the **esp-idf**. We have provided them in the form of patch



## Patch file description

### 1. ESP-IDF freertos

>  The function of freertos patch is to put the stack of some tasks on PSRAM

 **Use scenario:**  When using PSRAM, some default tasks stack will be put into PSRAM to save memory. If this patch is not put on, the following errors will occur during compilation .This patch need put on in `IDF` path

- `idf_v3.3_freertos.patch` # for esp-idf version v3.3
- `idf_v4.1_freertos.patch` # for esp-idf version v4.0
- `idf_v4.1_freertos.patch` # for esp-idf version v4.1
- `idf_v4.2_freertos.patch` # for esp-idf version v4.2
- `idf_v4.3_freertos.patch` # for esp-idf version v4.3
- `idf_v4.4_freertos.patch` # for esp-idf version v4.4

```
If the current IDF is not the above IDF versions, you need to manually type this patch
```

### 2. ESP-IDF esp-http-client

> The function of esp-http-client patch avoid multiple esp_http_client problems

 **Use scenario:**  `errno` is not thread safe in multiple HTTP-clients, so it is necessary to save the errno number of HTTP clients to avoid reading and writing exceptions of HTTP-clients caused by errno exceptions

- `idf_v3.3_esp_http_client.patch` # for esp-idf version v3.3
- `idf_v4.1_esp_http_client.patch` # for esp-idf version v4.0
- `idf_v4.1_esp_http_client.patch` # for esp-idf version v4.1
- `idf_v4.2_esp_http_client.patch` # for esp-idf version v4.2
- `idf_v4.3_esp_http_client.patch` # for esp-idf version v4.3

```
If the current IDF is not the above IDF versions, you need to manually type this patch, but esp-idf release/v4.4 and later versions do not need this patch
```

 ### 3. ESP_IDF light_sleep

  > The function of idf_v3.3_light_sleep.path is to enable the MCLK of I2S when esp32 enters light sleep

 **Use scenario:**  When you need to enter light sleep, but you need to keep real-time with codec when waking up esp32, you need to put this patch on.This patch need put on in `IDF` patch

- `idf_v3.3_light_sleep.patch` # for esp-idf version v3.3

```
 This issue just for esp-idf release/v3.3
 ```