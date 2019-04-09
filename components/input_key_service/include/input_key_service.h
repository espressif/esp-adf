#ifndef _INPUT_KEY_SERVICE_H_
#define _INPUT_KEY_SERVICE_H_

#include "esp_peripherals.h"
#include "periph_service.h"
#include "input_key_service_default_config.h"
#include "board.h"

/**
 * @brief input key action id
 */
typedef enum {
    INPUT_KEY_SERVICE_ACTION_UNKNOWN = -1,   /*!< unknown action id */
    INPUT_KEY_SERVICE_ACTION_CLICK,          /*!< click action id */
    INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE,  /*!< click release action id */
    INPUT_KEY_SERVICE_ACTION_PRESS,          /*!< long press action id */
    INPUT_KEY_SERVICE_ACTION_PRESS_RELEASE   /*!< long press release id */
} input_key_service_action_id_t;

/**
 *@breif input key's infomation
 */
typedef struct {
    esp_periph_id_t  type;             /*!< ID of peripherals */
    int              user_id;          /*!< The key's user id */
    int              act_id;           /*!< The key's action id */
} input_key_service_info_t;

/**
 * @brief Initialize and start the input key service
 *
 * @param periph_set_handle The handler of esp_peripheral set
 *
 * @return NULL    failed
 *         others  input key service handle
 */

periph_service_handle_t input_key_service_create(esp_periph_set_handle_t periph_set_handle);

/**
 * @brief Get the state of input key service
 *
 * @param input_handle The handle of input key service
 *
 * @return state of input key service
 */
periph_service_state_t get_input_key_service_state(periph_service_handle_t input_handle);

/**
 * @brief Add input key's information to service list
 *
 * @param input_key_handle  handle of service
 * @param input_key_info    input key's information
 * @param add_key_num       number of keys
 *
 * @return ESP_OK   success
 *         ESP_FAIL failed
 */
esp_err_t input_key_service_add_key(periph_service_handle_t input_key_handle, input_key_service_info_t *input_key_info, int add_key_num);

#endif
