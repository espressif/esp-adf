# Voice activity detection (VAD) example

The example demonstrates the voice activity detection, also known as speech activity detection or speech detection.

1. To run this example you need ESP32-LyraT, ESP32-LyraTD-MSC or another compatible board.
2. Run make menuconfig and select matching development board under Audio HAL.
3. After running this example say something to the board and you should see a message "Speech detected" printed out in the monitor.

## Note

This application will likely report as the speech some other randoms sounds, e.g. knocking the board, music with certain tones, etc.
