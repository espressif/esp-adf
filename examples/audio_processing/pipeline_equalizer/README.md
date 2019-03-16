# Playback Example of WAV File Processed by Equalizer

This example is playing back a WAV file processed by an equalizer.

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a WAV file 'test.wav' into board's card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example.


## Additional Information

To change the parameters of the equalizer, edit set_gain[] table in filr `equalizer_example.c`. The center frequencies of equalizer are 31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz and 16 kHz.

To run the example of playback, the following conditions should be met:

- The audio file should be in format supported by the equalizer:
    * Sampling rate of 11025 Hz, 22050 Hz, 44100 Hz or 48000 Hz.
    * Number of channels: 1 or 2.

- Audio file format for this example is WAV.

Contents of 'document' folder:

- The 'test.wav' file is the example file with sample rate of 44100 Hz and a single audio channel.
- The 'spectrum_before.png' is the spectrum image of the original 'test.wav' file.
<div  align="center"><img src="document/spectrum_before.png" width="700" alt ="spectrum_before" align=center /></div>
- The 'spectrum_after.png' is the spectrum image of the 'test.wav' file after processing by the equalizer, when the gain of equalizer is -13 dB.
<div align="center"><img src="document/spectrum_after.png" width="700" alt ="spectrum_after" align=center /></div>
- The 'amplitude_frequency.png' is the equalizer's frequency response diagram, when the gain of equalizer is 0 dB.
<div align="center"><img src="document/amplitude_frequency.png" width="700" alt ="amplitude_frequency" align=center /></div>
