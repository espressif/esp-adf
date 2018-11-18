# Play a file of selected fromat from HTTP

The demo plays either MP3, AAC, AMR or WAV file downloaded from HTTP. 

To run this example you need ESP32-LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Setup the Wi-Fi connection by `make menuconfig` -> `Example configuration` -> Fill in Wi-Fi SSID and Password
- Select MP3, AAC, AMR or WAV decoder by editing the following line:

  ```
  #define SELECT_AAC_DECODER 1
  ```
