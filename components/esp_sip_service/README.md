# ESP SIP Service

- [![Component Registry](https://components.espressif.com/components/espressif/esp_sip_service/badge.svg)](https://components.espressif.com/components/espressif/esp_sip_service)
- [中文版](./README_CN.md)

SIP service is a high-level wrapper over `esp_rtc`: link it with capture, dummy, or playback services and avoid handling SIP signaling and RTP frame plumbing directly.

A SIP call is full duplex, so unlike the RTSP and RTMP services this one has no role parameter. A single instance reports `ESP_MEDIA_ROLE_SRC_SINK` and serves two media streams:

- **Uplink** (`ESP_SIP_SERVICE_STREAM_UPLINK`, stream 0) — a sink that consumes a linked provider and sends it to the peer
- **Downlink** (`ESP_SIP_SERVICE_STREAM_DOWNLINK`, stream 1) — a source that provides what the peer sent

## Features

- Account-based or URI-based setup, over UDP, TCP or TLS
- Registration with keepalive and OPTIONS pings, plus a P2P mode that skips the registrar entirely
- Outgoing and incoming calls: `call`, `answer` and `bye`
- G.711 A-law / u-law / OPUS audio and MJPEG / H.264 video, subject to peer support
- SDES-SRTP negotiation with `off`, `prefer` and `required` modes, and a runtime activity query
- Out-of-band DTMF (RFC2833) in both directions
- Out-of-dialog SIP MESSAGE in both directions
- Signaling-only start, receive-only via a setup codec, or send/receive from linked tracks
- Custom INVITE headers, private headers and raw header inspection
- Media-service links instead of application-managed frame callbacks

## Data Flow

```mermaid
flowchart LR
    Capture["dummy / capture SRC"] -->|"link(cap,0 -> sip,0)"| Up["uplink sink"]
    subgraph SIP["esp_sip_service (ROLE_SRC_SINK)"]
        Up --> RTC["esp_rtc: SIP + RTP + SRTP"]
        RTC --> Down["downlink src"]
    end
    Down -->|"link(sip,1 -> player,0)"| Render["dummy / player SINK"]
```

Short rules:

- Both streams carry elementary audio/video frames, not container bytes
- The uplink is pulled by the protocol stack, so no push task of its own is needed
- Link the provider before starting, so the codecs can be read from its tracks
- Start the player before the SIP service, and the SIP service before the capture
- Media exists only during a call; `start` merely brings the device online

## Call Sequence

```mermaid
flowchart TD
    A[esp_sip_service_create] --> B[setup / set_account or set_uri]
    B --> C[link uplink tracks, then downlink]
    C --> D[esp_service_start: registers]
    D --> E[call / answer -> talk -> bye]
    E --> F[esp_service_stop]
    F --> G[unlink / deinit / free]
```

## Typical Usage

An endpoint is described by two independent things:

| Decides | Set by |
| --- | --- |
| **Signaling** — how the other party is reached, and whether a server is involved | `setup.p2p_mode` |
| **Media** — where the audio and video come from and go to | `esp_media_service_link()`, plus `setup.audio_codec` / `video_codec` when nothing is linked |

They combine freely: a registered endpoint may carry no media at all, and a P2P one may only receive.

### Signaling: registered endpoint

Registers with a PBX such as FreeSWITCH or Asterisk, then places calls by extension and lets the server route them. This is what `p2p_mode` left at its default `false` does.

```c
esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
esp_sip_service_t *sip = NULL;
ESP_ERROR_CHECK(esp_sip_service_create(&cfg, &sip));

/* Registering is the default: setup.p2p_mode stays false, so the service
   registers with the server named in the uri. The audio codec comes from the
   capture linked below, so this endpoint needs no esp_sip_service_setup()
   call at all. */

esp_sip_service_account_t account = {
    .transport = "udp",
    .user      = "1001",
    .password  = "secret",
    .server    = "192.168.1.10",
    .port      = 5060,
};
ESP_ERROR_CHECK(esp_sip_service_set_account(sip, &account));

/* capture -> uplink, downlink -> player */
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(capture), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                       ESP_SERVICE_BASE(player), ESP_MEDIA_DEFAULT_STREAM));

ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(player)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(sip)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(capture)));

/* after ESP_SIP_SERVICE_EVENT_REGISTERED */
ESP_ERROR_CHECK(esp_sip_service_call(sip, "1002"));
```

Answer an incoming call from the event handler with `esp_sip_service_answer()`, and end it with `esp_sip_service_bye()`. The service stays online for the next call.

### Signaling: P2P without a registrar

Never registers. The "server" of the URI is the other device, and `call()` invites it directly.

```c
esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
setup.p2p_mode = true;
ESP_ERROR_CHECK(esp_sip_service_setup(sip, &setup));
/* the "server" is simply the other device, and its port is also the one
   the stack binds locally in P2P mode */
esp_sip_service_account_t account = {
    .transport = "udp",
    .user      = "1001",
    .server    = "192.168.1.21",
    .port      = 5060,
};
ESP_ERROR_CHECK(esp_sip_service_set_account(sip, &account));
```

### Media: two-way, receive-only or none

Both examples above link a capture to the uplink and the downlink to a player, which is what makes them two-way. That half is interchangeable, and neither choice changes the signaling:

- **Two-way** — link capture to the uplink and the downlink to a player, as above. The linked tracks supply the codecs, so `setup` needs no codec.
- **Receive-only** — link only the downlink to a player and name the codec yourself in `setup.audio_codec` / `video_codec` (for example `ESP_FOURCC_ALAW`), since no uplink track can supply it. A doorbell or a listening device.
- **No media** — link neither side and leave both codecs at 0. The endpoint still registers and still exchanges SIP MESSAGE, it just never negotiates RTP. Compiling both directions out with Kconfig is the same mode.

## Setup and Account

`esp_sip_service_setup()` carries only what has no usable default: `audio_codec`, `video_codec` and `p2p_mode`. It requires the service to be stopped, and `NULL` restores those three.

`p2p_mode` is what separates the two session styles, not the codecs: left false the service registers with the server named in the uri, set true it invites that address directly and never registers. A registered endpoint whose codec comes from a linked uplink track therefore needs no `setup()` call at all.

Everything else already has a default and sits behind its own setter, each of which also requires a stopped service and treats 0 or `NULL` as "back to the default":

| Call | Covers |
| --- | --- |
| `esp_sip_service_set_local_port()` | Fixed local SIP port |
| `esp_sip_service_set_timeout()` | Connect and read/write timeouts |
| `esp_sip_service_set_nat_traversal()` | Keep-alive interval, OPTIONS ping, RFC3581 public address |
| `esp_sip_service_set_register_refresh()` | Refresh interval, suspend during a call |
| `esp_sip_service_set_srtp_mode()` | SDES-SRTP negotiation |
| `esp_sip_service_set_identity()` | User-Agent and domain |
| `esp_sip_service_set_frame_size()` | RTP frame buffers |
| `esp_sip_service_set_cache_size()` | Downlink track caches |
| `esp_sip_service_set_video_payload_type()` | SDP video payload type |

Use `esp_sip_service_set_account()` to build the URI from parts, or `esp_sip_service_set_uri()` to pass `transport://user:password@server:port` directly. Neither overwrites `setup` or any of the setters above. `esp_sip_service_set_local_addr()` overrides the address the stack binds and advertises; without it the default interface is used.

Codec resolution at start: SIP negotiates one codec per media type, and a stream reaches the SDP once a codec is known for it. A linked uplink track supplies it and wins; otherwise `audio_codec` / `video_codec` does. Neither leaves that stream out, so nothing known at all is a signaling-only start. The downlink tracks are created from the same resolved codec when the call reports a session begin.

## Configuration

`ESP_SIP_SERVICE_UPLINK_SUPPORT` and `ESP_SIP_SERVICE_DOWNLINK_SUPPORT` build the two directions in or out; `get_role` reports whichever are enabled. Either side may be compiled out. Disabling both, or leaving both setup codecs at 0 with nothing linked, is signaling-only.

`ESP_SIP_SERVICE_MCP_ENABLE` adds the MCP tools.

## Events

Subscribe with `esp_service_event_subscribe()` on `ESP_SERVICE_BASE(sip)`. Native SIP events are published as `ESP_SIP_SERVICE_EVENT_REGISTERED` through `ESP_SIP_SERVICE_EVENT_KEEPALIVE`, plus `ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED`. Base lifecycle transitions continue to use `ESP_SERVICE_EVENT_STATE_CHANGED`.

Call, hangup, error, MESSAGE and DTMF events each carry their own payload type (`esp_sip_service_call_payload_t`, `esp_sip_service_hangup_payload_t`, `esp_sip_service_error_payload_t`, `esp_sip_service_message_payload_t`, `esp_sip_service_dtmf_payload_t`). Other events have no payload. The underlying protocol exposes the peer name, hangup reason and MESSAGE body as internal storage that is only valid inside its own callback, so the service copies them into a dedicated heap payload before publishing, and subscribers must never call back into the stack for them. The hub releases that payload after the subscriber callback returns, or after the last queue-mode `esp_service_event_delivery_done()`, so copy anything needed beyond that point.

`ESP_SIP_SERVICE_EVENT_INCOMING` repeats while the peer keeps ringing, so trigger auto-answer only on the first one.

## Points of Attention

- `setup`, `set_uri`, `set_account`, `set_local_addr` and uplink `link` / `unlink` require the service to be stopped
- Call actions require the service to be running
- Only one SIP MESSAGE may be in flight; `esp_sip_service_send_message()` returns `ESP_ERR_INVALID_STATE` until `ESP_SIP_SERVICE_EVENT_MESSAGE_SENT` arrives
- An invalid stream id returns `ESP_ERR_INVALID_ARG`; the two valid ones are 0 and 1
- Stop producers before their consuming service during teardown
- Tear down with `esp_media_service_deinit()` and then free the service handle

## Protocol Library Limitations

These come from the pre-compiled `esp_media_protocols` stack and are worked around, not fixed, in this component:

- An empty password is `user:@host`. `set_account()` writes that form when the password is omitted.
- In P2P mode the stack replaces `fixed_local_port` with the port taken from the URI and binds `INADDR_ANY`, so the local port always equals the peer port and `esp_sip_service_set_local_port()` has no effect. Two instances on one device therefore cannot call each other, because they would share a socket.
- `esp_rtc_call()` needs a `user@host:port` target in P2P mode, so `esp_sip_service_call()` completes a bare user name from the host and port of the configured URI.
- `esp_rtc_config_t` exposes no stack size or priority, so `esp_sip_scheduler.h` is a placeholder and thread resources cannot be overridden yet. `esp_rtc_service_init()` spawns its own task and returns immediately, so `start` does not block until registration completes; wait for `ESP_SIP_SERVICE_EVENT_REGISTERED` instead.

## Example

See [`examples/sip_service`](./examples/sip_service/README.md) for a P2P call placed by a single instance with dummy media. It needs neither a registrar nor a connection to an external Wi-Fi access point; the example starts a local SoftAP to provide the active network interface required by lwIP.

For a full endpoint with a microphone, speaker and console, see `adf_examples/protocols/sip_cli`.

## MCP Tools

When `CONFIG_ESP_SIP_SERVICE_MCP_ENABLE=y` (depends on `CONFIG_ESP_MCP_ENABLE`):

- Setup/control tools mirror the C API: `setup`, the nine `set_*` tuning calls, `set_uri`, `set_account`, `set_local_addr`, `start`, `stop`, `call`, `answer`, `bye`, `send_dtmf`, `send_message` and the SRTP and peer queries
- Register with `esp_sip_service_mcp_schema_get()` and `esp_sip_service_tool_invoke()`
- Media link/unlink remains in `esp_media_service` MCP; frames never go over MCP

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)
