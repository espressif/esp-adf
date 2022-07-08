# ESP-RTSP example

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_complex.png "高级")

## 例程简介

ESP RTSP 是一个基于标准 RSTP 协议的客户端和服务器。

1. 支持作为 RTSP 服务器，使用 VLC/Potplayer/Kmplayer/FFMPEG 等播放器输入串流地址即可播放 RTSP 流。
    - 串口输入: ``` start 0 ```

2. 支持作为 RTSP Pusher，同时支持 TCP/UDP 推送到本地/公网 RTSP 服务器（比如 EasyDarwin），并使用其它客户端来播放直播流。
    - 串口输入: ``` start 1 rtsp://192.168.1.1:554/live ```

3. 支持作为 RTSP Player，将服务器的 RTSP 流拉下来进行播放。
    - 串口输入: ``` start 2 rtsp://192.168.1.1:554/live ```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

同时建议您使用 `ESP32-S3-Korvo-2L` 开发板来运行本例程，以支持 UVC 和 UAC。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

本例程还需给 IDF 合入 1 个 patch，合入命令如下：

```
cd $IDF_PATH
git apply $ADF_PATH/idf_patches/idf_v4.4_freertos.patch
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/get-started/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，连接默认 Wi-Fi 网络或者输入串口命令 ```join [SSID] [PASSWORD] ``` 来配置网络。
- 输入串口命令 ```start [mode] [uri] ``` 进入 Server 或 Client 的不同模式。
    - Start RTSP Server / Client
    - ``` [mode]  0: Server 1: Push 2: Play ```
    - ``` [uri]  rtsp://192.168.1.1:554/live ```
- 当您使用 Server 模式时，您可以使用 VLC/Potplayer/Kmplayer/FFMPEG 等播放器输入上面的串流地址，播放 RTSP 流。
- 当您使用 Push 模式时，如果您使用的是 EasyDarwin，将可以在服务器后台的网页中查看到已经推送的链接及推送时间和数据量，这时您可以使用 VLC/Potplayer/Kmplayer/FFMPEG 等播放器输入上面的串流地址，播放推送的 RTSP 流。
- 当您使用 Play 模式时，输入您要播放的 RTSP 流链接进行播放，目前暂时只支持音频播放。
- 输入串口命令 stop 停止 Server 或者 Client。

## 技术指标

- Hardware JPEG encode

- OV3660 XCLK 20 MHz

- ```FRAMESIZE_QXGA,     // 2048x1536    // 11.36 fps```
- ```FRAMESIZE_FHD,      // 1920x1080    // 16.67 fps```
- ```FRAMESIZE_HD,       // 1280x720     // 17.86 fps```
- ```FRAMESIZE_SVGA,     // 800x600      // 27.78 fps```
- ```FRAMESIZE_VGA,      // 640x480      // 27.78 fps```
- ```FRAMESIZE_HVGA,     // 480x320      // 27.78 fps```
- ```FRAMESIZE_QVGA,     // 320x240      // 27.78 fps```

- Software JPEG encode

- OV3660 XCLK 40 MHz

- ```FRAMESIZE_VGA,      // 640x480      // 12.50 fps```
- ```FRAMESIZE_HVGA,     // 480x320      // 17.86 fps```
- ```FRAMESIZE_QVGA,     // 320x240      // 22.73 fps```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
