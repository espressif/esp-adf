# Play MP3 files from microSD with touch pad control 

The demo plays MP3 files stored on the SD card using audio pipeline API.

The playback control is done using ESP32 touch pad functionality. User can start, stop, pause, resume playback and advance to the next song as well as adjust volume. When playing, the application automatically advances to the next song once previous finishes.

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a MP3 files 'test.mp3', 'test1.mp3' and 'test2.mp3' into board's slot.
