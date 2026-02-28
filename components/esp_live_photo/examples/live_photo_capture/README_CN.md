# Live Photo 采集示例

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示在乐鑫 SoC 上采集一段短视频，自动选取封面帧，封装为 Motion Photo 风格文件（JPEG 封面 + 内嵌带音频的 MP4），写入 SD 卡，并通过解析文件完成输出校验。

### 典型场景

- 智能摄像头与智能门铃
- AI 抓拍与 IoT 图像设备
- 需要「静态封面 + 短时视频」单文件输出的嵌入式媒体记录设备

### 技术说明

本例程演示使用 `esp_capture` 同步采集音视频以及对封面图片评分、`esp_live_photo_muxer` 完成 Motion Photo 封装，以及 `esp_live_photo_extractor` 进行解析校验；板级外设通过 `esp_board_manager` 完成初始化。

### 运行机制

采集流程：

- 采集 → 评分并选取最佳帧 → 编码封面 JPEG → 将 JPEG 与 MP4（视频 + 音频）封装 → 写入 `/sdcard/live_photo.jpg`

校验流程：

- 打开 `/sdcard/live_photo.jpg` → 使用 Live Photo 解析器读取并打印统计信息。

## 环境配置

### 硬件要求

- 由 `esp_board_manager` 支持的乐鑫开发板，需具备：
  - 摄像头
  - 音频 ADC / 麦克风通路
  - 以 FatFs 文件系统挂载的 microSD 卡

建议使用 [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html)。

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）与 release/v5.5（>= v5.5.2）分支。

### 软件要求

- 首次编译时会根据 `idf_component.yml` 自动拉取托管组件（需联网）。

## 编译和下载

### 编译准备

编译前请确认已配置 ESP-IDF 环境。若尚未配置，请在 ESP-IDF 根目录执行以下命令（完整步骤见 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/index.html)）。

```
./install.sh
. ./export.sh
```

进入本例程目录：

```
cd <path-to-esp-adf>/components/esp_live_photo/examples/live_photo_capture
```

使用板级管理列出板型并选择与实际硬件匹配的配置：

```bash
idf.py gen-bmgr-config -l
idf.py gen-bmgr-config -b esp32_p4_function_ev
```

请将 `esp32_p4_function_ev` 替换为实际目标板 ID。

### 项目配置

在 `main/settings.h` 中调整采集参数（优于此处频繁修改 menuconfig）：

- 视频：`VIDEO_WIDTH`、`VIDEO_HEIGHT`、`VIDEO_FPS`、`VIDEO_FORMAT`
- 音频：`AUDIO_FORMAT`、`AUDIO_CHANNEL`、`AUDIO_SAMPLE_RATE`
- 片段时长：`LIVE_PHOTO_DURATION_MS`
- JPEG 缓冲上限：`MAX_JPEG_SIZE`

### 编译与烧录

- 编译与烧录：

```
idf.py -p PORT flash monitor
```

- 退出串口监视器：`Ctrl-]`

## 如何使用例程

### 功能和用法

- 上电后，应用通过 board manager 初始化摄像头、音频 ADC 与 SD 卡，依次执行采集、封装到 `/sdcard/live_photo.jpg` 以及校验。
- 封面选取采用简单评分策略（例如优先中段帧；在摄像头统计信息可用时可结合亮度、运动、清晰度类信号）。可在 `live_photo_capture.c` 中继续优化逻辑。
- 成功时日志中会打印 `Live photo capture and verify success`。

### 日志输出

除上述成功提示外，还可关注标签 `LIVE_PHOTO_CAPTURE`、`LIVE_PHOTO_VERIFY`、`LIVE_PHOTO_MAIN` 的串口输出，用于排查采集、封装与校验各阶段状态。

## 查看 Live Photo

将 `live_photo.jpg` 从 SD 卡复制到电脑后，使用支持 Motion Photo / JPEG 内嵌 MP4 的查看器打开（例如 **Windows 11** 自带的「照片」应用），或将文件拖入 [Motion Photo Viewer](https://dj0001.github.io/Motion-Photo-Viewer/) 网页，即可在浏览器中预览。

## 故障排除

### SD 卡或设备初始化失败

若日志出现 `Skip muxer for mount sdcard failed`，请确认 SD 卡已格式化为 FAT、安装到位，且板级配置已启用 SD 卡设备。请确认摄像头与音频 ADC 初始化成功后再进行封装。

### 采集或封装错误

若出现 `Failed to create video source`、`Fail to create capture` 或 `Failed to create muxer` 等错误，多为板级包与硬件不匹配，或 `settings.h` 中的视频/音频流水线配置与板级编码路径不兼容。请对照芯片上 `esp_capture` 的默认能力检查 `VIDEO_FORMAT`、`AUDIO_FORMAT` 等参数。

### 校验失败

若出现 `Live photo verify failed`，表示解析器无法正确解析文件或读取轨道。请确认封装步骤已完整执行，且 `/sdcard/live_photo.jpg` 未因磁盘满或掉电等原因被截断。

## 技术支持

- 技术讨论：[esp32.com 论坛](https://esp32.com/viewforum.php?f=20)
- 问题反馈：[ESP-ADF GitHub Issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
