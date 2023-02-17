# RTMP 测试例程
- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

RTMP 广泛应用于直播领域，通过服务器转发实现一对多的直播支持。本例程展示了如何在开发板上实现推流、拉流、服务器应用等 RTMP 的常用场景，以及 `esp_rtmp` 的基本用法。 
* [推流应用](./main/rtmp_push_app.c) 将从麦克风和摄像头采集到的音视频数据发送给 RTMP 服务器。应用支持对视频分辨率、是否采用软件编码、视频帧率、音频采样率、音频声道、音频格式等进行配置。
* [拉流应用](./main/rtmp_src_app.c) 将从 RTMP 服务器获取到的音视频数据存储到 FLV 格式的文件中，方便离线浏览或者后续扩展为本地播放。 
* [服务器应用](./main/rtmp_server_app.c) 将在本地搭建 RTMP 服务器，处理客户端的连接请求，将推流发送的媒体数据广播给拉流客户端。

其中，推流应用和服务器应用可以同时运行，用以搭建本地直播服务器。   
`esp_rtmp` 支持的音视频编码格式见下表：

|       |esp_rtmp 支持|标准协议支持|说明|
| :-----| :---- | :---- | :---- |
|PCM  |Y|Y||
|G711 alaw  |Y|Y||
|G711 ulaw  |Y|Y||
|AAC  |Y|Y||
|MP3  |Y|Y||
|H264 |Y|Y||
|MJPEG|Y|N|使用 CodecID 1 传输 MJPEG |

如需在 PC 上播放 RTMP 直播流或者存储的 FLV 文件，请参考 [功能和用法](#功能和用法)。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

本例程测试的摄像头型号为 OV3660，其他型号请参考 [esp32-camera](https://github.com/espressif/esp32-camera)。

## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 IDF release/v4.4 分支。

### IDF 分支

IDF release/v4.4 分支切换命令如下：

  ```
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout release/v4.4
  git submodule update --init --recursive
  ```

### 配置

本例程默认选择的开发板是 `ESP32-S3-Korvo-2`。
选择 RTMP 应用工作模式：
```
menuconfig > RTMP APP Configuration > Choose RTMP APP Work Mode
```
配置无线网络名称以及密码：
```
 menuconfig > RTMP APP Configuration > `WiFi SSID` and `WiFi Password`
```
配置 RTMP 服务器连接地址：
```
 menuconfig > RTMP APP Configuration > RTMP Server URI
```
如需使用 USB 摄像头，请打开以下配置，并修改 USB 对应配置 [record_src_cfg.h](../components/av_record/record_src_cfg.h)
```
 menuconfig > AV Record Configuration > Support For USB Camera
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

### 功能和用法

* 电脑端 RTMP 服务器搭建  
	Ubuntu 系统请参见 [nginx RTMP 服务器配置](https://www.digitalocean.com/community/tutorials/how-to-set-up-a-video-streaming-server-using-nginx-rtmp-on-ubuntu-20-04)
* RTMP 媒体流推送和播放  
	RTMP 应用运行后，可以用常用的 RTMP 推流 (`ffmpeg`) 或者拉流软件（`ffmpeg、VLC、Pot Player 等`）进行接入测试。  
	由于例程使用了官方尚未支持的 MJPEG 视频编码，RTMP 推流应用生成的实时流以及拉流录制的 FLV 文件无法在 PC 上解析出视频。如需播放，Windows 平台可以直接解压 [ffmpeg.7z](../../recorder/av_muxer_sdcard/ffmpeg.7z)，使用编译好的 `ffplay.exe`。针对其他平台，请导入 [ffmpeg_mjpeg.patch](../../recorder/av_muxer_sdcard/ffmpeg_mjpeg.patch) 修改 [FFmpeg](https://github.com/FFmpeg/FFmpeg) 源码，请参考 [FFmpeg 官方编译文档](https://trac.ffmpeg.org/wiki/CompilationGuide) 进行编译。

* 使用 `ffmpeg` 推流到开发板服务器应用：  
	请修改 YOUR_PUSH_FILE 为需要推送的文件路径，RTMP_SERVER_IP_ADDRESS 为开发板的 IP 地址
	```
	ffmpeg -stream_loop -1 -re -i YOUR_PUSH_FILE.mp4 -c copy -f flv  rtmp://RTMP_SERVER_IP_ADDRESS:1935/live/streams
	```

* 使用 `ffmpeg` 播放开发板推送的直播流或者拉流录制的 FLV 文件：  
	```
	ffplay -fflags nobuffer rtmp://RTMP_SERVER_IP_ADDRESS:1935/live/stream
	ffplay src.flv
	```
* CLI 设定
   + `help` 检索支持的命令列表
   + `set` 设定音视频信息
     + `sw_jpeg` 设定视频是否采用软件 JPEG 编码 
     + `quality` 设定视频分辨率，参考 `av_record_video_quality_t`
     + `fps` 设定视频帧率
     + `channel` 设定音频声道数
     + `sample_rate` 设定音频采样率
     + `afmt` 设定音频格式，参考 `av_record_audio_fmt_t`
     + `vfmt` 设定音频格式，参考 `av_record_video_fmt_t`
     + `url` 设定 RTMP 或者 RTMPS 服务器地址
	  ```
	  配置实例：
	  set vfmt 1 sw_jpeg 0 quality 2 fps 25 afmt 1 channel 1 sample_rate 22050
	  ```
   + `start` 命令，启动配置的应用程序
   + `stop` 命令，停止应用程序运行
   + `i` 命令，获取当前 CPU 占用率和内存占用情况
   + `connect` 命令，指定 SSID 和密码来连接到 WIFI
* 基准性能
  + 摄像头编码 JPEG 最高帧率
	|分辨率|最高帧率|
	| :-----| :---- |
	|1280 X 720|17|
	|1024 X 768|27|
	|640 X 480|27|
	|480 X 320|32|
	|320 X 240|27|

  + 软件编码 JPEG 最高帧率
	|分辨率|最高帧率|
	| :-----| :---- |
	|640 X 480|11|
	|480 X 320|22|
	|320 X 240|22|
  + 软件编码 H264 最高帧率
	|分辨率|最高帧率|
	| :-----| :---- |
	|320 X 240|10|

### 推流到 Youtube
* 从 Youtube 获取 RTMP，RTMPS 推流地址  
   + 登入 [Youtube](https://www.youtube.com/)
   + 点击右上角的摄像头图标r
   + 在下拉菜单中选择 `进行直播`, 你将看到下述页面  
	 ![alt text](stream_url.jpg "stream_url.jpg")
   + 将 Stream URL 和 Stream Key 拼接起来形成 RTMP，RTMPS 推流地址
	 ```  
	 rtmp://a.rtmp.youtube.com/live2/************
	 rtmps://a.rtmp.youtube.com/live2/***********
	 ```
* 推流到 Youtube  
  + 登入 [Youtube](https://www.youtube.com/) 并开启直播频道
  + 参考 [配置](#配置) 进行设定，平台开机后发送 `start` 命令, 即可在 Youtube 上看到推送的视频。如果你想停止推送视频流，可发送 `stop` 命令.
  + 注意: Youtube 仅支持 H264 视频编码, 推送 MJPEG 编码格式将导致连接失败。 有些时候 DNS 会解析到不可用的 IP 地址, 可以尝试使用下面的 URL `rtmp://142.250.179.172/live2/***********`。

### 调试指南
 * 停止应用过程中会打印一些错误信息，在系统运行正常的情况下可以予以忽略
 * 请优先使用默认配置，以便 RAM 空间够用
   如果遇到 `MEDIA_OS: Fail to create thread` 的错误， 请选择用 IDF 4.4.3 或者更高的版本来将任务的堆栈空间放到外部 SPI-RAM，或者移除 [media_lib_os_freertos.c](../../../components/esp-adf-libs/media_lib_sal/port/media_lib_os_freertos.c) 文件中 `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` 的限定  
 * 当遇到系统不响应时, 有时是网络原因导致的，可以等待一会。如果长时间不响应可以开启如下配置，在系统卡住时发送 `assert` 命令用 GDB 检查整个系统的状态 (命令: thread apply all bt)
   ```
   menuconfig > ESP System Settings > GDBStub on panic
   ```
 * 使用 OV3660 摄像头时，有时会遇到: `cam_hal: FB-SIZE: 138240 != 153600`， 这是摄像头传感器相关的问题，可以换作 OV2640 或者其他摄像头来测试
 * 遇到 `RTMP_Connect: Remove fd 57 for timeout:74 size:346710` 的错误输出，尤其在 RTMP 服务器发送高码率视频时容易出现， 意味着客户端在 FIFO 超过设定上限还没及时读走。可以尝试加大 [rtmp_server_app.c](main/rtmp_server_app.c) 中 FIFO 的上限设定 `RTMP_SERVER_CLIENT_CACHE_SIZE` 或者降低视频码率
 * RTMP 应用在运行超时后自动停止， 可以通过修改 [main.c](main/main.c) 中 `RTMP_APP_RUN_DURATION` 来改变运行时间
 * 为了提高性能， AAC 编码输入被限制为 16K，单声道，可以从 [rtmp_push_app.c](main/rtmp_push_app.c) 中移除这个限制
  
## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
