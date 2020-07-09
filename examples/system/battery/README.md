# Example of `Battery Service`

This example shows the way to configure the `battery_service` and get the voltage report.

## How to use example

### Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

### Configure the project

1. Run `make menuconfig` or `idf.py menuconfig`.
2. Select the board type in `Audio HAL`

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

Here is an example console output. This example create a voltage monitor and battery service, report voltage in two different frequency.

```
I (0) cpu_start: App cpu up.
I (247) heap_init: Initializing. RAM available for dynamic allocation:
I (253) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (259) heap_init: At 3FFB2F00 len 0002D100 (180 KiB): DRAM
I (266) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (272) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (278) heap_init: At 40088A20 len 000175E0 (93 KiB): IRAM
I (285) cpu_start: Pro cpu start user code
I (303) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (304) BATTERY_EXAMPLE: [1.0] create battery service
I (304) BATTERY_EXAMPLE: [1.1] start battery service
I (314) BATTERY_EXAMPLE: [1.2] start battery voltage report
W (2304) BATTERY_EXAMPLE: battery full 2134
I (4304) BATTERY_EXAMPLE: got voltage 2134
I (8304) BATTERY_EXAMPLE: got voltage 2132
I (12304) BATTERY_EXAMPLE: got voltage 2131
I (16304) BATTERY_EXAMPLE: got voltage 2133
I (20304) BATTERY_EXAMPLE: got voltage 2131
I (24304) BATTERY_EXAMPLE: got voltage 2132
I (28304) BATTERY_EXAMPLE: got voltage 2133
I (32304) BATTERY_EXAMPLE: got voltage 2132
I (36304) BATTERY_EXAMPLE: got voltage 2135
I (40304) BATTERY_EXAMPLE: got voltage 2132
I (44304) BATTERY_EXAMPLE: got voltage 2134
I (48304) BATTERY_EXAMPLE: got voltage 2134
I (52304) BATTERY_EXAMPLE: got voltage 2132
I (56304) BATTERY_EXAMPLE: got voltage 2134
I (60304) BATTERY_EXAMPLE: got voltage 2132
I (60314) BATTERY_EXAMPLE: [1.3] change battery voltage report freqency
I (62304) BATTERY_EXAMPLE: got voltage 2134
I (64304) BATTERY_EXAMPLE: got voltage 2132
I (66304) BATTERY_EXAMPLE: got voltage 2131
I (68304) BATTERY_EXAMPLE: got voltage 2132
I (70304) BATTERY_EXAMPLE: got voltage 2132
I (72304) BATTERY_EXAMPLE: got voltage 2133
I (74304) BATTERY_EXAMPLE: got voltage 2134
I (76304) BATTERY_EXAMPLE: got voltage 2133
I (78304) BATTERY_EXAMPLE: got voltage 2132
I (80304) BATTERY_EXAMPLE: got voltage 2134
I (82304) BATTERY_EXAMPLE: got voltage 2134
I (84304) BATTERY_EXAMPLE: got voltage 2133
I (86304) BATTERY_EXAMPLE: got voltage 2132
I (88304) BATTERY_EXAMPLE: got voltage 2135
I (90304) BATTERY_EXAMPLE: got voltage 2133
I (92304) BATTERY_EXAMPLE: got voltage 2133
I (94304) BATTERY_EXAMPLE: got voltage 2135
I (96304) BATTERY_EXAMPLE: got voltage 2133
I (98304) BATTERY_EXAMPLE: got voltage 2133
I (100304) BATTERY_EXAMPLE: got voltage 2134
I (102304) BATTERY_EXAMPLE: got voltage 2134
I (104304) BATTERY_EXAMPLE: got voltage 2135
I (106304) BATTERY_EXAMPLE: got voltage 2135
I (108304) BATTERY_EXAMPLE: got voltage 2131
I (110304) BATTERY_EXAMPLE: got voltage 2131
I (112304) BATTERY_EXAMPLE: got voltage 2133
I (114304) BATTERY_EXAMPLE: got voltage 2133
I (116304) BATTERY_EXAMPLE: got voltage 2133
I (118304) BATTERY_EXAMPLE: got voltage 2134
I (120304) BATTERY_EXAMPLE: got voltage 2131
I (120314) BATTERY_EXAMPLE: [2.0] stop battery voltage report
I (120314) BATTERY_EXAMPLE: [2.1] destroy battery service
I (120314) BATTERY_SERVICE: battery service destroyed
I (120314) VOL_MONITOR: vol monitor destroyed
```

## Troubleshooting

### Voltage does not meet the correct one

Please refer to the hardware design, check the IO setting and whether the voltage is divided.
