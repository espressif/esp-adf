# Changelog

## v0.5.0

### Features

- Initial version of `esp_sip_service`
  - Support SIP registration, outgoing calls, incoming calls, answer and hangup
  - Support full-duplex media on a single instance as `ESP_MEDIA_ROLE_SRC_SINK`
    (stream 0 uplink sink, stream 1 downlink source)
  - Support SDES-SRTP negotiation modes, out-of-band DTMF (RFC2833) and SIP MESSAGE
  - Support custom INVITE headers, private headers and raw header inspection
  - Support P2P mode that skips registration for direct device-to-device calls
  - Support media-service links and MCP control tools
