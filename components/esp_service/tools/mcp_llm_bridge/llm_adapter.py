#!/usr/bin/env python3

"""
LLM adapters.

Each adapter converts MCP tool definitions into the function-calling format
used by a specific LLM provider and executes a single chat-completions call.
The bridge (see ``bridge.py``) drives them with a Strategy pattern so the
core conversation loop is provider-agnostic.

Supported providers (created by :func:`create_adapter_from_config`):

* ``gemini``   – Google Gemini
* ``deepseek`` – DeepSeek (OpenAI-compatible)
* ``openai``   – OpenAI GPT
* ``doubao``   – ByteDance Doubao / Volcano Engine (via OpenAI SDK)
"""

import json
import os
from abc import ABC, abstractmethod
from typing import Dict, List

import requests


# -----------------------------------------------------------------------------
#  Abstract base
# -----------------------------------------------------------------------------

class LLMAdapter(ABC):
    """Abstract base class for LLM adapters."""

    @abstractmethod
    def convert_mcp_to_functions(self, mcp_tools: List[Dict]) -> List[Dict]:
        """Convert MCP tool schemas to the LLM's function-calling format."""

    @abstractmethod
    def call_with_tools(self, messages: List[Dict], tools: List[Dict]) -> Dict:
        """Call the LLM with tools and return the raw response dict."""

    @abstractmethod
    def extract_tool_calls(self, response: Dict) -> List[Dict]:
        """Extract tool call requests from an LLM response."""

    @abstractmethod
    def extract_text(self, response: Dict) -> str:
        """Extract the final assistant text from an LLM response."""

    def build_turn_messages(self, raw_response: Dict, tool_calls: List[Dict],
                            tool_results: List[str]) -> List[Dict]:
        """Build follow-up messages to append after tool execution.

        Default implementation uses a plain assistant/user exchange, which
        works for Gemini and other non-OpenAI formats.  OpenAI-compatible
        adapters override this to produce role='tool' messages with
        tool_call_id as required by the OpenAI chat completions API.
        """
        call_summary = [{'tool': tc['name'], 'arguments': tc['arguments']}
                        for tc in tool_calls]
        result_summary = [{'tool': tc['name'], 'result': r}
                          for tc, r in zip(tool_calls, tool_results)]
        return [
            {'role': 'assistant', 'content': f'Tool calls: {json.dumps(call_summary)}'},
            {'role': 'user', 'content': f'Tool results: {json.dumps(result_summary)}'},
        ]


# -----------------------------------------------------------------------------
#  OpenAI-compatible adapters
# -----------------------------------------------------------------------------

class OpenAICompatibleAdapter(LLMAdapter):
    """
    Base adapter for all OpenAI-compatible APIs (DeepSeek, OpenAI, Doubao, ...).

    Centralises convert/extract/call logic shared by any provider that follows
    the OpenAI chat-completions format.
    """

    def __init__(self, api_key: str, model: str, base_url: str):
        self.api_key = api_key
        self.model = model
        self.base_url = base_url

    def convert_mcp_to_functions(self, mcp_tools: List[Dict]) -> List[Dict]:
        functions = []
        for tool in mcp_tools:
            func: Dict = {
                'type': 'function',
                'function': {
                    'name': tool['name'],
                    'description': tool.get('description', ''),
                },
            }
            if 'inputSchema' in tool:
                func['function']['parameters'] = tool['inputSchema']
            functions.append(func)
        return functions

    def call_with_tools(self, messages: List[Dict], tools: List[Dict]) -> Dict:
        request_data = {
            'model': self.model,
            'messages': messages,
            'tools': tools,
            'tool_choice': 'auto',
        }
        headers = {
            'Content-Type': 'application/json',
            'Authorization': f'Bearer {self.api_key}',
        }
        response = requests.post(
            self.base_url,
            json=request_data,
            headers=headers,
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    def extract_tool_calls(self, response: Dict) -> List[Dict]:
        tool_calls = []
        for choice in response.get('choices', []):
            tc_list = choice.get('message', {}).get('tool_calls')
            if tc_list:
                for tc in tc_list:
                    if tc.get('type') == 'function':
                        function = tc.get('function', {})
                        tool_calls.append({
                            'id': tc.get('id', ''),
                            'name': function.get('name', ''),
                            'arguments': json.loads(function.get('arguments', '{}')),
                        })
        return tool_calls

    def build_turn_messages(self, raw_response: Dict, tool_calls: List[Dict],
                            tool_results: List[str]) -> List[Dict]:
        """Build role='tool' follow-up messages required by OpenAI-compatible APIs."""
        assistant_msg = (raw_response.get('choices') or [{}])[0].get('message', {})
        msgs: List[Dict] = [assistant_msg]
        for tc, result in zip(tool_calls, tool_results):
            msgs.append({
                'role': 'tool',
                'tool_call_id': tc.get('id', ''),
                'content': result,
            })
        return msgs

    def extract_text(self, response: Dict) -> str:
        choices = response.get('choices', [])
        if choices:
            content = choices[0].get('message', {}).get('content')
            return content or ''
        return ''


class DeepSeekAdapter(OpenAICompatibleAdapter):
    """Adapter for DeepSeek API."""

    def __init__(self, api_key: str, model: str = 'deepseek-chat'):
        super().__init__(api_key, model, 'https://api.deepseek.com/v1/chat/completions')


class OpenAIAdapter(OpenAICompatibleAdapter):
    """Adapter for OpenAI API (GPT-4, etc.)."""

    def __init__(self, api_key: str, model: str = 'gpt-4-turbo-preview'):
        super().__init__(api_key, model, 'https://api.openai.com/v1/chat/completions')


class DoubaoAdapter(OpenAICompatibleAdapter):
    """
    Adapter for Doubao (ByteDance Volcano Engine API).

    Uses the OpenAI SDK pointed at the Volcano Engine base URL.
    Ref: https://www.volcengine.com/docs/82379/1399008
    """

    def __init__(self, api_key: str, model: str = 'doubao-seed-1-8-251228', endpoint_id: str = ''):
        """
        Args:
            api_key:     Volcano Engine API key (``ARK_API_KEY``).
            model:       Model name (e.g. ``doubao-seed-1-8-251228``).
            endpoint_id: Endpoint ID from the Volcano Engine console
                         (overrides ``model`` when non-empty).
        """
        effective_model = endpoint_id if endpoint_id else model
        super().__init__(api_key, effective_model, 'https://ark.cn-beijing.volces.com/api/v3')
        self._client = None

    def _get_client(self):
        """Lazy-init the OpenAI client for Volcano Engine."""
        if self._client is None:
            try:
                from openai import OpenAI
            except ImportError as exc:
                raise ImportError(
                    'openai package required for DoubaoAdapter. '
                    "Install: pip install --upgrade 'openai>=1.0'"
                ) from exc
            self._client = OpenAI(
                base_url=self.base_url,
                api_key=self.api_key,
            )
        return self._client

    def call_with_tools(self, messages: List[Dict], tools: List[Dict]) -> Dict:
        client = self._get_client()
        completion = client.chat.completions.create(
            model=self.model,
            messages=messages,
            tools=tools,
            tool_choice='auto',
        )
        return completion.model_dump()


# -----------------------------------------------------------------------------
#  Gemini adapter (non-OpenAI format)
# -----------------------------------------------------------------------------

class GeminiAdapter(LLMAdapter):
    """Adapter for Google Gemini API."""

    def __init__(self, api_key: str, model: str = 'gemini-pro'):
        self.api_key = api_key
        self.model = model
        self.base_url = (
            f'https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent'
        )

    def convert_mcp_to_functions(self, mcp_tools: List[Dict]) -> List[Dict]:
        functions = []
        for tool in mcp_tools:
            func: Dict = {
                'name': tool['name'],
                'description': tool.get('description', ''),
            }
            if 'inputSchema' in tool:
                schema = tool['inputSchema']
                if schema.get('type') == 'object':
                    func['parameters'] = {
                        'type': 'object',
                        'properties': schema.get('properties', {}),
                    }
                    if 'required' in schema:
                        func['parameters']['required'] = schema['required']
            functions.append(func)
        return functions

    def call_with_tools(self, messages: List[Dict], tools: List[Dict]) -> Dict:
        request_data = {
            'contents': messages,
            'tools': [{'functionDeclarations': tools}],
        }
        response = requests.post(
            self.base_url,
            json=request_data,
            headers={
                'Content-Type': 'application/json',
                'x-goog-api-key': self.api_key,
            },
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    def extract_tool_calls(self, response: Dict) -> List[Dict]:
        tool_calls = []
        for candidate in response.get('candidates', []):
            for part in candidate.get('content', {}).get('parts', []):
                if 'functionCall' in part:
                    func_call = part['functionCall']
                    tool_calls.append({
                        'name': func_call.get('name', ''),
                        'arguments': func_call.get('args', {}),
                    })
        return tool_calls

    def extract_text(self, response: Dict) -> str:
        candidates = response.get('candidates', [])
        if candidates:
            parts = candidates[0].get('content', {}).get('parts', [])
            if parts and 'text' in parts[0]:
                return parts[0]['text']
        return ''


# -----------------------------------------------------------------------------
#  Factory
# -----------------------------------------------------------------------------

def create_adapter_from_config(config: Dict) -> LLMAdapter:
    """
    Create an :class:`LLMAdapter` from a configuration dict.

    Config values fall back to environment variables when left empty:

    * ``deepseek`` – ``DEEPSEEK_API_KEY``, ``DEEPSEEK_MODEL``
    * ``gemini``   – ``GEMINI_API_KEY``, ``GEMINI_MODEL``
    * ``doubao``   – ``DOUBAO_API_KEY``, ``DOUBAO_MODEL``, ``DOUBAO_ENDPOINT_ID``
    * ``openai``   – ``OPENAI_API_KEY``

    Args:
        config: ``{"provider": ..., "api_key": ..., "model": ...}`` and
                optionally ``"endpoint_id"`` for Doubao.

    Returns:
        An :class:`LLMAdapter` instance.

    Raises:
        ValueError: If ``provider`` is unknown or no API key is available.
    """
    provider = config.get('provider', '').lower()
    api_key = config.get('api_key', '')
    model = config.get('model', '')
    endpoint_id = ''

    if provider == 'doubao':
        api_key = api_key or os.environ.get('DOUBAO_API_KEY', '')
        model = model or os.environ.get('DOUBAO_MODEL', 'doubao-seed-1-8-251228')
        endpoint_id = config.get('endpoint_id', '') or os.environ.get('DOUBAO_ENDPOINT_ID', '')
    elif provider == 'deepseek':
        api_key = api_key or os.environ.get('DEEPSEEK_API_KEY', '')
        model = model or os.environ.get('DEEPSEEK_MODEL', 'deepseek-chat')
    elif provider == 'gemini':
        api_key = api_key or os.environ.get('GEMINI_API_KEY', '')
        model = model or os.environ.get('GEMINI_MODEL', 'gemini-pro')
    elif provider == 'openai':
        api_key = api_key or os.environ.get('OPENAI_API_KEY', '')
        model = model or 'gpt-4-turbo-preview'

    if not api_key:
        raise ValueError('API key is required')

    if provider == 'gemini':
        return GeminiAdapter(api_key, model or 'gemini-pro')
    if provider == 'deepseek':
        return DeepSeekAdapter(api_key, model or 'deepseek-chat')
    if provider == 'openai':
        return OpenAIAdapter(api_key, model or 'gpt-4-turbo-preview')
    if provider == 'doubao':
        return DoubaoAdapter(api_key, model, endpoint_id)

    raise ValueError(f'Unsupported provider: {provider}')
