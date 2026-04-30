# adf_event_hub 多服务示例

通过 `adf_event_hub` 把五个独立的服务连接起来的可运行示例，可以打印完整的
事件流。同一份源码同时支持 ESP-IDF 目标和 PC 构建（FreeRTOS POSIX 模拟器），
集成逻辑的迭代无需烧录。

## 目录结构

```
examples/
├── CMakeLists.txt        # ESP-IDF 顶层工程
├── sdkconfig.defaults    # IDF 构建默认配置
├── main/                 # 仅 IDF 入口
│   ├── CMakeLists.txt    #   组件注册（引入 ../common/*.c）
│   └── app_main.c        #   IDF 入口（创建任务调用 example_run）
├── pc/                   # 仅 PC 端所需
│   ├── CMakeLists.txt    #   独立 CMake，FetchContent 拉取 FreeRTOS-Kernel
│   ├── FreeRTOSConfig.h  #   POSIX 模拟器配置
│   └── main.c            #   PC 入口（启动调度器并运行 example 任务）
└── common/               # 共享源码（IDF + PC 都编译）
    ├── example_run.[ch]  #   平台无关的仿真主体
    ├── demo_button_service.*      #   发布端：随机按键（演示桩，非 esp_button_service 组件）
    ├── demo_wifi_service.*      #   响应按键，发布 wifi 事件（演示桩）
    ├── demo_ota_service.*         #   响应 wifi，发布 OTA 进度（演示桩，非 esp_ota_service 组件）
    ├── demo_player_service.*      #   响应 button/wifi/ota（演示桩）
    └── demo_led_service.*         #   队列模式消费所有 domain（演示桩）
```

## 构建与运行

### PC（FreeRTOS POSIX 模拟器）

```bash
cd components/adf_event_hub/examples/pc
cmake -S . -B build -G Ninja
cmake --build build
./build/event_hub_example
```

仿真结束（约 10 秒）后进程会自行退出。

### ESP-IDF 目标

```bash
cd components/adf_event_hub/examples
idf.py set-target esp32s3   # 或任意受支持的芯片
idf.py build flash monitor
```

`app_main()` 会将同一个 `event_hub_example_run()` 放入独立任务执行，与 PC
端输出一致。

## 事件流

示例中 5 个 domain hub 并行运行，各 `*_service_create()` 中建立的订阅关系
如下图所示：

```
                  ┌─────────────────────────────────────────────┐
                  │   monitor hub  (回调，对 4 个 domain 全订阅) │
                  │   led hub      (队列，对 4 个 domain 全订阅) │
                  └──────▲─────────▲──────────▲────────▲────────┘
                         │         │          │        │
  button ──PROVISION,MODE┤         │          │        │
  button ──PLAY,VOL_UP,VOL_DOWN────┐          │        │
                         │         │          │        │
                         ▼         ▼          │        │
                       wifi      player       │        │
                         │         ▲          │        │
                         │         │          │        │
                         └GOT_IP─► ota ──COMPLETE──────┘
                                   │
  wifi ──DISCONNECTED─► player ◄───┘
```

每个订阅者对应的事件表（参见 `*_service_create()` 中的 `info` 配置）：

| 订阅者  | Domain   | 事件                                   | 模式     |
|---------|----------|----------------------------------------|----------|
| wifi    | button   | `PROVISION`、`MODE`                    | 回调     |
| ota     | wifi     | `GOT_IP`                               | 回调     |
| player  | button   | `PLAY`、`VOL_UP`、`VOL_DOWN`           | 回调     |
| player  | wifi     | `DISCONNECTED`                         | 回调     |
| player  | ota      | `COMPLETE`                             | 回调     |
| led     | button / wifi / ota / player | ANY_ID             | 队列     |
| monitor | button / wifi / ota / player | ANY_ID             | 回调     |

各发布者发出的事件：

- **button** — `PROVISION`、`PLAY`、`VOL_UP`、`VOL_DOWN`、`MODE`（随机按压）。
- **wifi** — `CONNECTING`、`CONNECTED`、`DISCONNECTED`、`GOT_IP`、
  `SCAN_DONE`、`RSSI_UPDATE`（`RSSI_UPDATE` 携带 `int32_t` payload 并带
  release 回调）。
- **ota** — `CHECK_START`、`UPDATE_AVAILABLE`、`PROGRESS`（`uint8_t` 百分比）、
  `COMPLETE`、`ERROR`（C 字符串）、`NO_UPDATE`。
- **player** — `STARTED`、`PAUSED`、`STOPPED`、`PROGRESS`（`uint8_t` 百分比）、
  `VOLUME_CHANGED`（`int32_t`）、`ERROR`（C 字符串）。

每个 publisher 发布各事件后的完整运行时联动（下游会触发哪些事件、哪些状态
跃迁会被屏蔽等）详见 [`EVENT_FLOW_LOG_REPORT.md`](EVENT_FLOW_LOG_REPORT.md)。

## 演示内容

- **跨域扇出** — button 事件驱动 wifi/player/led 反应。
- **混合投递方式** — 回调订阅者用于低延迟监控，队列订阅者（LED）用于批量消费。
- **Payload 生命周期** — 服务发布带 release 回调的堆 payload，可验证引用计数。
- **运行时可见性** — 拆除前会调用 `adf_event_hub_dump()` 与
  `adf_event_hub_get_stats()` 打印订阅者数量和 envelope 池使用情况。
