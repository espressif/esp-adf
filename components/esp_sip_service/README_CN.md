# ESP SIP Service

- [![Component Registry](https://components.espressif.com/components/espressif/esp_sip_service/badge.svg)](https://components.espressif.com/components/espressif/esp_sip_service)
- [English Version](./README.md)

SIP service 是 `esp_rtc` 的高层封装：把它与采集、dummy 或播放服务建链即可，无需直接处理 SIP 信令与 RTP 帧的搬运。

SIP 通话是全双工的，因此与 RTSP、RTMP 服务不同，本服务没有 role 参数。单个实例上报 `ESP_MEDIA_ROLE_SRC_SINK`，并服务两条媒体流：

- **上行**（`ESP_SIP_SERVICE_STREAM_UPLINK`，stream 0）—— sink，消费已建链的 provider 并发送给对端
- **下行**（`ESP_SIP_SERVICE_STREAM_DOWNLINK`，stream 1）—— source，提供对端发来的媒体

## 功能

- 支持按账号或按 URI 配置，传输方式可选 UDP、TCP 或 TLS
- 支持注册，含 keepalive 与 OPTIONS 保活，并提供完全跳过注册服务器的 P2P 模式
- 支持呼出与呼入：`call`、`answer` 与 `bye`
- 支持 G.711 A-law / u-law / OPUS 音频与 MJPEG / H.264 视频，取决于对端支持情况
- 支持 SDES-SRTP 协商的 `off`、`prefer` 与 `required` 三种模式，并可在运行时查询是否生效
- 支持双向带外 DTMF（RFC2833）
- 支持双向的对话外 SIP MESSAGE
- 支持纯信令启动、只下行时用 setup 报编码，或从已链接的上行 track 取编码
- 支持自定义 INVITE 头部、私有头部与原始头部读取
- 使用媒体服务建链，而非由应用管理帧回调
- 可通过 `esp_service_scheduler` 覆盖线程资源

## 数据流

```mermaid
flowchart LR
    Capture["dummy / capture SRC"] -->|"link(cap,0 -> sip,0)"| Up["上行 sink"]
    subgraph SIP["esp_sip_service (ROLE_SRC_SINK)"]
        Up --> RTC["esp_rtc：SIP + RTP + SRTP"]
        RTC --> Down["下行 src"]
    end
    Down -->|"link(sip,1 -> player,0)"| Render["dummy / player SINK"]
```

要点：

- 两条流传输的都是音视频基本流帧，而非容器字节流
- 上行由协议栈主动拉取，因此不需要自建推送任务
- 请在 start 前建链，以便从 provider 的 track 中读出编码格式
- 启动顺序为先播放器、再 SIP 服务、最后采集
- 媒体仅在通话期间存在，`start` 只是让设备上线

## 调用时序

```mermaid
flowchart TD
    A[esp_sip_service_create] --> B[setup / set_account 或 set_uri]
    B --> C[先链接上行 track，再链下行]
    C --> D[esp_service_start：完成注册]
    D --> E[call / answer -> 通话 -> bye]
    E --> F[esp_service_stop]
    F --> G[unlink / deinit / free]
```

## 典型用法

一个终端由两件互不相干的事决定：

| 决定什么 | 由谁决定 |
| --- | --- |
| **信令** —— 怎么找到对方，要不要经过服务器 | `setup.p2p_mode` |
| **媒体** —— 音视频从哪来、到哪去 | `esp_media_service_link()`，以及无上行链接时的 `setup.audio_codec` / `video_codec` |

两者可以自由组合：注册型终端可以完全不带媒体，P2P 终端也可以只收不发。

### 信令：注册型终端

向 FreeSWITCH、Asterisk 之类的 PBX 注册，之后按分机号呼叫，由服务器负责路由。`p2p_mode` 保持默认的 `false` 就是这种。

```c
esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
esp_sip_service_t *sip = NULL;
ESP_ERROR_CHECK(esp_sip_service_create(&cfg, &sip));

/* 注册是默认行为：setup.p2p_mode 保持 false，服务就会向 URI 里的 server 注册。
   音频编码来自下面链接的采集服务，所以这种终端完全不用调 esp_sip_service_setup() */

esp_sip_service_account_t account = {
    .transport = "udp",
    .user      = "1001",
    .password  = "secret",
    .server    = "192.168.1.10",
    .port      = 5060,
};
ESP_ERROR_CHECK(esp_sip_service_set_account(sip, &account));

/* 采集 -> 上行，下行 -> 播放 */
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(capture), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                       ESP_SERVICE_BASE(player), ESP_MEDIA_DEFAULT_STREAM));

ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(player)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(sip)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(capture)));

/* 收到 ESP_SIP_SERVICE_EVENT_REGISTERED 之后 */
ESP_ERROR_CHECK(esp_sip_service_call(sip, "1002"));
```

在事件回调中用 `esp_sip_service_answer()` 接听来电，用 `esp_sip_service_bye()` 结束通话。通话结束后服务仍保持在线，可继续下一次呼叫。

### 信令：P2P，无注册服务器

完全不注册。URI 里的“服务器”就是对端设备，`call()` 直接向它发 INVITE。

```c
esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
setup.p2p_mode = true;
ESP_ERROR_CHECK(esp_sip_service_setup(sip, &setup));
/* 此时 URI 中的“服务器”就是对端设备，P2P 模式下协议栈也用这个端口做本地绑定 */
esp_sip_service_account_t account = {
    .transport = "udp",
    .user      = "1001",
    .server    = "192.168.1.21",
    .port      = 5060,
};
ESP_ERROR_CHECK(esp_sip_service_set_account(sip, &account));
```

### 媒体：双向、纯接收或无媒体

上面两个例子都把采集链到上行、把下行链到播放器，所以是双向通话。这一半可以单独替换，换成哪种都不影响信令形态：

- **双向** —— 采集链上行，下行链播放器，即上面的写法。编码由链接的 track 提供，`setup` 不用填 codec。
- **纯接收** —— 只把下行链到播放器，并在 `setup.audio_codec` / `video_codec` 里自己写明编码（例如 `ESP_FOURCC_ALAW`），因为没有上行 track 能提供它。门铃、监听类设备属于这种。
- **无媒体** —— 两侧都不链接，两个 codec 保持 0。设备照常注册、照常收发 SIP MESSAGE，只是不协商 RTP。用 Kconfig 把两个方向都不编进固件，效果相同。

## 配置与账号

`esp_sip_service_setup()` 只保留没有合理默认值的三项：`audio_codec`、`video_codec` 和 `p2p_mode`。该接口要求服务处于停止状态，传 `NULL` 可把这三项恢复默认。

区分两种会话形态的是 `p2p_mode`，不是 codec：保持 false 就向 URI 里的 server 注册，置为 true 则直接呼叫该地址、完全不注册。因此一个编码来自上行 track 的注册型终端，根本不需要调 `setup()`。

其余都有默认值，各自放在对应的 set 接口里。这些接口同样要求服务已停止，并且把 0 或 `NULL` 当作“恢复默认”：

| 接口 | 覆盖内容 |
| --- | --- |
| `esp_sip_service_set_local_port()` | 固定本地 SIP 端口 |
| `esp_sip_service_set_timeout()` | 连接超时与读写超时 |
| `esp_sip_service_set_nat_traversal()` | keepalive 间隔、OPTIONS 保活、RFC3581 公网地址 |
| `esp_sip_service_set_register_refresh()` | 注册刷新间隔、通话期间是否暂停 |
| `esp_sip_service_set_srtp_mode()` | SDES-SRTP 协商模式 |
| `esp_sip_service_set_identity()` | User-Agent 与 domain |
| `esp_sip_service_set_frame_size()` | RTP 帧缓冲 |
| `esp_sip_service_set_cache_size()` | 下行 track 缓存 |
| `esp_sip_service_set_video_payload_type()` | SDP 视频 payload type |

可用 `esp_sip_service_set_account()` 按各字段拼出 URI，也可用 `esp_sip_service_set_uri()` 直接传入 `transport://user:password@server:port`。两者都只改 URI，不会覆盖 `setup`，也不会覆盖上面这些 set 接口设过的值。`esp_sip_service_set_local_addr()` 用于覆盖协议栈绑定并对外通告的地址；不设置时使用默认网络接口。

start 时的编码决策：SIP 对每种媒体类型只协商一份编码，只要这一路拿得到编码就会进 SDP。有上行 track 时以该 track 为准；否则用 `audio_codec` / `video_codec`。两者都没有，这一路就不协商，因此什么都不填就是纯信令。下行 track 在通话上报 session begin 时按同一份编码创建。

## 配置项

`ESP_SIP_SERVICE_UPLINK_SUPPORT` 与 `ESP_SIP_SERVICE_DOWNLINK_SUPPORT` 决定两个方向是否编入固件，`get_role` 会按实际使能情况上报。可以只编其中一个。两者都关，或者 setup 里两个 codec 都是 0 且没有链接上行，就是纯信令。

`ESP_SIP_SERVICE_MCP_ENABLE` 用于开启 MCP 工具。

## 调度

协议栈会创建四条工作线程。`start` 用 `esp_service_scheduler_get_thread_cfg()` 填写对应的 `esp_rtc_config_t` 字段。在 `esp_service_scheduler_set_cb()` 里按 create 时的 `name` 和下列线程名覆盖即可：

| 线程名宏 | 默认名 | 默认栈 / 优先级 / 核 |
| --- | --- | --- |
| `ESP_SIP_SCHED_SESSION_TASK` | `sip_task` | 10K / 20 / 0 |
| `ESP_SIP_SCHED_LISTEN_TASK` | `listen_task` | 2K / 20 / 0 |
| `ESP_SIP_SCHED_AUDIO_RECV_TASK` | `_rtp_audio_recv` | 4K / 20 / 0 |
| `ESP_SIP_SCHED_VIDEO_RECV_TASK` | `_rtp_video_recv` | 3K / 15 / 1 |

## 事件

在 `ESP_SERVICE_BASE(sip)` 上使用 `esp_service_event_subscribe()` 订阅。原生 SIP 事件以 `ESP_SIP_SERVICE_EVENT_REGISTERED` 到 `ESP_SIP_SERVICE_EVENT_KEEPALIVE` 的形式发布，另有 `ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED`。基类生命周期变化仍使用 `ESP_SERVICE_EVENT_STATE_CHANGED`。

呼叫、挂断、错误、MESSAGE 与 DTMF 事件各自携带独立的 payload 类型（`esp_sip_service_call_payload_t`、`esp_sip_service_hangup_payload_t`、`esp_sip_service_error_payload_t`、`esp_sip_service_message_payload_t`、`esp_sip_service_dtmf_payload_t`）。其余事件没有 payload。底层协议把对端名称、挂断原因与 MESSAGE 正文暴露为内部存储，仅在其自身回调内有效，因此服务会在发布前将它们拷入独立的堆上 payload；订阅者切勿再回调底层接口去取这些数据。hub 会在订阅者回调返回后，或队列模式最后一次 `esp_service_event_delivery_done()` 之后释放该 payload，因此超出该生命周期仍需使用的内容请自行拷贝。

`ESP_SIP_SERVICE_EVENT_INCOMING` 会在对端持续振铃期间重复上报，因此自动接听只应在首次触发。

## 注意事项

- `setup`、`set_uri`、`set_account`、`set_local_addr` 以及上行 `link` / `unlink` 要求服务处于停止状态
- 通话类动作要求服务处于运行状态
- 同一时刻只能有一条 SIP MESSAGE 在发送中；在 `ESP_SIP_SERVICE_EVENT_MESSAGE_SENT` 到来前，`esp_sip_service_send_message()` 返回 `ESP_ERR_INVALID_STATE`
- 非法 stream id 返回 `ESP_ERR_INVALID_ARG`，合法值只有 0 与 1
- 拆除时请先停生产者，再停其消费服务
- 使用 `esp_media_service_deinit()` 释放后再 free 服务句柄

## 协议库限制

以下限制来自预编译的 `esp_media_protocols`，本组件只做了规避，并未修复：

- 空密码写成 `user:@host`。`set_account()` 在未提供密码时用这种形式。
- P2P 模式下协议栈会用 URI 中的端口覆盖 `fixed_local_port`，并绑定 `INADDR_ANY`，因此本地端口始终等于对端端口，`esp_sip_service_set_local_port()` 在这种模式下不起作用。也正因如此，同一设备上的两个实例无法互相呼叫，它们会被迫共用一个 socket。
- P2P 模式下 `esp_rtc_call()` 需要 `user@host:port` 形式的目标，因此 `esp_sip_service_call()` 会用已配置 URI 中的主机与端口补全裸用户名。
- `esp_rtc_service_init()` 会自行创建任务并立即返回，因此 `start` 不会阻塞到注册完成，请改为等待 `ESP_SIP_SERVICE_EVENT_REGISTERED`。

## 示例

参见 [`examples/sip_service`](./examples/sip_service/README_CN.md)：单个实例配合 dummy 媒体发起一次 P2P 呼叫。它既不需要注册服务器，也不需要连接外部 Wi-Fi AP；例程会启动本地 SoftAP，为 lwIP 提供所需的活动网络接口。

若需要带麦克风、扬声器与控制台的完整终端，请参见 `adf_examples/protocols/sip_cli`。

## MCP 工具

当 `CONFIG_ESP_SIP_SERVICE_MCP_ENABLE=y`（依赖 `CONFIG_ESP_MCP_ENABLE`）时：

- 配置与控制类工具与 C 接口一一对应：`setup`、九个 `set_*` 调优接口、`set_uri`、`set_account`、`set_local_addr`、`start`、`stop`、`call`、`answer`、`bye`、`send_dtmf`、`send_message`，以及 SRTP 与对端信息查询
- 通过 `esp_sip_service_mcp_schema_get()` 与 `esp_sip_service_tool_invoke()` 注册
- 媒体的 link/unlink 仍在 `esp_media_service` MCP 中；媒体帧不会经由 MCP 传输

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与需求：[GitHub issue](https://github.com/espressif/esp-adf/issues)
