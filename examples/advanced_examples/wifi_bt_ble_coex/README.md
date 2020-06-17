ESP-ADF Wifi, A2DP and BLE Coexistence Demo
=================================

This demo demonstrates the coexistence of Wi-Fi, A2DP and BLE.

This demo creates several gatt services and starts ADV. The ADV name is BLUFI_DEVICE, then waits to be connected. The device can be configured to connect the wifi with the blufi service. When wifi is connected, the demo will create http stream module. It can play music with http url.

The classic bluetooth A2DP part of the demo implements Advanced Audio Distribution Profile to receive an audio stream. After the program is started, ohther bluetooth devices such as smart phones can discover a device named `ESP_COEX_EXAMPLE`. Once a connection is estalished, use AVRCP profile to control bluetooth music state.

# Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

## How to use this example

### Configure the project

```
make menuconfig
```

* Enable bluetooth dual mode for a2dp and gatts.

* Enable ble auto latency, used to enhance classic bt performance while br/edr and ble are enabled at the same time.

* Enable PSRAM for coex-demo.

The `sdkconfig.defaults` is recommended.

## Usage

- Connect with Bluetooth on your smartphone to the audio board identified as `ESP_COEX_EXAMPLE`.
- Conect with `ESPBlufi` APP on your smartphone to the ble device named "BLUFI_DEVICE", then configure the ssid and password to connect the Wi-Fi.
- Press button `mode` to enter BT mode or WIFI mode, the audio board can play the music from http and a2dp stream.
