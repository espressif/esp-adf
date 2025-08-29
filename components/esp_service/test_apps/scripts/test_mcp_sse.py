#!/usr/bin/env python3

"""
MCP HTTP+SSE transport smoke client.

Opens GET /mcp/sse, reads the mandatory ``endpoint`` event, then sends JSON-RPC
via POST /mcp/messages?sessionId=... and reads matching ``message`` events on
the SSE stream (MCP 2024-11-05 style).

Usage:
    python3 scripts/test_mcp_sse.py <device-ip> [http-port]

Requirements:
    pip install requests

Firmware: enable ESP_SVC_TEST_ENABLE_SSE_TRANSPORT and ESP_SVC_TEST_SSE_HOLD_AFTER_SMOKE
so the server stays up after on-device Unity smoke.
"""

from __future__ import annotations

import json
import queue
import sys
import threading
import time
from urllib.parse import urljoin

try:
    import requests
except ImportError:
    print("Error: 'requests' package required.")
    print('Install with: pip install requests')
    sys.exit(1)


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
        self._resp = requests.get(sse_url, stream=True, timeout=(15, 120))
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
            headers={'Content-Type': 'application/json'},
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


def main() -> int:
    if len(sys.argv) < 2:
        print('Usage: python3 test_mcp_sse.py <device-ip> [http-port]')
        print('Example: python3 test_mcp_sse.py 192.168.1.50')
        print('Example: python3 test_mcp_sse.py 192.168.1.50 8080')
        return 1

    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080

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

    run_case('tools/list', tools_list_ok)
    run_case('tools/call player_service_get_volume', call_get_volume_ok)
    run_case('invalid method JSON-RPC error', invalid_method_ok)

    sess.close()

    print('')
    print('=' * 60)
    failed = sum(1 for _, ok in results if not ok)
    for name, ok in results:
        print(f'  [{"PASS" if ok else "FAIL"}] {name}')
    print(f'  Total: {len(results)}  Failed: {failed}')
    print('=' * 60)
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
