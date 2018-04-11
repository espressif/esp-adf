# Recording WAV file and upload to HTTP Server

By using the REC button on the LyraT board, when the button is pressed, it will record and upload data to HTTP Stream, and when the button is released, it will stop recording, server will write all data to .wav file.

Prepare the Server:
- Require python 2.7 or higher
- Connect the computer to same Wi-Fi network with ESP32 board
- Run `python server.py` - place the file server.py at root of this example, and make sure this directory is writable.

Build the example: 
- `make menuconfig` and provide Wi-Fi `ssid`, `password`. Also, server URI, ex: `http://http://192.168.0.248:8000/upload` with `http://192.168.0.248` is the server LAN IP address.

To stop the pipeline:
 - Press MODE Button on the ESP32 LyraT board
