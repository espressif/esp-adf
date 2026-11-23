"""Offline regressions for bridge safety and LLM adapter wire contracts."""

import contextlib
import copy
import io
import json
import unittest
from unittest.mock import Mock, patch

import mcp_bridge
from bridge import MCPLLMBridge
from llm_adapter import DeepSeekAdapter, DoubaoAdapter, GeminiAdapter, OpenAIAdapter
from mcp_client import MCPClient, MCPSSEClient, _same_origin_endpoint


class AdapterProtocolTests(unittest.TestCase):
    def test_gemini_text_request_uses_native_parts_and_keeps_input(self):
        adapter = GeminiAdapter('test-key', 'test-model')
        messages = [{'role': 'user', 'content': 'hello'}]
        with patch('llm_adapter.requests.post') as post:
            post.return_value.json.return_value = {}
            adapter.call_with_tools(messages, [])
        self.assertEqual([{'role': 'user', 'parts': [{'text': 'hello'}]}],
                         post.call_args.kwargs['json']['contents'])
        self.assertNotIn('tools', post.call_args.kwargs['json'])
        self.assertEqual([{'role': 'user', 'content': 'hello'}], messages)
        self.assertEqual('test-key', post.call_args.kwargs['headers']['x-goog-api-key'])
        self.assertEqual(30, post.call_args.kwargs['timeout'])

    def test_gemini_keeps_complete_json_schema(self):
        schema = {'type': 'object', 'properties': {'names': {'type': 'array', 'items': {'type': 'string'}}},
                  'required': ['names'], 'additionalProperties': False}
        self.assertEqual([{'name': 'lookup', 'description': '', 'parametersJsonSchema': schema}],
                         GeminiAdapter('key').convert_mcp_to_functions([{'name': 'lookup', 'inputSchema': schema}]))

    def test_gemini_conversation_preserves_signatures_ids_and_parallel_results(self):
        adapter = GeminiAdapter('test-key', 'test-model')
        client = Mock()
        client.list_tools.return_value = [{'name': 'lookup', 'inputSchema': {'type': 'object'}}]
        client.call_tool.side_effect = ['first-result', 'second-result']
        model_content = {'role': 'model', 'parts': [
            {'text': 'private-thought', 'thought': True, 'thoughtSignature': 'opaque-thought'},
            {'functionCall': {'id': 'call-1', 'name': 'lookup', 'args': {'value': 1}},
             'thoughtSignature': 'opaque-call'},
            {'functionCall': {'id': 'call-2', 'name': 'lookup', 'args': {'value': 2}}},
        ]}
        responses = [
            {'candidates': [{'content': model_content}, {'content': {'parts': [
                {'functionCall': {'name': 'unselected', 'args': {}}},
            ]}}]},
            {'candidates': [{'content': {'role': 'model', 'parts': [
                {'text': 'private-thought', 'thought': True}, {'text': 'All '}, {'text': 'done'},
            ]}}]},
        ]
        unchanged = copy.deepcopy(responses)
        with patch('llm_adapter.requests.post') as post, contextlib.redirect_stdout(io.StringIO()) as output:
            post.return_value.json.side_effect = responses
            self.assertEqual('All done', MCPLLMBridge(client, adapter, allow_execution=True).run_conversation('lookup'))
        self.assertEqual(2, client.call_tool.call_count)
        self.assertEqual([unittest.mock.call('lookup', {'value': 1}), unittest.mock.call('lookup', {'value': 2})],
                         client.call_tool.call_args_list)
        follow_up = post.call_args.kwargs['json']['contents']
        self.assertEqual(model_content, follow_up[1])
        self.assertEqual({'role': 'user', 'parts': [
            {'functionResponse': {'id': 'call-1', 'name': 'lookup', 'response': {'result': 'first-result'}}},
            {'functionResponse': {'id': 'call-2', 'name': 'lookup', 'response': {'result': 'second-result'}}},
        ]}, follow_up[2])
        self.assertEqual(unchanged, responses)
        for call in post.call_args_list:
            self.assertEqual([{'functionDeclarations': [{
                'name': 'lookup', 'description': '', 'parametersJsonSchema': {'type': 'object'},
            }]}], call.kwargs['json'].get('tools'))
            self.assertNotIn('tool_choice', call.kwargs['json'])
        self.assertNotIn('private-thought', output.getvalue())
        self.assertNotIn('first-result', output.getvalue())
        self.assertNotIn('test-key', output.getvalue())

    def test_gemini_omits_absent_call_id(self):
        adapter = GeminiAdapter('key')
        response = {'candidates': [{'content': {'role': 'model', 'parts': [
            {'functionCall': {'name': 'lookup', 'args': {}}},
        ]}}]}
        messages = adapter.build_turn_messages(response, adapter.extract_tool_calls(response), ['ok'])
        self.assertNotIn('id', messages[1]['parts'][0]['functionResponse'])

    def test_openai_compatible_conversation_pairs_only_selected_choice(self):
        for adapter in (OpenAIAdapter('key', 'test-model'), DeepSeekAdapter('key', 'test-model'),
                        DoubaoAdapter('key', 'test-model')):
            with self.subTest(adapter=type(adapter).__name__):
                client = Mock()
                client.list_tools.return_value = [{'name': 'lookup', 'inputSchema': {'type': 'object'}}]
                client.call_tool.return_value = 'tool-result'
                assistant = {'role': 'assistant', 'content': None, 'tool_calls': [
                    {'id': 'chosen-1', 'type': 'function',
                     'function': {'name': 'lookup', 'arguments': json.dumps({'value': 1})}},
                ]}
                responses = [
                    {'choices': [{'message': assistant}, {'message': {'tool_calls': [
                        {'id': 'not-chosen', 'type': 'function',
                         'function': {'name': 'unselected', 'arguments': '{}'}},
                    ]}}]},
                    {'choices': [{'message': {'role': 'assistant', 'content': 'done'}}]},
                ]
                with patch('llm_adapter.requests.post') as post, \
                        patch.object(DoubaoAdapter, '_get_client') as sdk, \
                        contextlib.redirect_stdout(io.StringIO()):
                    post.return_value.json.side_effect = responses
                    sdk.return_value.chat.completions.create.return_value.model_dump.side_effect = responses
                    self.assertEqual('done', MCPLLMBridge(client, adapter, allow_execution=True).run_conversation('lookup'))
                client.call_tool.assert_called_once_with('lookup', {'value': 1})
                payload = sdk.return_value.chat.completions.create.call_args.kwargs if isinstance(adapter, DoubaoAdapter) \
                    else post.call_args.kwargs['json']
                self.assertEqual([{'role': 'user', 'content': 'lookup'}, assistant,
                                  {'role': 'tool', 'tool_call_id': 'chosen-1', 'content': 'tool-result'}], payload['messages'])
                for call in (sdk.return_value.chat.completions.create.call_args_list if isinstance(adapter, DoubaoAdapter)
                             else post.call_args_list):
                    payload = call.kwargs if isinstance(adapter, DoubaoAdapter) else call.kwargs['json']
                    self.assertEqual([{'type': 'function', 'function': {
                        'name': 'lookup', 'description': '', 'parameters': {'type': 'object'},
                    }}], payload.get('tools'))
                    self.assertEqual('auto', payload.get('tool_choice'))

    def test_malformed_arguments_fail_before_mcp_execution(self):
        client = Mock()
        client.list_tools.return_value = [{'name': 'lookup'}]
        bridge = MCPLLMBridge(client, OpenAIAdapter('key'), allow_execution=True)
        for arguments in ('not-json', '[]'):
            with self.subTest(arguments=arguments), patch('llm_adapter.requests.post') as post, \
                    contextlib.redirect_stdout(io.StringIO()):
                post.return_value.json.return_value = {'choices': [{'message': {'tool_calls': [
                    {'id': 'bad', 'type': 'function', 'function': {'name': 'lookup', 'arguments': arguments}},
                ]}}]}
                with self.assertRaises(ValueError):
                    bridge.run_conversation('lookup')
        client.call_tool.assert_not_called()
        client.call_tool.return_value = 'retry-result'
        with patch('llm_adapter.requests.post') as post, contextlib.redirect_stdout(io.StringIO()):
            post.return_value.json.side_effect = [
                {'choices': [{'message': {'role': 'assistant', 'tool_calls': [
                    {'id': 'retry', 'type': 'function', 'function': {'name': 'lookup', 'arguments': '{}'}},
                ]}}]},
                {'choices': [{'message': {'content': 'done'}}]},
            ]
            self.assertEqual('done', bridge.run_conversation('retry'))
        client.call_tool.assert_called_once_with('lookup', {})

    def test_openai_text_request_omits_empty_tools(self):
        for adapter in (OpenAIAdapter('key'), DeepSeekAdapter('key'), DoubaoAdapter('key')):
            with self.subTest(adapter=type(adapter).__name__), patch('llm_adapter.requests.post') as post, \
                    patch.object(DoubaoAdapter, '_get_client') as sdk:
                post.return_value.json.return_value = {}
                adapter.call_with_tools([{'role': 'user', 'content': 'hello'}], [])
                payload = sdk.return_value.chat.completions.create.call_args.kwargs if isinstance(adapter, DoubaoAdapter) \
                    else post.call_args.kwargs['json']
                self.assertNotIn('tools', payload)
                self.assertNotIn('tool_choice', payload)


class BridgeSafetyTests(unittest.TestCase):
    def test_discovery_never_calls_tools(self):
        client = Mock()
        client.list_tools.return_value = [{'name': 'erase_flash', 'inputSchema': {'type': 'object'}}]
        with patch.object(mcp_bridge, 'get_mcp_url_from_env_or_config', return_value='http://device/mcp'), \
                patch.object(mcp_bridge, 'MCPClient', return_value=client), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(0, mcp_bridge._cmd_discover(None))
        client.list_tools.assert_called_once()
        client.call_tool.assert_not_called()
        client.close.assert_called_once()

    def test_cli_defaults_reject_execution_before_network_or_config_loading(self):
        with patch.object(mcp_bridge, 'MCPClient') as client, \
                patch.object(mcp_bridge, 'load_config') as config, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(1, mcp_bridge._cmd_test(None))
            self.assertEqual(1, mcp_bridge._cmd_run('unused.json', 'batch'))
        client.assert_not_called()
        config.assert_not_called()

    def test_library_requires_explicit_authorization(self):
        client = Mock()
        bridge = MCPLLMBridge(client, Mock())
        bridge.mcp_tools = [{'name': 'erase_flash'}]
        with self.assertRaises(PermissionError):
            bridge.execute_tool_call({'name': 'erase_flash', 'arguments': {}})
        client.call_tool.assert_not_called()

    def test_authorized_call_checks_name_and_type_without_logging_secrets(self):
        client = Mock()
        client.call_tool.return_value = 'sensitive-response'
        bridge = MCPLLMBridge(client, Mock(), allow_execution=True)
        bridge.mcp_tools = [{'name': 'wifi_reconnect'}]
        with self.assertRaises(ValueError):
            bridge.execute_tool_call({'name': 'unknown', 'arguments': {}})
        with self.assertRaises(ValueError):
            bridge.execute_tool_call({'name': 'wifi_reconnect', 'arguments': []})
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual('sensitive-response', bridge.execute_tool_call(
                {'name': 'wifi_reconnect', 'arguments': {'password': 'sensitive-password'}}))
        self.assertNotIn('sensitive-password', output.getvalue())
        self.assertNotIn('sensitive-response', output.getvalue())
        client.call_tool.assert_called_once_with('wifi_reconnect', {'password': 'sensitive-password'})

    def test_sse_accepts_only_same_origin_endpoints(self):
        base = 'http://device:8080'
        for endpoint in ('/mcp/messages?id=1', 'mcp/messages?id=1', 'http://device:8080/mcp/messages'):
            self.assertTrue(_same_origin_endpoint(base, endpoint).startswith(base))
        self.assertEqual('http://device:80/messages', _same_origin_endpoint('http://device', 'http://device:80/messages'))
        for endpoint in ('http://evil/messages', '//evil/messages', 'https://device:8080/messages',
                         'http://device:80/messages', 'http://device:0/messages',
                         'http://user:password@device:8080/messages', '/messages#fragment',
                         'file:///tmp/private', 'http://device:bad/messages'):
            with self.subTest(endpoint=endpoint), self.assertRaises(ValueError):
                _same_origin_endpoint(base, endpoint)

    def test_invalid_endpoint_drops_old_binding_and_never_posts_credentials(self):
        client = MCPSSEClient('http://device:8080', auth_token='private-token')
        try:
            client._dispatch_event('endpoint', '/messages')
            with self.assertRaises(ValueError):
                client._dispatch_event('endpoint', 'http://evil/messages')
            self.assertIsNone(client._messages_url)
            with patch('mcp_client.requests.post') as post, self.assertRaises(RuntimeError):
                client.call_tool('erase_flash', {})
            post.assert_not_called()
        finally:
            client.close()

    def test_http_redirect_is_not_followed(self):
        response = Mock(status_code=302)
        with patch('mcp_client.requests.post', return_value=response) as post:
            with self.assertRaises(ValueError):
                MCPClient('http://device/mcp', auth_token='private-token').list_tools()
        self.assertFalse(post.call_args.kwargs['allow_redirects'])
        response.json.assert_not_called()

    def test_sse_post_redirect_cleans_pending_request(self):
        client = MCPSSEClient('http://device:8080', auth_token='private-token')
        try:
            client._dispatch_event('endpoint', '/messages')
            with patch('mcp_client.requests.post', return_value=Mock(status_code=307)) as post:
                with self.assertRaises(ValueError):
                    client.call_tool('erase_flash', {})
            self.assertFalse(post.call_args.kwargs['allow_redirects'])
            self.assertEqual({}, client._pending)
        finally:
            client.close()


if __name__ == '__main__':
    unittest.main()
