#!/usr/bin/env python3

"""
MCP client transports and shared tool utilities.

Two transports are provided:

* ``MCPClient``     – plain HTTP POST (single JSON-RPC endpoint).
* ``MCPSSEClient``  – HTTP + Server-Sent Events, the MCP-spec recommended
                      transport (used by Vercel AI SDK and other modern
                      clients).

Use :func:`create_mcp_client` as the generic factory.
"""

import json
import threading
from typing import Any, Dict, List, Optional

import requests


# -----------------------------------------------------------------------------
#  Plain HTTP transport
# -----------------------------------------------------------------------------

class MCPClient:
    """
    MCP client for plain HTTP POST transport.

    Connects to an MCP server that exposes a single JSON-RPC POST endpoint,
    e.g. ``http://<device>:8080/mcp``.
    """

    def __init__(self, base_url: str, timeout: int = 10):
        """
        Args:
            base_url: MCP server URL (e.g. ``http://10.18.37.200:8080/mcp``)
            timeout:  Request timeout in seconds.
        """
        self.base_url = base_url
        self.timeout = timeout
        self.request_id = 0

    def _next_id(self) -> int:
        self.request_id += 1
        return self.request_id

    def _jsonrpc_call(self, method: str, params: Optional[Dict] = None) -> Dict:
        """Make a JSON-RPC 2.0 POST call."""
        request = {
            'jsonrpc': '2.0',
            'id': self._next_id(),
            'method': method,
        }
        if params:
            request['params'] = params

        response = requests.post(
            self.base_url,
            json=request,
            headers={'Content-Type': 'application/json'},
            timeout=self.timeout,
        )
        response.raise_for_status()
        return response.json()

    def list_tools(self) -> List[Dict]:
        """List all available MCP tools."""
        result = self._jsonrpc_call('tools/list')
        if 'result' in result and 'tools' in result['result']:
            return result['result']['tools']
        return []

    def call_tool(self, name: str, arguments: Optional[Dict] = None) -> str:
        """
        Call an MCP tool and return the result text.

        Args:
            name:      Tool name.
            arguments: Tool arguments (optional).

        Returns:
            Result text from tool execution.
        """
        params = {'name': name, 'arguments': arguments or {}}
        result = self._jsonrpc_call('tools/call', params)

        if 'result' in result:
            content = result['result'].get('content', [])
            if content:
                return content[0].get('text', '')
        elif 'error' in result:
            return f'Error: {result['error'].get('message', 'Unknown error')}'
        return ''


# -----------------------------------------------------------------------------
#  HTTP + Server-Sent Events transport
# -----------------------------------------------------------------------------

class MCPSSEClient:
    """
    MCP client for the HTTP + Server-Sent Events (SSE) transport.

    Protocol flow
    -------------
    1. Open a persistent ``GET <sse_url>`` connection.
    2. Parse the ``endpoint`` SSE event to learn the messages URL
       (``/mcp/messages?sessionId=<id>``).
    3. For each JSON-RPC call: POST the request to the messages URL.
    4. The response arrives asynchronously as a ``message`` SSE event on the
       open GET connection.

    The SSE stream is consumed in a background thread. Responses are matched
    to in-flight requests by JSON-RPC ``id``.

    Usage
    -----
    >>> client = MCPSSEClient("http://10.18.37.200:8080")
    >>> with client:
    ...     tools  = client.list_tools()
    ...     result = client.call_tool("led_service_blink", {})
    """

    def __init__(self, base_url: str, sse_path: str = '/mcp/sse', timeout: int = 10):
        """
        Args:
            base_url: Device base URL (e.g. ``http://10.18.37.200:8080``).
            sse_path: SSE subscription path (default ``/mcp/sse``).
            timeout:  Timeout in seconds for POST requests.
        """
        self.base_url = base_url.rstrip('/')
        self.sse_path = sse_path
        self.timeout = timeout

        self._request_id = 0
        self._messages_url: Optional[str] = None
        self._pending: Dict[int, threading.Event] = {}
        self._results: Dict[int, Dict] = {}
        self._lock = threading.Lock()
        self._ready = threading.Event()
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._session = requests.Session()

    # -- context manager -----------------------------------------------------

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *_):
        self.close()

    # -- connection lifecycle ------------------------------------------------

    def connect(self, wait_timeout: float = 10.0) -> None:
        """Open the SSE channel and wait for the endpoint event."""
        self._stop.clear()
        self._ready.clear()
        self._thread = threading.Thread(target=self._sse_reader, daemon=True)
        self._thread.start()

        if not self._ready.wait(timeout=wait_timeout):
            self.close()
            raise TimeoutError(
                f'SSE endpoint event not received within {wait_timeout}s. '
                'Check that the device is running and the URL is correct.'
            )

    def close(self) -> None:
        """Shut down the SSE reader thread."""
        self._stop.set()
        self._session.close()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)

    # -- SSE reader (background thread) --------------------------------------

    def _sse_reader(self) -> None:
        """Maintain the SSE GET connection and parse events."""
        url = self.base_url + self.sse_path
        try:
            with self._session.get(
                url,
                stream=True,
                headers={'Accept': 'text/event-stream', 'Cache-Control': 'no-cache'},
                timeout=(5, None),  # connect_timeout only; block on read
            ) as resp:
                resp.raise_for_status()
                self._parse_sse_stream(resp)
        except Exception as exc:
            if not self._stop.is_set():
                print(f'[MCPSSEClient] SSE reader error: {exc}')
        finally:
            self._stop.set()
            with self._lock:
                for ev in self._pending.values():
                    ev.set()

    def _parse_sse_stream(self, response: Any) -> None:
        """Parse raw SSE bytes from the HTTP response."""
        event_type = ''
        data_lines: List[str] = []

        for raw_line in response.iter_lines(decode_unicode=True):
            if self._stop.is_set():
                break

            line: str = raw_line

            if not line:
                if data_lines:
                    self._dispatch_event(event_type, '\n'.join(data_lines))
                event_type = ''
                data_lines = []
                continue

            if line.startswith(':'):
                # SSE comment (keepalive) -- ignore
                continue
            if line.startswith('event:'):
                event_type = line[len('event:'):].strip()
            elif line.startswith('data:'):
                data_lines.append(line[len('data:'):].strip())

    def _dispatch_event(self, event_type: str, data: str) -> None:
        """Handle a fully assembled SSE event."""
        if event_type == 'endpoint':
            path = data.strip()
            if path.startswith('http'):
                self._messages_url = path
            else:
                self._messages_url = self.base_url + path
            self._ready.set()

        elif event_type == 'message':
            try:
                payload = json.loads(data)
            except json.JSONDecodeError:
                return

            req_id = payload.get('id')
            if req_id is not None:
                with self._lock:
                    self._results[req_id] = payload
                    ev = self._pending.get(req_id)
                    if ev:
                        ev.set()

    # -- JSON-RPC over SSE ---------------------------------------------------

    def _next_id(self) -> int:
        with self._lock:
            self._request_id += 1
            return self._request_id

    def _jsonrpc_call(self, method: str, params: Optional[Dict] = None) -> Dict:
        """
        Send a JSON-RPC request via POST and wait for the SSE response.

        Raises:
            RuntimeError: If the SSE channel is not open.
            TimeoutError: If no SSE response arrives within ``self.timeout``.
        """
        if not self._messages_url:
            raise RuntimeError(
                "SSE channel not open. Call connect() first (or use 'with' statement)."
            )

        req_id = self._next_id()
        request: Dict[str, Any] = {'jsonrpc': '2.0', 'id': req_id, 'method': method}
        if params:
            request['params'] = params

        ready_event = threading.Event()
        with self._lock:
            self._pending[req_id] = ready_event

        try:
            resp = requests.post(
                self._messages_url,
                json=request,
                headers={'Content-Type': 'application/json'},
                timeout=self.timeout,
            )
            resp.raise_for_status()
            # Server returns 202 Accepted; real response arrives over SSE.
        except Exception:
            with self._lock:
                self._pending.pop(req_id, None)
            raise

        if not ready_event.wait(timeout=self.timeout):
            with self._lock:
                self._pending.pop(req_id, None)
                self._results.pop(req_id, None)
            raise TimeoutError(
                f'No SSE response for request id={req_id} within {self.timeout}s'
            )

        with self._lock:
            self._pending.pop(req_id, None)
            return self._results.pop(req_id, {})

    def list_tools(self) -> List[Dict]:
        """List all available MCP tools via the SSE transport."""
        result = self._jsonrpc_call('tools/list')
        if 'result' in result and 'tools' in result['result']:
            return result['result']['tools']
        return []

    def call_tool(self, name: str, arguments: Optional[Dict] = None) -> str:
        """Call an MCP tool via the SSE transport and return the result text."""
        params = {'name': name, 'arguments': arguments or {}}
        result = self._jsonrpc_call('tools/call', params)

        if 'result' in result:
            content = result['result'].get('content', [])
            if content:
                return content[0].get('text', '')
        elif 'error' in result:
            return f'Error: {result['error'].get('message', 'Unknown error')}'
        return ''


# -----------------------------------------------------------------------------
#  Factory + shared utilities
# -----------------------------------------------------------------------------

def create_mcp_client(base_url: str,
                      transport: str = 'http',
                      timeout: int = 10,
                      sse_path: str = '/mcp/sse'):
    """
    Create the right MCP client for the given transport.

    Args:
        base_url:  HTTP transport: full endpoint URL (``http://ip:8080/mcp``).
                   SSE transport:  device base URL  (``http://ip:8080``).
        transport: ``"http"`` (plain POST) or ``"sse"``.
        timeout:   Request timeout in seconds.
        sse_path:  SSE subscription path (SSE only; default ``/mcp/sse``).

    Returns:
        ``MCPClient`` or ``MCPSSEClient`` instance.
    """
    if transport == 'sse':
        return MCPSSEClient(base_url, sse_path=sse_path, timeout=timeout)
    return MCPClient(base_url, timeout=timeout)


def print_tools(tools: List[Dict]) -> None:
    """Pretty-print MCP tools grouped by ``<service>_service_`` prefix."""
    print(f'\nAvailable Tools ({len(tools)}):')
    print('-' * 60)
    services: Dict[str, list] = {}
    for tool in tools:
        service_name = tool.get('name', '').split('_service_')[0] + '_service'
        services.setdefault(service_name, []).append(tool)

    for service_name, service_tools in services.items():
        print(f'\n{service_name.upper()}:')
        for tool in service_tools:
            name = tool.get('name', '')
            method = name.split('_service_', 1)[1] if '_service_' in name else name
            print(f'  - {method}')
            print(f'    {tool.get('description', 'No description')}')
            props = tool.get('inputSchema', {}).get('properties', {})
            if props:
                print('    Parameters:')
                for param_name, param_info in props.items():
                    ptype = param_info.get('type', 'any')
                    pdesc = param_info.get('description', '')
                    print(f'      - {param_name} ({ptype}): {pdesc}')


def generate_tool_test_cases(tools: List[Dict]) -> List[Dict]:
    """Generate minimal schema-based test cases from discovered tools."""
    cases = []
    for tool in tools:
        name = tool.get('name', '')
        props = tool.get('inputSchema', {}).get('properties', {})
        arguments: Dict[str, Any] = {}
        for prop_name, prop_info in props.items():
            ptype = prop_info.get('type', 'string')
            if ptype == 'integer':
                lo = prop_info.get('minimum', 0)
                hi = prop_info.get('maximum', 100)
                arguments[prop_name] = (lo + hi) // 2
            elif ptype == 'string':
                arguments[prop_name] = 'test'
            elif ptype == 'boolean':
                arguments[prop_name] = True
        cases.append({'name': name, 'arguments': arguments})
    return cases
