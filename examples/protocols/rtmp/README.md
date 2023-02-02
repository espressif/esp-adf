# RTMP
- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

RTMP protocol is widely used in live streaming, realising one-to-many live broadcast support through server forwarding. This example demonstrates how to implement common RTMP scenarios such as Pusher, Puller, and Server Application on the ESP32 series boards, as well as the basic usage of the `esp_rtmp` module. 
 * [Pusher Application](./main/rtmp_push_app.c)  It gathers video and audio data from the camera and microphone, and then sends it to RTMP Server. It supports the configuration of video resolution, video frame rate, audio format, audio sample rate, and audio channel.
 * [Puller Application](./main/rtmp_src_app.c) It fetches audio and video data from the RTMP server and stores them into SDCard with FLV format. It is suitable for offline playback or extends to playback on board afterwards.
 * [Server Application](./main/rtmp_server_app.c) It sets up a RTMP server on board. It supports to connect into multiple clients and broadcast stream data from Pusher to Puller.  

Pusher Application and Server Application can be run on board at the same time in order to set up a local live streaming server.  
Detailed codec support for `esp_rtmp` is as follows:

|       |esp_rtmp Supported|Standard RTMP Supported|Notes|
| :-----| :---- | :---- | :---- |
|PCM  |Y|Y||
|G711 alaw  |Y|Y||
|G711 ulaw  |Y|Y||
|AAC  |Y|Y||
|MP3  |Y|Y||
|H264 |Y|Y||
|MJPEG|Y|N|Use CodecID 1 for MJPEG|

To play RTMP living stream or saved FLV file on a PC, please refer to [Example Functionality](#example-functionality).

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

The camera model tested in this example is OV3660. For other models, please refer to [esp32-camera](https://github.com/espressif/esp32-camera).

## Build and Flash


### Default IDF Branch

This example supports IDF release/v4.4 and later branches. By default, it runs on IDF release/v4.4.

### IDF Branch

- The command to switch to IDF release/v4.4 branch is as follows:

  ```
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout release/v4.4
  git submodule update --init --recursive
  ```

### Configuration

The default board for this example is `ESP32-S3-Korvo-2`.

Choose RTMP Application work mode:
```
menuconfig > RTMP APP Configuration > Choose RTMP APP Work Mode
```
Configuration for Wireless network SSID and Password:
```
menuconfig > RTMP APP Configuration > `WiFi SSID` and `WiFi Password`
```
Configuration for RTMP server connection URI address:
```
menuconfig > RTMP APP Configuration > RTMP Server URI
```
If you are using a USB camera, please enable the following configuration, and change USB setting in [record_src_cfg.h](../components/av_record/record_src_cfg.h)
```
 menuconfig > AV Record Configuration > Support For USB Camera
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example

### Example Functionality

  * Set up RTMP server on PC  
	If you are using an Ubuntu system, please refer to [nginx RTMP server setting](https://www.digitalocean.com/community/tutorials/how-to-set-up-a-video-streaming-server-using-nginx-rtmp-on-ubuntu-20-04)
 * Push and Playback RTMP media stream  
   Once the RTMP application is running, you can access and test it with a common RTMP Pusher (`ffmpeg`) or RTMP Puller Software (`ffmpeg, VLC, Pot Player, etc`).  
   Since example uses MJPEG codec which have no official support yet, the video content in generated living stream by RTMP Pusher application and saved FLV file can't be recognized by common PC player. To play it, you can unzip [ffmpeg.7z](../../recorder/av_muxer_sdcard/ffmpeg.7z) to get `ffplay.exe` directly on Windows, or you can modify the source code referring to [ffmpeg_mjpeg.patch](../../recorder/av_muxer_sdcard/ffmpeg_mjpeg.patch) for latest [FFmpeg](https://github.com/FFmpeg/FFmpeg) and follow [FFmpeg Compilation Guide](https://trac.ffmpeg.org/wiki/CompilationGuide) to build. 

* Push RTMP stream to RTMP server on board, using `ffmpeg`  
	Please replace `YOUR_PUSH_FILE` with your push file path, and replace `RTMP_SERVER_IP_ADDRESS` with your board IP address
	```
	ffmpeg -stream_loop -1 -re -i YOUR_PUSH_FILE.mp4 -c copy -f flv  rtmp://RTMP_SERVER_IP_ADDRESS:1935/live/streams
	```
* Using `ffmpeg` to play RTMP stream or recorded FLV file from the board
	```
	ffplay -fflags nobuffer rtmp://RTMP_SERVER_IP_ADDRESS:1935/live/stream
	ffplay src.flv
	```
* CLI settings
    + `help` Query all supported commands
	+ `set` Setup audio and video configuration
		+ `sw_jpeg` Whether using software JPEG encoder 
		+ `quality` Setup video quality, refer to `av_record_video_quality_t`
		+ `fps` Setup video framerate
		+ `channel` Setup audio channel
		+ `sample_rate` Setup audio sample rate
		+ `afmt` Setup audio format, refer to  `av_record_audio_fmt_t`
		+ `vfmt` Setup audio format, refer to  `av_record_video_fmt_t`
		+ `url` Setup RTMP or RTMPS server URL
	  ```
	  Configuration example:
	  set vfmt 1 sw_jpeg 0 quality 2 fps 25 afmt 1 channel 1 sample_rate 22050
	  ```
	+ `start` Start to run RTMP applications
	+ `stop` Stop the running applications
	+ `i` Query current CPU loading and memory usage
	+ `connect` Connect to wifi using ssid and password
* Performance measured
  + Maximum video framerate when using camera encoded JPEG
	|Video Resolution|Max FPS|
	| :-----| :---- |
	|1280 X 720|17|
	|1024 X 768|27|
	|640 X 480|27|
	|480 X 320|32|
	|320 X 240|27|

  + Maximum video framerate when software encoded JPEG
	|Video Resolution|Max FPS|
	| :-----| :---- |
	|640 X 480|11|
	|480 X 320|22|
	|320 X 240|22|
  + Maximum video framerate when software encoded H264
	|Video Resolution|Max FPS|
	| :-----| :---- |
	|320 X 240|10|

### How to push stream to Youtube
* Get RTMP or RTMPS stream URL from Youtube  
   + Login in to [Youtube](https://www.youtube.com/)
   + Click the camera icon in the top-right corner
   + Click `Go Live` in the dropdown menu, you will see following page  
	 ![alt text](stream_url.jpg "stream_url.jpg")
   + Combine the Stream URL and Stream key together to form RTMP or RTMPS URL
	 ```  
	 rtmp://a.rtmp.youtube.com/live2/************
	 rtmps://a.rtmp.youtube.com/live2/***********
	 ```
* Streaming to Youtube  
  + Login in to [Youtube](https://www.youtube.com/) and start live stream channel firstly
  + Configuration according [Configuration](#configuration) and send `start` after system boot-up, then you can see the streaming video on Youtube. If you want to stop streaming process, just send `stop` command.
  + Notes: Youtube only support H264 video codec, push MJPEG codec will cause connection fail. Sometimes DNS get wrong IP address, please try URL `rtmp://142.250.179.172/live2/***********` instead.

### Debug Tips
 * Some error log will print out during stopping process, it is normal, please ignore it if system runs OK
 * Prefer to use the default settings so that RAM space is enough
   If meet `MEDIA_OS: Fail to create thread`, please use IDF 4.4.3 or higher version to put thread stack onto SPI-RAM or else remove the condition `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` in [media_lib_os_freertos.c](../../../components/esp-adf-libs/media_lib_sal/port/media_lib_os_freertos.c)
 * If you meet system not response issue, sometimes it is network issue please wait for a while. If it doesn't response for long time, please enable following config, when issue happen send `assert` command to use GDB to check the overall system status (command: thread apply all bt)
   ```
   menuconfig > ESP System Settings > GDBStub on panic
   ```
 * If you are using OV3660, sometimes camera send error log: `cam_hal: FB-SIZE: 138240 != 153600`, it is sensor error, using OV2640 no such error log shown.
 * If you meet error log  `RTMP_Connect: Remove fd 57 for timeout:74 size:346710` when you are sending out high quality video from RTMP server. It means client doesn't read data until fifo exceed limit. You can either enlarge the fifo limit `RTMP_SERVER_CLIENT_CACHE_SIZE` in [rtmp_server_app.c](main/rtmp_server_app.c) or send lower quality video.
 * All RTMP application will continue to run until timeout. You can change timeout setting `RTMP_APP_RUN_DURATION` in [main.c](main/main.c) to do long test.
 * To have better performance, AAC encoder input is limited to be 16k, 1 channel. You can remove the limit in [rtmp_push_app.c](main/rtmp_push_app.c).

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
