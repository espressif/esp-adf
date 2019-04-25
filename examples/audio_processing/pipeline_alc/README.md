# Playback Example of WAV File Processed by ALC

This example is playing back a WAV file processed by an ALC.

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a WAV file 'test.wav' into board's card slot.

Load and run the example.

## Additional Information

Two methods can implement volume setting with ALC. 
1. The one is through the function `i2s_alc_volume_set` in `i2s_stream.h`.
2. The other is ALC as a independent element to be used.
If `USE_ALONE_ALC` is defined, the second method will be selected. If not, the first will be selected.
