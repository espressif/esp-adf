# SIP Service 示例

- [English Version](./README.md)

- 常规示例：![alt text](../../../../docs/_static/level_regular.png "Regular Example") —— 演示 `esp_sip_service` 的全双工通话

## 示例简介

- 本示例用一个 `esp_sip_service` 实例装配全双工 P2P 通话并发起呼叫。P2P 模式会跳过注册，因此不涉及注册服务器与账号。可直接复用的流程位于 `main/simple_sip.c`。
- 一个 dummy 源挂在上行、一个 dummy sink 消费下行数据，因此无需麦克风与扬声器。dummy 上添加一条 OPUS track，供 `start` 从链路读取编码。由于 SIP 是本仓库唯一的 `ESP_MEDIA_ROLE_SRC_SINK` 服务，两条链路都挂在同一个实例上。
- 对端默认指向本机环回地址，因此单板即可运行。该 INVITE 不会有人接听；把 `main/settings.h` 中的 `SIP_PEER_IP` 改为另一个 SIP 端点，即可建立媒体并看到下行帧计数。

### 前置条件

- 了解 [`esp_sip_service`](../../README_CN.md) 与 [`esp_media_service`](../../../esp_media_service/README_CN.md)
- 默认运行无需其他条件；若希望有真实媒体，则需要一个可达的 SIP 端点

### 目录内容

```text
sip_service/
├── main/
│   ├── app_main.c          NVS、媒体适配层、SoftAP netif 与演示通话
│   ├── simple_sip.c        可直接复用的 P2P 通话流程
│   ├── simple_sip.h
│   └── settings.h          对端地址、端口、用户与时长
└── pytest_esp_sip_service_example.py  启动与通话冒烟测试
```

## 环境配置

### 硬件需求

- 一块带 PSRAM 的 ESP 开发板（推荐 ESP32-S3 或 ESP32-P4）
- 用于控制台的 USB 线

即使通话在本地完成，启动时仍会开启 SoftAP：lwIP 需要有接口处于 up 状态，UDP socket 才能完成绑定。

## 编译和烧写

本示例支持 ESP-IDF release/v5.5 及之后的分支，默认使用 ADF 内置的 `$ADF_PATH/esp-idf`。

默认运行无需任何配置，直接编译烧写：

```bash
cd components/esp_sip_service/examples/sip_service
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

使用 `Ctrl-]` 退出监视器。

若要呼叫真实端点，请修改 `main/settings.h` 中的 `SIP_PEER_IP`、`SIP_PEER_PORT` 与 `SIP_PEER_USER`，并把 `app_main.c` 中的 SoftAP 换成连接该端点所在网络的 station 模式。

## 工作原理

实例以 `p2p_mode = true` 创建，账号中的“服务器”即为对端：

| 字段 | 取值 | 含义 |
| --- | --- | --- |
| `SIP_LOCAL_USER` | 1001 | 本地 URI 中的用户 |
| `SIP_PEER_USER` | 1002 | 被呼叫的用户 |
| `SIP_PEER_IP` / `SIP_PEER_PORT` | 127.0.0.1:5062 | INVITE 的目的地 |

一个实例同时承载两个媒体方向：

```
dummy_src ──link(0 → 上行)──> esp_sip_service ──link(下行 → 0)──> dummy_sink
                                       ⇅ RTP
                                      对端
```

流程为：创建实例，给 dummy 源添加一条 OPUS track，建立双向链路，先启动消费者再启动生产者，发起 `call`，等待 `CALL_ANSWERED`，运行 5 秒，读取 sink 的帧计数，然后 `bye` 并拆除全部资源。

P2P 模式下协议栈绑定的是 URI 中的端口，而不是配置的本地端口，因此这里调用 `esp_sip_service_set_local_port()` 传入 `SIP_PEER_PORT` 只是为了写法上保持一致。同一行为也决定了同一设备上的两个实例无法互相呼叫：它们会被迫共用一个 socket。

## 示例输出

```text
I (645) SIP_EX: SIP service example is ready
I (719) SIP_EX: Calling 1002 at 127.0.0.1:5062
I (1098) SIP_EX: CALLING
W (5849) SIP_EX: Nobody answered at 127.0.0.1:5062, set SIP_PEER_IP to a real endpoint for media
I (5985) SIP_EX: HANGUP reason=
W (6190) SIP_EX: ERROR reject_reason=0
I (6426) SIP_EX: SIP_SERVICE_EXAMPLE_PASSED
I (6430) SIP_EX: SIP_SERVICE_EXAMPLE_DONE
```

接入真实对端后，警告会被 `CALL_ANSWERED` 与 `AUDIO_SESSION_BEGIN` 取代，运行结束时打印 `Downlink received N audio frames`。示例仅在没有收到 `CALLING` 事件时失败，这意味着 INVITE 根本没有发出设备。

## 常见问题

- 没有 `CALLING` 事件：SoftAP 未起来，socket 无法绑定。请检查启动日志中的 Wi-Fi 报错。
- 接入真实对端后仍是 `Downlink received 0 audio frames`：对端接听了但没有发流，或者 `CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT` 未开启。
- 如需试验 SRTP，请在 `sip_create()` 中设置 `srtp_mode`；使用 `required` 时，若两端未都提供 crypto 行，通话会被拒绝。

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与需求：[GitHub issue](https://github.com/espressif/esp-adf/issues)
