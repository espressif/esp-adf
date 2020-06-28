# Example of ESP_DISPATCHER

When task's stack is located in external memory, flash operation is forbidden in it, this example shows two ways to write and read nvs with `esp_dispatcher` and `nvs_action` in this kind of task.

## How to use example

### Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |

### Configure the project

1. Run `make menuconfig` or `idf.py menuconfig`.
2. Select the board type in `Audio HAL`.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

Or, for CMake based build system (replace PORT with serial port name):

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example output

Here is an example console output.

```
I (0) cpu_start: App cpu up.
I (1197) spiram: SPI SRAM memory test OK
I (1197) heap_init: Initializing. RAM available for dynamic allocation:
I (1197) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1203) heap_init: At 3FFB3780 len 0002C880 (178 KiB): DRAM
I (1210) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1216) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1223) heap_init: At 4008E488 len 00011B78 (70 KiB): IRAM
I (1229) cpu_start: Pro cpu start user code
I (1234) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (249) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (250) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
E (310) DISPATCHER: exe first list: 0x0


I (310) DISPATCHER: dispatcher_event_task is running...
I (310) DISPATCHER_EXAMPLE: Task create

I (310) DISPATCHER_EXAMPLE: Sync invoke begin
I (320) DISPATCHER_EXAMPLE: Write Hello, ESP dispatcher! to NVS ...
I (340) DISPATCHER_EXAMPLE: Operation Done
I (340) DISPATCHER_EXAMPLE: Committing updates in NVS ...
I (340) DISPATCHER_EXAMPLE: Operation Done
I (340) DISPATCHER_EXAMPLE: Sync invoke end with 0

I (350) DISPATCHER_EXAMPLE: Async invoke begin
I (350) DISPATCHER_EXAMPLE: async invoke end

I (350) DISPATCHER_EXAMPLE: Read from NVS ...
I (360) DISPATCHER_EXAMPLE: Operation Done
I (370) DISPATCHER_EXAMPLE: Read result: Hello, ESP dispatcher!
I (2360) DISPATCHER_EXAMPLE: Write and Read NVS with dispatcher and actions.
I (2360) DISPATCHER_EXAMPLE: Open
I (2360) DISPATCHER_EXAMPLE: Set a u8 number
I (2360) DISPATCHER_EXAMPLE: Commit
I (2370) DISPATCHER_EXAMPLE: Read from nvs
I (2370) DISPATCHER_EXAMPLE: Read 171 from nvs
I (2380) DISPATCHER_EXAMPLE: Close
W (2380) DISPATCHER_EXAMPLE: Task exit
```
