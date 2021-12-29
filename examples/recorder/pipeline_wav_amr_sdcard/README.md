# Recoding WAV and AMR file to microSD

The example records 10 seconds WAV and AMR(AMR-NB or AMR-WB) audio file to microSD card via audio pipeline API.

## Compatibility

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Usage

Prepare the audio board:

- Insert a microSD card required memory of 1 MB into board's SD card slot.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.
- You may select encoders between AMR-NB and AMR-WB in `menuconfig` > `Example configuration` > `Audio encoder file type`.

Load and run the example:

- Speak to the board once prompted on the serial monitor.
- After finish, you can open `/sdcard/rec_out.wav`and `/sdcard/rec_out.amr` (or `/sdcard/rec_out.Wamr`) to hear the recorded file.
