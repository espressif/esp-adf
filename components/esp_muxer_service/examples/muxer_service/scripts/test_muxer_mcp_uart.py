#!/usr/bin/env python3

"""
Muxer service MCP UART test client.

Usage:
    python3 scripts/test_muxer_mcp_uart.py <serial-port> [baud-rate]
"""

import argparse
import json
import sys
import time

try:
    import serial
except ImportError:
    print("Error: 'pyserial' package required. Install with: pip install pyserial")
    sys.exit(1)


EXPECTED_TOOLS = {
    'esp_muxer_service_setup',
    'esp_muxer_service_set_storage_url',
    'esp_muxer_service_start',
    'esp_muxer_service_stop',
    'esp_media_service_link',
    'esp_media_service_unlink',
    'esp_media_dummy_service_start',
    'esp_media_dummy_service_stop',
}

SRC_NAME = 'media_dummy_src'
MUXER_NAME = 'esp_muxer_service'


class MCPUartClient:
    def __init__(self, port, baud_rate=115200, timeout=5.0):
        self.port = port
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser = None
        self.request_id = 0

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.disconnect()

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    def connect(self):
        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout,
            write_timeout=self.timeout,
        )
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        time.sleep(0.1)
        while self.ser.in_waiting:
            self.ser.read(self.ser.in_waiting)
            time.sleep(0.05)
        print(f'Connected to {self.port} @ {self.baud_rate} baud')

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f'Disconnected from {self.port}')
        self.ser = None

    def send_raw(self, data):
        if not self.ser or not self.ser.is_open:
            raise RuntimeError('Serial port is not open')
        line = data.strip() + '\n'
        self.ser.write(line.encode('utf-8'))
        self.ser.flush()
        print(f'>>> {data}')
        start_time = time.time()
        while time.time() - start_time < self.timeout:
            raw = self.ser.readline()
            if not raw:
                continue
            text = raw.decode('utf-8', errors='replace').strip()
            if not text:
                continue
            if text.startswith('{'):
                print(f'<<< {text}')
                return text
            print(f'--- skip non-json: {text}')
        raise TimeoutError('Timed out waiting for a JSON-RPC response')

    def call(self, method, params=None):
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method,
        }
        if params is not None:
            request['params'] = params
        response = self.send_raw(json.dumps(request, separators=(',', ':')))
        return json.loads(response)

    def list_tools(self):
        return self.call('tools/list')

    def call_tool(self, name, arguments=None):
        return self.call('tools/call', {
            'name': name,
            'arguments': arguments or {},
        })


def assert_response_ok(response, context):
    if response is None:
        raise AssertionError(f'{context}: no response')
    if 'error' in response:
        raise AssertionError(f'{context}: JSON-RPC error: {response["error"]}')
    if 'result' not in response:
        raise AssertionError(f'{context}: missing result field: {response}')
    return response['result']


def parse_tool_text_result(response, context, allow_tool_error=False):
    result = assert_response_ok(response, context)
    is_error = bool(result.get('isError', False))
    if is_error and not allow_tool_error:
        raise AssertionError(f'{context}: tool returned error: {result}')
    content = result.get('content', [])
    if not content:
        raise AssertionError(f'{context}: missing MCP content: {result}')
    text = content[0].get('text', '')
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        parsed = text
    return parsed, is_error


def print_json(title, value):
    print(f'\n{title}')
    print(json.dumps(value, indent=2, ensure_ascii=False) if not isinstance(value, str) else value)


def expect_ok(parsed, context):
    if not isinstance(parsed, dict) or not parsed.get('ok', False):
        raise AssertionError(f'{context}: expected ok=true, got {parsed}')


def run_tests(client, run_seconds):
    print('\n== Test: tools/list ==')
    listed = assert_response_ok(client.list_tools(), 'tools/list')
    tools = listed.get('tools', [])
    tool_names = {tool.get('name') for tool in tools}
    missing = sorted(EXPECTED_TOOLS - tool_names)
    if missing:
        raise AssertionError(f'Missing MCP tools: {missing}')
    print(f'Found all {len(EXPECTED_TOOLS)} expected MCP tools')

    print('\n== Test: esp_muxer_service_setup ==')
    setup, _ = parse_tool_text_result(
        client.call_tool('esp_muxer_service_setup', {
            'muxer_type': 'TS',
            'mode': 'streaming_only',
            'ram_cache_size': 16384,
        }),
        'esp_muxer_service_setup',
    )
    expect_ok(setup, 'setup')
    print_json('Setup:', setup)

    print('\n== Test: esp_media_service_link ==')
    linked, _ = parse_tool_text_result(
        client.call_tool('esp_media_service_link', {
            'src_name': SRC_NAME,
            'src_stream': 0,
            'sink_name': MUXER_NAME,
            'sink_stream': 0,
        }),
        'esp_media_service_link',
    )
    expect_ok(linked, 'link')
    print_json('Link:', linked)

    print('\n== Test: start muxer then dummy src ==')
    muxer_start, _ = parse_tool_text_result(
        client.call_tool('esp_muxer_service_start'),
        'esp_muxer_service_start',
    )
    expect_ok(muxer_start, 'muxer start')
    src_start, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_start'),
        'esp_media_dummy_service_start',
    )
    expect_ok(src_start, 'dummy start')

    print(f'\n== Wait {run_seconds}s ==')
    time.sleep(run_seconds)

    print('\n== Test: stop dummy src then muxer ==')
    src_stop, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_stop'),
        'esp_media_dummy_service_stop',
    )
    expect_ok(src_stop, 'dummy stop')
    muxer_stop, _ = parse_tool_text_result(
        client.call_tool('esp_muxer_service_stop'),
        'esp_muxer_service_stop',
    )
    expect_ok(muxer_stop, 'muxer stop')

    print('\n== Test: unlink then set_storage_url ==')
    unlinked, _ = parse_tool_text_result(
        client.call_tool('esp_media_service_unlink', {
            'src_name': SRC_NAME,
            'src_stream': 0,
            'sink_name': MUXER_NAME,
            'sink_stream': 0,
        }),
        'esp_media_service_unlink',
    )
    expect_ok(unlinked, 'unlink')

    set_url, _ = parse_tool_text_result(
        client.call_tool('esp_muxer_service_set_storage_url', {
            'url': '/sdcard/muxer/mcp.ts',
        }),
        'esp_muxer_service_set_storage_url',
    )
    expect_ok(set_url, 'set_storage_url')
    print_json('Set storage URL:', set_url)
    print('\nAll muxer MCP UART checks passed')


def main():
    parser = argparse.ArgumentParser(description='Test muxer MCP tools over UART')
    parser.add_argument('port', help='Serial port connected to the MCP UART pins')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='UART baud rate')
    parser.add_argument('--timeout', type=float, default=5.0, help='Serial read timeout in seconds')
    parser.add_argument('--run-seconds', type=float, default=2.0, help='Seconds to stream before stop')
    args = parser.parse_args()

    print('=' * 72)
    print('  Muxer Service MCP UART Test')
    print('=' * 72)
    try:
        with MCPUartClient(args.port, args.baud, args.timeout) as client:
            run_tests(client, args.run_seconds)
    except Exception as exc:
        print(f'\n[FAIL] {exc}')
        return 1
    print('\n[PASS]')
    return 0


if __name__ == '__main__':
    sys.exit(main())
