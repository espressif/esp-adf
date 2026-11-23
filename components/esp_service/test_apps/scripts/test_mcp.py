#!/usr/bin/env python3
"""Run MCP transport checks against a device using one of http, sse, ws, or uart.

These checks invoke tools on the selected target. Use a test device.
Optional dependencies: requests (HTTP/SSE), websockets (WS), pyserial (UART).
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import queue
import socket
import sys
import threading
import time
from urllib.parse import urljoin

try:
    import requests
except ImportError:
    requests = None

try:
    import serial
except ImportError:
    serial = None

try:
    import websockets
except ImportError:
    websockets = None

AUTH_TOKEN = os.environ.get('MCP_AUTH_TOKEN', '')
AUTH_HEADERS = {'Authorization': 'Bearer ' + AUTH_TOKEN} if AUTH_TOKEN else {}
HTTP_CORS_ENABLED = os.environ.get('MCP_CORS', '1') != '0'
WS_CORS_ENABLED = os.environ.get('MCP_CORS', '0') != '0'


def report_suite(results):
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


# HTTP

MAX_REQUEST_SIZE = 4096
RECV_TIMEOUT_S = 15

FAILURES = []


def check(cond, name, detail=''):
    tag = 'PASS' if cond else 'FAIL'
    print(f'  [{tag}] {name}' + (f' -- {detail}' if detail and not cond else ''))
    if not cond:
        FAILURES.append(name)
    return cond


class MCPClient:
    """MCP HTTP client for testing"""

    def __init__(self, base_url):
        self.base_url = base_url
        self.request_id = 0

    def _next_id(self):
        self.request_id += 1
        return self.request_id

    def _call(self, method, params=None):
        """Send JSON-RPC request, return (http_status, parsed_json_or_None)"""
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
                headers={'Content-Type': 'application/json', **AUTH_HEADERS},
                timeout=5
            )
        except requests.exceptions.RequestException as e:
            print(f'!!! Error: {e}')
            return None, None

        print(f'<<< HTTP {response.status_code}')
        try:
            result = response.json()
            print(json.dumps(result, indent=2))
        except ValueError:
            result = None
            print(response.text)
        if result is not None:
            check(response.status_code == 200, f'{method}: HTTP 200', f'got {response.status_code}')
            check(result.get('id') == request['id'], f'{method}: response id matches request id')
        return response.status_code, result

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


def section(title):
    print('\n' + '=' * 60)
    print(title)
    print('=' * 60)


def rpc_tool_case(client, available, name, arguments=None):
    """Call a tool; it must succeed when registered and be reported as unknown otherwise."""
    status, result = client.call_tool(name, arguments)
    if not check(result is not None, f'{name}: JSON-RPC response received'):
        return
    if name in available:
        check('result' in result and not result['result'].get('isError', False),
              f'{name}: tools/call succeeded', json.dumps(result)[:200])
    else:
        print(f'  [SKIP] {name}: not registered on this device, expecting an error reply')
        check('error' in result or result.get('result', {}).get('isError', False),
              f'{name}: unknown tool is reported as an error')


def check_transport_rules(mcp_url, host, port):
    section('Transport rules: 401 / 415 / 413 / 202 / 204 / 408')
    body = json.dumps({'jsonrpc': '2.0', 'id': 900, 'method': 'tools/list'})
    json_hdr = {'Content-Type': 'application/json', **AUTH_HEADERS}

    if AUTH_TOKEN:
        r = requests.post(mcp_url, data=body, headers={'Content-Type': 'application/json'}, timeout=5)
        check(r.status_code == 401, 'missing bearer token -> 401', f'got {r.status_code}')
        check(r.headers.get('WWW-Authenticate', '').startswith('Bearer'),
              'missing bearer token -> WWW-Authenticate: Bearer', repr(r.headers.get('WWW-Authenticate')))
        r = requests.post(mcp_url, data=body,
                          headers={'Content-Type': 'application/json', 'Authorization': 'Bearer wrong-' + AUTH_TOKEN},
                          timeout=5)
        check(r.status_code == 401, 'wrong bearer token -> 401', f'got {r.status_code}')
    else:
        print('  [SKIP] MCP_AUTH_TOKEN not set: 401 checks skipped')

    r = requests.post(mcp_url, data=body, headers={'Content-Type': 'text/plain', **AUTH_HEADERS}, timeout=5)
    check(r.status_code == 415, 'Content-Type: text/plain -> 415', f'got {r.status_code}')

    # requests adds no Content-Type when a plain string body is given without one
    r = requests.post(mcp_url, data=body, headers=AUTH_HEADERS, timeout=5)
    check(r.status_code == 415, 'missing Content-Type -> 415', f'got {r.status_code}')

    r = requests.post(mcp_url, data=body, headers={'Content-Type': 'application/json; charset=utf-8', **AUTH_HEADERS},
                      timeout=5)
    check(r.status_code == 200, 'Content-Type with charset parameter -> 200', f'got {r.status_code}')

    big = json.dumps({'jsonrpc': '2.0', 'id': 901, 'method': 'tools/list', 'params': {'pad': 'x' * MAX_REQUEST_SIZE}})
    r = requests.post(mcp_url, data=big, headers=json_hdr, timeout=5)
    check(r.status_code == 413, f'body of {len(big)} bytes -> 413', f'got {r.status_code}')

    r = requests.post(mcp_url, data=json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}),
                      headers=json_hdr, timeout=5)
    check(r.status_code == 202, 'notification -> 202', f'got {r.status_code}')
    check(r.content == b'', 'notification -> empty body', repr(r.content[:80]))

    r = requests.options(mcp_url, headers={'Origin': 'http://example.com',
                                           'Access-Control-Request-Method': 'POST'}, timeout=5)
    check(r.status_code == 204, 'OPTIONS preflight -> 204', f'got {r.status_code}')
    acao = r.headers.get('Access-Control-Allow-Origin')
    if HTTP_CORS_ENABLED:
        check(acao == '*', 'CORS enabled: Access-Control-Allow-Origin: *', repr(acao))
        r = requests.post(mcp_url, data=body, headers={'Origin': 'http://example.com', **AUTH_HEADERS}, timeout=5)
        check(r.status_code == 415 and r.headers.get('Access-Control-Allow-Origin') == '*',
              'CORS headers present on a 415 rejection', f'{r.status_code} {dict(r.headers)}')
    else:
        check(acao is None, 'CORS disabled: no Access-Control-Allow-Origin', repr(acao))

    if os.environ.get('MCP_SKIP_408') == '1':
        print('  [SKIP] MCP_SKIP_408=1: slow-body timeout check skipped')
        return

    path = mcp_url.split(f':{port}', 1)[1]
    head = (f'POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\n'
            f'Content-Type: application/json\r\nContent-Length: {len(body)}\r\n')
    if AUTH_TOKEN:
        head += f'Authorization: Bearer {AUTH_TOKEN}\r\n'
    head += '\r\n'
    print(f'  ... sending a partial body and waiting up to {RECV_TIMEOUT_S + 10} s for 408')
    with socket.create_connection((host, port), timeout=RECV_TIMEOUT_S + 10) as s:
        s.sendall(head.encode() + body[:10].encode())
        t0 = time.monotonic()
        raw = b''
        try:
            while b'\r\n\r\n' not in raw:
                chunk = s.recv(1024)
                if not chunk:
                    break
                raw += chunk
        except socket.timeout:
            pass
        elapsed = time.monotonic() - t0
    status_line = raw.split(b'\r\n', 1)[0].decode(errors='replace')
    check(status_line.startswith('HTTP/1.1 408'), 'partial body -> 408 Request Timeout', repr(status_line))
    check(b'connection: close' in raw.lower(), 'partial body -> Connection: close')
    check(elapsed < RECV_TIMEOUT_S + 10, f'408 answered within {RECV_TIMEOUT_S + 10} s', f'{elapsed:.1f} s')


def run_http(device_ip, port):
    FAILURES.clear()
    mcp_url = f'http://{device_ip}:{port}/mcp'

    print('=' * 60)
    print('  MCP HTTP Transport Test')
    print('=' * 60)
    print(f'Target: {mcp_url}')
    print('')

    client = MCPClient(mcp_url)

    section('Test 1: List all tools')
    status, result = client.list_tools()
    available = set()
    if check(result is not None and 'result' in result, 'tools/list returns a result'):
        tools = result['result'].get('tools', [])
        available = {t.get('name') for t in tools}
        print(f'\nFound {len(tools)} tools:')
        for i, tool in enumerate(tools):
            print(f"  [{i}] {tool['name']}")
            print(f"      {tool.get('description', 'No description')}")
        check(len(tools) > 0, 'tools/list is not empty')
        check(all('name' in t and 'inputSchema' in t for t in tools), 'every tool has name and inputSchema')

    section('Test 2: Player tools')
    rpc_tool_case(client, available, 'player_service_play')
    rpc_tool_case(client, available, 'player_service_set_volume', {'volume': 75})
    status, result = client.call_tool('player_service_get_volume')
    if 'player_service_get_volume' in available and result and 'result' in result:
        text = ''.join(c.get('text', '') for c in result['result'].get('content', []))
        check('75' in text, 'player_service_get_volume reports the volume set before', text)
    rpc_tool_case(client, available, 'player_service_get_status')

    section('Test 3: LED tools')
    rpc_tool_case(client, available, 'led_service_blink', {'period_ms': 1000})
    rpc_tool_case(client, available, 'led_service_get_state')
    rpc_tool_case(client, available, 'led_service_set_brightness', {'brightness': 80})

    section('Test 4: WiFi tools (if advertised)')
    rpc_tool_case(client, available, 'wifi_service_get_status')
    rpc_tool_case(client, available, 'wifi_service_get_ssid')
    rpc_tool_case(client, available, 'wifi_service_get_rssi')

    section('Test 5: OTA tools (if advertised)')
    rpc_tool_case(client, available, 'ota_service_get_version')
    rpc_tool_case(client, available, 'ota_service_check_update')
    rpc_tool_case(client, available, 'ota_service_get_progress')

    section('Test 6: JSON-RPC error paths')
    status, result = client.call_tool('invalid_tool_name')
    check(result is not None and ('error' in result or result.get('result', {}).get('isError', False)),
          'invalid tool name is reported as an error')
    status, result = client._call('invalid/method')
    check(result is not None and result.get('error', {}).get('code') == -32601,
          'invalid method -> JSON-RPC -32601', json.dumps(result)[:200] if result else 'no response')

    check_transport_rules(mcp_url, device_ip, port)

    print('\n' + '=' * 60)
    if FAILURES:
        print(f'  {len(FAILURES)} check(s) FAILED:')
        for name in FAILURES:
            print(f'    - {name}')
        print('=' * 60)
        return 1
    print('  All checks passed')
    print('=' * 60)
    return 0


# SSE

class MCPSseSession:
    """Minimal MCP client over HTTP+SSE (one SSE connection, sequential RPC)."""

    def __init__(self, host: str, port: int = 8080) -> None:
        self.base = f'http://{host}:{port}'
        self._stop = threading.Event()
        self._rx: queue.Queue = queue.Queue()
        self._post_url: str | None = None
        self._post_ready = threading.Event()
        self._thread: threading.Thread | None = None
        self._resp: requests.Response | None = None
        self._req_id = 0

    def start(self) -> None:
        sse_url = f'{self.base}/mcp/sse'
        self._resp = requests.get(sse_url, stream=True, timeout=(15, 120), headers=AUTH_HEADERS)
        self._resp.raise_for_status()
        self._thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._thread.start()
        if not self._post_ready.wait(timeout=20):
            raise TimeoutError('Timed out waiting for SSE endpoint event')

    def close(self) -> None:
        self._stop.set()
        if self._resp is not None:
            try:
                self._resp.close()
            except OSError:
                pass
        if self._thread is not None:
            self._thread.join(timeout=3.0)

    def _reader_loop(self) -> None:
        event_type: str | None = None
        data_parts: list[str] = []
        try:
            assert self._resp is not None
            for raw in self._resp.iter_lines(decode_unicode=True):
                if self._stop.is_set():
                    break
                if raw is None:
                    continue
                line = raw.strip('\r')
                if line == '':
                    self._dispatch_event(event_type, data_parts)
                    event_type = None
                    data_parts = []
                elif line.startswith('event:'):
                    event_type = line[6:].strip()
                elif line.startswith('data:'):
                    data_parts.append(line[5:].lstrip())
                elif line.startswith(':'):
                    continue
        except Exception as e:
            self._rx.put(('error', str(e)))

    def _dispatch_event(self, event_type: str | None, data_parts: list[str]) -> None:
        if not data_parts:
            return
        payload = '\n'.join(data_parts)
        if event_type == 'endpoint':
            path = payload.strip()
            self._post_url = urljoin(self.base + '/', path)
            self._post_ready.set()
        elif event_type == 'message':
            try:
                obj = json.loads(payload)
            except json.JSONDecodeError:
                return
            self._rx.put(('msg', obj))

    def rpc(self, method: str, params: dict | None = None, timeout: float = 30.0) -> dict | None:
        if not self._post_url:
            return None
        self._req_id += 1
        rid = self._req_id
        body: dict = {'jsonrpc': '2.0', 'id': rid, 'method': method}
        if params is not None:
            body['params'] = params
        r = requests.post(
            self._post_url,
            json=body,
            headers={'Content-Type': 'application/json', **AUTH_HEADERS},
            timeout=15,
        )
        r.raise_for_status()
        if r.status_code != 202:
            print(f'  Expected HTTP 202 from POST, got {r.status_code}')
            return None
        t0 = time.monotonic()
        while time.monotonic() - t0 < timeout:
            try:
                kind, item = self._rx.get(timeout=1.0)
            except queue.Empty:
                continue
            if kind == 'error':
                print(f'  SSE reader error: {item}')
                return None
            if kind != 'msg':
                continue
            msg = item
            if msg.get('id') == rid:
                return msg
        print('  Timeout waiting for SSE message event')
        return None


def run_sse(host, port):
    print('=' * 60)
    print('  MCP HTTP+SSE transport smoke')
    print('=' * 60)
    print(f'Target: http://{host}:{port}/mcp/sse')
    print('')

    sess = MCPSseSession(host, port)
    try:
        sess.start()
    except Exception as e:
        print(f'[FAIL] SSE connect: {e}')
        return 1

    print('[PASS] SSE stream open and endpoint received')
    print(f'       POST URL: {sess._post_url}')

    results: list[tuple[str, bool]] = []

    def run_case(name: str, fn) -> None:
        try:
            ok = fn()
        except Exception as e:
            print(f'  [FAIL] {name}: {e}')
            results.append((name, False))
            return
        results.append((name, ok))
        tag = 'PASS' if ok else 'FAIL'
        print(f'  [{tag}] {name}')

    def tools_list_ok() -> bool:
        r = sess.rpc('tools/list')
        if not r or 'result' not in r:
            print(f'    response: {r}')
            return False
        tools = r['result'].get('tools', [])
        names = [t.get('name') for t in tools]
        return any(n and 'player_service' in n for n in names)

    def call_get_volume_ok() -> bool:
        r = sess.rpc(
            'tools/call',
            {'name': 'player_service_get_volume', 'arguments': {}},
        )
        if not r or 'result' not in r:
            print(f'    response: {r}')
            return False
        return not r['result'].get('isError', False)

    def invalid_method_ok() -> bool:
        r = sess.rpc('invalid/method')
        if not r:
            return False
        return 'error' in r and r['error'].get('code') == -32601

    def expect_status(name: str, expected: int, **kwargs) -> None:
        def case() -> bool:
            r = requests.post(timeout=15, **kwargs)
            if r.status_code != expected:
                print(f'    expected HTTP {expected}, got {r.status_code}')
                return False
            return True
        run_case(name, case)

    body = json.dumps({'jsonrpc': '2.0', 'id': 900, 'method': 'tools/list'})
    json_hdr = {'Content-Type': 'application/json', **AUTH_HEADERS}
    messages_url = sess._post_url.split('?', 1)[0]

    run_case('tools/list', tools_list_ok)
    run_case('tools/call player_service_get_volume', call_get_volume_ok)
    run_case('invalid method JSON-RPC error', invalid_method_ok)

    if AUTH_HEADERS:
        expect_status('POST without bearer token -> 401', 401, url=sess._post_url, data=body,
                      headers={'Content-Type': 'application/json'})
        run_case('GET /mcp/sse without bearer token -> 401',
                 lambda: requests.get(f'{sess.base}/mcp/sse', stream=True, timeout=15).status_code == 401)
    else:
        print('  [SKIP] MCP_AUTH_TOKEN not set: 401 checks skipped')
    expect_status('POST without sessionId -> 400', 400, url=messages_url, data=body, headers=json_hdr)
    expect_status('POST with unknown sessionId -> 404', 404,
                  url=f'{messages_url}?sessionId=00000000000000000000000000000000', data=body, headers=json_hdr)
    expect_status('POST with Content-Type: text/plain -> 415', 415, url=sess._post_url, data=body,
                  headers={'Content-Type': 'text/plain', **AUTH_HEADERS})
    expect_status('POST without Content-Type -> 415', 415, url=sess._post_url, data=body, headers=AUTH_HEADERS)
    big = json.dumps({'jsonrpc': '2.0', 'id': 901, 'method': 'tools/list', 'params': {'pad': 'x' * 4096}})
    expect_status(f'POST body of {len(big)} bytes -> 413', 413, url=sess._post_url, data=big, headers=json_hdr)
    expect_status('POST notification -> 202', 202, url=sess._post_url,
                  data=json.dumps({'jsonrpc': '2.0', 'method': 'notifications/initialized'}), headers=json_hdr)
    run_case('session still serves tools/list after the rejected requests', tools_list_ok)

    sess.close()

    print('')
    print('=' * 60)
    failed = sum(1 for _, ok in results if not ok)
    for name, ok in results:
        print(f'  [{"PASS" if ok else "FAIL"}] {name}')
    print(f'  Total: {len(results)}  Failed: {failed}')
    print('=' * 60)
    return 0 if failed == 0 else 1


# WebSocket

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
        self.ws = await websockets.connect(self.ws_url, additional_headers=AUTH_HEADERS)
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

        print(f"  >>> Sending: {data[:120]}{'...' if len(data) > 120 else ''}")
        await self.ws.send(data)

        try:
            response = await asyncio.wait_for(self.ws.recv(), timeout=5.0)
            print(f"  <<< Received: {response[:120]}{'...' if len(response) > 120 else ''}")
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

async def _upgrade_status(ws_url, headers, origin=None):
    """Attempt an upgrade; return 101 on success or the rejection status code"""
    kwargs = {'additional_headers': headers}
    if origin is not None:
        kwargs['origin'] = origin
    try:
        ws = await websockets.connect(ws_url, **kwargs)
    except Exception as e:  # InvalidStatus (>=13) / InvalidStatusCode (<13)
        response = getattr(e, 'response', None)
        status = getattr(response, 'status_code', None) if response is not None else getattr(e, 'status_code', None)
        if status is None:
            raise
        return status
    await ws.close()
    return 101


async def ws_test_upgrade_rules(ws_url):
    """Test 0: Upgrade rules - bearer token (401) and same-origin (403)"""
    print('\n' + '=' * 60)
    print('Test 0: Upgrade Rules (401 / 403)')
    print('=' * 60)
    ok = True

    if AUTH_TOKEN:
        status = await _upgrade_status(ws_url, {})
        if status == 401:
            print('  [PASS] upgrade without bearer token -> 401')
        else:
            print(f'  [FAIL] upgrade without bearer token -> {status}, expected 401')
            ok = False
        status = await _upgrade_status(ws_url, {'Authorization': 'Bearer wrong-' + AUTH_TOKEN})
        if status == 401:
            print('  [PASS] upgrade with wrong bearer token -> 401')
        else:
            print(f'  [FAIL] upgrade with wrong bearer token -> {status}, expected 401')
            ok = False
    else:
        print('  [SKIP] MCP_AUTH_TOKEN not set: 401 checks skipped')

    status = await _upgrade_status(ws_url, AUTH_HEADERS, origin='http://evil.example')
    if WS_CORS_ENABLED:
        if status == 101:
            print('  [PASS] CORS enabled: cross-origin upgrade accepted')
        else:
            print(f'  [FAIL] CORS enabled: cross-origin upgrade -> {status}, expected 101')
            ok = False
    else:
        if status == 403:
            print('  [PASS] CORS disabled: cross-origin upgrade -> 403')
        else:
            print(f'  [FAIL] CORS disabled: cross-origin upgrade -> {status}, expected 403')
            ok = False

    host = ws_url.split('://', 1)[1].split('/', 1)[0]
    status = await _upgrade_status(ws_url, AUTH_HEADERS, origin='http://' + host)
    if status == 101:
        print('  [PASS] same-origin upgrade accepted')
    else:
        print(f'  [FAIL] same-origin upgrade -> {status}, expected 101')
        ok = False

    return ok


async def ws_test_connection(ws_url):
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


async def ws_test_list_tools(ws_url):
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
            print(f"    [{i}] {tool['name']} - {tool.get('description', 'N/A')}")

        assert len(tools) > 0, 'Should have at least one tool'
        print('  [PASS] Tool listing works correctly')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def ws_test_tool_invocation(ws_url):
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
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] Player play')

        # Test 3b: Player - Set Volume
        print('\n  --- 3b: Player Set Volume to 75 ---')
        result = await client.call_tool('player_service_set_volume', {'volume': 75})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] Player set volume')

        # Test 3c: Player - Get Volume
        print('\n  --- 3c: Player Get Volume ---')
        result = await client.call_tool('player_service_get_volume')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] Player get volume')

        # Test 3d: Player - Get Status
        print('\n  --- 3d: Player Get Status ---')
        result = await client.call_tool('player_service_get_status')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] Player get status')

        # Test 3e: LED - Blink
        print('\n  --- 3e: LED Blink (1000ms) ---')
        result = await client.call_tool('led_service_blink', {'period_ms': 1000})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] LED blink')

        # Test 3f: LED - Get State
        print('\n  --- 3f: LED Get State ---')
        result = await client.call_tool('led_service_get_state')
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
        print('  [PASS] LED get state')

        # Test 3g: LED - Set Brightness
        print('\n  --- 3g: LED Set Brightness to 80 ---')
        result = await client.call_tool('led_service_set_brightness', {'brightness': 80})
        assert result is not None, 'Should receive response'
        assert 'result' in result, "Response should have 'result'"
        print(f"  Result: {json.dumps(result['result'], indent=2)}")
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


async def ws_test_persistent_connection(ws_url):
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


async def ws_test_error_handling(ws_url):
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
            print(f"  Error response: {json.dumps(result['error'], indent=2)}")
            print('  [PASS] Invalid tool returns error')
        elif 'result' in result:
            # Some implementations return error in result field
            print(f"  Result: {json.dumps(result['result'], indent=2)}")
            print('  [PASS] Invalid tool handled')

        # Test 5b: Invalid MCP method
        print('\n  --- 5b: Invalid MCP method ---')
        result = await client.call('invalid/method')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f"  Error response: {json.dumps(result['error'], indent=2)}")
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
                    print(f"  Error response: {json.dumps(result['error'], indent=2)}")
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
                print(f"  Error response: {json.dumps(result['error'], indent=2)}")
            print('  [PASS] Missing method field handled')
        else:
            print('  [PASS] Missing method field handled (no response)')

    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        await client.disconnect()

    return passed


async def ws_test_rapid_requests(ws_url):
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


async def ws_test_concurrent_clients(ws_url):
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


async def ws_test_reconnection(ws_url):
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


async def ws_test_json_rpc_compliance(ws_url):
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
            f"Response id ({response.get('id')}) should match request id ({request_id})"
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


async def ws_test_large_payload(ws_url):
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
                print(f"  Error (expected for invalid args): {result['error'].get('message', '')}")
            else:
                print(f"  Result: {json.dumps(result.get('result', {}), indent=2)}")
            print('  [PASS] Large payload handled')
        else:
            print('  [PASS] Large payload handled (no response)')

        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        await client.disconnect()


async def run_ws(ws_url):
    """Run all functional tests"""
    print('=' * 60)
    print('  MCP WebSocket Transport Functional Test Suite')
    print('=' * 60)
    print(f'Target: {ws_url}')
    print('')

    tests = [
        ('Upgrade Rules',         ws_test_upgrade_rules),
        ('Connection',            ws_test_connection),
        ('List Tools',            ws_test_list_tools),
        ('Tool Invocation',       ws_test_tool_invocation),
        ('Persistent Connection', ws_test_persistent_connection),
        ('Error Handling',        ws_test_error_handling),
        ('Rapid Requests',        ws_test_rapid_requests),
        ('Concurrent Clients',    ws_test_concurrent_clients),
        ('Reconnection',          ws_test_reconnection),
        ('JSON-RPC Compliance',   ws_test_json_rpc_compliance),
        ('Large Payload',         ws_test_large_payload),
    ]

    results = {}
    for name, test_func in tests:
        try:
            results[name] = await test_func(ws_url)
        except Exception as e:
            print(f'  [FAIL] Unexpected error in {name}: {e}')
            results[name] = False

    return report_suite(results)


# UART

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
        print(f"  >>> Sent: {data[:100]}{'...' if len(data) > 100 else ''}")

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
                print(f"  <<< Recv: {resp_str[:100]}{'...' if len(resp_str) > 100 else ''}")
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

def uart_test_connection(port, baud):
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


def uart_test_list_tools(port, baud):
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
            print(f"    [{i}] {tool['name']} - {tool.get('description', 'N/A')}")

        assert len(tools) > 0, 'Should have at least one tool'
        print('  [PASS] Tool listing works correctly')
        return True
    except Exception as e:
        print(f'  [FAIL] {e}')
        return False
    finally:
        client.disconnect()


def uart_test_tool_invocation(port, baud):
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
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] Player play')

        # Test 3b: Player - Set Volume
        print('\n  --- 3b: Player Set Volume to 75 ---')
        result = client.call_tool('player_service_set_volume', {'volume': 75})
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] Player set volume')

        # Test 3c: Player - Get Volume
        print('\n  --- 3c: Player Get Volume ---')
        result = client.call_tool('player_service_get_volume')
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] Player get volume')

        # Test 3d: Player - Get Status
        print('\n  --- 3d: Player Get Status ---')
        result = client.call_tool('player_service_get_status')
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] Player get status')

        # Test 3e: LED - Blink
        print('\n  --- 3e: LED Blink (1000ms) ---')
        result = client.call_tool('led_service_blink', {'period_ms': 1000})
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] LED blink')

        # Test 3f: LED - Get State
        print('\n  --- 3f: LED Get State ---')
        result = client.call_tool('led_service_get_state')
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] LED get state')

        # Test 3g: LED - Set Brightness
        print('\n  --- 3g: LED Set Brightness to 80 ---')
        result = client.call_tool('led_service_set_brightness', {'brightness': 80})
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] LED set brightness')

        # Test 3h: LED - On
        print('\n  --- 3h: LED On ---')
        result = client.call_tool('led_service_on')
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
        print('  [PASS] LED on')

        # Test 3i: LED - Off
        print('\n  --- 3i: LED Off ---')
        result = client.call_tool('led_service_off')
        assert result is not None and 'result' in result
        print(f"  Result: {json.dumps(result['result'])}")
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


def uart_test_multiple_requests(port, baud):
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


def uart_test_error_handling(port, baud):
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
            print(f"  Error: {json.dumps(result['error'])}")
            print('  [PASS] Invalid tool returns error')
        elif 'result' in result:
            is_err = result['result'].get('isError', False)
            print(f"  isError={is_err}, result={json.dumps(result['result'])}")
            print('  [PASS] Invalid tool handled')

        # Test 5b: Invalid MCP method
        print('\n  --- 5b: Invalid MCP method ---')
        result = client.call('invalid/method')
        assert result is not None, 'Should receive error response'
        if 'error' in result:
            print(f"  Error: {json.dumps(result['error'])}")
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
                    print(f"  Error: {json.dumps(result['error'])}")
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
                print(f"  Error: {json.dumps(result['error'])}")
            print('  [PASS] Missing method field handled')
        else:
            print('  [PASS] Missing method field handled (no response)')

    except Exception as e:
        print(f'  [FAIL] {e}')
        passed = False
    finally:
        client.disconnect()

    return passed


def uart_test_rapid_requests(port, baud):
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


def uart_test_json_rpc_compliance(port, baud):
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
            f"Response id ({response.get('id')}) should match request id ({req_id})"
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


def uart_test_reconnection(port, baud):
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


def run_uart(port, baud):
    """Run all functional tests"""
    print('=' * 60)
    print('  MCP UART Transport Functional Test Suite')
    print('=' * 60)
    print(f'Serial Port: {port}')
    print(f'Baud Rate:   {baud}')
    print('')

    tests = [
        ('Connection',            lambda: uart_test_connection(port, baud)),
        ('List Tools',            lambda: uart_test_list_tools(port, baud)),
        ('Tool Invocation',       lambda: uart_test_tool_invocation(port, baud)),
        ('Multiple Requests',     lambda: uart_test_multiple_requests(port, baud)),
        ('Error Handling',        lambda: uart_test_error_handling(port, baud)),
        ('Rapid Requests',        lambda: uart_test_rapid_requests(port, baud)),
        ('JSON-RPC Compliance',   lambda: uart_test_json_rpc_compliance(port, baud)),
        ('Reconnection',          lambda: uart_test_reconnection(port, baud)),
    ]

    results = {}
    for name, test_func in tests:
        try:
            results[name] = test_func()
        except Exception as e:
            print(f'  [FAIL] Unexpected error in {name}: {e}')
            results[name] = False

    return report_suite(results)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    modes = parser.add_subparsers(dest='transport', required=True)
    for mode, port in (('http', 8080), ('sse', 8080), ('ws', 8081)):
        sub = modes.add_parser(mode)
        sub.add_argument('host', help='device IP or hostname')
        sub.add_argument('port', nargs='?', type=int, default=port,
                         help=f'transport port (default: {port})')
    uart = modes.add_parser('uart')
    uart.add_argument('device', help='serial port, e.g. /dev/ttyUSB0 or COM3')
    uart.add_argument('baud', nargs='?', type=int, default=115200,
                      help='baud rate (default: 115200)')
    args = parser.parse_args(argv)

    dependency, package = {
        'http': (requests, 'requests'),
        'sse': (requests, 'requests'),
        'ws': (websockets, 'websockets'),
        'uart': (serial, 'pyserial'),
    }[args.transport]
    if dependency is None:
        parser.error(f'{args.transport} requires {package}; install it with pip install {package}')

    if args.transport == 'http':
        return run_http(args.host, args.port)
    if args.transport == 'sse':
        return run_sse(args.host, args.port)
    if args.transport == 'ws':
        return 0 if asyncio.run(run_ws(f'ws://{args.host}:{args.port}/mcp')) else 1
    return 0 if run_uart(args.device, args.baud) else 1


if __name__ == '__main__':
    sys.exit(main())
