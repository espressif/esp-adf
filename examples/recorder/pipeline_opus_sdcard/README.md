# Recoding OPUS file to microSD 

The example encodes 10 seconds of audio using OPUS encoder and saves it to SD Card.

## Hardware Preparation

Before running this example, please complete the following steps:

1. Get an ESP32-LyraT or other compatible boards.
2. Connect your speakers or headphones to your development board.
3. Insert a MicroSD card to your development board.

## Program Description

1. The input data comes from I2S.
2. After finishing runing the example, you can open `/sdcard/rec.opus` to hear the recorded file.
