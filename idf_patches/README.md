## Abstract 

 About the idf_ patches folder, the files stored in it are mainly the features that we have used in advance but have not yet been officially incorporated into the **esp-idf**. We have provided them in the form of patch 



## Patch file description 

### 1. idf_v3.3_freertos.patch

>  The function of idf_v3.3_freertos.patch is to put the stack of some tasks on PSRAM 

 **Use scenario:**  When using PSRAM, some default tasks stack will be put into PSRAM to save memory. If this patch is not put on, the following errors will occur during compilation .This patch need put on in `IDF` path

```c
Not found right xTaskCreateRestrictedPinnedToCore. Please apply the $ADF_PATH/idf_patches/idf_v3.3_freertos.patch first
```

### 2. idf_v3.3_light_sleep.path

>  The function of idf_v3.3_light_sleep.path is to enable the MCLK of I2S when esp32 enters light sleep

 **Use scenario:**  When you need to enter light sleep, but you need to keep real-time with codec when waking up esp32, you need to put this patch on.This patch need put on in `IDF` patch

### 3.idf_v3.3_esp_http_client.patch & adf_http_stream.patch

> The function of idf_v3.3_esp_http_client.patch avoid multiple `esp_http_client` problems 

>  When problems occur, they are as follows
> - There are log phenomena such as `Got 104 errno(Connection reset by peer), reconnect to peer` or `Got 11 errno(No more processes), reconnect to peer`, then there will be decoding failures such as m4a/aac

**Use scenario** :When multiple `esp_http_client` are used in `ADF`, the errno may not be accurate, so this patch is needed. `idf_v3.3_esp_http_client.patch` patch need put on in `IDF_PATH` path, `adf_http_stream.patch` patch need put on in `ADF_PATH` path
