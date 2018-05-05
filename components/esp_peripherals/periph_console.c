/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"

#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "rom/queue.h"
#include "argtable3/argtable3.h"
#include "periph_console.h"

static const char *TAG = "PERIPH_CONSOLE";


#define CONSOLE_BUFFER_SIZE (128)
#define CONSOLE_MAX_ARGUMENTS (5)

static const int STOPPED_BIT = BIT1;

typedef struct periph_console *periph_console_handle_t;

typedef struct periph_console {
    char                        *buffer;
    int                         total_bytes;
    bool                        run;
    const periph_console_cmd_t  *commands;
    int                         command_num;
    EventGroupHandle_t          state_event_bits;
    int                         task_stack;
    int                         task_prio;
    char                        *prompt_string;
} periph_console_t;

static char *conslole_parse_arguments(char *str, char **saveptr)
{
    char *p;

    if (str != NULL) {
        *saveptr = str;
    }

    p = *saveptr;
    if (!p) {
        return NULL;
    }
    /* Skipping white space.*/
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p == '"') {
        /* If an argument starts with a double quote then its delimiter is another quote.*/
        p++;
        *saveptr = strstr(p, "\"");
    } else {
        /* The delimiter is white space.*/
        *saveptr = strpbrk(p, " \t");
    }

    /* Replacing the delimiter with a zero.*/
    if (*saveptr != NULL) {
        *(*saveptr)++ = '\0';
    }
    return *p != '\0' ? p : NULL;
}

bool console_get_line(periph_console_handle_t console, unsigned max_size, TickType_t time_to_wait)
{
    char c;
    char tx[3];

    int nread = uart_read_bytes(CONFIG_CONSOLE_UART_NUM, (uint8_t *)&c, 1, time_to_wait);
    if (nread <= 0) {
        return false;
    }
    if ((c == 8) || (c == 127)) {  // backspace or del
        if (console->total_bytes > 0) {
            console->total_bytes --;
            tx[0] = c;
            tx[1] = 0x20;
            tx[2] = c;
            uart_write_bytes(CONFIG_CONSOLE_UART_NUM, (const char *)tx, 3);
        }
        return false;
    }
    if (c == '\n' || c == '\r') {
        tx[0] = '\r';
        tx[1] = '\n';
        uart_write_bytes(CONFIG_CONSOLE_UART_NUM, (const char *)tx, 2);
        console->buffer[console->total_bytes] = 0;
        return true;
    }

    if (c < 0x20) {
        return false;
    }
    uart_write_bytes(CONFIG_CONSOLE_UART_NUM, (const char *)&c, 1);
    console->buffer[console->total_bytes++] = (char)c;
    if (console->total_bytes > max_size) {
        console->total_bytes = 0;
    }

    return false;
}

static bool console_exec(esp_periph_handle_t self, char *cmd, int argc, char *argv[])
{
    periph_console_handle_t console = (periph_console_handle_t)esp_periph_get_data(self);
    int i;
    for (i = 0; i < console->command_num; i++) {
        if (strcasecmp(cmd, console->commands[i].cmd) == 0) {
            if (console->commands[i].func) {
                console->commands[i].func(self, argc, argv);
                return true;
            }
            int cmd_id = console->commands[i].id;
            if (cmd_id == 0) {
                cmd_id = i;
            }
            esp_periph_send_event(self, cmd_id, argv, argc);
            return true;
        }
    }
    printf("----------------------\r\n");
    printf("Perpheral console HELP\r\n");
    printf("----------------------\r\n");
    for (i = 0; i < console->command_num; i++) {
        printf("%s \t%s\r\n", console->commands[i].cmd, console->commands[i].help);
    }
    return false;
}

static esp_err_t _console_destroy(esp_periph_handle_t self)
{
    periph_console_handle_t console = (periph_console_handle_t)esp_periph_get_data(self);
    console->run = false;
    xEventGroupWaitBits(console->state_event_bits, STOPPED_BIT, false, true, portMAX_DELAY);
    vEventGroupDelete(console->state_event_bits);
    if (console->prompt_string) {
        free(console->prompt_string);
    }
    free(console->buffer);
    free(console);
    return ESP_OK;
}

static void _console_task(void *pv)
{
    esp_periph_handle_t self = (esp_periph_handle_t)pv;
    char *lp, *cmd, *tokp;
    char *args[CONSOLE_MAX_ARGUMENTS + 1];
    int n;

    periph_console_handle_t console = (periph_console_handle_t)esp_periph_get_data(self);
    if (console->total_bytes >= CONSOLE_BUFFER_SIZE) {
        console->total_bytes = 0;
    }
    console->run = true;
    xEventGroupClearBits(console->state_event_bits, STOPPED_BIT);
    const char *prompt_string = CONSOLE_DEFAULT_PROMPT_STRING;
    if (console->prompt_string) {
        prompt_string = console->prompt_string;
    }
    printf("\r\n%s ", prompt_string);
    while (console->run) {
        if (console_get_line(console, CONSOLE_BUFFER_SIZE, 10 / portTICK_RATE_MS)) {
            if (console->total_bytes) {
                ESP_LOGD(TAG, "Read line: %s", console->buffer);
            }
            lp = conslole_parse_arguments(console->buffer, &tokp);
            cmd = lp;
            n = 0;
            while ((lp = conslole_parse_arguments(NULL, &tokp)) != NULL) {
                if (n >= CONSOLE_MAX_ARGUMENTS) {
                    printf("too many arguments\r\n");
                    cmd = NULL;
                    break;
                }
                args[n++] = lp;
            }
            args[n] = NULL;
            if (console->total_bytes > 0) {
                console_exec(self, cmd, n, args);
                console->total_bytes = 0;
            }
            printf("%s ", prompt_string);
        }

    }
    xEventGroupSetBits(console->state_event_bits, STOPPED_BIT);
    vTaskDelete(NULL);
}

static esp_err_t _console_init(esp_periph_handle_t self)
{
    periph_console_handle_t console = (periph_console_handle_t)esp_periph_get_data(self);

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    uart_driver_install(CONFIG_CONSOLE_UART_NUM, CONSOLE_BUFFER_SIZE * 2, 0, 0, NULL, 0);

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);


    console->buffer = (char*) malloc(CONSOLE_BUFFER_SIZE);
    AUDIO_MEM_CHECK(TAG, console->buffer, {
        return ESP_ERR_NO_MEM;
    });

    if (xTaskCreate(_console_task, "console_task", console->task_stack, self, console->task_prio, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Error create console task, memory exhausted?");
        return ESP_FAIL;
    }
    return ESP_OK;
}


esp_periph_handle_t periph_console_init(periph_console_cfg_t *config)
{
    esp_periph_handle_t periph = esp_periph_create(PERIPH_ID_CONSOLE, "periph_console");
    AUDIO_MEM_CHECK(TAG, periph, return NULL);
    periph_console_t *console = calloc(1, sizeof(periph_console_t));
    AUDIO_MEM_CHECK(TAG, console, return NULL);
    console->commands = config->commands;
    console->command_num = config->command_num;
    console->task_stack = CONSOLE_DEFAULT_TASK_STACK;
    console->task_prio = CONSOLE_DEFAULT_TASK_PRIO;
    if (config->task_stack > 0) {
        console->task_stack = config->task_stack;
    }
    if (config->task_prio) {
        console->task_prio = config->task_prio;
    }
    if (config->prompt_string) {
        console->prompt_string = strdup(config->prompt_string);
        AUDIO_MEM_CHECK(TAG, console->prompt_string, {
            free(console);
            return NULL;
        });
    }
    console->state_event_bits = xEventGroupCreate();
    esp_periph_set_data(periph, console);
    esp_periph_set_function(periph, _console_init, NULL, _console_destroy);
    return periph;
}
