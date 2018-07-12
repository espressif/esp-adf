# Play mp3 files with different sample rates

This example plays the same music at three different sample rates (8000Hz, 20500Hz and 44100Hz) in a loop. The music is stored in three separate files provided in 'main' folder. The files get embedded in the application during compilation. After application upload, the files are played one after another to show differences in audio quality.

The sample rate may be checked during playback on a terminal:

```
I (160) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (12610) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (25030) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (37320) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (49740) PLAY_MP3_RATES: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
...
```

To run this example you need ESP32 LyraT or compatible board with speakers or headphones connected.
