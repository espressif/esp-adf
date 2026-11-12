# Play MP3 files from microSD with butons control 

The demo plays MP3 files stored on the SD card using audio pipeline API.

The playback control is done using buttons connected on GPIO32 and GPIO35. User can start, stop, pause, resume playback and advance to the next song. When playing, the application automatically advances to the next song once previous finishes.

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a MP3 files 'test.mp3', 'test1.mp3' and 'test2.mp3' into board's slot.
- Connect play button to GPIO35, next song button to GPIO32

Adding volume up/ down function just as same as examples/player/pipeline_sdcard_mp3_control
