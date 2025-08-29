/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file service_integration_test.c
 * @brief  Integration test — all interaction modes coexisting with event-driven flow
 *
 *         Simulates a realistic embedded device with four services running concurrently
 *         and three distinct interaction paths sharing the same instances:
 *
 *         Mode A — Direct C API
 *         Application firmware and event callbacks call service functions directly.
 *         Zero overhead, used for latency-critical or on-device logic.
 *
 *         Mode B — Service Manager tool dispatch
 *         An on-device automation controller calls esp_service_manager_invoke_tool()
 *         by tool name, without any network transport or JSON-RPC framing.
 *
 *         Mode C — MCP HTTP JSON-RPC
 *         A remote AI agent sends tools/call requests over HTTP to the MCP server.
 *
 *         Event-driven — adf_event_hub callbacks
 *         button_service (mock) publishes key-press events (PLAY, VOL_UP, VOL_DOWN, MODE).
 *         wifi_service publishes connectivity events (CONNECTING, CONNECTED, GOT_IP,
 *         DISCONNECTED).  Both drive player and LED via Mode A and Mode B.
 *
 *         ┌──────────────────────────────────────────────────────────────────┐
 *         │                     FreeRTOS POSIX process                        │
 *         │                                                                  │
 *         │  [task_a] direct_api_task   ────────────────────────────────┐   │
 *         │  [task_b] local_ctrl_task   ──────────────────────────────┐ │   │
 *         │  [task_c] mcp_agent_task    ──── HTTP POST /mcp ─────────┐│ │   │
 *         │                                        │                  ││ │   │
 *         │  button_service (async sim) ── EVT ──► on_button_event()  ││ ▼   │
 *         │  wifi_service   (async sim) ── EVT ──► on_wifi_event()   ─┘└►player_service_t │
 *         │                                                              └►led_service_t   │
 *         └──────────────────────────────────────────────────────────────────┘
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "esp_log.h"
#include "esp_service_manager.h"

#include "player_service.h"
#include "led_service.h"
#include "button_service.h"
#include "wifi_service.h"
#include "adf_event_hub.h"
#include "svc_helpers.h"

static const char *TAG = "INTEGRATION";

#define MCP_PORT          9877
#define TEST_DURATION_MS  12000
#define TASK_PRIORITY     (tskIDLE_PRIORITY + 5)

/**
 * Set by main() when --llm is passed on the command line.
 * Keeps the MCP HTTP server alive after the automated test finishes so
 * that an external LLM bridge (mcp_bridge.py) can connect and issue tool
 * calls interactively or in batch mode.
 */
static volatile bool s_llm_mode = false;
static volatile bool s_sigterm_got = false;
static SemaphoreHandle_t s_sigterm_sem = NULL;

static void sigterm_handler(int sig)
{
    (void)sig;
    s_sigterm_got = true;
    if (s_sigterm_sem) {
        /* Signal from a C signal handler: xSemaphoreGiveFromISR is not safe
         * here, but on POSIX a signal handler runs in a pthread context and
         * xSemaphoreGive is effectively safe for counting semaphores. */
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(s_sigterm_sem, &woken);
    }
}

static esp_service_manager_t *s_mgr = NULL;
static player_service_t *s_player = NULL;
static led_service_t *s_led = NULL;
static button_service_t *s_button = NULL;
static wifi_service_t *s_wifi = NULL;
static esp_service_mcp_server_t *s_mcp = NULL;
static esp_service_mcp_trans_t *s_transport = NULL;
static adf_event_hub_t s_app_hub = NULL;

static volatile bool s_stop_all = false;

/* Completion semaphore: main task waits for all three tasks to finish. */
static SemaphoreHandle_t s_task_done;
static volatile int s_direct_calls = 0;    /* Mode A + event-driven direct calls */
static volatile int s_dispatch_calls = 0;  /* Mode B + event-driven tool dispatch */
static volatile int s_mcp_calls = 0;       /* Mode C HTTP calls */
static volatile int s_event_calls = 0;     /* events received from button + wifi */

/* ── HTTP client helper ─────────────────────────────────────────────────── */

/**
 * @brief  Send one MCP JSON-RPC request over HTTP POST and read the response.
 *
 * @param[in]   body       JSON-RPC request body string
 * @param[out]  resp       Buffer to receive raw HTTP response
 * @param[in]   resp_size  Size of resp buffer
 * @return
 *       - Pointer  to JSON body (after HTTP headers) inside resp, or NULL.
 */
static const char *mcp_http_post(const char *body, char *resp, size_t resp_size)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return NULL;
    }

    /* 3-second receive timeout so a slow/busy server doesn't stall the task. */
    struct timeval tv = {.tv_sec = 3, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MCP_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NULL;
    }

    char req[1024];
    int body_len = (int)strlen(body);
    int req_len = snprintf(req, sizeof(req),
                           "POST /mcp HTTP/1.1\r\n"
                           "Host: 127.0.0.1:%u\r\n"
                           "Content-Type: application/json\r\n"
                           "Accept: application/json, text/event-stream\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "%s",
                           MCP_PORT, body_len, body);
    send(fd, req, req_len, 0);

    /**
     * Read until EOF (server closes after response) or buffer full.
     * Retry on EINTR: FreeRTOS POSIX port fires SIGALRM at every tick,
     * which interrupts blocking recv() with EINTR.
     */
    int total = 0;
    for (;;) {
        int n = (int)recv(fd, resp + total, resp_size - 1 - total, 0);
        if (n > 0) {
            total += n;
            if (total >= (int)resp_size - 1) {
                break;
            }
        } else if (n == 0) {
            break;  /* server closed the connection */
        } else {
            if (errno == EINTR) {
                continue;  /* interrupted by signal — retry */
            }
            break;  /* SO_RCVTIMEO expired or real error */
        }
    }
    resp[total] = '\0';
    close(fd);

    if (total == 0) {
        return NULL;
    }
    /* Skip HTTP header, return pointer to JSON body. */
    const char *json = strstr(resp, "\r\n\r\n");
    /* SSE wraps body as "data: {...}\n\n" — skip that prefix too. */
    if (json) {
        json += 4;
        if (strncmp(json, "data: ", 6) == 0) {
            json += 6;
        }
    }
    return json;
}

static void on_button_event(const adf_event_t *ev, void *ctx)
{
    char result[128] = {0};
    s_event_calls++;

    switch (ev->event_id) {
        case BUTTON_EVT_PLAY:
            ESP_LOGI(TAG, "[EVT][button] PLAY       → [A] player_service_play");
            player_service_play(s_player);
            s_direct_calls++;
            break;

        case BUTTON_EVT_VOL_UP:
            ESP_LOGI(TAG, "[EVT][button] VOL_UP     → [B] invoke set_volume(75)");
            esp_service_manager_invoke_tool(s_mgr, "player_service_set_volume",
                                            "{\"volume\":75}", result, sizeof(result));
            s_dispatch_calls++;
            break;

        case BUTTON_EVT_VOL_DOWN:
            ESP_LOGI(TAG, "[EVT][button] VOL_DOWN   → [B] invoke set_volume(30)");
            esp_service_manager_invoke_tool(s_mgr, "player_service_set_volume",
                                            "{\"volume\":30}", result, sizeof(result));
            s_dispatch_calls++;
            break;

        case BUTTON_EVT_MODE:
            ESP_LOGI(TAG, "[EVT][button] MODE       → [A] led_service_blink(200)");
            led_service_blink(s_led, 200);
            s_direct_calls++;
            break;

        case BUTTON_EVT_PROVISION:
            ESP_LOGI(TAG, "[EVT][button] PROVISION  → [A] led_service_on");
            led_service_on(s_led);
            s_direct_calls++;
            break;

        default:
            break;
    }
}

static void on_wifi_event(const adf_event_t *ev, void *ctx)
{
    char result[128] = {0};
    s_event_calls++;

    switch (ev->event_id) {
        case WIFI_EVT_CONNECTING:
            ESP_LOGI(TAG, "[EVT][wifi]   CONNECTING → [A] led_service_blink(500)");
            led_service_blink(s_led, 500);
            s_direct_calls++;
            break;

        case WIFI_EVT_CONNECTED:
            ESP_LOGI(TAG, "[EVT][wifi]   CONNECTED  → [A] player_service_play (network ready)");
            player_service_play(s_player);
            s_direct_calls++;
            break;

        case WIFI_EVT_GOT_IP: {
            const wifi_ip_info_t *ip = (const wifi_ip_info_t *)ev->payload;
            if (ip) {
                ESP_LOGI(TAG, "[EVT][wifi]   GOT_IP    → [B] invoke led_service_on (ip=%s)", ip->ip);
            } else {
                ESP_LOGI(TAG, "[EVT][wifi]   GOT_IP    → [B] invoke led_service_on");
            }
            esp_service_manager_invoke_tool(s_mgr, "led_service_on", "{}", result, sizeof(result));
            s_dispatch_calls++;
            break;
        }

        case WIFI_EVT_DISCONNECTED:
            ESP_LOGI(TAG, "[EVT][wifi]   DISCONN    → [A] led_service_blink(100) (warn user)");
            led_service_blink(s_led, 100);
            s_direct_calls++;
            break;

        case WIFI_EVT_RSSI_UPDATE:
            ESP_LOGI(TAG, "[EVT][wifi]   RSSI_UPD   → (no action, informational)");
            s_event_calls--;  /* don't count as actionable */
            break;

        default:
            break;
    }
}

static void subscribe_events(void)
{
    adf_event_hub_create("app_core", &s_app_hub);

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = BUTTON_DOMAIN;
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler = on_button_event;
    adf_event_hub_subscribe(s_app_hub, &sub);

    sub.event_domain = WIFI_DOMAIN;
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler = on_wifi_event;
    adf_event_hub_subscribe(s_app_hub, &sub);

    ESP_LOGI(TAG, "Subscribed to 'button' and 'wifi' event domains");
}

static void event_source_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "[EVT_SRC] Event source task started");

    /**
     * Button event sequence: cycle through a variety of key presses that
     * exercise both direct-API and tool-dispatch callback paths.
     */
    static const uint16_t btn_seq[] = {
        BUTTON_EVT_PROVISION,
        BUTTON_EVT_PLAY,
        BUTTON_EVT_VOL_UP,
        BUTTON_EVT_MODE,
        BUTTON_EVT_VOL_DOWN,
        BUTTON_EVT_PLAY,
        BUTTON_EVT_VOL_UP,
        BUTTON_EVT_MODE,
    };

    /**
     * WiFi state machine: one full connect → get-ip → disconnect cycle,
     * then a second round, to exercise all connectivity event handlers.
     */
    static const uint16_t wifi_seq[] = {
        WIFI_EVT_CONNECTING,
        WIFI_EVT_CONNECTED,
        WIFI_EVT_GOT_IP,
        WIFI_EVT_CONNECTING,
        WIFI_EVT_CONNECTED,
        WIFI_EVT_GOT_IP,
        WIFI_EVT_DISCONNECTED,
    };

    const int n_btn = (int)(sizeof(btn_seq) / sizeof(btn_seq[0]));
    const int n_wifi = (int)(sizeof(wifi_seq) / sizeof(wifi_seq[0]));
    adf_event_hub_t button_hub = NULL;
    adf_event_hub_t wifi_hub = NULL;
    int btn_idx = 0;
    int wifi_idx = 0;
    int tick = 0;

    ESP_ERROR_CHECK(esp_service_get_event_hub((esp_service_t *)s_button, &button_hub));
    ESP_ERROR_CHECK(esp_service_get_event_hub((esp_service_t *)s_wifi, &wifi_hub));

    while (!s_stop_all) {
        /* Publish one button event every ~1 400 ms. */
        if (tick % 2 == 0 && btn_idx < n_btn) {
            adf_event_t ev = {
                .domain = BUTTON_DOMAIN,
                .event_id = btn_seq[btn_idx],
                .payload = NULL,
                .payload_len = 0,
            };
            adf_event_hub_publish(button_hub, &ev, NULL, NULL);
            btn_idx++;
        }

        /* Publish one WiFi event every ~2 100 ms. */
        if (tick % 3 == 0 && wifi_idx < n_wifi) {
            static wifi_ip_info_t ip_info = {"192.168.1.100", "192.168.1.1", "255.255.255.0"};
            adf_event_t ev = {
                .domain = WIFI_DOMAIN,
                .event_id = wifi_seq[wifi_idx],
                .payload = (wifi_seq[wifi_idx] == WIFI_EVT_GOT_IP) ? &ip_info : NULL,
                .payload_len = (wifi_seq[wifi_idx] == WIFI_EVT_GOT_IP)
                                   ? sizeof(ip_info)
                                   : 0,
            };
            adf_event_hub_publish(wifi_hub, &ev, NULL, NULL);
            wifi_idx++;
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(700));
    }

    ESP_LOGI(TAG, "[EVT_SRC] Event source done  (btn=%d wifi=%d events sent)",
             btn_idx, wifi_idx);
    xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

static void direct_api_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "[A] Direct C API task started");

    static const struct {
        int  gpio_op;  /* 0=off 1=on 2=blink 3=brightness */
        int  val;
    } led_seq[] = {
        {1, 0}, {2, 300}, {3, 80}, {0, 0}, {1, 0}, {2, 600}, {3, 50}, {0, 0},
    };
    int seq_len = (int)(sizeof(led_seq) / sizeof(led_seq[0]));
    int idx = 0;

    while (!s_stop_all) {
        switch (led_seq[idx % seq_len].gpio_op) {
            case 0:
                led_service_off(s_led);
                break;
            case 1:
                led_service_on(s_led);
                break;
            case 2:
                led_service_blink(s_led, led_seq[idx % seq_len].val);
                break;
            case 3:
                led_service_set_brightness(s_led, led_seq[idx % seq_len].val);
                break;
        }
        player_service_play(s_player);
        s_direct_calls++;
        idx++;
        vTaskDelay(pdMS_TO_TICKS(600));
    }

    ESP_LOGI(TAG, "[A] Direct C API task done  — %d calls", s_direct_calls);
    xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

static void local_ctrl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "[B] Local dispatch task started");

    static const struct {
        const char *tool;
        const char *args;
    } cmds[] = {
        {"player_service_set_volume", "{\"volume\":60}"},
        {"led_service_set_brightness", "{\"brightness\":40}"},
        {"player_service_get_status", "{}"},
        {"led_service_get_state", "{}"},
        {"player_service_play", "{}"},
        {"led_service_blink", "{\"period_ms\":400}"},
        {"player_service_set_volume", "{\"volume\":90}"},
        {"led_service_on", "{}"},
    };
    const int n_cmds = (int)(sizeof(cmds) / sizeof(cmds[0]));
    int idx = 0;

    while (!s_stop_all) {
        char result[256] = {0};
        esp_err_t ret = esp_service_manager_invoke_tool(
            s_mgr,
            cmds[idx % n_cmds].tool,
            cmds[idx % n_cmds].args,
            result, sizeof(result));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "[B] %-38s → %s", cmds[idx % n_cmds].tool, result);
            s_dispatch_calls++;
        } else {
            ESP_LOGW(TAG, "[B] Invoke '%s' failed: 0x%x",
                     cmds[idx % n_cmds].tool, ret);
        }
        idx++;
        vTaskDelay(pdMS_TO_TICKS(900));
    }

    ESP_LOGI(TAG, "[B] Local dispatch task done  — %d calls", s_dispatch_calls);
    xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

static void mcp_agent_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "[C] MCP HTTP agent task started");

    /* Wait briefly for the HTTP server listener to be ready. */
    vTaskDelay(pdMS_TO_TICKS(600));

    char resp[4096];
    int id = 200;

    /* --- initialize --- */
    {
        char body[256];
        snprintf(body, sizeof(body),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,"
                 "\"method\":\"initialize\","
                 "\"params\":{\"protocolVersion\":\"2024-11-05\","
                 "\"capabilities\":{},"
                 "\"clientInfo\":{\"name\":\"integration_agent\","
                 "\"version\":\"1.0\"}}}",
                 id++);
        const char *json = mcp_http_post(body, resp, sizeof(resp));
        if (json) {
            ESP_LOGI(TAG, "[C] Initialize → %.80s", json);
            s_mcp_calls++;
        } else {
            ESP_LOGW(TAG, "[C] Initialize failed (server not ready)");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* --- tools/list --- */
    {
        char body[128];
        snprintf(body, sizeof(body),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,"
                 "\"method\":\"tools/list\",\"params\":{}}", id++);
        const char *json = mcp_http_post(body, resp, sizeof(resp));
        if (json) {
            /* Count discovered tools by counting "\"name\"" occurrences. */
            int count = 0;
            const char *p = json;
            while ((p = strstr(p, "\"name\"")) != NULL) {
                count++;
                p++;
            }
            ESP_LOGI(TAG, "[C] tools/list → %d tools discovered", count);
            s_mcp_calls++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* --- tools/call loop --- */
    static const struct {
        const char *name;
        const char *args;
    } mcp_cmds[] = {
        {"player_service_play", "{}"},
        {"player_service_set_volume", "{\"volume\":80}"},
        {"led_service_on", "{}"},
        {"led_service_set_brightness", "{\"brightness\":70}"},
        {"player_service_get_volume", "{}"},
        {"led_service_get_state", "{}"},
        {"player_service_get_status", "{}"},
        {"led_service_blink", "{\"period_ms\":250}"},
        {"player_service_set_volume", "{\"volume\":55}"},
        {"led_service_off", "{}"},
    };
    const int n_mcp = (int)(sizeof(mcp_cmds) / sizeof(mcp_cmds[0]));
    int cidx = 0;

    while (!s_stop_all) {
        char body[256];
        snprintf(body, sizeof(body),
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,"
                 "\"method\":\"tools/call\","
                 "\"params\":{\"name\":\"%s\","
                 "\"arguments\":%s}}",
                 id++,
                 mcp_cmds[cidx % n_mcp].name,
                 mcp_cmds[cidx % n_mcp].args);

        const char *json = mcp_http_post(body, resp, sizeof(resp));
        const char *status = "FAIL";
        if (json) {
            if (strstr(json, "\"result\"")) {
                status = "OK";
            }
            else if (strstr(json, "\"error\"")) {
                status = "ERR";
            }
            s_mcp_calls++;
        }
        ESP_LOGI(TAG, "[C] tools/call %-38s → %s", mcp_cmds[cidx % n_mcp].name, status);
        cidx++;
        vTaskDelay(pdMS_TO_TICKS(1100));
    }

    ESP_LOGI(TAG, "[C] MCP HTTP agent task done  — %d calls", s_mcp_calls);
    xSemaphoreGive(s_task_done);
    vTaskDelete(NULL);
}

static void main_task(void *arg)
{
    (void)arg;
    esp_err_t ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "  Integration Test — All interaction modes + events");
    ESP_LOGI(TAG, "    Mode A  Direct C API       (application firmware)");
    ESP_LOGI(TAG, "    Mode B  Tool dispatch       (on-device controller)");
    ESP_LOGI(TAG, "    Mode C  MCP HTTP JSON-RPC   (remote AI agent)");
    ESP_LOGI(TAG, "    Events  button + wifi → drive player & LED");
    ESP_LOGI(TAG, "  Duration: %d ms", TEST_DURATION_MS);
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");

    /* ── Create services ────────────────────────────────────────────────── */
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = true;
    ret = esp_service_manager_create(&mgr_cfg, &s_mgr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Manager create failed");
        goto fail;
    }

    player_service_cfg_t pcfg = {
        .uri = "http://example.com/audio.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    ret = player_service_create(&pcfg, &s_player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Player create failed");
        goto fail;
    }
    svc_register_player(s_mgr, s_player, "audio");

    led_service_cfg_t lcfg = {.gpio_num = 2, .blink_period = 500};
    ret = led_service_create(&lcfg, &s_led);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LED create failed");
        goto fail;
    }
    svc_register_led(s_mgr, s_led, "display");

    /* ── Subscribe to events BEFORE starting event-source services ──────── */
    /**
     * Subscriptions can be registered before the publisher's hub exists.
     * The event hub auto-creates the target domain on first subscribe.
     * This guarantees no events are lost when button/wifi start simulating.
     */
    subscribe_events();

    /* ── button_service (mock): event-only; event_source_task drives events directly */
    button_service_cfg_t bcfg = {
        .button_id = 0,
        .long_press_ms = 2000,
        .sim_presses = 0,  /* no internal sim — event_source_task publishes instead */
    };
    ret = button_service_create(&bcfg, &s_button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button service create failed");
        goto fail;
    }
    {
        esp_service_registration_t reg = {
            .service = (esp_service_t *)s_button,
            .category = "input",
        };
        esp_service_manager_register(s_mgr, &reg);
    }

    /* ── wifi_service: event-only; event_source_task drives events directly */
    wifi_service_cfg_t wcfg = {
        .ssid = "TestAP",
        .password = "password123",
        .max_retry = 3,
        .sim_rounds = 0,  /* no internal sim — event_source_task publishes instead */
    };
    ret = wifi_service_create(&wcfg, &s_wifi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi service create failed");
        goto fail;
    }
    {
        esp_service_registration_t reg = {
            .service = (esp_service_t *)s_wifi,
            .category = "network",
        };
        esp_service_manager_register(s_mgr, &reg);
    }

    /* ── Start MCP HTTP transport + server ─────────────────────────────── */
    ret = svc_mcp_http_start(s_mgr, MCP_PORT, &s_transport, &s_mcp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCP HTTP server start failed");
        goto fail;
    }

    ESP_LOGI(TAG, "MCP HTTP server ready on http://127.0.0.1:%u/mcp", MCP_PORT);
    ESP_LOGI(TAG, "");

    /* ── Launch all four concurrent tasks ───────────────────────────────── */
    s_task_done = xSemaphoreCreateCounting(4, 0);

    xTaskCreate(direct_api_task, "direct_api", 4096, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(local_ctrl_task, "local_ctrl", 4096, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(mcp_agent_task, "mcp_agent", 8192, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(event_source_task, "evt_src", 4096, NULL, TASK_PRIORITY, NULL);

    /* ── Run the test for TEST_DURATION_MS ──────────────────────────────── */
    vTaskDelay(pdMS_TO_TICKS(TEST_DURATION_MS));
    s_stop_all = true;
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Stop signal sent — waiting for tasks to finish...");

    /* Wait for all four tasks to call xSemaphoreGive(s_task_done). */
    for (int i = 0; i < 4; i++) {
        xSemaphoreTake(s_task_done, pdMS_TO_TICKS(3000));
    }

    /* ── Verify: manager state after concurrent access ──────────────────── */
    uint16_t svc_count = 0;
    esp_service_manager_get_count(s_mgr, &svc_count);

    esp_service_t *found_player = NULL;
    esp_service_t *found_led = NULL;
    esp_service_manager_find_by_name(s_mgr, "player", &found_player);
    esp_service_manager_find_by_name(s_mgr, "led", &found_led);

    led_state_t final_led_state;
    led_service_get_state(s_led, &final_led_state);

    uint32_t final_volume = 0;
    player_service_get_volume(s_player, &final_volume);
    int btn_presses = button_service_get_press_count(s_button);
    int btn_evts_sent = button_service_get_events_sent(s_button);
    int wifi_evts_sent = wifi_service_get_events_sent(s_wifi);
    int total_ops = s_direct_calls + s_dispatch_calls + s_mcp_calls;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "  Results");
    ESP_LOGI(TAG, "    Mode A  Direct C API calls   : %d  (incl. event-driven)", s_direct_calls);
    ESP_LOGI(TAG, "    Mode B  Tool dispatch calls  : %d  (incl. event-driven)", s_dispatch_calls);
    ESP_LOGI(TAG, "    Mode C  MCP HTTP calls       : %d", s_mcp_calls);
    ESP_LOGI(TAG, "    Total service operations     : %d", total_ops);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  Event statistics:");
    ESP_LOGI(TAG, "    button presses simulated     : %d", btn_presses);
    ESP_LOGI(TAG, "    button events published      : %d", btn_evts_sent);
    ESP_LOGI(TAG, "    wifi   events published      : %d", wifi_evts_sent);
    ESP_LOGI(TAG, "    event callbacks received     : %d", s_event_calls);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  Service state after concurrent access:");
    ESP_LOGI(TAG, "    Registered services          : %u (player+led+button+wifi)", svc_count);
    ESP_LOGI(TAG, "    player found by name         : %s",
             found_player == (esp_service_t *)s_player ? "OK" : "FAIL");
    ESP_LOGI(TAG, "    led    found by name         : %s",
             found_led == (esp_service_t *)s_led ? "OK" : "FAIL");
    ESP_LOGI(TAG, "    Final player volume          : %u", (unsigned)final_volume);
    ESP_LOGI(TAG, "    Final LED state              : %s",
             final_led_state == LED_STATE_OFF   ? "OFF" :
             final_led_state == LED_STATE_ON    ? "ON" :
             final_led_state == LED_STATE_BLINK ? "BLINK" : "?");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════════");

    bool pass = (svc_count == 4)
                && (found_player == (esp_service_t *)s_player)
                && (found_led == (esp_service_t *)s_led)
                && (total_ops >= 30)
                && (s_event_calls >= 1);

    ESP_LOGI(TAG, "  VERDICT: %s", pass ? "PASS" : "FAIL");
    ESP_LOGI(TAG, "");

    /* ── LLM-mode: keep server alive for mcp_bridge.py ─────────────────── */
    if (s_llm_mode) {
        ESP_LOGI(TAG, "════════════════════════════════════════════════════════");
        ESP_LOGI(TAG, "  LLM MODE: MCP server staying alive on port %u", MCP_PORT);
        ESP_LOGI(TAG, "  All %u services remain registered and running.", svc_count);
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  Connect with mcp_bridge.py:");
        ESP_LOGI(TAG, "    cd tools/mcp_llm_bridge");
        ESP_LOGI(TAG, "    python mcp_bridge.py discover configs/pc_integration.json");
        ESP_LOGI(TAG, "    python mcp_bridge.py test    configs/pc_integration.json");
        ESP_LOGI(TAG, "    python mcp_bridge.py run     configs/pc_integration.json batch");
        ESP_LOGI(TAG, "    python mcp_bridge.py run     configs/pc_integration.json interactive");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  Press Ctrl-C or send SIGTERM to shut down.");
        ESP_LOGI(TAG, "════════════════════════════════════════════════════════");

        /* Wait until SIGTERM is received. */
        s_sigterm_sem = xSemaphoreCreateCounting(1, 0);
        signal(SIGTERM, sigterm_handler);
        signal(SIGINT, sigterm_handler);
        xSemaphoreTake(s_sigterm_sem, portMAX_DELAY);
        vSemaphoreDelete(s_sigterm_sem);
        s_sigterm_sem = NULL;
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "Shutdown signal received — tearing down.");
    }

    /* ── Teardown ────────────────────────────────────────────────────────── */
    svc_mcp_http_stop(s_mcp, s_transport);

    /**
     * Unsubscribe from button and wifi domains BEFORE stopping services.
     * button/wifi on_stop callbacks publish events; if our handlers are still
     * registered at that point they would dereference already-freed service
     * handles (e.g. s_led after led is destroyed).
     */
    if (s_app_hub) {
        adf_event_hub_unsubscribe(s_app_hub, BUTTON_DOMAIN, ADF_EVENT_ANY_ID);
        adf_event_hub_unsubscribe(s_app_hub, WIFI_DOMAIN, ADF_EVENT_ANY_ID);
        adf_event_hub_destroy(s_app_hub);
        s_app_hub = NULL;
    }

    esp_service_manager_destroy(s_mgr);
    player_service_destroy(s_player);
    led_service_destroy(s_led);
    button_service_destroy(s_button);
    wifi_service_destroy(s_wifi);
    vSemaphoreDelete(s_task_done);

    fflush(stdout);
    exit(pass ? 0 : 1);

fail:
    ESP_LOGE(TAG, "Setup failed — aborting");
    fflush(stdout);
    exit(1);
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_DFL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--llm") == 0) {
            s_llm_mode = true;
        }
    }

    xTaskCreate(main_task, "main_task", 8192, NULL, TASK_PRIORITY, NULL);
    vTaskStartScheduler();

    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
