# Check ESP32-LyraTD-MSC on Board Buttons

The ESP32-LyraTD-MSC has six buttons (Set, Play, Rec, Mode, Vol- and Vol+) that are connected through a ladder of resistors to ADC 1, channel 3 of the ESP32. A status of these buttons may be read by the application using a dedicated API "ADC Button" and then used to control the application flow. 

The purpose of this application is to provide a simple demonstration how to read the status of buttons as well as a quick verification that all buttons are operational. Load the application the the board and then press each button to see a response logged the the serial terminal.

To run this example you need ESP32-LyraTD-MSC board.
