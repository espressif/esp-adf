#!/usr/bin/env python3

"""
MCP UART Transport Test Client

Functional test suite for MCP Server over UART transport.
Tests line-based JSON-RPC 2.0 protocol, tool listing,
tool invocation, error handling, and rapid sequential requests.

Protocol:
  - Send:    one JSON-RPC request per line (terminated by '\\n')
  - Receive: one JSON-RPC response per line (terminated by '\\n')

Usage:
    python test_mcp_uart.py <serial-port> [baud-rate]

Examples:
    python test_mcp_uart.py /dev/ttyUSB1
    python test_mcp_uart.py /dev/ttyUSB1 115200
    python test_mcp_uart.py COM3 115200

Requirements:
    pip install pyserial
"""

import sys
import json
import time
import threading

try:
    import serial
except ImportError:
    print("Error: 'pyserial' package required.")
    print('Install with: pip install pyserial')
    sys.exit(1)


class MCPUartClient:
    """MCP UART client for testing"""

    def __init__(self, port, baud_rate=115200, timeout=3.0):
        self.port = port
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser = None
        self.request_id = 0

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    def connect(self):
        """Open serial port"""
        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout,
            write_timeout=self.timeout,
        )
        # Flush any stale data
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        time.sleep(0.1)
        # Drain any leftover data
        while self.ser.in_waiting:
            self.ser.read(self.ser.in_waiting)
            time.sleep(0.05)
        print(f'  Connected to {self.port} @ {self.baud_rate} baud')

    def disconnect(self):
        """Close serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None
            print(f'  Disconnected from {self.port}')

    def send_raw(self, data):
        """Send raw string followed by newline and read response line"""
        if not self.ser or not self.ser.is_open:
            raise RuntimeError('Not connected')

        # Flush input before sending
        self.ser.reset_input_buffer()

        # Send the request line
        line = data.strip() + '\n'
        self.ser.write(line.encode('utf-8'))
        self.ser.flush()
        print(f'  >>> Sent: {data[:100]}{'...' if len(data) > 100 else ''}')

        # Read response line(s) - look for valid JSON
        start_time = time.time()
        while time.time() - start_time < self.timeout:
            resp_line = self.ser.readline()
            if not resp_line:
                continue
            resp_str = resp_line.decode('utf-8', errors='replace').strip()
            if not resp_str:
                continue

            # Check if it looks like a JSON response (starts with '{')
            if resp_str.startswith('{'):
                print(f'  <<< Recv: {resp_str[:100]}{'...' if len(resp_str) > 100 else ''}')
                return resp_str
            else:
                # Likely a log line from the device, skip it
                pass

        print('  !!! Timeout waiting for response')
        return None

    def call(self, method, params=None):
        """Send JSON-RPC request and return parsed response"""
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method
        }
        if params is not None:
            request['params'] = params

        # JSON-RPC must be single line (no pretty-print)
        request_str = json.dumps(request, separators=(',', ':'))
        response_str = self.send_raw(request_str)

        if response_str:
            try:
                result = json.loads(response_str)
                return result
            except json.JSONDecodeError as e:
                print(f'  !!! JSON decode error: {e}')
                print(f'  !!! Raw: {response_str}')
                return None
        return None

    def list_tools(self):
        """List all available tools"""
        return self.call('tools/list')

    def call_tool(self, name, arguments=None):
        """Call a specific tool"""
        params = {
            'name': name,
            'arguments': arguments or {}
        }
        return self.call('tools/call', params)


# ── Test Functions ──

def test_connection(port, baud):
    """Test 1: Basic serial connection"""
    print('\n' + '=' * 60)
    print('Test 1: Serial Connection')
    print('=' * 60)

    try:
        client = MCPUartClient(port, baud)
        client.connect()
        assert client.ser is not None, 'Serial port should be open'
        assert client.ser.is_open, 'Serial port should be open'
        print('  [PASS] Serial connection established')

        # Verify connection works with a simple request
        result = client.list_tools()
        assert result is not None, 'Should receive response'
        print('  [PASS] MCP server responds over UART')

        client.disconnect()
        print('  [PASS] Serial disconnection clean')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False


def test_list_tools(port, baud):
    """Test 2: List all available MCP tools"""
    print('\n' + '=' * 60)
    print('Test 2: List Tools (tools/list)')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    try:
        client.connect()
        result = client.list_tools()

        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result' field"
        assert 'tools' in result['result'], "Result should have 'tools' field"

        tools = result['result']['tools']
        print(f'  Found {len(tools)} tools:')
        for i, tool in enumerate(tools):
            print(f'    [{i}] {tool['name']} - {tool.get('description', 'N/A')}')

        assert len(tools) > 0, 'Should have at least one tool'
        print('  [PASS] Tool listing works correctly')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        client.disconnect()


def test_tool_invocation(port, baud):
    """Test 3: Invoke tools through UART"""
    print('\n' + '=' * 60)
    print('Test 3: Tool Invocation (tools/call)')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    passed = True
    try:
        client.connect()

        # Test 3a: Player - Play
        print('\n  --- 3a: Player Play ---')
        result = client.call_tool('player_service_play')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] Player play')

        # Test 3b: Player - Set Volume
        print('\n  --- 3b: Player Set Volume to 75 ---')
        result = client.call_tool('player_service_set_volume', {'volume': 75})
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] Player set volume')

        # Test 3c: Player - Get Volume
        print('\n  --- 3c: Player Get Volume ---')
        result = client.call_tool('player_service_get_volume')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] Player get volume')

        # Test 3d: Player - Get Status
        print('\n  --- 3d: Player Get Status ---')
        result = client.call_tool('player_service_get_status')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] Player get status')

        # Test 3e: LED - Blink
        print('\n  --- 3e: LED Blink (1000ms) ---')
        result = client.call_tool('led_service_blink', {'period_ms': 1000})
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] LED blink')

        # Test 3f: LED - Get State
        print('\n  --- 3f: LED Get State ---')
        result = client.call_tool('led_service_get_state')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] LED get state')

        # Test 3g: LED - Set Brightness
        print('\n  --- 3g: LED Set Brightness to 80 ---')
        result = client.call_tool('led_service_set_brightness', {'brightness': 80})
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] LED set brightness')

        # Test 3h: LED - On
        print('\n  --- 3h: LED On ---')
        result = client.call_tool('led_service_on')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] LED on')

        # Test 3i: LED - Off
        print('\n  --- 3i: LED Off ---')
        result = client.call_tool('led_service_off')
        assert result is not None and 'result' in result
        print(f'  Result: {json.dumps(result['result'])}')
        print('  [PASS] LED off')

    except AssertionError as e:
        print(f'  [FAIL] Assertion: {e}')
        passed = False
    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        client.disconnect()

    return passed


def test_multiple_requests(port, baud):
    """Test 4: Multiple sequential requests over single connection"""
    print('\n' + '=' * 60)
    print('Test 4: Multiple Sequential Requests')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    try:
        client.connect()

        num_requests = 5
        for i in range(num_requests):
            result = client.list_tools()
            assert result is not None, f'Request {i+1} should receive response'
            assert 'result' in result, f"Request {i+1} should have 'result'"
            print(f'  Request {i+1}/{num_requests}: OK')

        print(f'  [PASS] {num_requests} sequential requests succeeded')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        client.disconnect()


def test_error_handling(port, baud):
    """Test 5: Error handling for invalid requests"""
    print('\n' + '=' * 60)
    print('Test 5: Error Handling')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    passed = True
    try:
        client.connect()

        # Test 5a: Invalid tool name
        print('\n  --- 5a: Invalid tool name ---')
        result = client.call_tool('nonexistent_tool')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f'  Error: {json.dumps(result['error'])}')
            print('  [PASS] Invalid tool returns error')
        elif 'result' in result:
            is_err = result['result'].get('isError', False)
            print(f'  isError={is_err}, result={json.dumps(result['result'])}')
            print('  [PASS] Invalid tool handled')

        # Test 5b: Invalid MCP method
        print('\n  --- 5b: Invalid MCP method ---')
        result = client.call('invalid/method')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f'  Error: {json.dumps(result['error'])}')
            print('  [PASS] Invalid method returns error')
        else:
            print(f'  Response: {json.dumps(result)}')
            print('  [PASS] Invalid method handled')

        # Test 5c: Malformed JSON
        print('\n  --- 5c: Malformed JSON ---')
        response = client.send_raw('{broken json!!!')
        if response:
            try:
                result = json.loads(response)
                if 'error' in result:
                    print(f'  Error: {json.dumps(result['error'])}')
                    print('  [PASS] Malformed JSON returns error')
                else:
                    print('  [PASS] Malformed JSON handled')
            except json.JSONDecodeError:
                print('  [PASS] Malformed JSON handled (non-JSON response)')
        else:
            print('  [PASS] Malformed JSON handled (no response)')

        # Test 5d: Missing required fields
        print("\n  --- 5d: Missing 'method' field ---")
        response = client.send_raw(json.dumps({'jsonrpc': '2.0', 'id': 999}))
        if response:
            result = json.loads(response)
            if 'error' in result:
                print(f'  Error: {json.dumps(result['error'])}')
            print('  [PASS] Missing method field handled')
        else:
            print('  [PASS] Missing method field handled (no response)')

    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        client.disconnect()

    return passed


def test_rapid_requests(port, baud):
    """Test 6: Rapid sequential requests (throughput test)"""
    print('\n' + '=' * 60)
    print('Test 6: Rapid Sequential Requests')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    try:
        client.connect()

        num_requests = 10
        success_count = 0
        start_time = time.time()

        for i in range(num_requests):
            result = client.call_tool('player_service_get_volume')
            if result and ('result' in result or 'error' in result):
                success_count += 1

        elapsed = time.time() - start_time
        print(f'  Sent {num_requests} requests in {elapsed:.2f}s')
        print(f'  Success: {success_count}/{num_requests}')
        print(f'  Average: {elapsed/num_requests*1000:.1f}ms per request')
        print(f'  Throughput: {num_requests/elapsed:.1f} req/s')

        assert success_count == num_requests, \
            f'Expected {num_requests} successes, got {success_count}'
        print('  [PASS] Rapid sequential requests')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        client.disconnect()


def test_json_rpc_compliance(port, baud):
    """Test 7: JSON-RPC 2.0 protocol compliance"""
    print('\n' + '=' * 60)
    print('Test 7: JSON-RPC 2.0 Compliance')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    passed = True
    try:
        client.connect()

        # Test 7a: Response format
        print('\n  --- 7a: Response format ---')
        result = client.list_tools()
        assert result is not None, 'Should receive response'
        assert result.get('jsonrpc') == '2.0', "Should have jsonrpc: '2.0'"
        assert 'id' in result, "Should have 'id' field"
        print('  [PASS] Response contains jsonrpc and id fields')

        # Test 7b: Response ID matching
        print('\n  --- 7b: Request/Response ID matching ---')
        req_id = client._next_id()
        request = {'jsonrpc': '2.0', 'id': req_id, 'method': 'tools/list'}
        resp_str = client.send_raw(json.dumps(request, separators=(',', ':')))
        assert resp_str is not None, 'Should receive response'
        response = json.loads(resp_str)
        assert response.get('id') == req_id, \
            f'Response id ({response.get('id')}) should match request id ({req_id})'
        print(f'  [PASS] Response id matches request id ({req_id})')

    except AssertionError as e:
        print(f'  [FAIL] Assertion: {e}')
        passed = False
    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        client.disconnect()

    return passed


def test_reconnection(port, baud):
    """Test 8: Disconnect and reconnect"""
    print('\n' + '=' * 60)
    print('Test 8: Reconnection')
    print('=' * 60)

    client = MCPUartClient(port, baud)
    try:
        # First connection
        client.connect()
        result = client.list_tools()
        assert result is not None, 'First connection should work'
        print('  First connection: OK')

        # Disconnect
        client.disconnect()
        print('  Disconnected')

        time.sleep(0.5)

        # Reconnect
        client.connect()
        result = client.list_tools()
        assert result is not None, 'Reconnection should work'
        assert 'result' in result, "Reconnected response should have 'result'"
        print('  Reconnected: OK')

        print('  [PASS] Reconnection works correctly')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        client.disconnect()


def run_all_tests(port, baud):
    """Run all functional tests"""
    print('=' * 60)
    print('  MCP UART Transport Functional Test Suite')
    print('=' * 60)
    print(f'Serial Port: {port}')
    print(f'Baud Rate:   {baud}')
    print('')

    tests = [
        ('Connection',            lambda: test_connection(port, baud)),
        ('List Tools',            lambda: test_list_tools(port, baud)),
        ('Tool Invocation',       lambda: test_tool_invocation(port, baud)),
        ('Multiple Requests',     lambda: test_multiple_requests(port, baud)),
        ('Error Handling',        lambda: test_error_handling(port, baud)),
        ('Rapid Requests',        lambda: test_rapid_requests(port, baud)),
        ('JSON-RPC Compliance',   lambda: test_json_rpc_compliance(port, baud)),
        ('Reconnection',          lambda: test_reconnection(port, baud)),
    ]

    results = {}
    for name, test_func in tests:
        try:
            results[name] = test_func()
        except Exception as e:
            print(f'  [FAIL] Unexpected error in {name}: {e}')
            results[name] = False

    # Summary
    print('\n' + '=' * 60)
    print('  Test Summary')
    print('=' * 60)

    total = len(results)
    passed = sum(1 for v in results.values() if v)
    failed = total - passed

    for name, result in results.items():
        status = 'PASS' if result else 'FAIL'
        print(f'  [{status}] {name}')

    print('')
    print(f'  Total: {total}  Passed: {passed}  Failed: {failed}')

    if failed == 0:
        print('\n  All tests PASSED!')
    else:
        print(f'\n  {failed} test(s) FAILED!')

    print('=' * 60)
    return failed == 0


def main():
    if len(sys.argv) < 2:
        print('Usage: python test_mcp_uart.py <serial-port> [baud-rate]')
        print('')
        print('Examples:')
        print('  python test_mcp_uart.py /dev/ttyUSB1')
        print('  python test_mcp_uart.py /dev/ttyUSB1 115200')
        print('  python test_mcp_uart.py COM3')
        print('')
        print('Hardware connection (ESP32-S3 UART0 / board USB-serial bridge):')
        print('  GPIO43 U0TXD ----> adapter RX (often wired on-module to USB chip)')
        print('  GPIO44 U0RXD <---- adapter TX')
        print('  GND ------------ GND')
        print('  Or set ESP_SVC_TEST_UART_TX_PIN / ESP_SVC_TEST_UART_RX_PIN in menuconfig for other pins.')
        sys.exit(1)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    success = run_all_tests(port, baud)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
