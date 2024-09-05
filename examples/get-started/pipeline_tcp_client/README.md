# Play Back MP3 Audio from TCP Stream

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to play back MP3 audio from the online TCP stream with ESP-ADF.

The pipeline of this example is as follows:

```
[tcp_server] ---> tcp_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information first. Go to `menuconfig> Example Configuration` and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

Then, configure the TCP server information (IP address and port number). Go to `menuconfig> Example Configuration` and fill in the`TCP URL` and `TCP PORT`.

```
menuconfig > Example Configuration > (192.168.5.72) TCP URL > (8080) TCP PORT
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example

### Example Functionality

- First of all, run the Python script with python 3.x. Make sure the development board and TCP server are connected to the same Wi-Fi network. Prepare an MP3 audio file, name it `esp32.mp3`, and copy it to the same directory where the Python script is. Below is the log of running the script.

```c
python tcp_server.py
Get the esp32.mp3 size is 1453677
starting listen on ip 192.168.5.72, port 8080
waiting for client to connect

```

- After the example starts running, it initiates connection with the Wi-Fi hotspot. If the connection succeeds, it will try connecting to the TCP server and then obtain the MP3 audio for playback. The log is as follows:

```c
I (0) cpu_start: App cpu up.
I (525) heap_init: Initializing. RAM available for dynamic allocation:
I (532) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (538) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (544) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (551) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (557) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (563) cpu_start: Pro cpu start user code
I (582) spi_flash: detected chip: gd
I (582) spi_flash: flash io: dio
W (583) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (592) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (639) TCP_CLIENT_EXAMPLE: [ 1 ] Start codec chip
I (659) TCP_CLIENT_EXAMPLE: [2.0] Create audio pipeline for playback
I (659) TCP_CLIENT_EXAMPLE: [2.1] Create i2s stream to write data to codec chip
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create mp3 decoder to decode mp3 file
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create tcp client stream to read data
I (689) TCP_CLIENT_EXAMPLE: [2.3] Register all elements to audio pipeline
I (699) TCP_CLIENT_EXAMPLE: [2.4] Link it together tcp-->mp3-->i2s
I (709) TCP_CLIENT_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (709) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (2879) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4639) TCP_CLIENT_EXAMPLE: [ 4 ] Set up  event listener
I (4639) TCP_CLIENT_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4639) TCP_CLIENT_EXAMPLE: [4.2] Listening event from peripherals
I (4649) TCP_CLIENT_EXAMPLE: [ 5 ] Start audio_pipeline
I (4699) TCP_CLIENT_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
E (94929) AUDIO_ELEMENT: IN-[tcp] AEL_STATUS_ERROR_INPUT
E (94929) AUDIO_ELEMENT: [tcp] ERROR_PROCESS, AEL_IO_FAIL
W (94929) AUDIO_ELEMENT: [tcp] audio_element_on_cmd_error,3

```


### Example Log

- A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 11:20:38
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x23954 (145748) map
I (167) esp_image: segment 1: paddr=0x0003397c vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (172) esp_image: segment 2: paddr=0x00036cf0 vaddr=0x40080000 size=0x09328 ( 37672) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (190) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0x9eee8 (650984) map
0x400d0020: _stext at ??:?

I (438) esp_image: segment 4: paddr=0x000def10 vaddr=0x40089328 size=0x0e1b4 ( 57780) load
0x40089328: xPortStartScheduler at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:317

I (476) boot: Loaded app from partition at offset 0x10000
I (476) boot: Disabling RNG early entropy source...
I (477) cpu_start: Pro cpu up.
I (480) cpu_start: Application information:
I (485) cpu_start: Project name:     tcp_client_example
I (491) cpu_start: App version:      v2.2-252-g0cc4fd6d
I (497) cpu_start: Compile time:     Nov 26 2021 15:26:12
I (503) cpu_start: ELF file SHA256:  f0bc8f5381316fac...
I (509) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (515) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (525) heap_init: Initializing. RAM available for dynamic allocation:
I (532) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (538) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (544) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (551) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (557) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (563) cpu_start: Pro cpu start user code
I (582) spi_flash: detected chip: gd
I (582) spi_flash: flash io: dio
W (583) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (592) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (639) TCP_CLIENT_EXAMPLE: [ 1 ] Start codec chip
I (659) TCP_CLIENT_EXAMPLE: [2.0] Create audio pipeline for playback
I (659) TCP_CLIENT_EXAMPLE: [2.1] Create i2s stream to write data to codec chip
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create mp3 decoder to decode mp3 file
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create tcp client stream to read data
I (689) TCP_CLIENT_EXAMPLE: [2.3] Register all elements to audio pipeline
I (699) TCP_CLIENT_EXAMPLE: [2.4] Link it together tcp-->mp3-->i2s
I (709) TCP_CLIENT_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (709) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (2879) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4639) TCP_CLIENT_EXAMPLE: [ 4 ] Set up  event listener
I (4639) TCP_CLIENT_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4639) TCP_CLIENT_EXAMPLE: [4.2] Listening event from peripherals
I (4649) TCP_CLIENT_EXAMPLE: [ 5 ] Start audio_pipeline
I (4699) TCP_CLIENT_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
E (94929) AUDIO_ELEMENT: IN-[tcp] AEL_STATUS_ERROR_INPUT
E (94929) AUDIO_ELEMENT: [tcp] ERROR_PROCESS, AEL_IO_FAIL
W (94929) AUDIO_ELEMENT: [tcp] audio_element_on_cmd_error,3
```

- The log on the TCP server is as follows:

```c
python2 tcp_server.py
Get the esp32.mp3 size is 1453677
starting listen on ip 192.168.5.72, port 8080
waiting for client to connect
total size  [1024/1453677]
total size  [2048/1453677]
total size  [3072/1453677]
total size  [4096/1453677]
total size  [5120/1453677]
total size  [6144/1453677]
total size  [7168/1453677]
total size  [8192/1453677]
total size  [9216/1453677]

...

total size  [1445888/1453677]
total size  [1446912/1453677]
total size  [1447936/1453677]
total size  [1448960/1453677]
total size  [1449984/1453677]
total size  [1451008/1453677]
total size  [1452032/1453677]
total size  [1453056/1453677]
total size  [1453677/1453677]
total size  [1453677/1453677]
get all data for esp32.mp3
close client connect

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
