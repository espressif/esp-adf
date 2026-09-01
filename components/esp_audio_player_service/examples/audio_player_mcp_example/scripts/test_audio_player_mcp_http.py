#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
#
# SPDX-License-Identifier: Apache-2.0

"""
Audio Player Service MCP HTTP test client.

Protocol:
  - POST JSON-RPC to http://<device-ip>:<port>/mcp

Usage:
    python3 scripts/test_audio_player_mcp_http.py <device-ip>
    python3 scripts/test_audio_player_mcp_http.py --url http://192.168.4.1:8080/mcp

SoftAP default (this example):
    Connect PC Wi-Fi to SSID "esp-audio-mcp", then:
    python3 scripts/test_audio_player_mcp_http.py 192.168.4.1

By default the script also plays file:///sdcard/test.mp3 (set_url + play).
Skip with --no-play.

Requirements:
    pip install requests
"""

from __future__ import annotations

import argparse
import json
import sys
import time

try:
    import requests
except ImportError:
    print("Error: 'requests' package required. Install with: pip install requests")
    sys.exit(1)

DEFAULT_MEDIA_URI = 'file:///sdcard/test.mp3'


AUDIO_TOOLS = {
    'esp_audio_player_service_get_status',
    'esp_audio_player_service_set_output_volume',
    'esp_audio_player_service_get_output_volume',
    'esp_audio_player_service_set_volume',
    'esp_audio_player_service_get_volume',
    'esp_audio_player_service_set_url',
    'esp_audio_player_service_play',
    'esp_audio_player_service_pause',
    'esp_audio_player_service_resume',
    'esp_audio_player_service_stop',
    'esp_audio_player_service_seek',
    'esp_audio_player_service_set_speed',
    'esp_audio_player_service_playlist_next',
    'esp_audio_player_service_playlist_prev',
    'esp_audio_player_service_playlist_play_index',
    'esp_audio_player_service_set_repeat_mode',
    'esp_audio_player_service_enable_id3_parse',
    'esp_audio_player_service_get_id3_info',
    'esp_audio_player_service_set_mix_cfg',
    'esp_audio_player_service_get_preempt_state',
}


class MCPHttpClient:
    def __init__(self, base_url, timeout=5.0):
        self.base_url = base_url.rstrip('/')
        self.timeout = timeout
        self.request_id = 0

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    def call(self, method, params=None):
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method,
        }
        if params is not None:
            request['params'] = params

        print(f'\n>>> {json.dumps(request, separators=(",", ":"))}')
        response = requests.post(
            self.base_url,
            json=request,
            headers={'Content-Type': 'application/json'},
            timeout=self.timeout,
        )
        response.raise_for_status()
        result = response.json()
        print(f'<<< {json.dumps(result, separators=(",", ":"))}')
        return result

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


def run_tests(client):
    print('\n== Test: tools/list ==')
    listed = assert_response_ok(client.list_tools(), 'tools/list')
    tools = listed.get('tools', [])
    tool_names = {tool.get('name') for tool in tools}
    missing = sorted(AUDIO_TOOLS - tool_names)
    if missing:
        raise AssertionError(f'Missing audio player MCP tools: {missing}')
    print(f'Found all {len(AUDIO_TOOLS)} audio player MCP tools')

    print('\n== Test: set_output_volume / get_output_volume ==')
    set_out, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_set_output_volume', {'volume': 70}),
        'set_output_volume',
    )
    if is_error or not isinstance(set_out, dict) or set_out.get('ok') is not True:
        raise AssertionError(f'Invalid set_output_volume response: {set_out}')

    get_out, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_get_output_volume'),
        'get_output_volume',
    )
    if is_error or not isinstance(get_out, dict) or get_out.get('volume') != 70:
        raise AssertionError(f'Invalid get_output_volume response: {get_out}')
    print_json('Output volume:', get_out)

    print('\n== Test: set_volume / get_status ==')
    set_vol, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_set_volume', {'stream': 0, 'volume': 55}),
        'set_volume',
    )
    if is_error or not isinstance(set_vol, dict) or set_vol.get('ok') is not True:
        raise AssertionError(f'Invalid set_volume response: {set_vol}')

    status, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_get_status', {'stream': 0}),
        'get_status',
    )
    if is_error or not isinstance(status, dict):
        raise AssertionError(f'Invalid get_status response: {status}')
    for key in ('state', 'volume', 'output_volume'):
        if key not in status:
            raise AssertionError(f'Status response missing key: {key}')
    if status.get('volume') != 55 or status.get('output_volume') != 70:
        raise AssertionError(f'Status volume mismatch: {status}')
    print_json('Status:', status)


def run_play_test(client, media_uri, wait_s=8.0, poll_s=0.5):
    print(f'\n== Test: set_url / play ({media_uri}) ==')
    set_url, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_set_url', {
            'stream': 0,
            'url': media_uri,
        }),
        'set_url',
    )
    if is_error or not isinstance(set_url, dict) or set_url.get('ok') is not True:
        raise AssertionError(f'Invalid set_url response: {set_url}')

    play, is_error = parse_tool_text_result(
        client.call_tool('esp_audio_player_service_play', {'stream': 0}),
        'play',
    )
    if is_error or not isinstance(play, dict) or play.get('ok') is not True:
        raise AssertionError(f'Invalid play response: {play}')

    deadline = time.time() + wait_s
    last_status = None
    while time.time() < deadline:
        status, is_error = parse_tool_text_result(
            client.call_tool('esp_audio_player_service_get_status', {'stream': 0}),
            'get_status(after play)',
        )
        if is_error or not isinstance(status, dict):
            raise AssertionError(f'Invalid get_status after play: {status}')
        last_status = status
        state = status.get('state')
        if state == 'PLAYING':
            print_json('Playing status:', status)
            return
        if state == 'ERROR':
            raise AssertionError(f'Playback entered ERROR: {status}')
        time.sleep(poll_s)

    raise AssertionError(
        f'Timed out waiting for PLAYING within {wait_s}s; last status={last_status}. '
        f'Check that {media_uri} exists on the device SD card.'
    )


def main():
    parser = argparse.ArgumentParser(description='Test esp_audio_player_service MCP tools over HTTP')
    parser.add_argument('host', nargs='?', help='Device IP (default SoftAP: 192.168.4.1)')
    parser.add_argument('--url', help='Full MCP URL, e.g. http://192.168.4.1:8080/mcp')
    parser.add_argument('--port', type=int, default=8080, help='HTTP port when using host (default 8080)')
    parser.add_argument('--path', default='/mcp', help='URI path when using host (default /mcp)')
    parser.add_argument('--timeout', type=float, default=5.0, help='Per-request timeout in seconds')
    parser.add_argument('--media', default=DEFAULT_MEDIA_URI,
                        help=f'Media URI for play smoke (default {DEFAULT_MEDIA_URI})')
    parser.add_argument('--no-play', action='store_true', help='Skip set_url/play smoke test')
    parser.add_argument('--play-wait', type=float, default=8.0,
                        help='Seconds to wait for PLAYING after play (default 8)')
    args = parser.parse_args()

    if args.url:
        url = args.url
    elif args.host:
        url = f'http://{args.host}:{args.port}{args.path}'
    else:
        parser.error('device IP (positional) or --url is required')

    print('  Audio Player Service MCP HTTP Test')
    print(f'  url={url}')

    client = MCPHttpClient(url, args.timeout)
    run_tests(client)
    if not args.no_play:
        run_play_test(client, args.media, wait_s=args.play_wait)

    print('\nAll audio player service MCP HTTP checks passed')
    print('\n[PASS] Audio player service MCP HTTP test completed')


if __name__ == '__main__':
    main()
