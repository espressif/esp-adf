ESP-ADF A2DP-SINK-Stream demo
=============================

Demo of A2DP audio sink role

This is the demo of API implementing Advanced Audio Distribution Profile to receive an audio stream.

This example involves the use of Bluetooth legacy profile A2DP for audio stream reception, AVRCP for media information notifications.

Applications such as bluetooth speakers can take advantage of this example as a reference of basic functionalities.

## How to use this example

### Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |


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

- Connect with Bluetooth on your smartphone to the audio board identified as "ESP_SINK_STREAM_DEMO"
- Play some audio from the smartphone and it will be transmitted over Bluetooth to the audio bard.
- The audio playback may be controlled from the smartphone, as well as from the audio board. The following controls may be used:

    |   Smartphone   | Audio Board |
    |:--------------:|:-----------:|
    |   Play Music   |    [Play]   |
    |   Stop Music   |    [Set]    |
    |   Next Song    | [long Vol+] |
    | Previous Song  | [long Vol-] |
    |   Volume Up    |    [n/a]    |
    |  Volume Down   |    [n/a]    |

Once A2DP connection is set up, there will be a notification message with the remote device's bluetooth MAC address like the following:

```
I (106427) A2DP_STREAM: A2DP bd address:, [64:a2:f9:69:57:a4]
I (106427) A2DP_STREAM: A2DP connection state =  CONNECTED
```

If connection is terminated, there will be such message:

```
I (5100) A2DP_STREAM: A2DP connection state =  DISCONNECTED
```


## Troubleshooting
* For current stage, the supported audio codec in ESP32 A2DP is SBC. SBC data stream is transmitted to A2DP sink and then decoded into PCM samples as output. The PCM data format is normally of 44.1kHz sampling rate, two-channel 16-bit sample stream. Other decoder configurations in ESP32 A2DP sink is supported but need additional modifications of protocol stack settings.
* As a usage limitation, ESP32 A2DP sink can support at most one connection with remote A2DP source devices. Also, A2DP sink cannot be used together with A2DP source at the same time, but can be used with other profiles such as SPP and HFP.