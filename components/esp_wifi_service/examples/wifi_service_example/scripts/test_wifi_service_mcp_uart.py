#!/usr/bin/env python3

"""
Wi-Fi Service MCP UART test client.

Protocol:
  - Send: one JSON-RPC request per line, terminated by '\\n'
  - Receive: one JSON-RPC response per line, terminated by '\\n'

Usage:
    python3 scripts/test_wifi_service_mcp_uart.py <serial-port> [baud-rate]

Example:
    python3 scripts/test_wifi_service_mcp_uart.py /dev/ttyUSB1 115200

Requirements:
    pip install pyserial
"""

import argparse
import json
import random
import string
import sys
import time

try:
    import serial
except ImportError:
    print("Error: 'pyserial' package required. Install with: pip install pyserial")
    sys.exit(1)


WIFI_TOOLS = {
    'esp_wifi_service_get_status',
    'esp_wifi_service_list_profiles',
    'esp_wifi_service_add_profile',
    'esp_wifi_service_set_profile_enabled',
    'esp_wifi_service_delete_profile',
    'esp_wifi_service_clear_profiles',
    'esp_wifi_service_prov_start',
    'esp_wifi_service_prov_stop',
    'esp_wifi_service_request_reeval',
    'esp_wifi_service_request_connect',
}


class MCPUartClient:
    def __init__(self, port, baud_rate=115200, timeout=3.0):
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


def generate_test_profile():
    suffix = ''.join(random.choice(string.ascii_lowercase + string.digits) for _ in range(8))
    return {
        'ssid': f'mcp_ap_{suffix}',
        'password': f'mcp_pw_{suffix}',
        'priority': random.randint(0, 20),
    }


def run_tests(client, include_mutating, include_negative):
    print('\n== Test: tools/list ==')
    listed = assert_response_ok(client.list_tools(), 'tools/list')
    tools = listed.get('tools', [])
    tool_names = {tool.get('name') for tool in tools}
    missing = sorted(WIFI_TOOLS - tool_names)
    if missing:
        raise AssertionError(f'Missing Wi-Fi MCP tools: {missing}')
    print(f'Found all {len(WIFI_TOOLS)} Wi-Fi MCP tools')

    print('\n== Test: esp_wifi_service_get_status ==')
    status, is_error = parse_tool_text_result(
        client.call_tool('esp_wifi_service_get_status'),
        'esp_wifi_service_get_status',
    )
    if is_error or not isinstance(status, dict):
        raise AssertionError(f'Invalid status response: {status}')
    for key in ('service_state', 'connected', 'provisioning_running', 'profile_count'):
        if key not in status:
            raise AssertionError(f'Status response missing key: {key}')
    print_json('Status:', status)

    print('\n== Test: esp_wifi_service_list_profiles ==')
    profiles, is_error = parse_tool_text_result(
        client.call_tool('esp_wifi_service_list_profiles'),
        'esp_wifi_service_list_profiles',
    )
    if is_error or not isinstance(profiles, dict):
        raise AssertionError(f'Invalid profiles response: {profiles}')
    if 'profiles' not in profiles:
        raise AssertionError('Profiles response missing profiles array')
    print_json('Profiles:', profiles)

    print('\n== Test: esp_wifi_service_request_reeval ==')
    reeval, is_error = parse_tool_text_result(
        client.call_tool('esp_wifi_service_request_reeval'),
        'esp_wifi_service_request_reeval',
        allow_tool_error=True,
    )
    print_json('Re-eval result:', reeval)
    if is_error:
        print('Re-eval returned a tool error; this can be expected if selector is not running yet')

    if include_negative:
        print('\n== Test: invalid set_profile_enabled arguments ==')
        invalid_enable, is_error = parse_tool_text_result(
            client.call_tool('esp_wifi_service_set_profile_enabled', {'index': 0, 'enabled': True}),
            'invalid esp_wifi_service_set_profile_enabled',
            allow_tool_error=True,
        )
        if not is_error:
            raise AssertionError(f'Invalid set_profile_enabled unexpectedly succeeded: {invalid_enable}')
        print_json('Invalid argument result:', invalid_enable)

    if include_mutating:
        test_profile = generate_test_profile()
        test_ssid = test_profile['ssid']
        test_password = test_profile['password']
        test_priority = test_profile['priority']
        print_json('Generated test profile:', {
            'ssid': test_ssid,
            'password_len': len(test_password),
            'priority': test_priority,
        })

        print('\n== Test: esp_wifi_service_add_profile ==')
        add_profile, is_error = parse_tool_text_result(
            client.call_tool('esp_wifi_service_add_profile', test_profile),
            'esp_wifi_service_add_profile',
        )
        if is_error or not isinstance(add_profile, dict) or not add_profile.get('ok'):
            raise AssertionError(f'Add profile failed: {add_profile}')
        if add_profile.get('ssid') != test_ssid:
            raise AssertionError(f'Add profile response has wrong SSID: {add_profile}')
        if 'password' in add_profile:
            raise AssertionError('Add profile response must not echo password')
        print_json('Add profile:', add_profile)

        added_index = add_profile.get('index')
        if not isinstance(added_index, int) or added_index < 0:
            raise AssertionError(f'Add profile returned invalid index: {add_profile}')

        print('\n== Test: esp_wifi_service_set_profile_enabled success ==')
        enable_profile, is_error = parse_tool_text_result(
            client.call_tool('esp_wifi_service_set_profile_enabled', {
                'ssid': test_ssid,
                'enabled': True,
            }),
            'esp_wifi_service_set_profile_enabled success',
        )
        if is_error or not isinstance(enable_profile, dict) or not enable_profile.get('ok'):
            raise AssertionError(f'Set profile enabled failed: {enable_profile}')
        print_json('Set profile enabled:', enable_profile)

        print('\n== Test: list_profiles after add_profile ==')
        profiles_after_add, is_error = parse_tool_text_result(
            client.call_tool('esp_wifi_service_list_profiles'),
            'esp_wifi_service_list_profiles after add_profile',
        )
        if is_error or not any(item.get('ssid') == test_ssid for item in profiles_after_add.get('profiles', [])):
            raise AssertionError(f'Added SSID not found in profile list: {profiles_after_add}')
        print_json('Profiles after add:', profiles_after_add)

        print('\n== Test: esp_wifi_service_prov_start ==')
        prov_start, _ = parse_tool_text_result(
            client.call_tool('esp_wifi_service_prov_start'),
            'esp_wifi_service_prov_start',
            allow_tool_error=True,
        )
        print_json('Provisioning start:', prov_start)

        print('\n== Test: esp_wifi_service_prov_stop ==')
        prov_stop, _ = parse_tool_text_result(
            client.call_tool('esp_wifi_service_prov_stop'),
            'esp_wifi_service_prov_stop',
            allow_tool_error=True,
        )
        print_json('Provisioning stop:', prov_stop)
    else:
        print('\nSkipping add_profile and provisioning start/stop. Pass --mutating to run state-changing tools.')

    print('\nAll Wi-Fi service MCP UART checks passed')


def main():
    parser = argparse.ArgumentParser(description='Test esp_wifi_service MCP tools over UART')
    parser.add_argument('port', help='Serial port connected to the MCP UART pins')
    parser.add_argument('baud', nargs='?', type=int, default=115200, help='UART baud rate')
    parser.add_argument('--timeout', type=float, default=3.0, help='Serial read timeout in seconds')
    parser.add_argument('--mutating', action='store_true', help='Also call add_profile and provisioning start/stop tools')
    parser.add_argument('--negative', action='store_true', help='Also run expected-failure argument validation checks')
    args = parser.parse_args()

    print('=' * 72)
    print('  Wi-Fi Service MCP UART Test')
    print('=' * 72)
    print(f'Port:     {args.port}')
    print(f'Baud:     {args.baud}')
    print(f'Timeout:  {args.timeout}s')
    print(f'Mutating: {args.mutating}')
    print(f'Negative: {args.negative}')

    try:
        with MCPUartClient(args.port, args.baud, args.timeout) as client:
            run_tests(client, args.mutating, args.negative)
    except Exception as exc:
        print(f'\n[FAIL] {exc}')
        return 1

    print('\n[PASS] Wi-Fi service MCP UART test completed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
