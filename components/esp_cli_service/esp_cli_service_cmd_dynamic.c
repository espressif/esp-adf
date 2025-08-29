/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "adf_mem.h"
#include <string.h>

#include "cJSON.h"
#include "esp_console.h"
#include "esp_log.h"

#include "esp_cli_service_internal.h"

/* Stack buffer size for tool call JSON result text (see tool_handle_call). */
#define CLI_TOOL_RESULT_MAX  2048
#define CLI_KV_KEY_BUF_MAX   80
#define CLI_TOOL_ERRBUF_MAX  160

static const char *TAG = "esp_cli_service";

static int cmd_svc_dispatch(void *context, int argc, char **argv);
static int cmd_tool_dispatch(void *context, int argc, char **argv);

static esp_err_t find_service_for_cli(esp_cli_service_t *svc,
                                      const char *name,
                                      esp_service_t **out_service,
                                      char *category_buf,
                                      size_t category_buf_size,
                                      bool *out_tracked)
{
    if (svc == NULL || name == NULL || out_service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_tracked != NULL) {
        *out_tracked = false;
    }
    *out_service = NULL;
    if (category_buf != NULL && category_buf_size > 0) {
        category_buf[0] = '\0';
    }

    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        return ret;
    }

    int idx = cli_tracked_index_by_name_locked(svc, name);
    if (idx >= 0) {
        tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, (size_t)idx);
        *out_service = entry->service;
        if (category_buf != NULL && category_buf_size > 0) {
            const char *category = (entry->category != NULL) ? entry->category : "";
            snprintf(category_buf, category_buf_size, "%s", category);
        }
        if (out_tracked != NULL) {
            *out_tracked = true;
        }
        cli_lock_give(svc);
        return ESP_OK;
    }

    cli_lock_give(svc);

    esp_service_manager_t *mgr = NULL;
    esp_err_t mgr_ret = cli_manager_get(svc, &mgr);
    if (mgr_ret != ESP_OK) {
        return mgr_ret;
    }
    if (mgr == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_service_t *found = NULL;
    ret = esp_service_manager_find_by_name(mgr, name, &found);
    if (ret == ESP_OK) {
        *out_service = found;
        if (category_buf != NULL && category_buf_size > 0 && category_buf[0] == '\0') {
            snprintf(category_buf, category_buf_size, "<untracked>");
        }
    }
    return ret;
}

/* Deep-copies tool metadata under the manager mutex so the returned array
 * stays valid across concurrent esp_service_manager_unregister(). */
static esp_err_t collect_tools(esp_service_manager_t *mgr,
                               esp_service_tool_t **out_tools,
                               uint16_t *out_count)
{
    if (mgr == NULL || out_tools == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_service_manager_clone_tools(mgr, out_tools, out_count);
}

static const esp_service_tool_t *find_tool_by_name(const esp_service_tool_t *tools,
                                                   uint16_t count,
                                                   const char *name)
{
    for (uint16_t i = 0; i < count; i++) {
        if (tools[i].name != NULL && strcmp(tools[i].name, name) == 0) {
            return &tools[i];
        }
    }
    return NULL;
}

static bool prop_needs_json_fallback(cJSON *prop)
{
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(prop, "type");
    if (!cJSON_IsString(type_item) || type_item->valuestring == NULL) {
        return true;
    }

    const char *type = type_item->valuestring;
    if (strcmp(type, "string") == 0 ||
        strcmp(type, "integer") == 0 ||
        strcmp(type, "number") == 0 ||
        strcmp(type, "boolean") == 0) {
        return false;
    }
    return true;
}

static esp_err_t add_typed_arg(cJSON *args_obj,
                               const char *key,
                               const char *value,
                               cJSON *prop_def,
                               char *errbuf,
                               size_t errbuf_size)
{
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(prop_def, "type");
    const char *type = (cJSON_IsString(type_item) && type_item->valuestring) ? type_item->valuestring : NULL;

    if (type == NULL || strcmp(type, "string") == 0) {
        if (!cJSON_AddStringToObject(args_obj, key, value)) {
            snprintf(errbuf, errbuf_size, "No memory while adding '%s'", key);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    if (strcmp(type, "integer") == 0) {
        long long i64 = 0;
        if (cli_parse_i64(value, &i64) != ESP_OK) {
            snprintf(errbuf, errbuf_size, "Field '%s' expects integer", key);
            return ESP_ERR_INVALID_ARG;
        }
        if (!cJSON_AddNumberToObject(args_obj, key, (double)i64)) {
            snprintf(errbuf, errbuf_size, "No memory while adding '%s'", key);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    if (strcmp(type, "number") == 0) {
        double number = 0.0;
        if (cli_parse_double(value, &number) != ESP_OK) {
            snprintf(errbuf, errbuf_size, "Field '%s' expects number", key);
            return ESP_ERR_INVALID_ARG;
        }
        if (!cJSON_AddNumberToObject(args_obj, key, number)) {
            snprintf(errbuf, errbuf_size, "No memory while adding '%s'", key);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    if (strcmp(type, "boolean") == 0) {
        bool boolean_value = false;
        if (cli_parse_bool(value, &boolean_value) != ESP_OK) {
            snprintf(errbuf, errbuf_size, "Field '%s' expects boolean true/false", key);
            return ESP_ERR_INVALID_ARG;
        }
        if (!cJSON_AddBoolToObject(args_obj, key, boolean_value)) {
            snprintf(errbuf, errbuf_size, "No memory while adding '%s'", key);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    snprintf(errbuf, errbuf_size, "Field '%s' type '%s' not supported by key=value, use --json", key, type);
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t build_args_json_from_kv(const esp_service_tool_t *tool,
                                         int argc,
                                         char **argv,
                                         int start_idx,
                                         char **out_json,
                                         char *errbuf,
                                         size_t errbuf_size)
{
    if (tool == NULL || out_json == NULL || errbuf == NULL || errbuf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_json = NULL;
    errbuf[0] = '\0';

    if (tool->input_schema == NULL) {
        if (start_idx < argc) {
            snprintf(errbuf, errbuf_size, "Tool '%s' has no inputSchema, use --json", tool->name);
            return ESP_ERR_NOT_SUPPORTED;
        }
        *out_json = adf_strdup("{}");
        return (*out_json != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
    }

    cJSON *schema_root = cJSON_Parse(tool->input_schema);
    if (schema_root == NULL || !cJSON_IsObject(schema_root)) {
        if (schema_root != NULL) {
            cJSON_Delete(schema_root);
        }
        snprintf(errbuf, errbuf_size, "Tool '%s' inputSchema is not valid JSON object", tool->name);
        return ESP_FAIL;
    }

    cJSON *schema_type = cJSON_GetObjectItemCaseSensitive(schema_root, "type");
    if (cJSON_IsString(schema_type) && schema_type->valuestring && strcmp(schema_type->valuestring, "object") != 0) {
        snprintf(errbuf, errbuf_size,
                 "Tool '%s' schema type '%s' not supported by key=value, use --json",
                 tool->name, schema_type->valuestring);
        cJSON_Delete(schema_root);
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *properties = cJSON_GetObjectItemCaseSensitive(schema_root, "properties");
    if (properties != NULL && !cJSON_IsObject(properties)) {
        snprintf(errbuf, errbuf_size, "Tool '%s' schema 'properties' is not object, use --json", tool->name);
        cJSON_Delete(schema_root);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (properties != NULL) {
        cJSON *prop = NULL;
        cJSON_ArrayForEach(prop, properties)
        {
            if (!cJSON_IsObject(prop) || prop_needs_json_fallback(prop)) {
                snprintf(errbuf, errbuf_size,
                         "Tool '%s' field '%s' is nested/unsupported, use --json",
                         tool->name,
                         prop->string ? prop->string : "<unknown>");
                cJSON_Delete(schema_root);
                return ESP_ERR_NOT_SUPPORTED;
            }
        }
    }

    cJSON *args_obj = cJSON_CreateObject();
    if (args_obj == NULL) {
        cJSON_Delete(schema_root);
        return ESP_ERR_NO_MEM;
    }

    for (int i = start_idx; i < argc; i++) {
        char *token = argv[i];
        char *eq = strchr(token, '=');
        if (eq == NULL || eq == token) {
            snprintf(errbuf, errbuf_size, "Invalid token '%s', expected key=value", token);
            cJSON_Delete(args_obj);
            cJSON_Delete(schema_root);
            return ESP_ERR_INVALID_ARG;
        }

        size_t key_len = (size_t)(eq - token);
        char key[CLI_KV_KEY_BUF_MAX] = {0};
        if (key_len == 0 || key_len >= sizeof(key)) {
            snprintf(errbuf, errbuf_size, "Invalid key in token '%s'", token);
            cJSON_Delete(args_obj);
            cJSON_Delete(schema_root);
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(key, token, key_len);
        key[key_len] = '\0';

        const char *value = eq + 1;
        cJSON *existing = cJSON_GetObjectItemCaseSensitive(args_obj, key);
        if (existing != NULL) {
            snprintf(errbuf, errbuf_size, "Duplicated field '%s'", key);
            cJSON_Delete(args_obj);
            cJSON_Delete(schema_root);
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *prop_def = (properties != NULL) ? cJSON_GetObjectItemCaseSensitive(properties, key) : NULL;
        if (prop_def == NULL) {
            snprintf(errbuf, errbuf_size, "Unknown field '%s' for tool '%s'", key, tool->name);
            cJSON_Delete(args_obj);
            cJSON_Delete(schema_root);
            return ESP_ERR_INVALID_ARG;
        }

        esp_err_t add_ret = add_typed_arg(args_obj, key, value, prop_def, errbuf, errbuf_size);
        if (add_ret != ESP_OK) {
            cJSON_Delete(args_obj);
            cJSON_Delete(schema_root);
            return add_ret;
        }
    }

    cJSON *required = cJSON_GetObjectItemCaseSensitive(schema_root, "required");
    if (required != NULL && cJSON_IsArray(required)) {
        cJSON *req = NULL;
        cJSON_ArrayForEach(req, required)
        {
            if (!cJSON_IsString(req) || req->valuestring == NULL) {
                continue;
            }
            if (cJSON_GetObjectItemCaseSensitive(args_obj, req->valuestring) == NULL) {
                snprintf(errbuf, errbuf_size, "Missing required field '%s'", req->valuestring);
                cJSON_Delete(args_obj);
                cJSON_Delete(schema_root);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    char *json = cJSON_PrintUnformatted(args_obj);
    cJSON_Delete(args_obj);
    cJSON_Delete(schema_root);
    if (json == NULL) {
        snprintf(errbuf, errbuf_size, "Failed to serialize args JSON");
        return ESP_ERR_NO_MEM;
    }

    *out_json = json;
    return ESP_OK;
}

static void print_service_line(esp_service_t *service, const char *name, const char *category)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    const char *state_str = "UNKNOWN";
    esp_err_t last_err = ESP_OK;

    if (service != NULL) {
        if (esp_service_get_state(service, &state) == ESP_OK) {
            const char *tmp = NULL;
            if (esp_service_state_to_str(state, &tmp) == ESP_OK && tmp != NULL) {
                state_str = tmp;
            }
        }
        esp_service_get_last_error(service, &last_err);
    }

    printf("%s category=%s state=%s last_err=%s(0x%x)\n",
           name ? name : "<null>",
           category ? category : "",
           state_str,
           esp_err_to_name(last_err),
           (unsigned int)last_err);
}

static void svc_print_usage(void)
{
    printf("Usage:\n");
    printf("  svc list\n");
    printf("  svc info <service>\n");
    printf("  svc start <service>\n");
    printf("  svc stop <service>\n");
    printf("  svc pause <service>\n");
    printf("  svc resume <service>\n");
}

static int svc_handle_list(esp_cli_service_t *svc)
{
    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        printf("svc list failed: lock timeout\n");
        return 1;
    }

    size_t tracked_count = adf_vec_size(&svc->tracked);
    if (tracked_count == 0) {
        printf("No tracked service. Use esp_cli_service_track_service().\n");
        cli_lock_give(svc);
        return 0;
    }

    printf("Tracked services (%u):\n", (unsigned)tracked_count);
    for (size_t i = 0; i < tracked_count; i++) {
        tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, i);
        print_service_line(entry->service, entry->name, entry->category);
    }

    cli_lock_give(svc);
    return 0;
}

static int svc_handle_info(esp_cli_service_t *svc, const char *name)
{
    esp_service_t *target = NULL;
    char category[64] = {0};
    bool tracked = false;

    esp_err_t ret = find_service_for_cli(svc, name, &target, category, sizeof(category), &tracked);
    if (ret != ESP_OK || target == NULL) {
        printf("svc info: service '%s' not found\n", name);
        return 1;
    }

    printf("Service '%s' (%s)\n", name, tracked ? "tracked" : "manager fallback");
    print_service_line(target, name, category);
    return 0;
}

static int svc_handle_lifecycle(esp_cli_service_t *svc, const char *op, const char *name)
{
    esp_service_t *target = NULL;
    esp_err_t ret = find_service_for_cli(svc, name, &target, NULL, 0, NULL);
    if (ret != ESP_OK || target == NULL) {
        printf("svc %s: service '%s' not found\n", op, name);
        return 1;
    }

    if (strcmp(op, "start") == 0) {
        ret = esp_service_start(target);
    } else if (strcmp(op, "stop") == 0) {
        ret = esp_service_stop(target);
    } else if (strcmp(op, "pause") == 0) {
        ret = esp_service_pause(target);
    } else if (strcmp(op, "resume") == 0) {
        ret = esp_service_resume(target);
    } else {
        printf("svc: unsupported operation '%s'\n", op);
        return 1;
    }

    if (ret != ESP_OK) {
        printf("svc %s %s failed: %s\n", op, name, esp_err_to_name(ret));
        return 1;
    }

    printf("svc %s %s: OK\n", op, name);
    return 0;
}

static int cmd_svc_dispatch(void *context, int argc, char **argv)
{
    esp_cli_service_t *svc = (esp_cli_service_t *)context;
    if (svc == NULL) {
        return 1;
    }

    if (argc < 2) {
        svc_print_usage();
        return 1;
    }

    const char *sub = argv[1];
    if (strcmp(sub, "list") == 0) {
        if (argc != 2) {
            svc_print_usage();
            return 1;
        }
        return svc_handle_list(svc);
    }

    if (strcmp(sub, "info") == 0) {
        if (argc != 3) {
            svc_print_usage();
            return 1;
        }
        return svc_handle_info(svc, argv[2]);
    }

    if (strcmp(sub, "start") == 0 ||
        strcmp(sub, "stop") == 0 ||
        strcmp(sub, "pause") == 0 ||
        strcmp(sub, "resume") == 0) {
        if (argc != 3) {
            svc_print_usage();
            return 1;
        }
        return svc_handle_lifecycle(svc, sub, argv[2]);
    }

    svc_print_usage();
    return 1;
}

static void tool_print_usage(void)
{
    printf("Usage:\n");
    printf("  tool list\n");
    printf("  tool info <tool_name>\n");
    printf("  tool call <tool_name> [key=value ...]\n");
    printf("  tool call <tool_name> --json '<json>'\n");
}

static int tool_handle_list(esp_cli_service_t *svc)
{
    esp_service_manager_t *mgr = NULL;
    esp_err_t err = cli_manager_get(svc, &mgr);
    if (err == ESP_ERR_TIMEOUT) {
        printf("tool list: service lock timeout\n");
        return 1;
    }
    if (err != ESP_OK) {
        printf("tool list failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    if (mgr == NULL) {
        printf("tool list: manager not bound\n");
        return 1;
    }

    esp_service_tool_t *tools = NULL;
    uint16_t count = 0;
    esp_err_t ret = collect_tools(mgr, &tools, &count);
    if (ret != ESP_OK) {
        printf("tool list failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("Tools (%u):\n", (unsigned)count);
    for (uint16_t i = 0; i < count; i++) {
        const char *name = tools[i].name ? tools[i].name : "<null>";
        const char *desc = tools[i].description ? tools[i].description : "";
        printf("%s%s%s\n", name, desc[0] ? " - " : "", desc);
    }

    esp_service_manager_free_cloned_tools(tools, count);
    return 0;
}

static int tool_handle_info(esp_cli_service_t *svc, const char *tool_name)
{
    esp_service_manager_t *mgr = NULL;
    esp_err_t err = cli_manager_get(svc, &mgr);
    if (err == ESP_ERR_TIMEOUT) {
        printf("tool info: service lock timeout\n");
        return 1;
    }
    if (err != ESP_OK) {
        printf("tool info failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    if (mgr == NULL) {
        printf("tool info: manager not bound\n");
        return 1;
    }

    esp_service_tool_t *tools = NULL;
    uint16_t count = 0;
    esp_err_t ret = collect_tools(mgr, &tools, &count);
    if (ret != ESP_OK) {
        printf("tool info failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    const esp_service_tool_t *tool = find_tool_by_name(tools, count, tool_name);
    if (tool == NULL) {
        printf("tool info: tool '%s' not found\n", tool_name);
        esp_service_manager_free_cloned_tools(tools, count);
        return 1;
    }

    printf("name: %s\n", tool->name ? tool->name : "");
    printf("description: %s\n", tool->description ? tool->description : "");
    printf("inputSchema: %s\n", tool->input_schema ? tool->input_schema : "{}");

    esp_service_manager_free_cloned_tools(tools, count);
    return 0;
}

static int tool_handle_call(esp_cli_service_t *svc, int argc, char **argv)
{
    if (argc < 3) {
        tool_print_usage();
        return 1;
    }

    esp_service_manager_t *mgr = NULL;
    esp_err_t err = cli_manager_get(svc, &mgr);
    if (err == ESP_ERR_TIMEOUT) {
        printf("tool call: service lock timeout\n");
        return 1;
    }
    if (err != ESP_OK) {
        printf("tool call failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    if (mgr == NULL) {
        printf("tool call: manager not bound\n");
        return 1;
    }

    const char *tool_name = argv[2];

    esp_service_tool_t *tools = NULL;
    uint16_t count = 0;
    esp_err_t ret = collect_tools(mgr, &tools, &count);
    if (ret != ESP_OK) {
        printf("tool call failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    const esp_service_tool_t *tool = find_tool_by_name(tools, count, tool_name);
    if (tool == NULL) {
        printf("tool call: tool '%s' not found\n", tool_name);
        esp_service_manager_free_cloned_tools(tools, count);
        return 1;
    }

    char *args_json = NULL;
    char errbuf[CLI_TOOL_ERRBUF_MAX] = {0};

    if (argc >= 4 && strcmp(argv[3], "--json") == 0) {
        if (argc != 5) {
            printf("tool call: '--json' requires exactly one JSON argument\n");
            esp_service_manager_free_cloned_tools(tools, count);
            return 1;
        }

        cJSON *parsed = cJSON_Parse(argv[4]);
        if (parsed == NULL) {
            printf("tool call: invalid JSON for --json\n");
            esp_service_manager_free_cloned_tools(tools, count);
            return 1;
        }
        cJSON_Delete(parsed);

        args_json = adf_strdup(argv[4]);
        if (args_json == NULL) {
            printf("tool call: no memory for args\n");
            esp_service_manager_free_cloned_tools(tools, count);
            return 1;
        }
    } else {
        ret = build_args_json_from_kv(tool, argc, argv, 3, &args_json, errbuf, sizeof(errbuf));
        if (ret != ESP_OK) {
            printf("tool call: %s\n", errbuf[0] ? errbuf : esp_err_to_name(ret));
            esp_service_manager_free_cloned_tools(tools, count);
            return 1;
        }
    }

    /* Keep REPL task stack usage small: result buffer on heap, not stack. */
    char *result = adf_calloc(1, CLI_TOOL_RESULT_MAX);
    if (result == NULL) {
        adf_free(args_json);
        esp_service_manager_free_cloned_tools(tools, count);
        printf("tool call: no memory for result\n");
        return 1;
    }

    ret = esp_service_manager_invoke_tool(mgr, tool_name, args_json, result, CLI_TOOL_RESULT_MAX);
    adf_free(args_json);
    esp_service_manager_free_cloned_tools(tools, count);

    if (ret != ESP_OK) {
        printf("tool call %s failed: %s\n", tool_name, esp_err_to_name(ret));
        adf_free(result);
        return 1;
    }

    if (result[0] == '\0') {
        printf("{}\n");
    } else {
        printf("%s\n", result);
    }
    adf_free(result);
    return 0;
}

static int cmd_tool_dispatch(void *context, int argc, char **argv)
{
    esp_cli_service_t *svc = (esp_cli_service_t *)context;
    if (svc == NULL) {
        return 1;
    }

    if (argc < 2) {
        tool_print_usage();
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            tool_print_usage();
            return 1;
        }
        return tool_handle_list(svc);
    }

    if (strcmp(argv[1], "info") == 0) {
        if (argc != 3) {
            tool_print_usage();
            return 1;
        }
        return tool_handle_info(svc, argv[2]);
    }

    if (strcmp(argv[1], "call") == 0) {
        return tool_handle_call(svc, argc, argv);
    }

    tool_print_usage();
    return 1;
}

esp_err_t cli_register_core_commands(esp_cli_service_t *svc)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Register core commands failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_console_cmd_t svc_cmd = {
        .command = "svc",
        .help = "Service management: list/info/start/stop/pause/resume",
        .func_w_context = cmd_svc_dispatch,
        .context = svc,
    };

    esp_err_t ret = esp_console_cmd_register(&svc_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register core commands failed: register 'svc', %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_console_cmd_t tool_cmd = {
        .command = "tool",
        .help = "Dynamic tool management: list/info/call",
        .func_w_context = cmd_tool_dispatch,
        .context = svc,
    };

    ret = esp_console_cmd_register(&tool_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register core commands failed: register 'tool', %s", esp_err_to_name(ret));
    }
    return ret;
}
