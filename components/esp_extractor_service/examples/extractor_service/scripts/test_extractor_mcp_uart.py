#!/usr/bin/env python3

"""
Extractor service MCP UART test client.

Usage:
    python3 scripts/test_extractor_mcp_uart.py <serial-port> [baud-rate]
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
    'esp_extractor_service_set_extract_mask',
    'esp_extractor_service_set_out_pool_size',
    'esp_extractor_service_set_url',
    'esp_extractor_service_start',
    'esp_extractor_service_stop',
    'esp_media_service_link',
    'esp_media_service_unlink',
    'esp_media_dummy_service_start',
    'esp_media_dummy_service_stop',
    'esp_media_dummy_service_get_stats',
}

SRC_NAME = 'esp_extractor_service'
SINK_NAME = 'media_dummy_sink'


class MCPUartClient:
    def __init__(self, port, baud_rate=115200, timeout=8.0):
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


def run_tests(client, run_seconds, url):
    listed = assert_response_ok(client.list_tools(), 'tools/list')
    tool_names = {tool.get('name') for tool in listed.get('tools', [])}
    missing = sorted(EXPECTED_TOOLS - tool_names)
    if missing:
        raise AssertionError(f'Missing MCP tools: {missing}')
    print(f'Found all {len(EXPECTED_TOOLS)} expected MCP tools')

    expect_ok(parse_tool_text_result(
        client.call_tool('esp_extractor_service_set_extract_mask', {'mask': 'av'}),
        'set_extract_mask')[0], 'mask')
    expect_ok(parse_tool_text_result(
        client.call_tool('esp_extractor_service_set_out_pool_size', {'out_pool_size': 65536}),
        'set_out_pool_size')[0], 'pool')
    expect_ok(parse_tool_text_result(
        client.call_tool('esp_extractor_service_set_url', {'url': url}),
        'set_url')[0], 'set_url')
    expect_ok(parse_tool_text_result(client.call_tool('esp_media_service_link', {
        'src_name': SRC_NAME, 'src_stream': 0,
        'sink_name': SINK_NAME, 'sink_stream': 0,
    }), 'link')[0], 'link')

    expect_ok(parse_tool_text_result(client.call_tool('esp_media_dummy_service_start'), 'sink start')[0], 'sink start')
    expect_ok(parse_tool_text_result(client.call_tool('esp_extractor_service_start'), 'extractor start')[0], 'extractor start')
    time.sleep(run_seconds)
    parse_tool_text_result(client.call_tool('esp_extractor_service_stop'), 'extractor stop')
    parse_tool_text_result(client.call_tool('esp_media_dummy_service_stop'), 'sink stop')

    stats, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_get_stats', {'stream': 0}),
        'get_stats',
        allow_tool_error=True,
    )
    print('stats:', stats)
    parse_tool_text_result(client.call_tool('esp_media_service_unlink', {
        'src_name': SRC_NAME, 'src_stream': 0,
        'sink_name': SINK_NAME, 'sink_stream': 0,
    }), 'unlink')
    print('\nAll extractor MCP UART checks passed')


def main():
    parser = argparse.ArgumentParser(description='Test extractor MCP tools over UART')
    parser.add_argument('port')
    parser.add_argument('baud', nargs='?', type=int, default=115200)
    parser.add_argument('--timeout', type=float, default=8.0)
    parser.add_argument('--run-seconds', type=float, default=3.0)
    parser.add_argument('--url', default='/sdcard/video/test1.mp4')
    args = parser.parse_args()
    try:
        with MCPUartClient(args.port, args.baud, args.timeout) as client:
            run_tests(client, args.run_seconds, args.url)
    except Exception as exc:
        print(f'\n[FAIL] {exc}')
        return 1
    print('\n[PASS]')
    return 0


if __name__ == '__main__':
    sys.exit(main())
