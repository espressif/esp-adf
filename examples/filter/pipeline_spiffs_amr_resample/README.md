# Record and Playback with Resample

By using the Rec button on the LyraT board, when the button is pressed, it will record and when the button is released, it will playback the recorded file.

For Spiffs Partition:
- We need to add a spiffs partiton to the partition table like the last line of partition_spiffs_example.csv.

For the Recorder:
- We will setup I2S and get audio at sample rate 48000Hz, 16-bits, 2 channels.
- Using resample-filter to convert to 8000Hz, 16-bits, 1 channel. 
- Encode with AMRNB encoder.
- Write to Spiffs.

For the Playback:
- We will read the recorded file with sameple rate 8000hz, 16-bits, 1 channel.
- Decode with AMR decoder.
- Using resample-filter to convert to 48000Hz, 16-bits, 2 channels.
- Write the audio to I2S.

To stop the pipeline:
 - Press Mode Button in ESP32-LyraT board.

To run this example you need ESP32-LyraT or compatible board:

- Connect speakers or headphones to the board.
