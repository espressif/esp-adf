/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "cJSON.h"
#include "esp_service_mcp_server.h"

static const char *TAG = "MCP_SERVER";

/* JSON-RPC error codes */
#define JSONRPC_PARSE_ERROR       -32700
#define JSONRPC_INVALID_REQUEST   -32600
#define JSONRPC_METHOD_NOT_FOUND  -32601
#define JSONRPC_INVALID_PARAMS    -32602
#define JSONRPC_INTERNAL_ERROR    -32603

#define MCP_PROTOCOL_VERSION  "2024-11-05"

/**
 * @brief  MCP server structure
 *
 *         The server only holds a pointer to the abstract transport.
 *         It has zero knowledge of any concrete transport implementation.
 */
struct esp_service_mcp_server {
    esp_service_mcp_server_config_t  config;   /*!< Server configuration (copy, owns tool_provider/transport references) */
    bool                             running;  /*!< True while transport is started */
};

/**
 * @brief  Parsed JSON-RPC 2.0 request
 *
 *         Owns deep copies of the id node, method string, and params object so the
 *         original cJSON root can be freed immediately after parsing.
 */
typedef struct {
    cJSON *id;      /*!< Request ID node (deep copy, NULL for notifications); preserves original JSON type and full precision */
    char  *method;  /*!< JSON-RPC method name (heap-allocated copy) */
    cJSON *params;  /*!< Parameters object (deep copy; NULL if absent) */
} jsonrpc_request_t;

static esp_err_t parse_jsonrpc_request(const char *request_str, jsonrpc_request_t *req, int *out_jsonrpc_code)
{
    memset(req, 0, sizeof(jsonrpc_request_t));
    *out_jsonrpc_code = JSONRPC_PARSE_ERROR;

    cJSON *root = cJSON_Parse(request_str);
    if (!root) {
        ESP_LOGE(TAG, "Parse failed: malformed JSON request");
        *out_jsonrpc_code = JSONRPC_PARSE_ERROR;
        return ESP_FAIL;
    }

    cJSON *jsonrpc = cJSON_GetObjectItem(root, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0") != 0) {
        ESP_LOGW(TAG, "Parse failed: invalid or missing jsonrpc version");
        cJSON_Delete(root);
        *out_jsonrpc_code = JSONRPC_INVALID_REQUEST;
        return ESP_FAIL;
    }

    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (id && (cJSON_IsString(id) || cJSON_IsNumber(id))) {
        req->id = cJSON_Duplicate(id, 1);
        if (!req->id) {
            ESP_LOGE(TAG, "Parse failed: cannot duplicate request id (OOM)");
            cJSON_Delete(root);
            *out_jsonrpc_code = JSONRPC_INTERNAL_ERROR;
            return ESP_ERR_NO_MEM;
        }
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (!method || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Parse failed: missing or invalid 'method' field");
        cJSON_Delete(root);
        if (req->id) {
            cJSON_Delete(req->id);
            req->id = NULL;
        }
        *out_jsonrpc_code = JSONRPC_INVALID_REQUEST;
        return ESP_FAIL;
    }
    req->method = strdup(method->valuestring);
    if (!req->method) {
        ESP_LOGE(TAG, "Parse failed: cannot duplicate method name (OOM)");
        cJSON_Delete(root);
        if (req->id) {
            cJSON_Delete(req->id);
            req->id = NULL;
        }
        *out_jsonrpc_code = JSONRPC_INTERNAL_ERROR;
        return ESP_ERR_NO_MEM;
    }

    req->params = cJSON_GetObjectItem(root, "params");
    if (req->params) {
        req->params = cJSON_Duplicate(req->params, 1);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static void free_jsonrpc_request(jsonrpc_request_t *req)
{
    if (req->id) {
        cJSON_Delete(req->id);
    }
    if (req->method) {
        free(req->method);
    }
    if (req->params) {
        cJSON_Delete(req->params);
    }
}

static void jsonrpc_add_id_node(cJSON *obj, const cJSON *id)
{
    if (!id) {
        cJSON_AddNullToObject(obj, "id");
        return;
    }
    cJSON *dup = cJSON_Duplicate(id, 1);
    if (dup) {
        cJSON_AddItemToObject(obj, "id", dup);
    } else {
        cJSON_AddNullToObject(obj, "id");
    }
}

static void jsonrpc_add_id(cJSON *obj, const char *id)
{
    if (!id) {
        cJSON_AddNullToObject(obj, "id");
        return;
    }
    char *endptr;
    long id_num = strtol(id, &endptr, 10);
    if (*endptr == '\0') {
        cJSON_AddNumberToObject(obj, "id", id_num);
    } else {
        cJSON_AddStringToObject(obj, "id", id);
    }
}

static char *build_response_with_id_node(const cJSON *id, const char *result)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG, "Build response failed: cJSON_CreateObject OOM");
        return NULL;
    }
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    jsonrpc_add_id_node(response, id);

    cJSON *result_obj = cJSON_Parse(result);
    if (result_obj) {
        cJSON_AddItemToObject(response, "result", result_obj);
    } else {
        cJSON_AddStringToObject(response, "result", result);
    }

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!response_str) {
        ESP_LOGE(TAG, "Build response failed: cJSON_PrintUnformatted OOM");
    }
    return response_str;
}

static char *build_error_with_id_node(const cJSON *id, int code, const char *message)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG, "Build error failed: cJSON_CreateObject OOM");
        return NULL;
    }
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    jsonrpc_add_id_node(response, id);

    cJSON *error = cJSON_CreateObject();
    if (!error) {
        ESP_LOGE(TAG, "Build error failed: cJSON_CreateObject OOM (error node)");
        cJSON_Delete(response);
        return NULL;
    }
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(response, "error", error);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!response_str) {
        ESP_LOGE(TAG, "Build error failed: cJSON_PrintUnformatted OOM");
    }
    return response_str;
}

char *esp_service_mcp_build_response(const char *id, const char *result)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG, "Build response failed: cJSON_CreateObject OOM");
        return NULL;
    }
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    jsonrpc_add_id(response, id);

    cJSON *result_obj = cJSON_Parse(result);
    if (result_obj) {
        cJSON_AddItemToObject(response, "result", result_obj);
    } else {
        cJSON_AddStringToObject(response, "result", result);
    }

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!response_str) {
        ESP_LOGE(TAG, "Build response failed: cJSON_PrintUnformatted OOM");
    }
    return response_str;
}

char *esp_service_mcp_build_error(const char *id, int code, const char *message)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG, "Build error failed: cJSON_CreateObject OOM");
        return NULL;
    }
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    jsonrpc_add_id(response, id);

    cJSON *error = cJSON_CreateObject();
    if (!error) {
        ESP_LOGE(TAG, "Build error failed: cJSON_CreateObject OOM (error node)");
        cJSON_Delete(response);
        return NULL;
    }
    cJSON_AddNumberToObject(error, "code", code);
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(response, "error", error);

    char *response_str = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!response_str) {
        ESP_LOGE(TAG, "Build error failed: cJSON_PrintUnformatted OOM");
    }
    return response_str;
}

static cJSON *build_tools_cjson(const esp_service_tool_t **tools, uint16_t count)
{
    cJSON *tools_array = cJSON_CreateArray();

    for (uint16_t i = 0; i < count; i++) {
        const esp_service_tool_t *tool = tools[i];

        cJSON *tool_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_obj, "name", tool->name ? tool->name : "");
        cJSON_AddStringToObject(tool_obj, "description", tool->description ? tool->description : "");

        if (tool->input_schema) {
            cJSON *schema = cJSON_Parse(tool->input_schema);
            if (schema) {
                cJSON_AddItemToObject(tool_obj, "inputSchema", schema);
            }
        }

        cJSON_AddItemToArray(tools_array, tool_obj);
    }

    return tools_array;
}

char *esp_service_mcp_build_tools_list(const esp_service_tool_t **tools, uint16_t count)
{
    cJSON *tools_array = build_tools_cjson(tools, count);
    if (!tools_array) {
        ESP_LOGE(TAG, "Build tools list failed: cJSON_CreateArray OOM");
        return NULL;
    }
    char *result_str = cJSON_PrintUnformatted(tools_array);
    cJSON_Delete(tools_array);
    if (!result_str) {
        ESP_LOGE(TAG, "Build tools list failed: cJSON_PrintUnformatted OOM");
    }
    return result_str;
}

static char *handle_initialize(esp_service_mcp_server_t *srv, cJSON *params, int *err_code)
{
    (void)params;

    const char *name = srv->config.server_name ? srv->config.server_name : "esp-mcp-server";
    const char *version = srv->config.server_version ? srv->config.server_version : "1.0.0";

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        ESP_LOGE(TAG, "Handle initialize failed: cJSON_CreateObject OOM");
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    cJSON_AddStringToObject(result, "protocolVersion", MCP_PROTOCOL_VERSION);

    cJSON *caps = cJSON_CreateObject();
    cJSON *tools_cap = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools_cap, "listChanged", true);
    cJSON_AddItemToObject(caps, "tools", tools_cap);
    cJSON_AddItemToObject(result, "capabilities", caps);

    cJSON *info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "name", name);
    cJSON_AddStringToObject(info, "version", version);
    cJSON_AddItemToObject(result, "serverInfo", info);

    char *result_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!result_str) {
        ESP_LOGE(TAG, "Handle initialize failed: cJSON_PrintUnformatted OOM");
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }

    ESP_LOGD(TAG, "Handle initialize: %s v%s (protocol %s)", name, version, MCP_PROTOCOL_VERSION);
    return result_str;
}

static char *handle_ping(esp_service_mcp_server_t *srv, cJSON *params, int *err_code)
{
    (void)srv;
    (void)params;
    char *result_str = strdup("{}");
    if (!result_str) {
        ESP_LOGE(TAG, "Handle ping failed: strdup OOM");
        *err_code = JSONRPC_INTERNAL_ERROR;
    }
    return result_str;
}

static char *handle_tools_list(esp_service_mcp_server_t *srv, cJSON *params, int *err_code)
{
    (void)params;
    uint16_t max_tools = srv->config.max_tools;

    const esp_service_tool_t **tools = calloc(max_tools, sizeof(esp_service_tool_t *));
    if (!tools) {
        ESP_LOGE(TAG, "Handle tools/list failed: OOM allocating tools buffer (%u slots)", max_tools);
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }

    uint16_t tool_count = 0;
    esp_err_t ret = srv->config.tool_provider.get_tools(srv->config.tool_provider.ctx, tools, max_tools, &tool_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Handle tools/list failed: get_tools returned %s", esp_err_to_name(ret));
        free(tools);
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();
    if (!result) {
        ESP_LOGE(TAG, "Handle tools/list failed: cJSON_CreateObject OOM");
        free(tools);
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    cJSON *tools_array = build_tools_cjson(tools, tool_count);
    cJSON_AddItemToObject(result, "tools", tools_array);
    free(tools);

    char *result_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!result_str) {
        ESP_LOGE(TAG, "Handle tools/list failed: cJSON_PrintUnformatted OOM");
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    return result_str;
}

static char *handle_tools_call(esp_service_mcp_server_t *srv, cJSON *params, int *err_code)
{
    if (!params || !cJSON_IsObject(params)) {
        ESP_LOGW(TAG, "Handle tools/call: missing or non-object params");
        *err_code = JSONRPC_INVALID_PARAMS;
        return NULL;
    }

    cJSON *name = cJSON_GetObjectItem(params, "name");
    cJSON *arguments = cJSON_GetObjectItem(params, "arguments");

    if (!name || !cJSON_IsString(name)) {
        ESP_LOGW(TAG, "Handle tools/call: missing or invalid 'name' field");
        *err_code = JSONRPC_INVALID_PARAMS;
        return NULL;
    }

    /* Explicit NULL check is required: cJSON_PrintUnformatted can fail on OOM, and
     * forwarding NULL to invoke_tool would silently drop caller-supplied arguments. */
    char *args_json = arguments ? cJSON_PrintUnformatted(arguments) : strdup("{}");
    if (!args_json) {
        ESP_LOGE(TAG, "Handle tools/call failed: OOM serializing tool arguments");
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }

    size_t buf_size = srv->config.max_response_size;
    char *result_buffer = malloc(buf_size);
    if (!result_buffer) {
        ESP_LOGE(TAG, "Handle tools/call failed: OOM allocating result buffer (%u bytes)", (unsigned)buf_size);
        free(args_json);
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    result_buffer[0] = '\0';

    esp_err_t ret = srv->config.tool_provider.invoke_tool(srv->config.tool_provider.ctx, name->valuestring, args_json,
                                                          result_buffer, buf_size);
    free(args_json);

    cJSON *result = cJSON_CreateObject();
    cJSON *content_array = cJSON_CreateArray();
    cJSON *text_content = cJSON_CreateObject();
    if (!result || !content_array || !text_content) {
        ESP_LOGE(TAG, "Handle tools/call failed: cJSON_CreateObject/Array OOM");
        if (result) {
            cJSON_Delete(result);
        }
        if (content_array) {
            cJSON_Delete(content_array);
        }
        if (text_content) {
            cJSON_Delete(text_content);
        }
        free(result_buffer);
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    cJSON_AddStringToObject(text_content, "type", "text");

    if (ret == ESP_OK) {
        cJSON_AddStringToObject(text_content, "text", result_buffer);
        cJSON_AddItemToArray(content_array, text_content);
        cJSON_AddItemToObject(result, "content", content_array);
        cJSON_AddBoolToObject(result, "isError", false);
    } else {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Tool invocation failed: %s", esp_err_to_name(ret));
        cJSON_AddStringToObject(text_content, "text", error_msg);
        cJSON_AddItemToArray(content_array, text_content);
        cJSON_AddItemToObject(result, "content", content_array);
        cJSON_AddBoolToObject(result, "isError", true);
    }

    free(result_buffer);

    char *result_str = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    if (!result_str) {
        ESP_LOGE(TAG, "Handle tools/call failed: cJSON_PrintUnformatted OOM");
        *err_code = JSONRPC_INTERNAL_ERROR;
        return NULL;
    }
    return result_str;
}

static esp_err_t server_on_request(const char *request, char **response, void *user_data)
{
    esp_service_mcp_server_t *srv = (esp_service_mcp_server_t *)user_data;
    if (!srv || !request || !response) {
        ESP_LOGE(TAG, "Server on_request failed: srv, request or response is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_mcp_response_t resp;
    memset(&resp, 0, sizeof(resp));

    esp_err_t ret = esp_service_mcp_server_handle_request(srv, request, &resp);
    if (ret != ESP_OK) {
        /* handle_request already logged */
        *response = NULL;
        return ret;
    }

    if (resp.is_notification) {
        esp_service_mcp_response_free(&resp);
        *response = NULL;
    } else {
        *response = resp.response;
        resp.response = NULL;
    }

    return ESP_OK;
}

esp_err_t esp_service_mcp_server_create(const esp_service_mcp_server_config_t *config, esp_service_mcp_server_t **out_srv)
{
    if (!config || !config->tool_provider.get_tools || !config->tool_provider.invoke_tool
        || !config->transport || !out_srv) {
        ESP_LOGE(TAG, "Create failed: config, tool_provider callbacks, transport or out_srv is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_mcp_server_t *srv = calloc(1, sizeof(esp_service_mcp_server_t));
    if (!srv) {
        ESP_LOGE(TAG, "Create failed: OOM allocating server instance");
        return ESP_ERR_NO_MEM;
    }

    srv->config = *config;
    srv->running = false;

    /* Apply defaults for fields that may be zero-initialised by callers. */
    if (srv->config.server_name == NULL) {
        srv->config.server_name = "esp-mcp-server";
    }
    if (srv->config.server_version == NULL) {
        srv->config.server_version = "1.0.0";
    }
    if (srv->config.max_response_size == 0) {
        srv->config.max_response_size = 4096;
    }
    if (srv->config.max_tools == 0) {
        srv->config.max_tools = 64;
    }

    esp_err_t ret = esp_service_mcp_trans_set_request_handler(srv->config.transport, server_on_request, srv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: cannot bind request handler to transport: %s", esp_err_to_name(ret));
        free(srv);
        return ret;
    }

    ESP_LOGI(TAG, "Create 'mcp-server': server created");
    *out_srv = srv;
    return ESP_OK;
}

esp_err_t esp_service_mcp_server_destroy(esp_service_mcp_server_t *srv)
{
    if (!srv) {
        ESP_LOGE(TAG, "Destroy failed: srv is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (srv->running) {
        esp_service_mcp_server_stop(srv);
    }

    if (srv->config.transport) {
        esp_service_mcp_trans_set_request_handler(srv->config.transport, NULL, NULL);
    }

    free(srv);
    ESP_LOGI(TAG, "Destroy 'mcp-server': server destroyed");
    return ESP_OK;
}

esp_err_t esp_service_mcp_server_start(esp_service_mcp_server_t *srv)
{
    if (!srv) {
        ESP_LOGE(TAG, "Start failed: srv is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (srv->running) {
        ESP_LOGW(TAG, "Start 'mcp-server': already running");
        return ESP_OK;
    }

    esp_err_t ret = esp_service_mcp_trans_start(srv->config.transport);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: transport start returned %s", esp_err_to_name(ret));
        return ret;
    }

    srv->running = true;
    ESP_LOGI(TAG, "Start 'mcp-server': server started");
    return ESP_OK;
}

esp_err_t esp_service_mcp_server_stop(esp_service_mcp_server_t *srv)
{
    if (!srv) {
        ESP_LOGE(TAG, "Stop failed: srv is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!srv->running) {
        return ESP_OK;
    }

    esp_err_t ret = esp_service_mcp_trans_stop(srv->config.transport);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Stop 'mcp-server': transport stop returned %s", esp_err_to_name(ret));
    }

    srv->running = false;
    ESP_LOGI(TAG, "Stop 'mcp-server': server stopped");
    return ESP_OK;
}

esp_err_t esp_service_mcp_server_handle_request(esp_service_mcp_server_t *srv, const char *request, esp_service_mcp_response_t *response)
{
    if (!srv || !request || !response) {
        ESP_LOGE(TAG, "Handle request failed: srv, request or response is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(response, 0, sizeof(esp_service_mcp_response_t));

    jsonrpc_request_t req;
    int parse_code = 0;
    if (parse_jsonrpc_request(request, &req, &parse_code) != ESP_OK) {
        /* parse_jsonrpc_request already logged */
        const char *message;
        switch (parse_code) {
            case JSONRPC_INVALID_REQUEST:
                message = "Invalid request";
                break;
            case JSONRPC_INTERNAL_ERROR:
                message = "Internal error";
                break;
            case JSONRPC_PARSE_ERROR:
            default:
                message = "Parse error";
                break;
        }
        response->response = build_error_with_id_node(NULL, parse_code, message);
        response->error = ESP_FAIL;
        return ESP_OK;
    }

    response->is_notification = (req.id == NULL);

    /* Handlers set err_code when returning NULL so the dispatcher can distinguish
     * invalid-params (-32602) from internal errors (-32603). */
    char *result = NULL;
    int err_code = 0;

    if (strcmp(req.method, "initialize") == 0) {
        result = handle_initialize(srv, req.params, &err_code);
    } else if (strcmp(req.method, "notifications/initialized") == 0) {
        response->is_notification = true;
        ESP_LOGD(TAG, "Handle notifications/initialized: received from client");
        free_jsonrpc_request(&req);
        return ESP_OK;
    } else if (strcmp(req.method, "ping") == 0) {
        result = handle_ping(srv, req.params, &err_code);
    } else if (strcmp(req.method, "tools/list") == 0) {
        result = handle_tools_list(srv, req.params, &err_code);
    } else if (strcmp(req.method, "tools/call") == 0) {
        result = handle_tools_call(srv, req.params, &err_code);
    } else {
        ESP_LOGW(TAG, "Dispatch '%s': method not found", req.method);
        response->response = build_error_with_id_node(req.id, JSONRPC_METHOD_NOT_FOUND, "Method not found");
        response->error = ESP_ERR_NOT_FOUND;
        free_jsonrpc_request(&req);
        return ESP_OK;
    }

    if (result) {
        response->response = build_response_with_id_node(req.id, result);
        response->error = ESP_OK;
        free(result);
    } else {
        /* Default to invalid-params when the handler did not specify a code, to
         * preserve backward-compatible behavior. */
        int code = err_code ? err_code : JSONRPC_INVALID_PARAMS;
        const char *message = (code == JSONRPC_INTERNAL_ERROR) ? "Internal error" : "Invalid parameters";
        response->response = build_error_with_id_node(req.id, code, message);
        response->error = (code == JSONRPC_INTERNAL_ERROR) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
    }

    free_jsonrpc_request(&req);
    return ESP_OK;
}

esp_err_t esp_service_mcp_server_notify(esp_service_mcp_server_t *srv, const char *method, const char *params)
{
    if (!srv || !method) {
        ESP_LOGE(TAG, "Notify failed: srv or method is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *notification = cJSON_CreateObject();
    if (!notification) {
        ESP_LOGE(TAG, "Notify '%s' failed: cJSON_CreateObject OOM", method);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(notification, "jsonrpc", "2.0");
    cJSON_AddStringToObject(notification, "method", method);

    if (params) {
        cJSON *params_obj = cJSON_Parse(params);
        if (params_obj) {
            cJSON_AddItemToObject(notification, "params", params_obj);
        }
    }

    char *notification_str = cJSON_PrintUnformatted(notification);
    cJSON_Delete(notification);

    if (!notification_str) {
        ESP_LOGE(TAG, "Notify '%s' failed: cJSON_PrintUnformatted OOM", method);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGD(TAG, "Notify '%s': %s", method, notification_str);

    esp_err_t ret = esp_service_mcp_trans_broadcast(srv->config.transport, notification_str, strlen(notification_str));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Notify '%s': transport broadcast returned %s", method, esp_err_to_name(ret));
    }

    free(notification_str);
    return ret;
}

esp_err_t esp_service_mcp_server_get_capabilities(esp_service_mcp_server_t *srv, esp_service_mcp_server_capabilities_t *out_caps)
{
    if (!srv || !out_caps) {
        ESP_LOGE(TAG, "Get capabilities failed: srv or out_caps is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    out_caps->tools = true;
    out_caps->tools_list_changed = true;

    return ESP_OK;
}

void esp_service_mcp_response_free(esp_service_mcp_response_t *response)
{
    if (response && response->response) {
        free(response->response);
        response->response = NULL;
    }
}
