# Voice Changer Example with Sonic

With the Sonic feature, the output of a WAV file can be changed.

## Overview

The Sonic feature (voice changer) can adjust the output of a WAV file by changing the following parameters:

- Interpolation (linear interpolation)
- Speed
- Pitch

## Compatibility

| ESP32-LyraT | ESP32-LyraT-MSC |
|:-----------:|:---------------:|
| [![alt text](../../../docs/_static/esp32-lyrat-v4.2-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| ![alt text](../../../docs/_static/yes-button.png "Compatible") | ![alt text](../../../docs/_static/no-button.png "Not Compatible") |

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 
- Insert a MicroSD card to your development board.

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`.

Load and run the example.

## Button Description

The following buttons can be used to control the board:

- The [Mode] button: Users can switch between changing the speed or changing the pitch of a WAV file by pressing the [Mode] button on the audio board. 

- The [Rec] button:

	* Recording: Press the [Rec] button and hold on.
	* Playing: Release the [Rec] button.
