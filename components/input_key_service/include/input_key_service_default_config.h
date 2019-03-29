#ifndef _INPUT_KEY_SERVICE_CONFIG_H_
#define _INPUT_KEY_SERVICE_CONFIG_H_

/**
 * @brief input key user user-defined id
 */
typedef enum {
    USER_ID_UNKNOWN = -1,  /*!< unknown user id */
    USER_ID_REC,           /*!< user id for recording */
    USER_ID_SET,           /*!< user id for settings */
    USER_ID_PLAY,          /*!< user id for playing */
    USER_ID_MODE,          /*!< user id for mode */
    USER_ID_VOLDOWN,       /*!< user id for volume down */
    USER_ID_VOLUP,         /*!< user id for volume up */
} input_key_user_id_t;

#if (CONFIG_ESP_LYRAT_V4_3_BOARD || CONFIG_ESP_LYRAT_V4_2_BOARD)

#define INPUT_KEY_NUM     6

#define INPUT_REC_ID       GPIO_NUM_36
#define INPUT_MODE_ID      GPIO_NUM_39

#define INPUT_SET_ID       TOUCH_PAD_NUM9
#define INPUT_PLAY_ID      TOUCH_PAD_NUM8
#define INPUT_VOL_UP_ID    TOUCH_PAD_NUM7
#define INPUT_VOL_DOWN_ID  TOUCH_PAD_NUM4

#define INPUT_KEY_DEFAULT_INFO() {      \
     {                                  \
        .type = PERIPH_ID_BUTTON,       \
        .user_id = USER_ID_REC,         \
        .act_id = INPUT_REC_ID,         \
    },                                  \
    {                                   \
        .type = PERIPH_ID_BUTTON,       \
        .user_id = USER_ID_MODE,        \
        .act_id = INPUT_MODE_ID,        \
    },                                  \
    {                                   \
        .type = PERIPH_ID_TOUCH,        \
        .user_id = USER_ID_SET,         \
        .act_id = INPUT_SET_ID,         \
    },                                  \
    {                                   \
        .type = PERIPH_ID_TOUCH,        \
        .user_id = USER_ID_PLAY,        \
        .act_id = INPUT_PLAY_ID,        \
    },                                  \
    {                                   \
        .type = PERIPH_ID_TOUCH,        \
        .user_id = USER_ID_VOLUP,       \
        .act_id = INPUT_VOL_UP_ID,      \
    },                                  \
    {                                   \
        .type = PERIPH_ID_TOUCH,        \
        .user_id = USER_ID_VOLDOWN,     \
        .act_id = INPUT_VOL_DOWN_ID,    \
    }                                   \
}

#elif (CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)

#define INPUT_KEY_NUM  6

#define INPUT_SET_ID             0
#define INPUT_PLAY_ID            1
#define INPUT_REC_ID             2
#define INPUT_MODE_ID            3
#define INPUT_VOLDOWN_ID         4
#define INPUT_VOLUP_ID           5

#define INPUT_KEY_DEFAULT_INFO() {      \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_REC,         \
        .act_id = INPUT_REC_ID,         \
    },                                  \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_MODE,        \
        .act_id = INPUT_MODE_ID,        \
    },                                  \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_SET,         \
        .act_id = INPUT_SET_ID,         \
    },                                  \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_PLAY,        \
        .act_id = INPUT_PLAY_ID,        \
    },                                  \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_VOLUP,       \
        .act_id = INPUT_VOLUP_ID,       \
    },                                  \
    {                                   \
        .type = PERIPH_ID_ADC_BTN,      \
        .user_id = USER_ID_VOLDOWN,     \
        .act_id = INPUT_VOLDOWN_ID,     \
    }                                   \
}

#endif

#endif
