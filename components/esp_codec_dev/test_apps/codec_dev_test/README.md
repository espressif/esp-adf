| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 | ESP32-P4 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# esp_codec_dev: Codec device test application

There are two sets of tests in this application:
1. Using customized codec to test all related API
2. Test codec device on special board

The default test board is ESP32S3_KORVO2_V3, if you are using other board, please change definition in [test_boards.h](main/test_board.h) and realization code is [test_board.c](main/test_board.c).