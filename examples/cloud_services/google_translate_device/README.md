# Google translation device example

This application is demonstrating use of Google services to translate speech in one language into speech in another language.

Press [Rec] button, say something in Chinese and release the button. The device will then use three Google services to perform the following actions:

- Google Cloud Speech-To-Text - convert recorded audio file into text in Chinese - [Language Support](https://cloud.google.com/speech-to-text/docs/languages)
- Google Translate - translate text in Chinese into English - [Language Support](https://cloud.google.com/translate/docs/languages)
- Google TTS - convert translated text into an audio file - [Supported voices](https://cloud.google.com/text-to-speech/docs/voices)

Finally device will play back received audio file containing the message now spoken in English. With this demo you can change the language used by defining the language code, refer to the languages used support above.

## Compatibility

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`
- Get the Google Cloud API Key: https://cloud.google.com/docs/authentication/api-keys 
- Enter API Key, Wi-Fi `ssid` and `password` in `menuconfig` > `Example Configuration`.

Note: If this is the first time you are using above Google API, you need to enable each one of the tree APIs by vising the Google API console.

Load and run the example:

 - Wait for Wi-Fi network connection.
 - Press [Rec] button, and wait for **Red** LED blinking or `GOOGLE_SR: Start speaking now` yellow line in terminal.
 - Speak something in Chinese. If you do not know Chinese then use "Google Translate" to translate some text into Chinese and speak it for you.
 - After finish, release the [Rec] button. Wait a second or two for Google to receive and process the message and then the board to play it back.
- To stop the pipeline press [Mode] button on the audio board.
