#!/usr/bin/env python3

"""
MCP <-> LLM bridge and configuration loading.

This module glues an MCP client (see :mod:`mcp_client`) to an LLM adapter
(see :mod:`llm_adapter`) and runs a tool-using conversation loop. It also
exposes helpers to load JSON configs with ``${ENV_VAR}`` substitution and to
resolve the MCP server URL from either config or the ``MCP_URL`` environment
variable.
"""

import json
import os
import re
from typing import Dict, Optional

from llm_adapter import LLMAdapter
from mcp_client import MCPClient


# -----------------------------------------------------------------------------
#  Bridge
# -----------------------------------------------------------------------------

class MCPLLMBridge:
    """
    Facade that orchestrates an MCP device and an LLM.

    Design:

    * **Adapter** – MCP tools are re-shaped for the target LLM.
    * **Strategy** – the concrete LLM is chosen via the injected adapter.
    """

    def __init__(self, mcp_client: MCPClient, llm_adapter: LLMAdapter):
        self.mcp_client = mcp_client
        self.llm_adapter = llm_adapter
        self.mcp_tools: list = []
        self.llm_tools: list = []

    def discover_tools(self) -> int:
        """Fetch tools from the MCP server and convert them to LLM format."""
        print('Discovering MCP tools...')
        self.mcp_tools = self.mcp_client.list_tools()
        print(f'Found {len(self.mcp_tools)} MCP tools')

        print('Converting to LLM function format...')
        self.llm_tools = self.llm_adapter.convert_mcp_to_functions(self.mcp_tools)
        print(f'Converted {len(self.llm_tools)} tools')

        return len(self.mcp_tools)

    def execute_tool_call(self, tool_call: Dict) -> str:
        """Execute a single tool call via MCP and return its textual result."""
        name = tool_call.get('name', '')
        arguments = tool_call.get('arguments', {})

        print(f'  Executing: {name}({json.dumps(arguments)})')
        result = self.mcp_client.call_tool(name, arguments)
        print(f'  Result: {result}')
        return result

    def run_conversation(self, user_message: str, max_iterations: int = 5) -> str:
        """
        Run a tool-using conversation loop until the LLM produces a final
        answer or ``max_iterations`` is reached.
        """
        if not self.llm_tools:
            self.discover_tools()

        messages = [{'role': 'user', 'content': user_message}]

        for iteration in range(max_iterations):
            print(f'\n--- Iteration {iteration + 1} ---')
            print('Calling LLM...')

            response = self.llm_adapter.call_with_tools(messages, self.llm_tools)
            tool_calls = self.llm_adapter.extract_tool_calls(response)

            if not tool_calls:
                print('LLM provided final answer (no tool calls)')
                return self.llm_adapter.extract_text(response)

            print(f'LLM requested {len(tool_calls)} tool call(s):')
            tool_results: list = []
            for tool_call in tool_calls:
                result = self.execute_tool_call(tool_call)
                tool_results.append(result)

            for msg in self.llm_adapter.build_turn_messages(response, tool_calls, tool_results):
                messages.append(msg)

        print(f'\nReached max iterations ({max_iterations})')
        return 'Max iterations reached'


# -----------------------------------------------------------------------------
#  Configuration loading
# -----------------------------------------------------------------------------

def load_config(config_file: str) -> Dict:
    """
    Load a JSON config file and expand ``${ENV_VAR}`` / ``${ENV_VAR:-default}``
    placeholders.

    Unset ``${ENV_VAR}`` (without default) resolves to the empty string, so
    downstream fallback logic (e.g. :func:`create_adapter_from_config`) can
    substitute its own provider-specific env var or fail fast.

    The ``MCP_URL`` environment variable, when set, overrides
    ``mcp_server.url`` in the resulting dict.

    Example config::

        {
          "mcp_server": {"url": "http://10.18.37.200:8080/mcp", "timeout": 10},
          "llm": {"provider": "deepseek", "api_key": "${DEEPSEEK_API_KEY}",
                  "model": "deepseek-chat"}
        }
    """
    with open(config_file, 'r') as f:
        config = json.load(f)

    def replace_env_vars(obj):
        if isinstance(obj, dict):
            return {k: replace_env_vars(v) for k, v in obj.items()}
        if isinstance(obj, list):
            return [replace_env_vars(item) for item in obj]
        if isinstance(obj, str):
            def replacer(match):
                expr = match.group(1)
                if expr and ':-' in expr:
                    var_name, default = expr.split(':-', 1)
                    return os.getenv(var_name, default)
                var_name = expr or match.group(2)
                return os.getenv(var_name, '')
            return re.sub(r'\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*)', replacer, obj)
        return obj

    config = replace_env_vars(config)

    mcp_url = os.getenv('MCP_URL')
    if mcp_url and isinstance(config.get('mcp_server'), dict):
        config['mcp_server']['url'] = mcp_url
    return config


def get_mcp_url_from_env_or_config(config_path: Optional[str] = None) -> str:
    """
    Resolve the MCP server URL from either a config file or the ``MCP_URL``
    environment variable. Raises :class:`ValueError` if neither is set.
    """
    if config_path:
        config = load_config(config_path)
        url = config.get('mcp_server', {}).get('url', '') if isinstance(config.get('mcp_server'), dict) else ''
        if url:
            return url
    url = os.environ.get('MCP_URL', '')
    if url:
        return url
    raise ValueError(
        'MCP URL not set. Set MCP_URL environment variable or pass a config '
        'file (with mcp_server.url).'
    )
