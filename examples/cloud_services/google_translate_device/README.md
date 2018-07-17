# Google translation device example

This application is demonstrating use of Google services to translate speech in one language into speech in another language.

Press Rec button, say something in Chinese and release the button. The device will then use three Google services to perform the following actions:

- Google Cloud Speech-To-Text - convert recorded audio file into text in Chinese - [Language Support](https://cloud.google.com/speech-to-text/docs/languages)
- Google Translate - translate text in Chinese into English - [Language Support](https://cloud.google.com/translate/docs/languages)
- Google TTS - convert translated text into an audio file - [Supported voices](https://cloud.google.com/text-to-speech/docs/voices)

Finally device will playback received audio file containing the message now spoken in English. With this demo you can change the language used by defining the language code, refer to the languages used support above.

Build the example:
- Get the Google Cloud API Key: https://cloud.google.com/docs/authentication/api-keys 
- `make menuconfig` and provide API Key, Wi-Fi `ssid`, `password`

Note: If this is the first time you are using above Google API, you need to enable each one of the tree APIs by vising the Google API console.

To run this example you need ESP32 LyraT or compatible board:
 - Connect speakers or headphones to the board. 
 - Wait for Wi-Fi network connected
 - Press Rec button, and wait for **Red** Led blinking or `GOOGLE_SR: Start speaking now` yellow line in terminal
 - Speak something in Chinese. You use Google Translate to translate and speak some text in Chinese for you
 - After finish, release the Rec button. Wait a second or two for Google to receive and process the message and then the board to play it back.

To stop the pipeline:
 - Press Mode Button in ESP32 LyraT board 
