ESP-ADF A2DP-HFP-Stream demo
=============================

The example plays hfp with hfp_stream.

This is the demo of API implementing Hands Free Profile to receive and send an audio stream.

## How to use this example
### Compatibility

| ESP32-LyraT | ESP32-LyraTD-MSC | ESP32-LyraT-Mini |
|:-----------:|:----------------:|:----------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

### Configure the project

```
make menuconfig
```

* Enable Classic Bluetooth and A2DP under Component config --> Bluetooth --> Bluedroid Enable

* Select compatible audio board in `menuconfig` > `Audio HAL`.


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output.

```
make -j4 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Example Output

- Connect with Bluetooth on your smartphone to the audio board identified as "ESP-ADF-HFP"
- Call your smartphone
- Answer this call

## Troubleshooting
* For current stage, the supported audio codec in ESP32 HFP device is CVSD. CVSD data stream is transmitted to HFP device and then decoded into PCM samples as output. Other decoder configurations in ESP32 HFP device is supported but need additional modifications of protocol stack settings.
* As a usage limitation, ESP32 HFP device can support at most one connection with remote devices.

