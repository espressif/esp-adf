#ifndef _BOARD_PINS_CONFIG_H_
#define _BOARD_PINS_CONFIG_H_

#include <string.h>
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"

#define SDCARD_OPEN_FILE_NUM_MAX  5
#define SDCARD_INTR_GPIO          GPIO_NUM_34

#define BUTTON_REC_ID             GPIO_NUM_36
#define BUTTON_MODE_ID            GPIO_NUM_39
#define BUTTON_SET_ID             TOUCH_PAD_NUM9
#define BUTTON_PLAY_ID            TOUCH_PAD_NUM8
#define BUTTON_VOLUP_ID           TOUCH_PAD_NUM7
#define BUTTON_VOLDOWN_ID         TOUCH_PAD_NUM4

#define AUXIN_DETECT_GPIO         GPIO_NUM_12
#define PA_ENABLE_GPIO            GPIO_NUM_21

#define GREEN_LED_GPIO            GPIO_NUM_22
#define RED_LED_GPIO              GPIO_NUM_19

#define BOARD_PARAMETER_CHECK(TAG, a, action) if(!(a)) { \
        ESP_LOGE(TAG, "LINE: %d, (%s): %s", __LINE__, __FUNCTION__, "Invalid parameter"); \
        action; \
    }

/**
 * @brief                  Get i2c pins configuration
 *
 * @param      port        i2c port number to get configuration
 * @param[in]  i2c_config  i2c configuration parameters
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config);

/**
 * @brief                  Get i2s pins configuration
 *
 * @param      port        i2s port number to get configuration
 * @param[in]  i2s_config  i2s configuration parameters
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t get_i2s_pins(i2s_port_t port, i2s_pin_config_t *i2s_config);

/**
 * @brief                  Get spi pins configuration
 *
 * @param[in]      spi_config               spi bus configuration parameters
 * @param[in]  spi_device_interface_config  spi device configuration parameters
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t get_spi_pins(spi_bus_config_t *spi_config, spi_device_interface_config_t *spi_device_interface_config);

/**
 * @brief  Get the gpio number for sdcard interrupt
 *
 * @return  -1      non-existent
 *          Others  sdcard interrupt gpio number
 */
int8_t get_sdcard_intr_gpio(void);

/**
 * @brief  Get sdcard maximum number of open files
 *
 * @return  -1      error
 *          Others  max num
 */
int8_t get_sdcard_open_file_num_max(void);

/**
 * @brief  Get the gpio number for auxin detection
 *
 * @return  -1      non-existent
 *          Others  gpio number
 */
int8_t get_auxin_detect_gpio(void);

/**
 * @brief  Get the gpio number for PA enable
 *
 * @return  -1      non-existent
 *          Others  gpio number
 */
int8_t get_pa_enable_gpio(void);

/**
 * @brief  Get the gpio number for record-button
 *
 * @return  -1      non-existent
 *          Others  gpio number
 */
int8_t get_input_rec_id(void);

/**
 * @brief  Get the gpio number for mode-button
 *
 * @return  -1      non-existent
 *          Others  gpio number
 */
int8_t get_input_mode_id(void);

/**
 * @brief Get touchpad number for set function
 *
 * @return -1       non-existent
 *         Others   touchpad number
 */
int8_t get_input_set_id(void);

/**
 * @brief Get touchpad number for play function
 *
 * @return -1       non-existent
 *         Others   touchpad number
 */
int8_t get_input_play_id(void);

/**
 * @brief touchpad number for volume up function
 *
 * @return -1       non-existent
 *         Others   touchpad number
 */
int8_t get_input_volup_id(void);

/**
 * @brief Get touchpad number for volume down function
 *
 * @return -1       non-existent
 *         Others   touchpad number
 */
int8_t get_input_voldown_id(void);

/**
 * @brief Get red led gpio number
 *
 * @return -1       non-existent
 *         Others   gpio number
 */
int8_t get_red_led_gpio(void);

/**
 * @brief Get green led gpio number
 *
 * @return -1       non-existent
 *         Others   gpio number
 */
int8_t get_green_led_gpio(void);

#endif
