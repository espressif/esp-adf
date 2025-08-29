#!/usr/bin/env python3

"""
MCP Bridge — single entry point for all MCP operations.

  discover   Discover and test local MCP (发现测试本地 MCP). No LLM.
  run        Connect to LLM and test MCP (连接大模型测试 MCP). Batch or interactive.
  test       Run automated MCP test suite (自动测试).

Usage:
  python mcp_bridge.py discover [config.json]
  python mcp_bridge.py discover [config.json] --transport sse --sse-base-url http://<ip>:8080
  python mcp_bridge.py run <config.json> [batch|interactive]
  python mcp_bridge.py test [config.json]

Transport options:
  --transport http    Plain HTTP POST (default). MCP_URL / config url = full endpoint.
  --transport sse     HTTP + Server-Sent Events. Use --sse-base-url to override base URL.

MCP URL: set MCP_URL env, or provide a config file with mcp_server.url.
"""

import argparse
import json
import sys

from bridge import (
    MCPLLMBridge,
    get_mcp_url_from_env_or_config,
    load_config,
)
from llm_adapter import create_adapter_from_config
from mcp_client import (
    MCPClient,
    MCPSSEClient,
    generate_tool_test_cases,
    print_tools,
)


# -----------------------------------------------------------------------------
#  discover — Discover and test local MCP (no LLM)
# -----------------------------------------------------------------------------

def _cmd_discover(config_path, transport='http', sse_base_url=None):
    try:
        mcp_url = get_mcp_url_from_env_or_config(config_path)
    except ValueError as e:
        print(str(e))
        print('  Example: MCP_URL=<url> python mcp_bridge.py discover')
        print('           python mcp_bridge.py discover config.json')
        return 1

    print('=' * 70)
    print('  Discover & test local MCP (no LLM)')
    print('=' * 70)

    effective_transport = transport or 'http'

    # For SSE transport the base URL is device root; POST endpoint URL is secondary
    if effective_transport == 'sse':
        base = sse_base_url or mcp_url.rsplit('/mcp', 1)[0]
        print(f'MCP Server (SSE): {base}/mcp/sse\n')
        client = MCPSSEClient(base)
        client.connect()
    else:
        print(f'MCP Server (HTTP): {mcp_url}\n')
        client = MCPClient(mcp_url)

    try:
        print('Calling MCP server tools/list...')
        tools = client.list_tools()
        print(f'✓ Found {len(tools)} tools')
        print_tools(tools)

        llm_config = None
        if config_path:
            try:
                config = load_config(config_path)
                llm_config = config.get('llm')
            except Exception:
                pass

        if llm_config and tools:
            print('\n' + '=' * 70)
            print('  Tool format (for this provider)')
            print('=' * 70)
            try:
                cfg = dict(llm_config)
                if not cfg.get('api_key'):
                    cfg['api_key'] = 'dummy'
                adapter = create_adapter_from_config(cfg)
                sample = adapter.convert_mcp_to_functions(tools[:2])
                print(json.dumps(sample, indent=2))
            except Exception as e:
                print(f'  (skip: {e})')

        print('\n' + '=' * 70)
        print('  Tool execution (schema-based args)')
        print('=' * 70)
        for test in generate_tool_test_cases(tools):
            name, args = test['name'], test['arguments']
            try:
                result = client.call_tool(name, args)
                print(f'  ✓ {name}: {result[:80]}{'...' if len(result) > 80 else ''}')
            except Exception as e:
                print(f'  ✗ {name}: {e}')
    finally:
        if hasattr(client, 'close'):
            client.close()

    print('\nDone.')
    return 0


# -----------------------------------------------------------------------------
#  run — Connect to LLM and test MCP
# -----------------------------------------------------------------------------

def _interactive_mode(bridge):
    print('\n' + '=' * 70)
    print('  Interactive Mode')
    print('=' * 70)
    print("Enter your commands (or 'quit' to exit). Type 'tools' to list tools.\n")
    while True:
        try:
            user_input = input('You: ').strip()
            if not user_input:
                continue
            if user_input.lower() in ('quit', 'exit', 'q'):
                print('Goodbye!')
                break
            if user_input.lower() == 'tools':
                print_tools(bridge.mcp_tools)
                continue
            print('')
            response = bridge.run_conversation(user_input)
            print(f'\nLLM: {response}\n')
        except KeyboardInterrupt:
            print('\nInterrupted. Goodbye!')
            break
        except Exception as e:
            print(f'Error: {e}\n')


def _batch_mode(bridge, test_cases):
    print('\n' + '=' * 70)
    print('  Batch Test Mode')
    print('=' * 70)
    default_cases = [
        {'description': 'LED Control', 'prompt': 'Turn on the LED'},
        {'description': 'Volume Control', 'prompt': 'Set player volume to 70'},
        {'description': 'Status Query', 'prompt': 'What is the current status of the player and LED?'},
    ]
    cases = test_cases if test_cases else default_cases
    for i, tc in enumerate(cases, 1):
        prompt = tc.get('prompt', '')
        if not prompt:
            continue
        print(f'\n--- Test {i}: {tc.get('description', 'Test')} ---')
        print(f'User: {prompt}\n')
        try:
            response = bridge.run_conversation(prompt)
            print(f'LLM: {response}\n')
        except Exception as e:
            print(f'Error: {e}\n')


def _cmd_run(config_path, mode, transport='http', sse_base_url=None):
    print('Loading configuration...')
    try:
        config = load_config(config_path)
    except Exception as e:
        print(f'Error loading config: {e}')
        return 1

    mcp_cfg     = config['mcp_server']
    mcp_url     = mcp_cfg['url']
    timeout     = mcp_cfg.get('timeout', 10)
    # CLI flag takes precedence; fall back to config, then default to 'http'
    cfg_transport = mcp_cfg.get('transport', 'http')
    effective_transport = transport if transport is not None else cfg_transport

    print('=' * 70)
    print('  MCP + LLM Integration Test')
    print('=' * 70)
    print(f'MCP Server:    {mcp_url}')
    print(f'Transport:     {effective_transport}')
    print(f'LLM Provider:  {config['llm']['provider']}')
    print(f'LLM Model:     {config['llm']['model']}\n')

    if effective_transport == 'sse':
        base = sse_base_url or mcp_url.rsplit('/mcp', 1)[0]
        mcp_client = MCPSSEClient(base, timeout=timeout)
        mcp_client.connect()
    else:
        mcp_client = MCPClient(base_url=mcp_url, timeout=timeout)

    try:
        try:
            llm_adapter = create_adapter_from_config(config['llm'])
        except Exception as e:
            print(f'Error creating LLM adapter: {e}')
            return 1

        bridge = MCPLLMBridge(mcp_client, llm_adapter)
        print('Discovering tools from MCP server...')
        bridge.discover_tools()
        print(f'✓ Discovered {len(bridge.mcp_tools)} tools')
        print_tools(bridge.mcp_tools)

        if mode == 'interactive':
            _interactive_mode(bridge)
        else:
            _batch_mode(bridge, config.get('test_cases'))
    finally:
        if hasattr(mcp_client, 'close'):
            mcp_client.close()
    return 0


# -----------------------------------------------------------------------------
#  test — Automated MCP test suite
# -----------------------------------------------------------------------------

class _IntegrationTester:
    def __init__(self, mcp_url):
        self.mcp_client = MCPClient(mcp_url)
        self.tools = []

    def test_tool_discovery(self):
        print('\n[TEST] Tool Discovery')
        try:
            self.tools = self.mcp_client.list_tools()
            if len(self.tools) == 0:
                print('  ✗ FAIL: No tools found')
                return False
            print(f'  ✓ Discovered {len(self.tools)} tools')
            return True
        except Exception as e:
            print(f'  ✗ FAIL: {e}')
            return False

    def test_schema_validation(self):
        print('\n[TEST] Tool Schema Validation')
        for tool in self.tools:
            for field in ('name', 'description', 'inputSchema'):
                if field not in tool:
                    print(f"  ✗ FAIL: Tool {tool.get('name', '?')} missing '{field}'")
                    return False
            if tool.get('inputSchema', {}).get('type') != 'object':
                print(f"  ✗ FAIL: Tool {tool['name']} inputSchema type must be 'object'")
                return False
        print(f'  ✓ All {len(self.tools)} tool schemas valid')
        return True

    def test_tool_execution(self):
        print('\n[TEST] Tool Execution')
        passed = failed = 0
        for test in generate_tool_test_cases(self.tools):
            try:
                r = self.mcp_client.call_tool(test['name'], test['arguments'])
                print(f'  ✓ {test['name']}: {r[:50]}...')
                passed += 1
            except Exception as e:
                print(f'  ✗ {test['name']}: {e}')
                failed += 1
        print(f'\n  Summary: {passed} passed, {failed} failed')
        return failed == 0

    def test_error_handling(self):
        print('\n[TEST] Error Handling')
        try:
            self.mcp_client.call_tool('nonexistent_tool', {})
        except Exception:
            pass
        print('  ✓ Invalid tool handled')
        return True

    def test_sequential_calls(self):
        print('\n[TEST] Sequential Tool Calls')
        cases = generate_tool_test_cases(self.tools)
        if len(cases) < 2:
            print('  (skip: need at least 2 tools)')
            return True
        try:
            for test in cases[:3]:
                r = self.mcp_client.call_tool(test['name'], test['arguments'])
                print(f'  ✓ {test['name']}: {r[:60]}{'...' if len(r) > 60 else ''}')
            return True
        except Exception as e:
            print(f'  ✗ FAIL: {e}')
            return False

    def run_all(self):
        print('=' * 70)
        print('  MCP Integration Test Suite')
        print('=' * 70)
        tests = [
            self.test_tool_discovery,
            self.test_schema_validation,
            self.test_tool_execution,
            self.test_error_handling,
            self.test_sequential_calls,
        ]
        passed = sum(1 for t in tests if t())
        print('\n' + '=' * 70)
        print(f'  Test Results: {passed}/{len(tests)} passed')
        print('=' * 70)
        return passed == len(tests)


def _cmd_test(config_path):
    try:
        mcp_url = get_mcp_url_from_env_or_config(config_path)
    except ValueError as e:
        print(str(e))
        print('  Example: MCP_URL=<url> python mcp_bridge.py test')
        print('           python mcp_bridge.py test config.json')
        return 1
    print(f'MCP Server: {mcp_url}\n')
    tester = _IntegrationTester(mcp_url)
    return 0 if tester.run_all() else 1


# -----------------------------------------------------------------------------
#  Main
# -----------------------------------------------------------------------------

def _add_transport_args(parser):
    """Add --transport and --sse-base-url to a sub-command parser."""
    parser.add_argument(
        '--transport',
        choices=['http', 'sse'],
        default=None,
        help=(
            "Transport to use: 'http' (plain POST) or "
            "'sse' (HTTP + Server-Sent Events, MCP recommended). "
            "Defaults to the value in config, or 'http' if unset."
        ),
    )
    parser.add_argument(
        '--sse-base-url',
        default=None,
        metavar='URL',
        help=(
            'Device base URL for SSE transport (e.g. http://10.18.37.200:8080). '
            "Defaults to mcp_server.url with '/mcp' suffix stripped."
        ),
    )


def main():
    parser = argparse.ArgumentParser(
        description='MCP Bridge: discover local MCP, run with LLM, or run auto tests.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest='command', required=True)

    discover_p = sub.add_parser('discover', help='Discover and test local MCP (no LLM)')
    discover_p.add_argument('config', nargs='?', default=None,
                             help='Optional config for MCP URL and tool format')
    _add_transport_args(discover_p)

    run_p = sub.add_parser('run', help='Connect to LLM and test MCP (batch or interactive)')
    run_p.add_argument('config', help='Config file (mcp_server, llm, test_cases)')
    run_p.add_argument('mode', nargs='?', default='batch',
                        choices=['batch', 'interactive'],
                        help='batch (default) or interactive')
    _add_transport_args(run_p)

    test_p = sub.add_parser('test', help='Run automated MCP test suite')
    test_p.add_argument('config', nargs='?', default=None,
                         help='Optional config for MCP URL')

    args = parser.parse_args()

    if args.command == 'discover':
        sys.exit(_cmd_discover(args.config,
                               transport=args.transport,
                               sse_base_url=args.sse_base_url))
    if args.command == 'run':
        sys.exit(_cmd_run(args.config, args.mode,
                          transport=args.transport,
                          sse_base_url=args.sse_base_url))
    sys.exit(_cmd_test(args.config))


if __name__ == '__main__':
    main()
