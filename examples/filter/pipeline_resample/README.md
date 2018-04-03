# Record and Playback with Resample

By using the REC button on the LyraT board, when the button is pressed, it will record, and when the button is released, it will play back the recorded file.

For the Recorder:
- We will setup I2S and get audio at sample rates 48000Hz, 16-bits, 2 channel.
- Using resample-filter to convert to 16000Hz, 16-bits, 1 channel. 
- Encode with Wav encoder
- Write to microSD

For the Playback:
- We will read the recorded file, with sameple rates 16000hz, 16-bits, 1 channel,
- Decode with WAV decoder
- Using resample-filter to convert to 48000Hz, 16-bits, 2 channel.
- Write the audio to I2S

To stop the pipeline:
 - Press Mode Button in ESP32 LyraT board 

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Insert a microSD card 
