# Recoding AMR file to microSD 

The demo records 10 seconds AMR-NB or AMR-WB audio file to microSD Card. Selection between AMR-NB and AMR-WB is done in menuconfig. The AMR audio compression format is optimized for speech coding and produces much smaller output files comparing to other popular codecs.
This demo makes use of callback function instead of Message Queue for events.
For instance, [pipeline_amr_sdcard](examples/recorder/pipeline_amr_sdcard) example uses Message Queue for events.

To run this example you need ESP32-LyraT or compatible board.

- Connect speakers or headphones to the board. 
- Insert a microSD card 
- After finish, you can open `/sdcard/rec.amr` or `/sdcard/rec.Wamr` to hear the recorded file
