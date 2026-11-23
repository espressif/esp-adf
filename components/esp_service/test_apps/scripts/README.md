# MCP smoke client

Enable the matching transport and `ESP_SVC_TEST_*_HOLD_AFTER_SMOKE` option in
the [test firmware](../README.md). Run from `test_apps/` against a test device:
some checks invoke tools and change device state.

```bash
python3 scripts/test_mcp.py http <device-ip> [port]
python3 scripts/test_mcp.py sse  <device-ip> [port]
python3 scripts/test_mcp.py ws   <device-ip> [port]
python3 scripts/test_mcp.py uart <serial-port> [baud]
```

| Mode | Python dependency | Default |
| --- | --- | --- |
| HTTP / SSE | `requests` | Port 8080 |
| WebSocket | `websockets` | Port 8081 |
| UART | `pyserial` | 115200 baud |

Set `MCP_AUTH_TOKEN` to the device token to enable bearer checks. HTTP expects
CORS on by default; WebSocket expects it off. Set `MCP_CORS` to `0` or `1`
to match the firmware. For HTTP, `MCP_SKIP_408=1` skips the slow-body check.

On ESP32-S3 UART0, the default TX/RX pins are GPIO43/GPIO44. Cross TX/RX and
share GND if using an external adapter, or set the test pins in menuconfig.
