#!/usr/bin/env python3

"""
RTMP service MCP UART test client.

Usage:
    python3 scripts/test_rtmp_mcp_uart.py <serial-port> [baud-rate]
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
    'esp_rtmp_service_setup',
    'esp_rtmp_service_set_url',
    'esp_rtmp_service_start',
    'esp_rtmp_service_stop',
    'esp_rtmp_service_query',
    'esp_media_service_link',
    'esp_media_service_unlink',
    'esp_media_dummy_service_start',
    'esp_media_dummy_service_stop',
}

SRC_NAME = 'media_dummy_src'
RTMP_NAME = 'esp_rtmp_service'


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
            port=self.port, baudrate=self.baud_rate,
            bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE, timeout=self.timeout, write_timeout=self.timeout,
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
        self.ser = None

    def send_raw(self, data):
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
        request = {'jsonrpc': '2.0', 'id': self._next_id(), 'method': method}
        if params is not None:
            request['params'] = params
        return json.loads(self.send_raw(json.dumps(request, separators=(',', ':'))))

    def list_tools(self):
        return self.call('tools/list')

    def call_tool(self, name, arguments=None):
        return self.call('tools/call', {'name': name, 'arguments': arguments or {}})


def assert_response_ok(response, context):
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
    text = content[0].get('text', '') if content else ''
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError:
        parsed = text
    return parsed, is_error


def expect_ok(parsed, context):
    if not isinstance(parsed, dict) or not parsed.get('ok', False):
        raise AssertionError(f'{context}: expected ok=true, got {parsed}')


def run_tests(client, run_seconds, push_url):
    listed = assert_response_ok(client.list_tools(), 'tools/list')
    tool_names = {tool.get('name') for tool in listed.get('tools', [])}
    missing = sorted(EXPECTED_TOOLS - tool_names)
    if missing:
        raise AssertionError(f'Missing MCP tools: {missing}')
    print(f'Found all {len(EXPECTED_TOOLS)} expected MCP tools')

    setup, _ = parse_tool_text_result(
        client.call_tool('esp_rtmp_service_setup', {'chunk_size': 128}),
        'esp_rtmp_service_setup',
    )
    expect_ok(setup, 'setup')

    set_url, _ = parse_tool_text_result(
        client.call_tool('esp_rtmp_service_set_url', {'url': push_url}),
        'esp_rtmp_service_set_url',
    )
    expect_ok(set_url, 'set_url')

    linked, _ = parse_tool_text_result(
        client.call_tool('esp_media_service_link', {
            'src_name': SRC_NAME, 'src_stream': 0,
            'sink_name': RTMP_NAME, 'sink_stream': 0,
        }),
        'link',
    )
    expect_ok(linked, 'link')

    rtmp_start, _ = parse_tool_text_result(client.call_tool('esp_rtmp_service_start'), 'start')
    expect_ok(rtmp_start, 'rtmp start')
    src_start, _ = parse_tool_text_result(client.call_tool('esp_media_dummy_service_start'), 'dummy start')
    expect_ok(src_start, 'dummy start')
    time.sleep(run_seconds)

    # Stop sink first so the push task releases the provider before src teardown.
    parse_tool_text_result(client.call_tool('esp_rtmp_service_stop'), 'rtmp stop')
    parse_tool_text_result(client.call_tool('esp_media_dummy_service_stop'), 'dummy stop')
    parse_tool_text_result(client.call_tool('esp_media_service_unlink', {
        'src_name': SRC_NAME, 'src_stream': 0,
        'sink_name': RTMP_NAME, 'sink_stream': 0,
    }), 'unlink')

    query, query_err = parse_tool_text_result(
        client.call_tool('esp_rtmp_service_query'),
        'esp_rtmp_service_query',
        allow_tool_error=True,
    )
    print('query:', query, 'tool_error:', query_err)
    print('\nAll RTMP MCP UART checks passed')


def main():
    parser = argparse.ArgumentParser(description='Test RTMP MCP tools over UART')
    parser.add_argument('port')
    parser.add_argument('baud', nargs='?', type=int, default=115200)
    parser.add_argument('--timeout', type=float, default=15.0)
    parser.add_argument('--run-seconds', type=float, default=2.0)
    parser.add_argument('--push-url', default='rtmp://127.0.0.1:1935/live/stream0')
    args = parser.parse_args()
    try:
        with MCPUartClient(args.port, args.baud, args.timeout) as client:
            run_tests(client, args.run_seconds, args.push_url)
    except Exception as exc:
        print(f'\n[FAIL] {exc}')
        return 1
    print('\n[PASS]')
    return 0


if __name__ == '__main__':
    sys.exit(main())
