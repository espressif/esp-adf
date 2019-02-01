# Audio passthru

This demo passes audio received at the aux_in port to the headphone and speaker outputs.

Typical use cases:

- Exercising the audio pipeline from end to end when bringing up new hardware.
- Checking for left and right channel consistency through the audio path.
- Used in conjunction with an audio test set to measure THD+N, for production line testing or performance evaluation.

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Connect audio source to aux_in.