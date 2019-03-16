# Speech recognition example

The example demonstrates the speech recognition function on ESP32-LyraT and ESP32-LyraTD-MSC board. It contains nine keywords. 

## Keywords

| # | Keyword | English Meaning | Pronunciation|
|:-:|---------|--------------------|-------------------|
| 1 | 嗨，乐鑫 | Hi, Espressif      | Hāi, lè xīn        |
| 2 | 打开电灯 | Turn on the light  | Dǎkāi diàndēng     |
| 3 | 关闭电灯 | Turn off the light | Guānbì diàndēng    |
| 4 | 音量加大 | Increase volume    | Yīnliàng jiā dà    |
| 5 | 音量减小 | Volume down        | Yīnliàng jiǎn xiǎo |
| 6 | 播放    | Play               | Bòfàng             |
| 7 | 暂停    | Pause              | Zàntíng            |
| 8 | 静音    | Mute               | Jìngyīn            |
| 9 | 播放本地歌曲 | Play local music | Bòfàng běndì gēqǔ |

### Note

If your Chinese pronunciation is not good, you can try the "Alexa" keyword. You need to modify some configurations to use them, see "Usage" for details.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/yes-button.png "Compatible") |

## Usage

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- If you want to use "Alexa" as a keyword, open menuconfig, go to `Speech Recognition Configuration` and select:
    - `Speech recognition wake net to use` > `WakeNet 3 (quantized)`
    - `Name of wakeup word` > `Wakeup word is Alexa`

Load and run the example:

- Say the keywords to the board and you should see them printed out in the monitor.
