# MCP-to-LLM Bridge

Connect MCP-enabled ESP32 devices (or the PC mock server) to Large Language
Models — Gemini, DeepSeek, OpenAI, Doubao. Configuration-driven; no
hard-coded keys or URLs.

---

## Quick Start

```bash
cd tools/mcp_llm_bridge
pip install -r requirements.txt

# 1. Discover and test local MCP (no LLM needed)
export MCP_URL="http://<device-ip>:8080/mcp"
python mcp_bridge.py discover

# 2. Export the API key for the provider you want to use.
#    The example configs reference these variables via ${...} placeholders.
export DEEPSEEK_API_KEY="sk-..."     # for configs/deepseek.json, example.json, pc_integration.json
export DOUBAO_API_KEY="..."          # for configs/doubao.json, sse_test.json
# export GEMINI_API_KEY="..."
# export OPENAI_API_KEY="sk-..."

# 3. Run with an LLM (batch or interactive)
python mcp_bridge.py run configs/deepseek.json
python mcp_bridge.py run configs/doubao.json interactive

# 4. Automated MCP test suite (no LLM key needed)
python mcp_bridge.py test configs/example.json
```

---

## CLI: `mcp_bridge.py`

Single entry point with three subcommands.

| Command    | Purpose                      | Needs LLM? |
|------------|------------------------------|------------|
| `discover` | Discover and test local MCP  | No         |
| `run`      | Connect to LLM and test MCP  | Yes        |
| `test`     | Automated MCP test suite     | No         |

### `discover` — discover local MCP tools

Lists tools from the MCP server, optionally prints the tool format for a
configured provider, and runs each tool with schema-based arguments.

```bash
export MCP_URL="http://<host>:<port>/mcp"
python mcp_bridge.py discover

python mcp_bridge.py discover configs/doubao.json     # use config for URL + format
python mcp_bridge.py discover --transport sse \
                              --sse-base-url http://<host>:8080
```

### `run` — drive MCP through an LLM

Connects to MCP and an LLM, then runs either batch test cases from the
config or an interactive REPL. The config must include `mcp_server` and
`llm`.

```bash
# Batch (default)
python mcp_bridge.py run configs/deepseek.json
python mcp_bridge.py run configs/doubao.json

# Interactive
python mcp_bridge.py run configs/deepseek.json interactive
```

### `test` — automated MCP test suite

Runs the MCP-only suite: discovery, schema validation, tool execution,
error handling, sequential calls.

```bash
export MCP_URL="http://<host>:<port>/mcp"
python mcp_bridge.py test

python mcp_bridge.py test configs/example.json
```

Help: `python mcp_bridge.py --help` and
`python mcp_bridge.py <command> --help`.

---

## Configuration

API keys are **never committed**. Example configs reference the key through
an `${ENV_VAR}` placeholder; export the variable in your shell before
running.

Minimal config (see `configs/example.json`):

```json
{
  "mcp_server": {
    "url": "http://10.18.37.200:8080/mcp",
    "timeout": 10
  },
  "llm": {
    "provider": "deepseek",
    "api_key": "${DEEPSEEK_API_KEY}",
    "model": "deepseek-chat"
  }
}
```

```bash
export DEEPSEEK_API_KEY="sk-..."
python mcp_bridge.py run configs/example.json
```

Full config with transport and test cases (see `configs/sse_test.json`):

```json
{
  "mcp_server": {
    "url": "http://10.18.39.42:8080/mcp",
    "transport": "sse",
    "timeout": 15
  },
  "llm": {
    "provider": "doubao",
    "api_key": "${DOUBAO_API_KEY}",
    "model": "doubao-seed-1-8-251228"
  },
  "test_cases": [
    { "description": "LED control", "prompt": "Turn on the LED" }
  ]
}
```

- **Providers:** `gemini`, `deepseek`, `openai`, `doubao`.
- **Transports:** `http` (default) or `sse`.
- **Placeholder expansion:** any string value may use `${VAR}` or
  `${VAR:-default}`. Unset `${VAR}` (no default) resolves to empty string,
  which triggers the provider-specific fallback below or fails fast with
  `API key is required`.
- **Provider env-var fallback:** when `api_key` / `model` resolves to
  empty, the adapter factory reads
  `DEEPSEEK_API_KEY` / `DEEPSEEK_MODEL`, `GEMINI_API_KEY` / `GEMINI_MODEL`,
  `OPENAI_API_KEY`, `DOUBAO_API_KEY` / `DOUBAO_MODEL` /
  `DOUBAO_ENDPOINT_ID`.
- **`MCP_URL`** always overrides `mcp_server.url` when set.

---

## API Keys

| Provider | Where to get | Env vars (optional) |
|----------|--------------|---------------------|
| Gemini   | [Google AI](https://makersuite.google.com/app/apikey)       | `GEMINI_API_KEY`, `GEMINI_MODEL` |
| DeepSeek | [DeepSeek](https://platform.deepseek.com/)                   | `DEEPSEEK_API_KEY`, `DEEPSEEK_MODEL` |
| OpenAI   | [OpenAI](https://platform.openai.com/api-keys)               | `OPENAI_API_KEY` |
| Doubao   | [Volcano Engine](https://console.volcengine.com/ark)         | `DOUBAO_API_KEY`, `DOUBAO_ENDPOINT_ID`, `DOUBAO_MODEL` |

Put keys in a local config (gitignored) or in environment variables. Never
commit real keys.

---

## Architecture

```
User / CLI
    │
    ▼
MCPLLMBridge  (facade; drives the tool-using conversation loop)
    │ ┌────────────────────────┬─────────────────────────────┐
    ▼ ▼                        ▼                             ▼
LLM adapter              MCP client                    configs/*.json
(strategy)               (HTTP or SSE JSON-RPC)        (provider + URL)
    │                        │
    ▼                        ▼
Gemini / DeepSeek /     ESP32 / PC MCP server
OpenAI / Doubao         (LED, Player, ...)
```

End-to-end flow:

1. Bridge lists MCP tools via `tools/list`.
2. The LLM adapter converts them to the provider-specific function format.
3. User prompt + tools are sent to the LLM; the LLM may return tool calls.
4. Bridge executes each tool via MCP `tools/call` and feeds the results
   back to the LLM until the LLM returns a final answer or
   `max_iterations` is reached (default 5).

### Module boundaries

| Module            | Responsibility | Key exports |
|-------------------|----------------|-------------|
| `mcp_client.py`   | MCP protocol (HTTP + SSE JSON-RPC), tool printing, schema-based test-case generation | `MCPClient`, `MCPSSEClient`, `create_mcp_client`, `print_tools`, `generate_tool_test_cases` |
| `llm_adapter.py`  | Provider-specific request/response shaping | `LLMAdapter`, `OpenAICompatibleAdapter`, `DeepSeekAdapter`, `OpenAIAdapter`, `DoubaoAdapter`, `GeminiAdapter`, `create_adapter_from_config` |
| `bridge.py`       | Orchestration + config loading | `MCPLLMBridge`, `load_config`, `get_mcp_url_from_env_or_config` |
| `mcp_bridge.py`   | CLI over the three modules above | `discover` / `run` / `test` subcommands |

### Tool-format conversion

Input (MCP tool descriptor):

```json
{
  "name": "player_service_set_volume",
  "description": "Set the playback volume",
  "inputSchema": {
    "type": "object",
    "properties": { "volume": { "type": "integer", "minimum": 0, "maximum": 100 } },
    "required": ["volume"]
  }
}
```

Output — Gemini (`functionDeclarations`):

```json
{
  "name": "player_service_set_volume",
  "description": "Set the playback volume",
  "parameters": {
    "type": "object",
    "properties": { "volume": { "type": "integer", "minimum": 0, "maximum": 100 } },
    "required": ["volume"]
  }
}
```

Output — OpenAI-compatible (`tools`, used by OpenAI / DeepSeek / Doubao):

```json
{
  "type": "function",
  "function": {
    "name": "player_service_set_volume",
    "description": "Set the playback volume",
    "parameters": { "type": "object", "properties": { "...": "..." }, "required": ["volume"] }
  }
}
```

### Extension points

- **New LLM provider** — subclass `LLMAdapter` (or `OpenAICompatibleAdapter`
  if the provider speaks the OpenAI chat-completions format) and register
  it in `create_adapter_from_config()` in `llm_adapter.py`.
- **New transport** — implement `list_tools()` and
  `call_tool(name, arguments)` (mirror `MCPClient` / `MCPSSEClient`), and
  branch on it inside `create_mcp_client()` in `mcp_client.py`.

---

## Troubleshooting

**Device not reachable**

```bash
ping <device-ip>
curl -X POST http://<device-ip>:8080/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
```

**MCP URL not set**
Set `MCP_URL` or pass a config with `mcp_server.url`.

**API key errors**
Check the key in your config or environment (e.g. `echo $DEEPSEEK_API_KEY`).
Ensure the key is valid for the chosen model.

**Tool call failures**
Check device logs and confirm tool names / arguments match `inputSchema`.
