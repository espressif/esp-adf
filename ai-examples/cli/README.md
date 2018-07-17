# Uart Console example
This example shows how to use `periph_console` to control `esp_audio` APIs and system APIs.
To run this example you need ESP32 LyraT or compatible board:

- Setup Wi-Fi SSID and Password by console, refer to system commands join.
- Insert a microSD card loaded with 'test.wav', 'test.mp3', 'test.aac', 'test.ts' and 'test.m4a' into board's slot.
- Connect speakers or headphones to the board. 

## Support Commands
### Audio Commands
- play n: Play music with given index
- play URI: Play music by given URI, e.g.'play file://sdcard/test.mp3'.
- pause: Pause the playing music
- resume: Resume the paused music
- stop: Stop the playing music.
- setvol: Set volume
- getvol: Get volume
- getpos: Get current position in seconds.

### System Commands
- reboot: Restart system
- free: Get the free memory
- stat: Show the processor time amongst FreeRTOS tasks
- tasks: Get information about running tasks
- join: Connect Wi-Fi with ssid and password
- wifi: Get connected AP SSID

### Note:
- To run _stat_ command, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS must be enabled by make menuconfig Component Config -> FreeRTOS ->Enable FreeRTOS to collect run time stats.
- To run _tasklist_ command, CONFIG_FREERTOS_USE_TRACE_FACILITY must be enabled by make menuconfig Component Config -> FreeRTOS ->Enable FreeRTOS trace facility and Enable FreeRTOS stats formatting functions
- To run aac decoder, CONFIG_FREERTOS_HZ should be 1000Hz.

