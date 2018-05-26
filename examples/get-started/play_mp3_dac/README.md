# Play mp3 file using ESP32 internal DAC

This example plays a sample 7 second mp3 file provided in 'main' folder. The file gets embedded in the application during compilation. After application upload the file is then played from ESP32's internal flash.

To run this example you need an ESP32 board that has DAC GPIO 25 and 26 exposed and earphones. Connect the pins to the earphones according to schematic below.


Example connections:

```
GPIO25 +--------------+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
             330R     |
GND +-------/\/\/\----+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
                      |
GPIO26 +--------------+
```                      