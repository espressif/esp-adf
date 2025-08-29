#!/usr/bin/env python3

"""
MCP HTTP Transport Test Client

Usage:
    python test_mcp_http.py <device-ip>

Requirements:
    pip install requests
"""

import sys
import json
import requests

class MCPClient:
    """MCP HTTP client for testing"""

    def __init__(self, base_url):
        self.base_url = base_url
        self.request_id = 0

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    def _call(self, method, params=None):
        """Send JSON-RPC request"""
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method
        }
        if params is not None:
            request['params'] = params

        print(f'\n>>> Request to {method}')
        print(json.dumps(request, indent=2))

        try:
            response = requests.post(
                self.base_url,
                json=request,
                headers={'Content-Type': 'application/json'},
                timeout=5
            )
            response.raise_for_status()

            result = response.json()
            print(f'<<< Response')
            print(json.dumps(result, indent=2))
            return result

        except requests.exceptions.RequestException as e:
            print(f'!!! Error: {e}')
            return None

    def list_tools(self):
        """List all available tools"""
        return self._call('tools/list')

    def call_tool(self, name, arguments=None):
        """Call a specific tool"""
        params = {
            'name': name,
            'arguments': arguments or {}
        }
        return self._call('tools/call', params)


def main():
    if len(sys.argv) < 2:
        print('Usage: python test_mcp_http.py <device-ip>')
        print('Example: python test_mcp_http.py 192.168.1.100')
        sys.exit(1)

    device_ip = sys.argv[1]
    mcp_url = f'http://{device_ip}:8080/mcp'

    print('=' * 60)
    print('  MCP HTTP Transport Test')
    print('=' * 60)
    print(f'Target: {mcp_url}')
    print('')

    client = MCPClient(mcp_url)

    # Test 1: List all tools
    print('\n' + '=' * 60)
    print('Test 1: List all tools')
    print('=' * 60)
    result = client.list_tools()
    if result and 'result' in result:
        tools = result['result'].get('tools', [])
        print(f'\nFound {len(tools)} tools:')
        for i, tool in enumerate(tools):
            print(f'  [{i}] {tool['name']}')
            print(f'      {tool.get('description', 'No description')}')

    # Test 2: Player - Play
    print('\n' + '=' * 60)
    print('Test 2: Player - Play')
    print('=' * 60)
    client.call_tool('player_service_play')

    # Test 3: Player - Set Volume
    print('\n' + '=' * 60)
    print('Test 3: Player - Set Volume to 75')
    print('=' * 60)
    client.call_tool('player_service_set_volume', {'volume': 75})

    # Test 4: Player - Get Volume
    print('\n' + '=' * 60)
    print('Test 4: Player - Get Volume')
    print('=' * 60)
    client.call_tool('player_service_get_volume')

    # Test 5: Player - Get Status
    print('\n' + '=' * 60)
    print('Test 5: Player - Get Status')
    print('=' * 60)
    client.call_tool('player_service_get_status')

    # Test 6: LED - Blink
    print('\n' + '=' * 60)
    print('Test 6: LED - Blink (1000ms period)')
    print('=' * 60)
    client.call_tool('led_service_blink', {'period_ms': 1000})

    # Test 7: LED - Get State
    print('\n' + '=' * 60)
    print('Test 7: LED - Get State')
    print('=' * 60)
    client.call_tool('led_service_get_state')

    # Test 8: LED - Set Brightness
    print('\n' + '=' * 60)
    print('Test 8: LED - Set Brightness to 80')
    print('=' * 60)
    client.call_tool('led_service_set_brightness', {'brightness': 80})

    # Test 9: WiFi - Get Status
    print('\n' + '=' * 60)
    print('Test 9: WiFi - Get Connection Status')
    print('=' * 60)
    client.call_tool('wifi_service_get_status')

    # Test 10: WiFi - Get SSID
    print('\n' + '=' * 60)
    print('Test 10: WiFi - Get Connected SSID')
    print('=' * 60)
    client.call_tool('wifi_service_get_ssid')

    # Test 11: WiFi - Get RSSI
    print('\n' + '=' * 60)
    print('Test 11: WiFi - Get Signal Strength (RSSI)')
    print('=' * 60)
    client.call_tool('wifi_service_get_rssi')

    # Test 12: WiFi - Reconnect to new SSID
    print('\n' + '=' * 60)
    print("Test 12: WiFi - Reconnect to 'GuestNetwork'")
    print('=' * 60)
    client.call_tool('wifi_service_reconnect', {'ssid': 'GuestNetwork', 'password': 'guest123'})

    # Test 13: WiFi - Get SSID after reconnect (should show new SSID)
    print('\n' + '=' * 60)
    print('Test 13: WiFi - Get SSID after Reconnect')
    print('=' * 60)
    client.call_tool('wifi_service_get_ssid')

    # Test 14: OTA - Get Version
    print('\n' + '=' * 60)
    print('Test 14: OTA - Get Firmware Version')
    print('=' * 60)
    client.call_tool('ota_service_get_version')

    # Test 15: OTA - Check Update
    print('\n' + '=' * 60)
    print('Test 15: OTA - Check for Update')
    print('=' * 60)
    client.call_tool('ota_service_check_update')

    # Test 16: OTA - Get Progress
    print('\n' + '=' * 60)
    print('Test 16: OTA - Get Upgrade Progress')
    print('=' * 60)
    client.call_tool('ota_service_get_progress')

    # Test 17: Error case - Invalid tool
    print('\n' + '=' * 60)
    print('Test 17: Error - Invalid tool name')
    print('=' * 60)
    client.call_tool('invalid_tool_name')

    # Test 18: Error case - Invalid method
    print('\n' + '=' * 60)
    print('Test 18: Error - Invalid MCP method')
    print('=' * 60)
    client._call('invalid/method')

    print('\n' + '=' * 60)
    print('  All tests completed!')
    print('=' * 60)


if __name__ == '__main__':
    main()
