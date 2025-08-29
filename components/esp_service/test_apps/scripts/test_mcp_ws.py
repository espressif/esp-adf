#!/usr/bin/env python3

"""
MCP WebSocket Transport Test Client

Functional test suite for MCP Server over WebSocket transport.
Tests persistent connections, JSON-RPC 2.0 protocol, tool listing,
tool invocation, error handling, and concurrent client support.

Usage:
    python test_mcp_ws.py <device-ip> [port]

Requirements:
    pip install websockets
"""

import sys
import json
import asyncio
import time

try:
    import websockets
except ImportError:
    print("Error: 'websockets' package required.")
    print('Install with: pip install websockets')
    sys.exit(1)


class MCPWebSocketClient:
    """MCP WebSocket client for testing"""

    def __init__(self, ws_url):
        self.ws_url = ws_url
        self.request_id = 0
        self.ws = None

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    async def connect(self):
        """Establish WebSocket connection"""
        self.ws = await websockets.connect(self.ws_url)
        print(f'  Connected to {self.ws_url}')
        return self.ws

    async def disconnect(self):
        """Close WebSocket connection"""
        if self.ws:
            await self.ws.close()
            self.ws = None
            print(f'  Disconnected from {self.ws_url}')

    async def send_raw(self, data):
        """Send raw string data and receive response"""
        if not self.ws:
            raise RuntimeError('Not connected')

        print(f'  >>> Sending: {data[:120]}{'...' if len(data) > 120 else ''}')
        await self.ws.send(data)

        try:
            response = await asyncio.wait_for(self.ws.recv(), timeout=5.0)
            print(f'  <<< Received: {response[:120]}{'...' if len(response) > 120 else ''}')
            return response
        except asyncio.TimeoutError:
            print('  !!! Timeout waiting for response')
            return None

    async def call(self, method, params=None):
        """Send JSON-RPC request and return parsed response"""
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method
        }
        if params is not None:
            request['params'] = params

        request_str = json.dumps(request)
        response_str = await self.send_raw(request_str)

        if response_str:
            try:
                result = json.loads(response_str)
                return result
            except json.JSONDecodeError as e:
                print(f'  !!! JSON decode error: {e}')
                return None
        return None

    async def list_tools(self):
        """List all available tools"""
        return await self.call('tools/list')

    async def call_tool(self, name, arguments=None):
        """Call a specific tool"""
        params = {
            'name': name,
            'arguments': arguments or {}
        }
        return await self.call('tools/call', params)


# ── Test Functions ──

async def test_connection(ws_url):
    """Test 1: Basic WebSocket connection and disconnection"""
    print('\n' + '=' * 60)
    print('Test 1: WebSocket Connection')
    print('=' * 60)

    try:
        client = MCPWebSocketClient(ws_url)
        await client.connect()
        assert client.ws is not None, 'WebSocket should be connected'
        # Verify connection is alive by sending a simple request
        result = await client.list_tools()
        assert result is not None, 'Should receive response on new connection'
        print('  [PASS] WebSocket connection established')

        await client.disconnect()
        print('  [PASS] WebSocket disconnection clean')
        return True
    except Exception as e:
        print(f'  [FAIL] Connection error: {e}')
        return False


async def test_list_tools(ws_url):
    """Test 2: List all available MCP tools"""
    print('\n' + '=' * 60)
    print('Test 2: List Tools (tools/list)')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    try:
        await client.connect()
        result = await client.list_tools()

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
        await client.disconnect()


async def test_tool_invocation(ws_url):
    """Test 3: Invoke tools through WebSocket"""
    print('\n' + '=' * 60)
    print('Test 3: Tool Invocation (tools/call)')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    passed = True
    try:
        await client.connect()

        # Test 3a: Player - Play
        print('\n  --- 3a: Player Play ---')
        result = await client.call_tool('player_service_play')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] Player play')

        # Test 3b: Player - Set Volume
        print('\n  --- 3b: Player Set Volume to 75 ---')
        result = await client.call_tool('player_service_set_volume', {'volume': 75})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] Player set volume')

        # Test 3c: Player - Get Volume
        print('\n  --- 3c: Player Get Volume ---')
        result = await client.call_tool('player_service_get_volume')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] Player get volume')

        # Test 3d: Player - Get Status
        print('\n  --- 3d: Player Get Status ---')
        result = await client.call_tool('player_service_get_status')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] Player get status')

        # Test 3e: LED - Blink
        print('\n  --- 3e: LED Blink (1000ms) ---')
        result = await client.call_tool('led_service_blink', {'period_ms': 1000})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] LED blink')

        # Test 3f: LED - Get State
        print('\n  --- 3f: LED Get State ---')
        result = await client.call_tool('led_service_get_state')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] LED get state')

        # Test 3g: LED - Set Brightness
        print('\n  --- 3g: LED Set Brightness to 80 ---')
        result = await client.call_tool('led_service_set_brightness', {'brightness': 80})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f'  Result: {json.dumps(result['result'], indent=2)}')
        print('  [PASS] LED set brightness')

    except AssertionError as e:
        print(f'  [FAIL] Assertion: {e}')
        passed = False
    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        await client.disconnect()

    return passed


async def test_persistent_connection(ws_url):
    """Test 4: Multiple requests over a single persistent connection"""
    print('\n' + '=' * 60)
    print('Test 4: Persistent Connection (multiple requests)')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    try:
        await client.connect()

        # Send multiple requests over the same connection
        num_requests = 5
        for i in range(num_requests):
            result = await client.list_tools()
            assert result is not None, f'Request {i+1} should receive response'
            assert 'result' in result, f"Request {i+1} should have 'result'"
            print(f'  Request {i+1}/{num_requests}: OK')

        print(f'  [PASS] {num_requests} requests over single connection')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def test_error_handling(ws_url):
    """Test 5: Error handling for invalid requests"""
    print('\n' + '=' * 60)
    print('Test 5: Error Handling')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    passed = True
    try:
        await client.connect()

        # Test 5a: Invalid tool name
        print('\n  --- 5a: Invalid tool name ---')
        result = await client.call_tool('nonexistent_tool')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f'  Error response: {json.dumps(result['error'], indent=2)}')
            print('  [PASS] Invalid tool returns error')
        elif 'result' in result:
            # Some implementations return error in result field
            print(f'  Result: {json.dumps(result['result'], indent=2)}')
            print('  [PASS] Invalid tool handled')

        # Test 5b: Invalid MCP method
        print('\n  --- 5b: Invalid MCP method ---')
        result = await client.call('invalid/method')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f'  Error response: {json.dumps(result['error'], indent=2)}')
            print('  [PASS] Invalid method returns error')
        else:
            print(f'  Response: {json.dumps(result, indent=2)}')
            print('  [PASS] Invalid method handled')

        # Test 5c: Malformed JSON
        print('\n  --- 5c: Malformed JSON ---')
        response = await client.send_raw('{invalid json!!!')
        if response:
            try:
                result = json.loads(response)
                if 'error' in result:
                    print(f'  Error response: {json.dumps(result['error'], indent=2)}')
                    print('  [PASS] Malformed JSON returns error')
                else:
                    print(f'  Response: {response}')
                    print('  [PASS] Malformed JSON handled')
            except json.JSONDecodeError:
                print(f'  Raw response: {response}')
                print('  [PASS] Malformed JSON handled (non-JSON response)')
        else:
            print('  No response (server may have dropped the frame)')
            print('  [PASS] Malformed JSON handled (no response)')

        # Test 5d: Missing required fields
        print("\n  --- 5d: Missing 'method' field ---")
        response = await client.send_raw(json.dumps({
            'jsonrpc': '2.0',
            'id': 999
        }))
        if response:
            result = json.loads(response)
            if 'error' in result:
                print(f'  Error response: {json.dumps(result['error'], indent=2)}')
            print('  [PASS] Missing method field handled')
        else:
            print('  [PASS] Missing method field handled (no response)')

    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        await client.disconnect()

    return passed


async def test_rapid_requests(ws_url):
    """Test 6: Rapid sequential requests (stress test)"""
    print('\n' + '=' * 60)
    print('Test 6: Rapid Sequential Requests')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    try:
        await client.connect()

        num_requests = 10
        success_count = 0
        start_time = time.time()

        for i in range(num_requests):
            result = await client.call_tool('player_service_get_volume')
            if result and ('result' in result or 'error' in result):
                success_count += 1

        elapsed = time.time() - start_time
        print(f'  Sent {num_requests} requests in {elapsed:.2f}s')
        print(f'  Success: {success_count}/{num_requests}')
        print(f'  Average: {elapsed/num_requests*1000:.1f}ms per request')

        assert success_count == num_requests, \
            f'Expected {num_requests} successes, got {success_count}'
        print('  [PASS] Rapid sequential requests')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def test_concurrent_clients(ws_url):
    """Test 7: Multiple concurrent WebSocket clients"""
    print('\n' + '=' * 60)
    print('Test 7: Concurrent WebSocket Clients')
    print('=' * 60)

    num_clients = 3

    async def client_task(client_id):
        """Task for a single client"""
        client = MCPWebSocketClient(ws_url)
        try:
            await client.connect()
            print(f'  Client {client_id}: connected')

            # Each client sends a few requests
            for i in range(3):
                result = await client.list_tools()
                if result and 'result' in result:
                    tools_count = len(result['result'].get('tools', []))
                    print(f'  Client {client_id}: request {i+1} OK ({tools_count} tools)')
                else:
                    print(f'  Client {client_id}: request {i+1} FAILED')
                    return False

            return True
        except Exception as e:
            print(f'  Client {client_id}: error - {e}')
            return False
        finally:
            await client.disconnect()
            print(f'  Client {client_id}: disconnected')

    # Launch clients concurrently
    tasks = [client_task(i+1) for i in range(num_clients)]
    results = await asyncio.gather(*tasks, return_exceptions=True)

    success = all(r is True for r in results)
    if success:
        print(f'  [PASS] {num_clients} concurrent clients worked correctly')
    else:
        print(f'  [FAIL] Some clients failed: {results}')

    return success


async def test_reconnection(ws_url):
    """Test 8: Disconnect and reconnect"""
    print('\n' + '=' * 60)
    print('Test 8: Reconnection')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    try:
        # First connection
        await client.connect()
        result = await client.list_tools()
        assert result is not None, 'First connection should work'
        print('  First connection: OK')

        # Disconnect
        await client.disconnect()
        print('  Disconnected')

        # Wait briefly
        await asyncio.sleep(0.5)

        # Reconnect
        await client.connect()
        result = await client.list_tools()
        assert result is not None, 'Reconnection should work'
        assert 'result' in result, "Reconnected response should have 'result'"
        print('  Reconnected: OK')

        print('  [PASS] Reconnection works correctly')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def test_json_rpc_compliance(ws_url):
    """Test 9: JSON-RPC 2.0 protocol compliance"""
    print('\n' + '=' * 60)
    print('Test 9: JSON-RPC 2.0 Compliance')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    passed = True
    try:
        await client.connect()

        # Test 9a: Response should contain jsonrpc field
        print('\n  --- 9a: Response format ---')
        result = await client.list_tools()
        assert result is not None, 'Should receive response'
        assert result.get('jsonrpc') == '2.0', "Response should have jsonrpc: '2.0'"
        assert 'id' in result, "Response should have 'id' field"
        print('  [PASS] Response contains jsonrpc and id fields')

        # Test 9b: Response id should match request id
        print('\n  --- 9b: Request/Response ID matching ---')
        request_id = client._next_id()
        request = {
            'jsonrpc': '2.0',
            'id': request_id,
            'method': 'tools/list'
        }
        response_str = await client.send_raw(json.dumps(request))
        assert response_str is not None, 'Should receive response'
        response = json.loads(response_str)
        assert response.get('id') == request_id, \
            f'Response id ({response.get('id')}) should match request id ({request_id})'
        print(f'  [PASS] Response id matches request id ({request_id})')

        # Test 9c: Notification (no id) - should not get response
        print('\n  --- 9c: Notification (no id field) ---')
        notification = {
            'jsonrpc': '2.0',
            'method': 'notifications/test'
        }
        await client.ws.send(json.dumps(notification))
        try:
            response = await asyncio.wait_for(client.ws.recv(), timeout=2.0)
            print(f'  Got response to notification: {response}')
            print('  [INFO] Server responded to notification (implementation-specific)')
        except asyncio.TimeoutError:
            print('  [PASS] No response for notification (correct behavior)')

    except AssertionError as e:
        print(f'  [FAIL] Assertion: {e}')
        passed = False
    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        await client.disconnect()

    return passed


async def test_large_payload(ws_url):
    """Test 10: Large payload handling"""
    print('\n' + '=' * 60)
    print('Test 10: Large Payload Handling')
    print('=' * 60)

    client = MCPWebSocketClient(ws_url)
    try:
        await client.connect()

        # Send a request with large arguments (within the 4096 limit)
        large_value = 'x' * 2000
        result = await client.call_tool('player_service_set_volume',
                                         {'volume': 50, 'padding': large_value})
        if result:
            if 'error' in result:
                print(f'  Error (expected for invalid args): {result['error'].get('message', '')}')
            else:
                print(f'  Result: {json.dumps(result.get('result', {}), indent=2)}')
            print('  [PASS] Large payload handled')
        else:
            print('  [PASS] Large payload handled (no response)')

        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def run_all_tests(ws_url):
    """Run all functional tests"""
    print('=' * 60)
    print('  MCP WebSocket Transport Functional Test Suite')
    print('=' * 60)
    print(f'Target: {ws_url}')
    print('')

    tests = [
        ('Connection',            test_connection),
        ('List Tools',            test_list_tools),
        ('Tool Invocation',       test_tool_invocation),
        ('Persistent Connection', test_persistent_connection),
        ('Error Handling',        test_error_handling),
        ('Rapid Requests',        test_rapid_requests),
        ('Concurrent Clients',    test_concurrent_clients),
        ('Reconnection',          test_reconnection),
        ('JSON-RPC Compliance',   test_json_rpc_compliance),
        ('Large Payload',         test_large_payload),
    ]

    results = {}
    for name, test_func in tests:
        try:
            results[name] = await test_func(ws_url)
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
        print('Usage: python test_mcp_ws.py <device-ip> [port]')
        print('Example: python test_mcp_ws.py 192.168.1.100')
        print('Example: python test_mcp_ws.py 192.168.1.100 8081')
        sys.exit(1)

    device_ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8081
    ws_url = f'ws://{device_ip}:{port}/mcp'

    success = asyncio.run(run_all_tests(ws_url))
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
