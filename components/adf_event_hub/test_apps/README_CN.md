# adf_event_hub 测试工程

[English](README.md)

ESP-IDF 测试固件在真机和 EMU 上运行同一套 116 个 Unity 用例。用例位于
`common/test_*.c`，覆盖参数校验、事件投递和 payload 所有权、同步清理、
并发、统计和高订阅量场景。

## 真机

```bash
cd components/adf_event_hub/test_apps
idf.py set-target esp32s3
idf.py -p <serial-port> flash monitor
```

在 Unity 菜单输入 `*` 运行全部用例。汇总后固件会打印
`<<< ADF_EVENT_HUB_UT_DONE >>>`。

## EMU

在仓库根目录用 `idf.py` 编译并用 [`esp-emu`](https://github.com/espressif/esp-emulator) 运行：

```bash
idf.py -C components/adf_event_hub/test_apps -B components/adf_event_hub/test_apps/build_esp32s3_default \
  -D IDF_TARGET=esp32s3 build
pytest components/adf_event_hub/test_apps --embedded-services idf,espemu --target esp32s3
```

新增用例时在 `common/` 中添加 `test_*.c`，包含 `test_port.h` 并使用
`TEST_CASE("description", TEST_TAG)`。
