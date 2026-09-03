# RTSP 音视频推流

- [English Version](./README.md)
- 基础示例：⭐

## 例程简介

本例程基于 `esp_service` 将摄像头视频和麦克风音频推送到 RTSP 服务器；应用仅配置媒体参数和目标地址、连接 `esp_video_capture_service` 与 `esp_rtsp_service` 并控制其生命周期，无需分别处理采集、编码和 RTSP 传输。
service 数据链路如下：

```text
摄像头 + 麦克风
        |
esp_video_capture_service
        |
esp_media_service_link()
        |
esp_rtsp_service（SINK）
        |
远端 RTSP 服务器
```

推流使用 640 × 480、10 fps 的 MJPEG 视频和 16 kHz 单声道 AAC 音频，目标地址通过 menuconfig 配置。
例程还验证同一组已配置并连接的 service 实例可通过 stop/start 连续完成两轮 15 秒推流。

如需使用包含 server、push、pull 三种角色的 CLI 综合示例，请参阅 [`RTSP CLI`](../rtsp_cli/README_CN.md) 例程。

## 环境配置

### 硬件要求

- ESP32-P4 Function EV Board 或 ESP32-S3-Korvo-2 V3
- 所选开发板提供的摄像头和音频 ADC
- 与开发板位于同一网络、运行 RTSP 服务器的电脑

### 软件要求

本例程支持 ESP-IDF release/v5.4（>= v5.4.3）、release/v5.5（>= v5.5.2）以及 IDF v6.1。

推荐在电脑上运行 [MediaMTX](https://github.com/bluenviron/mediamtx) 作为 RTSP 服务器。
请在电脑上下载后执行：

```bash
./mediamtx
```

MediaMTX 无需额外配置即可在 `8554` 端口接收 RTSP 推流。

## 编译和烧录

激活 ESP-IDF 环境并进入例程目录：

```bash
. $IDF_PATH/export.sh
cd adf_examples/protocols/rtsp_push
```

选择开发板：

```bash
idf.py bmgr -b esp32_p4_function_ev_board
# 或
idf.py bmgr -b esp32_s3_korvo_2_3
```

配置 Wi-Fi 和推流地址：

```bash
idf.py menuconfig
```

在 **RTSP Push Example Configuration** 菜单中设置：

- **WiFi SSID**
- **WiFi Password**
- **RTSP push URL**，例如 `rtsp://192.168.1.10:8554/live`

URL 应填写电脑在局域网中的地址；`127.0.0.1` 指向 ESP 设备自身。

编解码格式、分辨率、帧率、采样率和码率等媒体参数集中定义在 `main/rtsp_push_settings.h` 中。

### 资源优化

RTSP 角色开关位于 **Component config → ESP-RTSP Service**，本基础例程仅保留 `CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT`；`sdkconfig.defaults` 已直接关闭采集解码、全部音视频解码器和未使用的音频编码器。若需针对目标芯片进一步减小固件体积，请进入 **Component config → Video Codec Configuration**，仅保留一种目标芯片支持的 MJPEG `CONFIG_VIDEO_ENCODER_*_SUPPORT` 实现，关闭 H264 以及重复的编码器实现，再执行 `idf.py fullclean` 后重新编译。

编译、烧录并查看串口输出：

```bash
idf.py build
idf.py -p PORT flash monitor
```

## 工作原理

`app_main()` 依次执行六个步骤：

1. 初始化 NVS、媒体适配器、摄像头、麦克风、编码器和 Wi-Fi。
2. 创建并配置 capture 和 RTSP service，包括媒体参数、目标 URL 和本地 IP 地址。
3. 将 capture source 与 RTSP sink 连接一次。
4. 先启动 RTSP sink，再启动 capture source。
5. 运行 15 秒后，先停止 capture source，再停止 RTSP sink；不重新创建、配置或连接，直接重启同一组实例，再推流 15 秒并再次停止。
6. 解除连接并反初始化两个 service。

启动成功后，设备会打印当前推流地址。
在电脑的另一个终端中播放：

```bash
ffplay -rtsp_transport udp rtsp://127.0.0.1:8554/live
```

第一次停止会断开播放器，请在第二轮推流开始后重新连接。
用于产品时，应将固定延时替换为应用自身的控制逻辑；如需持续推流，应保留第一次启动并删除定时停止、重启、第二次停止和最终清理流程。

## 故障排除

- Wi-Fi 连接失败时，检查 menuconfig 中的 SSID 和密码。
- RTSP 启动失败时，确认 MediaMTX 已运行，且 URL 使用电脑可访问的局域网地址。
- 无视频时，检查摄像头连接和开发板选择。
- 电脑无法播放时，检查主机防火墙是否允许 MediaMTX 和 UDP 流量。

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈：[esp-adf issues](https://github.com/espressif/esp-adf/issues)
