# Speech recognition example
The example demonstrates the speech recognition function on ESP32-LyraT and ESP32-LyraTD-MSC board. It contains
nine keywords. 

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

-------------------------------------------------------------------------------

If your Chinese pronunciation is not good, you can try the "Alexa" keyword.

(You need to modify some configurations to use them. See note for details)

-------------------------------------------------------------------------------

## Note

1. To run this example you need ESP32-LyraT, ESP32-LyraTD-MSC or compatible board.

2. Run make menuconfig and select matching development board under Audio HAL.

3. If you want to use "Alexa" as a keyword, open menuconfig, go to 'Speech Recognition Configuration' and select:
    Speech recognition wake net to use > WakeNet 3 (quantized)
    Name of wakeup word > Wakeup word is Alexa

4. After running this example, say the keywords to the board and you should see them printed out in the monitor.
