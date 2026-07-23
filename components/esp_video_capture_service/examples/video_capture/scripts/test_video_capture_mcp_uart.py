#!/usr/bin/env python3

"""
Video capture service MCP UART test client.

Protocol:
  - Send: one JSON-RPC request per line, terminated by '\\n'
  - Receive: one JSON-RPC response per line, terminated by '\\n'

Usage:
    python3 scripts/test_video_capture_mcp_uart.py <serial-port> [baud-rate]

Example:
    python3 scripts/test_video_capture_mcp_uart.py /dev/ttyUSB1 115200

Requirements:
    pip install pyserial
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
    'esp_video_capture_service_apply_setup',
    'esp_video_capture_service_start',
    'esp_video_capture_service_stop',
    'esp_video_capture_service_enable_stream',
    'esp_video_capture_service_start_record',
    'esp_video_capture_service_stop_record',
    'esp_video_capture_service_set_storage_url',
    'esp_video_capture_service_get_last_storage_url',
    'esp_video_capture_service_get_storage_file_info',
    'esp_video_capture_service_overlay_enable_redraw',
    'esp_video_capture_service_get_status',
    'esp_media_service_link',
    'esp_media_service_unlink',
    'esp_media_dummy_service_start',
    'esp_media_dummy_service_stop',
    'esp_media_dummy_service_get_stats',
}

CAPTURE_NAME = 'video-rec'
SINK_NAME = 'media_dummy_sink'


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

    print('\n== Test: esp_video_capture_service_get_status ==')
    status, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_get_status'),
        'esp_video_capture_service_get_status',
    )
    print_json('Status:', status)

    print('\n== Test: esp_video_capture_service_apply_setup ==')
    setup, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_apply_setup', {
            'fixed_src_sample_rate': 16000,
            'streams': [{
                'enabled': True,
                'video_codec': 'H264',
                'width': 640,
                'height': 480,
                'fps': 10,
                'audio_codec': 'AAC ',
                'sample_rate': 16000,
                'bits_per_sample': 16,
                'channel': 1,
                'audio_bitrate': 64000,
            }],
        }),
        'esp_video_capture_service_apply_setup',
    )
    expect_ok(setup, 'apply_setup')
    print_json('Apply setup:', setup)

    print('\n== Test: esp_media_service_link ==')
    linked, _ = parse_tool_text_result(
        client.call_tool('esp_media_service_link', {
            'src_name': CAPTURE_NAME,
            'src_stream': 0,
            'sink_name': SINK_NAME,
            'sink_stream': 0,
        }),
        'esp_media_service_link',
    )
    expect_ok(linked, 'link')
    print_json('Link:', linked)

    print('\n== Test: start sink then capture ==')
    sink_start, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_start'),
        'esp_media_dummy_service_start',
    )
    expect_ok(sink_start, 'sink start')
    cap_start, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_start'),
        'esp_video_capture_service_start',
    )
    expect_ok(cap_start, 'capture start')

    print(f'\n== Wait {run_seconds}s for frames ==')
    time.sleep(run_seconds)

    print('\n== Test: stop capture then sink ==')
    cap_stop, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_stop'),
        'esp_video_capture_service_stop',
    )
    expect_ok(cap_stop, 'capture stop')
    sink_stop, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_stop'),
        'esp_media_dummy_service_stop',
    )
    expect_ok(sink_stop, 'sink stop')

    print('\n== Test: esp_media_dummy_service_get_stats ==')
    stats, _ = parse_tool_text_result(
        client.call_tool('esp_media_dummy_service_get_stats', {'stream': 0}),
        'esp_media_dummy_service_get_stats',
    )
    print_json('Stats:', stats)
    if not isinstance(stats, dict):
        raise AssertionError(f'Invalid stats response: {stats}')
    if int(stats.get('video_frame_count', 0)) <= 0 and int(stats.get('audio_frame_count', 0)) <= 0:
        raise AssertionError(f'Expected video or audio frames > 0, got {stats}')

    print('\n== Test: esp_media_service_unlink (after streaming) ==')
    unlinked, _ = parse_tool_text_result(
        client.call_tool('esp_media_service_unlink', {
            'src_name': CAPTURE_NAME,
            'src_stream': 0,
            'sink_name': SINK_NAME,
            'sink_stream': 0,
        }),
        'esp_media_service_unlink',
    )
    expect_ok(unlinked, 'unlink')
    print_json('Unlink:', unlinked)

    storage_url = '/sdcard/video_capture/mcp_video.mp4'
    print('\n== Test: re-setup with muxer + overlay for storage / control tools ==')
    setup_storage, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_apply_setup', {
            'fixed_src_sample_rate': 16000,
            'share_overlay': True,
            'overlay': {
                'enabled': True,
                'show_camera_type': True,
                'show_datetime': True,
                'camera_type': 'mcp',
            },
            'streams': [{
                'enabled': True,
                'video_codec': 'H264',
                'width': 640,
                'height': 480,
                'fps': 10,
                'audio_codec': 'AAC ',
                'sample_rate': 16000,
                'bits_per_sample': 16,
                'channel': 1,
                'audio_bitrate': 64000,
                'muxer_type': 'MP4 ',
            }],
        }),
        'esp_video_capture_service_apply_setup (storage)',
    )
    expect_ok(setup_storage, 'apply_setup storage')
    print_json('Apply setup (storage/overlay):', setup_storage)

    set_url, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_set_storage_url', {
            'stream': 0,
            'url': storage_url,
        }),
        'esp_video_capture_service_set_storage_url',
    )
    expect_ok(set_url, 'set_storage_url')
    print_json('Set storage URL:', set_url)

    print('\n== Test: start capture for record / overlay / enable_stream ==')
    cap_start2, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_start'),
        'esp_video_capture_service_start (storage)',
    )
    expect_ok(cap_start2, 'capture start storage')

    enable_off, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_enable_stream', {'stream': 0, 'enable': False}),
        'esp_video_capture_service_enable_stream disable',
    )
    expect_ok(enable_off, 'enable_stream disable')
    print_json('Disable stream:', enable_off)

    enable_on, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_enable_stream', {'stream': 0, 'enable': True}),
        'esp_video_capture_service_enable_stream enable',
    )
    expect_ok(enable_on, 'enable_stream enable')
    print_json('Enable stream:', enable_on)

    overlay_on, overlay_err = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_overlay_enable_redraw', {'enable': True}),
        'esp_video_capture_service_overlay_enable_redraw enable',
        allow_tool_error=True,
    )
    print_json('Overlay redraw enable:', overlay_on)
    if overlay_err:
        print('Overlay redraw enable returned a tool error; continuing if board/overlay path is unavailable')

    overlay_off, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_overlay_enable_redraw', {'enable': False}),
        'esp_video_capture_service_overlay_enable_redraw disable',
        allow_tool_error=True,
    )
    print_json('Overlay redraw disable:', overlay_off)

    start_rec, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_start_record', {'stream': 0}),
        'esp_video_capture_service_start_record',
    )
    expect_ok(start_rec, 'start_record')
    print_json('Start record:', start_rec)

    print(f'\n== Wait {run_seconds}s while recording ==')
    time.sleep(run_seconds)

    stop_rec, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_stop_record', {'stream': 0}),
        'esp_video_capture_service_stop_record',
    )
    expect_ok(stop_rec, 'stop_record')
    print_json('Stop record:', stop_rec)

    last_url, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_get_last_storage_url', {'stream': 0}),
        'esp_video_capture_service_get_last_storage_url',
    )
    print_json('Last storage URL:', last_url)
    if not isinstance(last_url, dict) or not last_url.get('ok', False):
        raise AssertionError(f'get_last_storage_url expected ok=true, got {last_url}')
    if last_url.get('url') != storage_url:
        raise AssertionError(f'get_last_storage_url expected {storage_url}, got {last_url}')

    file_info, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_get_storage_file_info', {'stream': 0}),
        'esp_video_capture_service_get_storage_file_info',
    )
    print_json('Storage file info:', file_info)
    if not isinstance(file_info, dict) or not file_info.get('ok', False):
        raise AssertionError(f'get_storage_file_info expected ok=true, got {file_info}')
    if file_info.get('url') != storage_url:
        raise AssertionError(f'get_storage_file_info expected url={storage_url}, got {file_info}')
    if int(file_info.get('size', 0)) <= 0:
        raise AssertionError(f'get_storage_file_info expected size > 0, got {file_info}')

    cap_stop2, _ = parse_tool_text_result(
        client.call_tool('esp_video_capture_service_stop'),
        'esp_video_capture_service_stop (storage)',
    )
    expect_ok(cap_stop2, 'capture stop storage')
    print_json('Stop capture after record:', cap_stop2)

    print('\nAll video capture MCP UART checks passed')


def main():
    parser = argparse.ArgumentParser(description='Test video capture MCP tools over UART')
    parser.add_argument('port', help='Serial port connected to the MCP UART pins')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='UART baud rate')
    parser.add_argument('--timeout', type=float, default=5.0, help='Serial read timeout in seconds')
    parser.add_argument('--run-seconds', type=float, default=2.0, help='Seconds to stream before reading stats')
    args = parser.parse_args()

    print('=' * 72)
    print('  Video Capture Service MCP UART Test')
    print('=' * 72)
    print(f'Port:        {args.port}')
    print(f'Baud:        {args.baud}')
    print(f'Timeout:     {args.timeout}s')
    print(f'Run seconds: {args.run_seconds}s')

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
