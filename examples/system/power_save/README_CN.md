# Wi-Fi Power Save 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程演示了在 ADF 框架下实现 Wi-Fi 低功耗（Auto Light-sleep 模式）与 Deep-sleep 模式下唤醒源的实际使用。如果仅测试 Wi-Fi 低功耗下的平均电流，则只需将宏 `WIFI_POWER_SAVE_TEST` 设置为 1 即可，否则执行的是 Wi-Fi 低功耗模式以及 Deep-sleep 模式下的唤醒流程。Wi-Fi 低功耗模式下唤醒源较多，本例程只对 GPIO、UART、RTC Timer 唤醒源的使用进行介绍，并在唤醒源测试结束后，通过注册 GPIO/EXT 唤醒源实现在 Deep-sleep 模式下的唤醒。一般情况下，应用程序需要在进入低功耗模式前注册唤醒源，等待 GPIO、UART 等唤醒事件到达，成功唤醒后通过获取电源管理锁阻止系统自动进入低功耗模式，从而不影响用户程序的正常执行。


### 预备知识

Auto Light-sleep 是在 Light-sleep 模式基础上，通过 FreeRTOS idle task 实现自动进入低功耗的一种方式。具体细节详见：  
`portTASK_FUNCTION() --> portSUPPRESS_TICKS_AND_SLEEP() --> vApplicationSleep() --> enable timer wakeup and enter sleep`

对应系统不同工作状态（Wi-Fi TX，Wi-Fi RX，Modem-sleep (RF off)，Light-sleep，Deep-sleep），芯片主要分为 5 个电流级别，其典型值范围如下表所示（不同芯片略有差异，Wi-Fi TX 电流和发射功率相关，发射功率越大，理论上 TX 电流越高），通过电流波形可有效区分芯片所处的状态。当处于 Deep-sleep 和 Light-sleep 模式时电流较小，需借助较为精密的电流检测设备才能进行区分。在 Wi-Fi 低功耗模式下，典型电流波形从低到高依次为 Light-sleep、Modem-sleep、Wi-Fi RX、Wi-Fi TX，通过电流表观察到四种电流阶梯则可识别其所处模式。

| 模式  | Deep-sleep | Light-sleep | Modem-sleep | Wi-Fi RX  | Wi-Fi TX   |
| ----- | ---------- | ----------- | ----------- | --------- | ---------- |
| 电流  | 5-20 uA    | 100-800 uA  | 15-25 mA    | 70-110 mA | 200-300 mA |

睡眠模式及唤醒源：
[睡眠模式](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-reference/system/sleep_modes.html)，
[技术参考手册](https://www.espressif.com.cn/sites/default/files/documentation/esp32-s3_technical_reference_manual_cn.pdf)，
[Deep-sleep 唤醒桩](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-guides/deep-sleep-stub.html)

低功耗管理：
[电源管理](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-reference/system/power_management.html)，
[FreeRTOS 低功耗支持](https://freertos.org/zh-cn-cmn-s/low-power-tickless-rtos.html)

Wi-Fi 相关知识：
[Wi-Fi 常见问题解答](https://docs.espressif.com/projects/espressif-esp-faq/zh_CN/latest/software-framework/wifi.html#)，
[Wi-Fi 驱动程序](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/wifi.html)

参考例程：
[Wi-Fi 低功耗管理](https://github.com/espressif/esp-idf/tree/master/examples/wifi/power_save)，
[Light-sleep 睡眠及唤醒](https://github.com/espressif/esp-idf/tree/master/examples/system/light_sleep)，
[Deep-sleep 睡眠及唤醒](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep)，
[Deep-sleep 唤醒桩](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep_wake_stub)，
[芯片启动时间优化](https://github.com/espressif/esp-idf/tree/master/examples/system/startup_time)


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-S3-Korvo-2 v3`。由于 HTTP task 堆栈默认分配在 PSRAM 中，因此需要在 `$ADF_PATH/esp-idf` 目录下应用 `idf_v4.4_freertos.patch`。如果选择 `ESP32-C3-Lyra-v2.0`，为获得更好的音质效果，还需要在 `$ADF_PATH/esp-idf` 目录下应用 `idf_v4.4_i2s_c3_pdm_tx.patch`。选择开发板后也需要配置网络名称和密码以实现网络连接。

```
menuconfig > Audio HAL > ESP32-S3-Korvo-2 v3
menuconfig > Example Configuration > WiFi SSID
menuconfig > Example Configuration > WiFi Password
```

**注意：**

1. Wi-Fi 进入低功耗模式时，如果配置的是 MAX modem sleep 模式，则使用 listen interval 参数作为唤醒周期，如果配置的为 MIN modem sleep，则使用 AP DTIM 周期作为唤醒周期。为保证收包的及时与准确，需在外部电路增加 32.768 khz 晶体/晶振，如果使用内部 RC 振荡器，可能导致收包唤醒不准确，引起多次唤醒收包，增加功耗。若硬件不具备外部 32.768 khz 晶体/晶振，却将 `RTC clock source` 设置为 `External 32kHz crystal`，则系统运行时会自动切换到内部 RC 振荡器，对于 ESP32S3 来说，这种方式会引起 I2S 时钟输出异常，menuconfig 中的配置与实际硬件相匹配则不会出现该问题。

2. Wi-Fi 处于低功耗模式时，其功耗主要来源于以下几个方面：Light-sleep 模式底电流、Wi-Fi 唤醒周期、Beacon 数据包大小、数据帧收发频率、其他任务的能量消耗。  
如 ESP32-S3 模组，Light-sleep 底电流典型值为 250 uA，（MIN Modem-sleep 模式下 DTIM10 或 MAX Modem-sleep 模式下 listen interval 为 10）唤醒周期为 1.024s，收包持续时长约 3 ms（beacon 包大小约 300 字节），LWIP 中 `ARP_TMR_INTERVAL` 与 `DHCP_COARSE_TIMER_MSECS` 定时器唤醒周期为 1 s，可手动改为 10 s。

3. 进入 Deep-sleep 模式时，若配置 GPIO/EXT0/EXT1 唤醒源，推荐外接上拉或者下拉电阻，电阻阻值需进行测试，否则功耗较高或者唤醒不稳定。为确保 Deep-sleep 模式下 GPIO 输出电平不发生改变（如保持高电平），需调用 `gpio_hold_en()` 和 `gpio_deep_sleep_hold_en()`。

4. ESP32S3 默认使用 8 线 PSRAM，若下载后无法正常执行，则考虑重新配置 PSRAM：  
`Component config --> ESP32S3-Specific --> Support for external, SPI-connected RAM --> SPI RAM config --> Mode (QUAD/OCT) of SPI RAM chip in use`。( QUAD 表示 四线，OCT 表示 八线)

5. 为保证 ESP32C3、ESP32、ESP32S2、ESP32S3 四款芯片都能正常播放 HTTP 音频数据流并实现 Wi-Fi 低功耗及唤醒的功能，例程默认关闭 `WiFi IRAM speed optimization` 与 `WiFi RX IRAM speed optimization` 以减少 IRAM 的使用，但会对 Wi-Fi 性能和功耗造成一定影响。具体参考 menuconfig 中参数的具体说明，实际应用时可根据项目需要决定是否关闭该选项。

6. 部分音频开发板引脚设计并未考虑到低功耗需求，开发板 `ESP32-S3-Korvo-2 v3` 与 `ESP32-S2-Kaluga-1` 在 Deep-sleep 模式下使用 BOOT 引脚进行唤醒时会自动触发唤醒，而使用对应核心板则能正常唤醒。实际使用参见 [Deep-sleep 睡眠模式](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep)。


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32s3/index.html)。

## 如何使用例程

### 功能和用法

例程开始运行后，将循环执行如下流程：  
系统初始化（如 Wi-Fi、codec 等） ---> 系统正常运行（如通过 HTTP 获取音频数据并进行播放） ---> 终止非 Wi-Fi 相关的任务（如音频解码、播放等任务） ---> 使能 Light-sleep 唤醒源，进入 Wi-Fi 低功耗模式，等待唤醒源的到来。  
注意：
* 进入低功耗后，若长时间不进行唤醒，则默认在 20 秒后由 timer 唤醒源进行唤醒。
* 若使用按键唤醒，则按下 BOOT 键即可。
* 若使用 uart 进行唤醒，则通过 console(uart0) 发送 `123456789ABCDE\r\n` 或任意更长字符即可。

上述唤醒流程达到 4 次后，将进入 Deep-sleep 模式，并根据需要使能唤醒源，默认使用 EXT0 唤醒源，用户可通过 BOOT 按键进行唤醒。
由于 ESP32C3 不支持 EXT0/EXT1 唤醒源，因此改用 GPIO 唤醒源，可通过 `ESP32-C3-Lyra-v2.0` 开发板 'VOL+' 按键进行唤醒，不可使用 BOOT 按键。  
关键日志打印如下：

```c
I (1471) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1471) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23299) AUDIO_ELEMENT: [http] Element already stopped
E (23300) AUDIO_ELEMENT: [mp3] Element already stopped
E (23300) AUDIO_ELEMENT: [i2s] Element already stopped
I (23312) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23312) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23318) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (25306) AUDIO_SLEEP_WAKEUP: Send uart wakeup group event
W (25306) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: UART
I (25316) AUDIO_SLEEP_WAKEUP: Uart0 recved event:0
I (25316) AUDIO_SLEEP_WAKEUP: [UART DATA]: 7
▒ (25318) AUDIO_SLEEP_WAKEUP: [DATA EVT]: ▒▒
E (46159) AUDIO_ELEMENT: [http] Element already stopped
E (46159) AUDIO_ELEMENT: [mp3] Element already stopped
E (46160) AUDIO_ELEMENT: [i2s] Element already stopped
I (46176) AUDIO_SLEEP_WAKEUP: Enter power manage
I (46176) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (46178) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (49532) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (49535) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
E (70532) AUDIO_ELEMENT: [http] Element already stopped
E (70533) AUDIO_ELEMENT: [mp3] Element already stopped
E (70533) AUDIO_ELEMENT: [i2s] Element already stopped
I (70550) AUDIO_SLEEP_WAKEUP: Enter power manage
I (70550) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (70551) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (90669) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: TIMER
E (109624) AUDIO_ELEMENT: [http] Element already stopped
E (109625) AUDIO_ELEMENT: [mp3] Element already stopped
E (109626) AUDIO_ELEMENT: [i2s] Element already stopped
I (109721) AUDIO_SLEEP_WAKEUP: Enter power manage
I (109722) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (109722) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (112743) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (112743) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
I (117754) AUDIO_SLEEP_WAKEUP: Enable EXT0 wakeup in deep sleep
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x5 (DSLEEP),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950

```


### 日志输出

以下为本例程的完整日志。

```c
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950
I (25) boot: ESP-IDF v4.4.3-316-ge86181704a-dirty 2nd stage bootloader
I (25) boot: compile time 19:15:31
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 4MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (74) boot:  2 factory          factory app      00 00 00010000 00200000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c0b0020 size=26120h (155936) map
I (122) esp_image: segment 1: paddr=00036148 vaddr=3fc9ade0 size=048b4h ( 18612) load
I (126) esp_image: segment 2: paddr=0003aa04 vaddr=40374000 size=05614h ( 22036) load
I (132) esp_image: segment 3: paddr=00040020 vaddr=42000020 size=a1b00h (662272) map
I (255) esp_image: segment 4: paddr=000e1b28 vaddr=40379614 size=117c4h ( 71620) load
I (271) esp_image: segment 5: paddr=000f32f4 vaddr=600fe000 size=0002ch (    44) load
I (280) boot: Loaded app from partition at offset 0x10000
I (280) boot: Disabling RNG early entropy source...
I (292) opi psram: vendor id : 0x0d (AP)
I (292) opi psram: dev id    : 0x02 (generation 3)
I (292) opi psram: density   : 0x03 (64 Mbit)
I (296) opi psram: good-die  : 0x01 (Pass)
I (300) opi psram: Latency   : 0x01 (Fixed)
I (305) opi psram: VCC       : 0x01 (3V)
I (310) opi psram: SRF       : 0x01 (Fast Refresh)
I (315) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (321) opi psram: BurstLen  : 0x01 (32 Byte)
I (326) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (332) opi psram: DriveStrength: 0x00 (1/1)
I (337) spiram: Found 64MBit SPI RAM device
I (342) spiram: SPI RAM mode: sram 40m
I (346) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (353) cpu_start: Pro cpu up.
I (357) cpu_start: Starting app cpu, entry point is 0x40375764
0x40375764: call_start_cpu1 at /home/dailei/workspace/internal/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (1094) spiram: SPI SRAM memory test OK
W (1305) clk: 32 kHz XTAL not found, switching to internal 150 kHz oscillator
I (1313) cpu_start: Pro cpu start user code
I (1313) cpu_start: cpu freq: 240000000
I (1313) cpu_start: Application information:
I (1316) cpu_start: Project name:     power_save
I (1322) cpu_start: App version:      v2.4-179-g6dd42bac-dirty
I (1328) cpu_start: Compile time:     Jan 11 2023 19:15:27
I (1334) cpu_start: ELF file SHA256:  f01e0c851ff0486d...
I (1340) cpu_start: ESP-IDF:          v4.4.3-316-ge86181704a-dirty
I (1347) heap_init: Initializing. RAM available for dynamic allocation:
I (1354) heap_init: At 3FCA30D0 len 00046640 (281 KiB): D/IRAM
I (1361) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (1368) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1374) heap_init: At 600FE02C len 00001FD4 (7 KiB): RTCRAM
I (1380) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1389) spi_flash: detected chip: gd
I (1393) spi_flash: flash io: dio
W (1397) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1421) sleep: Configure to isolate all GPIO pins in sleep state
I (1422) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1432) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
W (1441) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: Undefined 0x1
I (1471) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1471) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23299) AUDIO_ELEMENT: [http] Element already stopped
E (23300) AUDIO_ELEMENT: [mp3] Element already stopped
E (23300) AUDIO_ELEMENT: [i2s] Element already stopped
I (23312) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23312) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23318) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (25306) AUDIO_SLEEP_WAKEUP: Send uart wakeup group event
W (25306) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: UART
I (25316) AUDIO_SLEEP_WAKEUP: Uart0 recved event:0
I (25316) AUDIO_SLEEP_WAKEUP: [UART DATA]: 7
▒ (25318) AUDIO_SLEEP_WAKEUP: [DATA EVT]: ▒▒
E (46159) AUDIO_ELEMENT: [http] Element already stopped
E (46159) AUDIO_ELEMENT: [mp3] Element already stopped
E (46160) AUDIO_ELEMENT: [i2s] Element already stopped
I (46176) AUDIO_SLEEP_WAKEUP: Enter power manage
I (46176) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (46178) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (49532) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (49535) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
E (70532) AUDIO_ELEMENT: [http] Element already stopped
E (70533) AUDIO_ELEMENT: [mp3] Element already stopped
E (70533) AUDIO_ELEMENT: [i2s] Element already stopped
I (70550) AUDIO_SLEEP_WAKEUP: Enter power manage
I (70550) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (70551) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (90669) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: TIMER
E (109624) AUDIO_ELEMENT: [http] Element already stopped
E (109625) AUDIO_ELEMENT: [mp3] Element already stopped
E (109626) AUDIO_ELEMENT: [i2s] Element already stopped
I (109721) AUDIO_SLEEP_WAKEUP: Enter power manage
I (109722) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (109722) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (112743) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (112743) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
I (117754) AUDIO_SLEEP_WAKEUP: Enable EXT0 wakeup in deep sleep
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x5 (DSLEEP),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950
I (24) boot: ESP-IDF v4.4.3-316-ge86181704a-dirty 2nd stage bootloader
I (25) boot: compile time 19:15:31
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 4MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (74) boot:  2 factory          factory app      00 00 00010000 00200000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c0b0020 size=26120h (155936) map
I (122) esp_image: segment 1: paddr=00036148 vaddr=3fc9ade0 size=048b4h ( 18612) load
I (126) esp_image: segment 2: paddr=0003aa04 vaddr=40374000 size=05614h ( 22036) load
I (132) esp_image: segment 3: paddr=00040020 vaddr=42000020 size=a1b00h (662272) map
I (255) esp_image: segment 4: paddr=000e1b28 vaddr=40379614 size=117c4h ( 71620) load
I (271) esp_image: segment 5: paddr=000f32f4 vaddr=600fe000 size=0002ch (    44)
I (280) boot: Loaded app from partition at offset 0x10000
I (280) boot: Disabling RNG early entropy source...
I (292) opi psram: vendor id : 0x0d (AP)
I (292) opi psram: dev id    : 0x02 (generation 3)
I (292) opi psram: density   : 0x03 (64 Mbit)
I (296) opi psram: good-die  : 0x01 (Pass)
I (300) opi psram: Latency   : 0x01 (Fixed)
I (305) opi psram: VCC       : 0x01 (3V)
I (310) opi psram: SRF       : 0x01 (Fast Refresh)
I (315) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (321) opi psram: BurstLen  : 0x01 (32 Byte)
I (326) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (332) opi psram: DriveStrength: 0x00 (1/1)
I (337) spiram: Found 64MBit SPI RAM device
I (342) spiram: SPI RAM mode: sram 40m
I (346) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (353) cpu_start: Pro cpu up.
I (357) cpu_start: Starting app cpu, entry point is 0x40375764
0x40375764: call_start_cpu1 at /home/dailei/workspace/internal/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (1094) spiram: SPI SRAM memory test OK
W (1305) clk: 32 kHz XTAL not found, switching to internal 150 kHz oscillator
I (1313) cpu_start: Pro cpu start user code
I (1313) cpu_start: cpu freq: 240000000
I (1313) cpu_start: Application information:
I (1316) cpu_start: Project name:     power_save
I (1321) cpu_start: App version:      v2.4-179-g6dd42bac-dirty
I (1328) cpu_start: Compile time:     Jan 11 2023 19:15:27
I (1334) cpu_start: ELF file SHA256:  f01e0c851ff0486d...
I (1340) cpu_start: ESP-IDF:          v4.4.3-316-ge86181704a-dirty
I (1347) heap_init: Initializing. RAM available for dynamic allocation:
I (1354) heap_init: At 3FCA30D0 len 00046640 (281 KiB): D/IRAM
I (1361) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (1368) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1374) heap_init: At 600FE02C len 00001FD4 (7 KiB): RTCRAM
I (1380) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1389) spi_flash: detected chip: gd
I (1393) spi_flash: flash io: dio
W (1397) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1421) sleep: Configure to isolate all GPIO pins in sleep state
I (1422) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1432) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
W (1441) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: EXT0
I (1470) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1470) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23074) AUDIO_ELEMENT: [http] Element already stopped
E (23074) AUDIO_ELEMENT: [mp3] Element already stopped
E (23075) AUDIO_ELEMENT: [i2s] Element already stopped
I (23087) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23087) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23093) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready

```


## 故障排除

1. 功耗较高（需通过电流表等查看实时电流进行排查）：
* 睡眠底电流较大则考虑硬件电路设计相关问题，本次测试结果均为模组电流。
* 收包持续时间较长（5-10 ms），则考虑路由器数据包大小问题。可通过网络抓包查看实际数据包大小，或者换个路由器进行测试。
* 多次连续出现间隔 100 ms 收包电流峰值且后续出现持续 100 ms 的收包电流，则考虑网络环境问题，通过增大 `Wifi sleep optimize when beacon lost` 参数提高收包准确度。
* 若周期性出现小电流峰值，则考虑某些任务正在执行，如 LWIP 定时器每秒唤醒一次，排查结束后根据需要选择是否终止该任务。
* 若系统无法睡眠，则考虑是否有任务延迟时间小于参数 `Minimum number of ticks to enter sleep mode for` 设置值。此外，也需检查电源管理锁是否仍未释放，可在 menuconfig 中打开 `Enable profiling counters for PM locks` 选项，在程序中周期性调用 `esp_pm_dump_locks(stdout)` 查看当前各种管理锁占用时间等信息，debug 结束后，需关闭该选项以减小开销。另外也可以打开 `Enable debug tracing of PM using GPIOs` 选项进行 GPIO 调试，详见 [pm_trace.c](https://github.com/espressif/esp-idf/blob/release/v4.4/components/esp_pm/pm_trace.c)。
* 若实际测试与 image 文件夹下有所不同但差别不大，则可能是模组批次问题或是网络问题，请等待一段时间，网络环境良好后再次进行测试。

2. GPIO 唤醒问题：
* 为保证 GPIO 能够成功唤醒系统，需将 GPIO 配置为电平触发模式，并在中断回调函数中及时禁止 GPIO 中断源，防止频繁触发中断。
* `esp_pm_configure()` 调用 `esp_sleep_enable_gpio_switch()` 以减小功耗，这样会导致在进入与退出 Light-sleep 时电平状态发生切换，引起多次触发中断。需在 `esp_pm_configure()` 之后调用 `gpio_sleep_sel_dis()` 保证唤醒前后电平一致，详见 `Disable all GPIO when chip at sleep`。
* 成功唤醒系统后，为不影响后续应用程序的执行，建议将该 GPIO 唤醒源恢复到正常状态，待需要用到时再次注册唤醒源。

3. UART 唤醒问题：
* 为确保 UART 及时唤醒系统，需发送一定数量的字节，在 115200 波特率时，发送 "123456789ABCDE\r\n" 或者更长字节可有效唤醒系统，波特率更高时，则需发送更多字节才能有效唤醒。唤醒后可根据特定协议保证收到数据的可靠性。
* 有时发送一次唤醒字符串无法准确唤醒系统，可连续多次发送唤醒字符，直至系统唤醒为止，双方可通过协议通信的方式实现这一过程。
* 唤醒最好的做法是创建电源管理锁（锁类型视具体业务需求确定）和信号量，在 UART ISR 中释放信号量，在 UART 任务中阻塞式获取信号量。获取到信号量后，立即获取电源管理锁以阻止系统自动进入睡眠状态，然后进行正常的串口数据收发。当需要进入低功耗模式时可以调用 `esp_pm_lock_release()` 释放电源管理锁，系统会在适当时机自动进入睡眠状态。

4. RTC Timer 唤醒问题：  
在 Auto Light-sleep 模式下，idle task 会自动调用 `esp_sleep_enable_timer_wakeup()` 注册定时唤醒，应用程序如果也调用该 API 则会被 idle task 覆盖。用户可通过 `esp_timer_create()` 重新创建定时器唤醒，参数 `skip_unhandled_events` 为 false 时，可实现定时唤醒系统。

5. Light-sleep 模式下串口输出断断续续：  
当使用函数 `esp_light_sleep_start()` 进入 Light-sleep 模式时，UART FIFO 不会被冲刷。与之相反，UART 输出将被暂停，FIFO 中的剩余字符将在 Light-sleep 唤醒后被发送。可在需要发送的位置后增加 `uart_wait_tx_idle_polling()`，以达到发送完成后才进入睡眠的效果。

6. Auto Light-sleep 模式下进入 Deep-sleep 会被定时器自动唤醒：  
Auto Light-sleep 模式下会在 idle task 中使能定时器唤醒源，若在 `esp_deep_sleep_start()` 之前调用 `vTaskDelay()` 或者其他会引起 idle task 运行的 API，则会在进入 deep sleep 后一段时间内唤醒。解决方法之一是在调用 `esp_deep_sleep_start()` 之前重新配置定时器唤醒时间：`esp_sleep_enable_timer_wakeup(portMAX_DELAY)`。

7. 低功耗模式下应用程序运行异常：  
当使能功耗管理后，系统将会在 `max_freq_mhz` 和 `min_freq_mhz` 之间动态切换，任务如果对系统频率（CPU 及 Bus 频率）有要求，则需要创建并维护满足任务需求的电源管理锁，如 `ESP_PM_CPU_FREQ_MAX`。

8. 云端数据是否可以唤醒系统：  
IEEE802.11 协议中有对 power save 的协议规范，现已支持，对于 IP 及以上层次，系统休眠及唤醒接收数据是无感的。

9. 一旦建立 TCP 连接，进入低功耗模式的一段时间内功耗较大：  
使用 HTTP 获取数据流时，当关闭 Wi-Fi 后，TCP 连接处于 wait time 状态，会持续等待 2 MSL (Maximum Segment Lifetime) 时间，默认两分钟，可在 menuconfig 中 `Maximum segment lifetime` 进行配置，一般不建议更改。等待时间结束后，TCP 连接完全关闭，此时功耗达到正常状态。`TCP timer interval` 是使用 TCP 连接后芯片周期性唤醒时间间隔，默认 250ms，增加唤醒间隔可降低功耗，但是不建议更改。

10. 如何加速芯片启动：  
* 如果对芯片启动时间有一定要求，可进行如下配置以减小启动时间，并保证在启动期间无任何警告错误等信息输出。  
`Bootloader config --> Bootloader log verbosity --> No output`  
`Component config --> ESP32S3-Specific --> Support for external, SPI-connected RAM --> SPI RAM config --> Run memory test on SPI RAM initialization` （取消勾选）  
`Component config --> Log output --> Default log verbosity --> Warning`
* 如果还需进一步缩短启动时间，可尝试使用如下配置，但可能会对系统运行有一定影响：  
`Bootloader config --> Skip image validation from power on reset (READ HELP FIRST)`  
`Component config --> ESP32S3-Specific --> Number of cycles for RTC_SLOW_CLK calibration --> 16`


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
