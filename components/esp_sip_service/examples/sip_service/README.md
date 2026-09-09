# SIP Service Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates a full-duplex `esp_sip_service` call

## Example Brief

- This example wires one `esp_sip_service` instance for a full-duplex P2P call and places the call. P2P mode skips registration, so no registrar or account is involved. The copy-ready flow lives in `main/simple_sip.c`.
- A dummy source is linked on the uplink and a dummy sink drains the downlink, so no microphone or speaker is required. The dummy advertises an OPUS track so `start` can take the codec from the link. Because SIP is the repository's only `ESP_MEDIA_ROLE_SRC_SINK` service, both links attach to the same instance.
- The peer defaults to the device itself over loopback, so the example runs with a single board. Nothing answers that INVITE; point `SIP_PEER_IP` in `main/settings.h` at another SIP endpoint to establish media and see the downlink frame count.

### Prerequisites

- Familiarity with [`esp_sip_service`](../../README.md) and [`esp_media_service`](../../../esp_media_service/README.md)
- Nothing else for the default run; a reachable SIP endpoint if you want real media

### Folder Contents

```text
sip_service/
├── main/
│   ├── app_main.c          NVS, media adapter, SoftAP netif, and the demo call
│   ├── simple_sip.c        Copy-ready P2P call flow
│   ├── simple_sip.h
│   └── settings.h          Peer address, port, users, and durations
└── pytest_esp_sip_service_example.py  Boot and call smoke test
```

## Environment Setup

### Hardware Required

- An ESP development board with PSRAM (recommended: ESP32-S3 or ESP32-P4)
- USB cable for the console

A SoftAP is started at boot even when the call stays local: lwIP needs an interface to be up before the UDP sockets can be bound.

## Build and Flash

This example supports ESP-IDF release/v5.5 and later branches. By default, it uses ADF's built-in `$ADF_PATH/esp-idf`.

The default run needs no configuration. Build and flash:

```bash
cd components/esp_sip_service/examples/sip_service
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Exit the monitor with `Ctrl-]`.

To call a real endpoint, set `SIP_PEER_IP`, `SIP_PEER_PORT` and `SIP_PEER_USER` in `main/settings.h`, and replace the SoftAP in `app_main.c` with a station connection to the network that endpoint is on.

## How It Works

The instance is created with `p2p_mode = true` and an account whose "server" is the peer:

| Field | Value | Meaning |
| --- | --- | --- |
| `SIP_LOCAL_USER` | 1001 | The user in the local URI |
| `SIP_PEER_USER` | 1002 | The user that is invited |
| `SIP_PEER_IP` / `SIP_PEER_PORT` | 127.0.0.1:5062 | Where the INVITE is sent |

One instance carries both media directions:

```
dummy_src ──link(0 → uplink)──> esp_sip_service ──link(downlink → 0)──> dummy_sink
                                       ⇅ RTP
                                     peer
```

The flow is: create the instance, add an OPUS track on the dummy source, link both directions, start the consumers before the producers, `call`, wait for `CALL_ANSWERED`, run for five seconds, read the sink frame count, then `bye` and tear everything down.

In P2P mode the protocol stack binds the port taken from the URI rather than the configured local port, so `esp_sip_service_set_local_port()` is called with `SIP_PEER_PORT` only to keep the two consistent on paper. The same behavior is why two instances on one device cannot call each other: they would have to share one socket.

## Example Output

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

With a real peer, `CALL_ANSWERED` and `AUDIO_SESSION_BEGIN` arrive instead of the warning, and the run ends with `Downlink received N audio frames`. The example fails only when no `CALLING` event was published, which means the INVITE never left the device.

## Troubleshooting

- No `CALLING` event: the SoftAP did not come up, so the sockets could not bind. Check the boot log for Wi-Fi errors.
- `Downlink received 0 audio frames` with a real peer: the peer answered but sent nothing, or `CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT` is disabled.
- To try SRTP, set `srtp_mode` in `sip_create()`; with `required` the call is rejected unless both sides offer a crypto line.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)
