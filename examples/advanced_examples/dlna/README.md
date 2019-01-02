# DLNA Example

This example allows you to run an UPnP/DLNA MediaRenderer in ESP32 with ADF.
To run this example you need ESP32-LyraT or compatible board:

- Connect speakers or headphone to the board.
- Setup the Wi-Fi connection by `make menuconfig` -> `Example configuration` -> Fill in Wi-Fi SSID and Password. Make sure the Wi-Fi network is the same with the Computer which runs Media Center.
- Install [Universal Media Center](https://www.universalmediaserver.com/) (or any DLNA Media Control point)
- Select ESP32 Renderer -> Select Mp3 file from the Computer -> Play
- The steps are the same as the picture below

![UMS](./ums.png)
