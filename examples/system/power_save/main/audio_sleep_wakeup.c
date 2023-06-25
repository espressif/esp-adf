/*  Audio sleep wakeup example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/rtc_io.h"
#include "soc/uart_pins.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "esp_timer.h"

static const char *TAG = "AUDIO_SLEEP_WAKEUP";

#define GPIO_WAKEUP_BIT   BIT(0)
#define UART_WAKEUP_BIT   BIT(1)
#define TIMER_WAKEUP_BIT  BIT(2)
#define ENTER_SLEEP_BIT   BIT(8)

#define WAKEUP_GROUP_BIT   (GPIO_WAKEUP_BIT | UART_WAKEUP_BIT | TIMER_WAKEUP_BIT)

#define EXAMPLE_UART_NUM   CONFIG_ESP_CONSOLE_UART_NUM
/* Notice that ESP32 has to use the iomux input to configure uart as wakeup source
 * Please use 'UxRXD_GPIO_NUM' as uart rx pin. No limitation to the other target */
#define EXAMPLE_UART_TX_IO_NUM   U0TXD_GPIO_NUM
#define EXAMPLE_UART_RX_IO_NUM   U0RXD_GPIO_NUM

#define EXAMPLE_UART_WAKEUP_THRESHOLD   3

#define EXAMPLE_READ_BUF_SIZE   1024
#define EXAMPLE_UART_BUF_SIZE   (EXAMPLE_READ_BUF_SIZE * 2)

// You can use other RTC gpio to wakeup system except VP and VN for esp32
#if CONFIG_IDF_TARGET_ESP32C3
#define GPIO_WAKEUP_NUM        9
#else
#define GPIO_WAKEUP_NUM        0
#endif

#define GPIO_WAKEUP_LEVEL      0

static QueueHandle_t           uart_evt_que;
static EventGroupHandle_t      wakeup_event_group;
static QueueHandle_t           gpio_evt_queue;
static esp_pm_lock_handle_t    s_pm_cpu_lock;

static TaskHandle_t            gpio_task_handle;
static esp_timer_handle_t      periodic_timer;

esp_sleep_wakeup_cause_t get_wakeup_cause(void)
{
    /* Determine wake up reason */
    const char* wakeup_reason;
    esp_sleep_wakeup_cause_t wakeup_source = esp_sleep_get_wakeup_cause();
    switch (wakeup_source) {
        case ESP_SLEEP_WAKEUP_TIMER:
            wakeup_reason = "TIMER";
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            wakeup_reason = "GPIO";
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            wakeup_reason = "EXT0";
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            wakeup_reason = "EXT1";
            break;
        case ESP_SLEEP_WAKEUP_UART:
            wakeup_reason = "UART";
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            wakeup_reason = "ULP";
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            wakeup_reason = "WIFI";
            break;
        case ESP_SLEEP_WAKEUP_BT:
            wakeup_reason = "Bluetooth";
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            wakeup_reason = "Undefined";
            ESP_LOGW(TAG, "Returned from sleep, reason: %s 0x%x", wakeup_reason, esp_rom_get_reset_reason(0));
            return wakeup_source;
        default:
            wakeup_reason = "Other";
            break;
    }
    ESP_LOGW(TAG, "Returned from sleep, reason: %s", wakeup_reason);
    return wakeup_source;
}

/*
 * See statement of esp_sleep_enable_uart_wakeup()
 * When baud rate is 115200, need send more than eight bytes for wakeup sysytem reliably, eg "123456789ABCDE\r\n"
 */
static void uart_wakeup_task(void *arg)
{
    uart_event_t event;
    if (uart_evt_que == NULL) {
        ESP_LOGE(TAG, "Uart_evt_que is NULL");
        abort();
    }

    uint8_t* dtmp = (uint8_t*) malloc(EXAMPLE_READ_BUF_SIZE);
    uint8_t uart_rx_len = 0;

    while(1) {
        /* Waiting for UART event */
        if (xQueueReceive(uart_evt_que, (void * )&event, (TickType_t)portMAX_DELAY)) {
#if !SOC_UART_SUPPORT_WAKEUP_INT
            if (xEventGroupGetBits(wakeup_event_group) & ENTER_SLEEP_BIT) {
                esp_pm_lock_acquire(s_pm_cpu_lock);
                ESP_LOGW(TAG, "Send uart wakeup group event");
                xEventGroupSetBits(wakeup_event_group, UART_WAKEUP_BIT);
                vTaskDelay(10 / portTICK_RATE_MS);
            }
#endif
            ESP_LOGD(TAG, "Uart%d recved event:%d", EXAMPLE_UART_NUM, event.type);
            switch(event.type) {
                case UART_DATA:
                    uart_rx_len = uart_read_bytes(EXAMPLE_UART_NUM, dtmp, SOC_UART_FIFO_LEN, 100 / portTICK_RATE_MS);
                    dtmp[uart_rx_len] = '\0';
                    ESP_LOGI(TAG, "[UART DATA]: %s, data size: %d", dtmp, uart_rx_len);
                    uart_wait_tx_idle_polling(EXAMPLE_UART_NUM);
                    break;
                /* Event of HW FIFO overflow detected */
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "Hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EXAMPLE_UART_NUM);
                    xQueueReset(uart_evt_que);
                    break;
                /* Event of UART ring buffer full */
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "Ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(EXAMPLE_UART_NUM);
                    xQueueReset(uart_evt_que);
                    break;
                /* Event of UART RX break detected */
                case UART_BREAK:
                    ESP_LOGI(TAG, "Uart rx break");
                    break;
                /* Event of UART parity check error */
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "Uart parity error");
                    break;
                /* Event of UART frame error */
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "Uart frame error");
                    break;
                /* ESP32 can wakeup by uart but there is no wake up interrupt */
#if SOC_UART_SUPPORT_WAKEUP_INT
                /* Event of waking up by UART */
                case UART_WAKEUP:
                    esp_pm_lock_acquire(s_pm_cpu_lock);
                    ESP_LOGI(TAG, "Uart wakeup");
                    xEventGroupSetBits(wakeup_event_group, UART_WAKEUP_BIT);
                    break;
#endif
                default:
                    ESP_LOGI(TAG, "Uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

static esp_err_t uart_initialization(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
    #else
        .source_clk = UART_SCLK_XTAL,
    #endif
    };
    /* Install UART driver, and get the queue */
    ESP_RETURN_ON_ERROR(uart_driver_install(EXAMPLE_UART_NUM, EXAMPLE_UART_BUF_SIZE, EXAMPLE_UART_BUF_SIZE, 20, &uart_evt_que, 0),
                        TAG, "Install uart failed");
    if (EXAMPLE_UART_NUM == CONFIG_ESP_CONSOLE_UART_NUM) {
        /* temp fix for uart garbled output, can be removed when IDF-5683 done */
        ESP_RETURN_ON_ERROR(uart_wait_tx_idle_polling(EXAMPLE_UART_NUM), TAG, "Wait uart tx done failed");
    }
    ESP_RETURN_ON_ERROR(uart_param_config(EXAMPLE_UART_NUM, &uart_cfg), TAG, "Configure uart param failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(EXAMPLE_UART_NUM, EXAMPLE_UART_TX_IO_NUM, EXAMPLE_UART_RX_IO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "Configure uart gpio pins failed");
    return ESP_OK;
}

static esp_err_t uart_wakeup_config(void)
{
    /* UART will wakeup the chip up from light sleep if the edges that RX pin received has reached the threshold
     * Besides, the Rx pin need extra configuration to enable it can work during light sleep */
    ESP_RETURN_ON_ERROR(gpio_sleep_set_direction(EXAMPLE_UART_RX_IO_NUM, GPIO_MODE_INPUT), TAG, "Set uart sleep gpio failed");
    ESP_RETURN_ON_ERROR(gpio_sleep_set_pull_mode(EXAMPLE_UART_RX_IO_NUM, GPIO_PULLUP_ONLY), TAG, "Set uart sleep gpio failed");
    ESP_RETURN_ON_ERROR(uart_set_wakeup_threshold(EXAMPLE_UART_NUM, EXAMPLE_UART_WAKEUP_THRESHOLD),
                        TAG, "Set uart wakeup threshold failed");
    /* Only uart0 and uart1 (if has) support to be configured as wakeup source */
    ESP_RETURN_ON_ERROR(esp_sleep_enable_uart_wakeup(EXAMPLE_UART_NUM),
                        TAG, "Configure uart as wakeup source failed");
    gpio_sleep_sel_dis(EXAMPLE_UART_RX_IO_NUM);
    gpio_sleep_sel_dis(EXAMPLE_UART_TX_IO_NUM);
    return ESP_OK;
}

static esp_err_t example_register_uart_wakeup(void)
{
    /* Initialize uart */
    ESP_RETURN_ON_ERROR(uart_initialization(), TAG, "Initialize uart%d failed", EXAMPLE_UART_NUM);
    /* Enable wakeup from uart */
    ESP_RETURN_ON_ERROR(uart_wakeup_config(), TAG, "Configure uart as wakeup source failed");

    xTaskCreate(uart_wakeup_task, "uart_wakeup_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Uart wakeup source is ready");
    return ESP_OK;
}

/* 
 * The uart wake-up source cannot wake the system from the deep sleep mode. You can consider setting
 * the uart RX pin to the ext0/1 or gpio wake-up source to wake it up by sending data through uart.
 */
static void register_ext0_wakeup(gpio_num_t gpio_num, uint8_t gpio_wakeup_level)
{
    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    if (gpio_wakeup_level) {
        ESP_ERROR_CHECK(gpio_pulldown_en(gpio_num));
        ESP_ERROR_CHECK(gpio_pullup_dis(gpio_num));
    } else {
        ESP_ERROR_CHECK(gpio_pullup_en(gpio_num));
        ESP_ERROR_CHECK(gpio_pulldown_dis(gpio_num));
    }

#if SOC_PM_SUPPORT_EXT_WAKEUP
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(gpio_num, gpio_wakeup_level));
#else
    ESP_LOGE(TAG, "Soc do not support ext0 wakeup source");
#endif
}

static void register_ext1_wakeup(gpio_num_t gpio_num, uint8_t gpio_wakeup_level)
{
   /* If there are no external pull-up/downs, tie wakeup pins to inactive level with internal pull-up/downs
    * via RTC IO during deepsleep. However, RTC IO relies on the RTC_PERIPH power domain.
    * Keeping this power domain on will increase some power comsumption.
    */
    if (gpio_wakeup_level) {
        ESP_ERROR_CHECK(gpio_pulldown_en(gpio_num));
        ESP_ERROR_CHECK(gpio_pullup_dis(gpio_num));
    } else {
        ESP_ERROR_CHECK(gpio_pullup_en(gpio_num));
        ESP_ERROR_CHECK(gpio_pulldown_dis(gpio_num));
    }
#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
    ESP_ERROR_CHECK(rtc_gpio_init(gpio_num));
#endif
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
#if SOC_PM_SUPPORT_EXT_WAKEUP
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(BIT(gpio_num), \
        gpio_wakeup_level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW));
#else
    ESP_LOGE(TAG, "Soc do not support ext1 wakeup source");
#endif
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    BaseType_t xHigherPriorityTaskWoken;
    gpio_intr_disable(gpio_num);
    ESP_ERROR_CHECK(esp_pm_lock_acquire(s_pm_cpu_lock));
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
    if (pdFALSE != xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t light_sleep_gpio_wakeup(gpio_num_t gpio_num, gpio_int_type_t intr_type, bool is_first)
{
    /* Initialize GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = false,
        .pull_up_en = true,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    /* Enable wake up from GPIO */
    gpio_wakeup_enable(gpio_num, intr_type);

    /* Make sure the GPIO is inactive and it won't trigger wakeup immediately */
    while (gpio_get_level(gpio_num) == (intr_type == GPIO_INTR_LOW_LEVEL ? 0 : 1)) {
        ESP_LOGI(TAG, "Wait to %s", intr_type == GPIO_INTR_LOW_LEVEL ? "high" : "low");
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    ESP_LOGI(TAG, "GPIO wakeup source(GPIO[%d], wakeup_level[%s]) is ready", gpio_num, intr_type == GPIO_INTR_LOW_LEVEL ? "LOW" : "HIGH");

    /* Avoid gpio dropping and causing interruption when automatically switching from active mode to sleep mode */
    /* See menuconfig: Disable all GPIO when chip at sleep */
    gpio_sleep_sel_dis(gpio_num);

    if (is_first) {
        esp_sleep_enable_gpio_wakeup();
        /* Install gpio isr service */
        gpio_install_isr_service(0);
    }
    /* Hook isr handler for specific gpio pin */
    gpio_isr_handler_add(gpio_num, gpio_isr_handler, (void*) gpio_num);

    return ESP_OK;
}

static void light_sleep_gpio_disable(gpio_num_t gpio_num)
{
    gpio_isr_handler_remove(gpio_num);
    gpio_reset_pin(gpio_num);
    gpio_uninstall_isr_service();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
}

static void gpio_task_example(void* arg)
{
    /* You can use `boot` button to wakeup system from light sleep */
    uint32_t gpio_wakeup_num = 0;

    light_sleep_gpio_wakeup(GPIO_WAKEUP_NUM, GPIO_INTR_LOW_LEVEL, true);

    /* Registering multiple GPIO wakeup sources by `gpio_wakeup_enable()` */
    // light_sleep_gpio_wakeup(2, GPIO_INTR_LOW_LEVEL, false);
    // light_sleep_gpio_wakeup(4, GPIO_INTR_LOW_LEVEL, false);
    // light_sleep_gpio_wakeup(5, GPIO_INTR_HIGH_LEVEL, false);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

    xQueueReceive(gpio_evt_queue, (void * )&gpio_wakeup_num, (TickType_t)portMAX_DELAY);

    ESP_LOGI(TAG, "GPIO[%d], val: %d", (int)gpio_wakeup_num, (int)gpio_get_level(gpio_wakeup_num));

    light_sleep_gpio_disable(gpio_wakeup_num);

    xEventGroupSetBits(wakeup_event_group, GPIO_WAKEUP_BIT);

    ESP_ERROR_CHECK(esp_pm_lock_release(s_pm_cpu_lock));

    ESP_LOGD(TAG, "Gpio wake up task has been deleted\n");
    gpio_task_handle = NULL;
    vTaskDelete(NULL);
}

void power_manage_config(int max_freq, int min_freq, bool enable)
{
#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // automatic light sleep is enabled if tickless idle support is enabled.
#if CONFIG_IDF_TARGET_ESP32
    esp_pm_config_esp32_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S2
    esp_pm_config_esp32s2_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32C3
    esp_pm_config_esp32c3_t pm_config = {
#elif CONFIG_IDF_TARGET_ESP32S3
    esp_pm_config_esp32s3_t pm_config = {
#endif
        .max_freq_mhz = max_freq,
        .min_freq_mhz = min_freq,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = enable
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif
}

static void periodic_timer_callback(void* arg)
{
    esp_pm_lock_acquire(s_pm_cpu_lock);
    xEventGroupSetBits(wakeup_event_group, TIMER_WAKEUP_BIT);
}

static void timer_wakeup(uint32_t periodic_ms, bool skip)
{
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &periodic_timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic",
        /* if true, timer will not wakeup system */
        .skip_unhandled_events = skip,
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    /* The timer has been created but is not running yet */

    /* Start the timers */
    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, periodic_ms * 1000));

    ESP_LOGI(TAG, "Timer wakeup source is ready");
}

esp_pm_lock_handle_t get_power_manage_lock_handle(void)
{
    if (s_pm_cpu_lock) {
        return s_pm_cpu_lock;
    } else {
        ESP_LOGE(TAG, "Power manage cpu lock is NULL");
        return NULL;
    }
}

// Set ARP_TMR_INTERVAL to 10000 can get lower power.
void power_manage_init(void)
{
    /* Reset the wake-up pin function to ensure the normal use of the application */
    gpio_reset_pin(GPIO_WAKEUP_NUM);
#if CONFIG_IDF_TARGET_ESP32C3
    gpio_reset_pin(GPIO_NUM_2);
#endif

#if CONFIG_PM_ENABLE && CONFIG_FREERTOS_USE_TICKLESS_IDLE
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cpu_max", &s_pm_cpu_lock));
    ESP_ERROR_CHECK(esp_pm_lock_acquire(s_pm_cpu_lock));
    wakeup_event_group = xEventGroupCreate();
    /* Call esp_pm_configure() firstly to avoid gpio level rollover */
    power_manage_config(CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ, CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ, true);
    example_register_uart_wakeup();

    /* Deinit USB_DP pin to reduce power consumption */
#if CONFIG_IDF_TARTGET_ESP32S2 || CONFIG_IDF_TARTGET_ESP32S3
    gpio_num_t usb_dp = 20;
#elif CONFIG_IDF_TARTGET_ESP32C3
    gpio_num_t usb_dp = 19;
#elif CONFIG_IDF_TARTGET_ESP32C6
    gpio_num_t usb_dp = 13;
#else
    gpio_num_t usb_dp = -1;
#endif
    if (usb_dp >= 0) {
        gpio_config_t cfg = {
            .pin_bit_mask = BIT64(usb_dp),
            .pull_down_en = true,
        };
        gpio_config(&cfg);
        gpio_sleep_sel_dis(usb_dp);
    }

    ESP_LOGI(TAG, "Power manage init ok");
#else
    ESP_LOGE(TAG, "Power management or tickless idle not enable");
#endif
}

static void light_sleep_wakeup_source_init(void)
{
    timer_wakeup(30 * 1000, false);
    xTaskCreate(gpio_task_example, "gpio_task_example", 4096, NULL, 10, &gpio_task_handle);
    vTaskDelay(10 / portTICK_RATE_MS);
}

static void light_sleep_wakeup_source_deinit(void)
{
    if (gpio_task_handle) {
        ESP_LOGD(TAG, "Gpio wakeup deinit");
        vQueueDelete(gpio_evt_queue);
        light_sleep_gpio_disable(GPIO_WAKEUP_NUM);
        vTaskDelete(gpio_task_handle);
        gpio_task_handle = NULL;
    }

    if (periodic_timer) {
        ESP_LOGD(TAG, "Timer wakeup deinit");
        esp_timer_stop(periodic_timer);
        esp_timer_delete(periodic_timer);
        periodic_timer = NULL;
    }
}

void enter_power_manage(void)
{
    ESP_LOGI(TAG, "Enter power manage");

    light_sleep_wakeup_source_init();

    while (esp_pm_lock_release(s_pm_cpu_lock) == ESP_OK) {
        vTaskDelay(50 / portTICK_RATE_MS);
    }
    xEventGroupSetBits(wakeup_event_group, ENTER_SLEEP_BIT);
    xEventGroupWaitBits(wakeup_event_group, WAKEUP_GROUP_BIT, true, false, portMAX_DELAY);

    // Exit power manage
    ESP_ERROR_CHECK(esp_pm_lock_acquire(s_pm_cpu_lock));
    xEventGroupClearBits(wakeup_event_group, ENTER_SLEEP_BIT);
    get_wakeup_cause();
    light_sleep_wakeup_source_deinit();
    vTaskDelay(10 / portTICK_RATE_MS);
}

static void register_gpio_wakeup_in_deep_sleep(gpio_num_t gpio_num, uint8_t gpio_wakeup_level)
{
#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    const gpio_config_t config = {
        .pin_bit_mask = BIT(gpio_num),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(BIT(gpio_num), \
        gpio_wakeup_level ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW));
#else
    ESP_LOGE(TAG, "Soc do not support gpio wakeup in deep sleep mode");
#endif
}

void enter_deep_sleep(esp_sleep_source_t wakeup_mode)
{
    /*
     * Reset the timer wake-up source to prevent the system from waking up by vTaskDelay() in auto light sleep.
     * Can also use esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER) to disable timer and then enable it by new time.
     */
    esp_sleep_enable_timer_wakeup(portMAX_DELAY);
    gpio_num_t deep_sleep_gpio_num = GPIO_WAKEUP_NUM;
    gpio_reset_pin(deep_sleep_gpio_num);
#if CONFIG_IDF_TARGET_ESP32
    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);
#endif

    switch (wakeup_mode) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Enable timer wakeup in deep sleep");
            esp_sleep_enable_timer_wakeup(1000 * 1000 * 5);
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Enable EXT0 wakeup in deep sleep");
            register_ext0_wakeup(deep_sleep_gpio_num, 0);
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Enable EXT1 wakeup in deep sleep");
            register_ext1_wakeup(deep_sleep_gpio_num, 0);
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Enable gpio wakeup in deep sleep");
            /* GPIO0~5 can be use to wakeup system from deep sleep for ESP32C3 */
            deep_sleep_gpio_num = GPIO_NUM_2;
            register_gpio_wakeup_in_deep_sleep(deep_sleep_gpio_num, 0);
            break;
        default:
            ESP_LOGW(TAG, "Wakeup mode not support in deep sleep");
    }
    esp_deep_sleep_start();
}
