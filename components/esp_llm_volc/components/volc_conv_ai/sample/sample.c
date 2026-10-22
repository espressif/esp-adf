#include "volc_conv_ai.h"
#include <stdio.h>
#include <cJSON.h>
#include <string.h>

#define CONFIG_VOLC_BOT_ID "xxxx"
#define CONFIG_VOLC_INSTANCE_ID "xxxx"
#define CONFIG_VOLC_PRODUCT_KEY "xxxx"
#define CONFIG_VOLC_PRODUCT_SECRET "xxxx"
#define CONFIG_VOLC_DEVICE_NAME "xxxx"

#define CONV_AI_CONFIG_FORMAT "{\
  \"ver\": 1,\
  \"iot\": {\
    \"instance_id\": \"%s\",\
    \"product_key\": \"%s\",\
    \"product_secret\": \"%s\",\
    \"device_name\": \"%s\"\
  },\
  \"rtc\": {\
    \"log_level\": 3,\
    \"audio\": {\
      \"publish\": true,\
      \"subscribe\": true,\
      \"codec\": 3\
    },\
    \"video\": {\
      \"publish\": false,\
      \"subscribe\": false,\
      \"codec\": 1\
    },\
  \"params\":[\
      \"{\\\"debug\\\":{\\\"log_to_console\\\":1}}\",\
      \"{\\\"rtc\\\":{\\\"access\\\":{\\\"concurrent_requests\\\":1}}}\",\
      \"{\\\"rtc\\\":{\\\"ice\\\":{\\\"concurrent_agents\\\":1}}}\",\
      \"{\\\"audio\\\":{\\\"codec\\\":{\\\"pcma\\\":{\\\"s_samples_per_frame\\\":480}}}}\"\
    ]\
  }\
}"

volc_engine_t engine;

static void __on_volc_event(volc_engine_t handle, volc_event_t *event, void *user_data)
{
    switch (event->code)
    {
    case VOLC_EV_CONNECTED:
        printf("Volc Engine connected \n");
        break;
    case VOLC_EV_DISCONNECTED:
        printf( "Volc Engine disconnected \n");
        break;
    case VOLC_EV_QUOTA_EXCEEDED:
        printf("Volc Engine quota exceeded\n");
    default:
        printf("Volc Engine event: %d \n", event->code);
        break;
    }
}

static void __on_volc_audio_data(volc_engine_t handle, const void *data_ptr, size_t data_len, volc_audio_frame_info_t *info_ptr, void *user_data)
{
    printf("__on_volc_audio_data \n");
    // play_audio_data(player_handle, data_ptr, data_len);
}

static void __on_subtitle_message_received(const cJSON* root) {
    /*
        {
            "data" :
            [
                {
                    "definite" : false,
                    "language" : "zh",
                    "mode" : 1,
                    "paragraph" : false,
                    "sequence" : 0,
                    "text" : "\\u4f60\\u597d",
                    "userId" : "voiceChat_xxxxx"
                }
            ],
            "type" : "subtitle"
        }
    */
    cJSON * type_obj = cJSON_GetObjectItem(root, "type");
    if (type_obj != NULL && strcmp("subtitle", cJSON_GetStringValue(type_obj)) == 0) {
        cJSON* data_obj_arr = cJSON_GetObjectItem(root, "data");
        cJSON* obji = NULL;
        cJSON_ArrayForEach(obji, data_obj_arr) {
            cJSON* user_id_obj = cJSON_GetObjectItem(obji, "userId");
            cJSON* text_obj = cJSON_GetObjectItem(obji, "text");
            cJSON* definite_obj = cJSON_GetObjectItem(obji, "definite");
            char* sub = cJSON_GetStringValue(text_obj);

            // print_string(sub + sub_offset -1);
            if(cJSON_IsTrue(definite_obj)){
               printf(" %s \n",sub);
            }
        }
    }
}

static void __on_volc_message_data(volc_engine_t handle, const void *message, size_t size, volc_message_info_t *info_ptr, void *user_data)
{
    static char message_buffer[4096];
     if (size > 8 && size < 4096) {
        memcpy(message_buffer, message, size);
        message_buffer[size] = 0;
        message_buffer[size + 1] = 0;
        cJSON *root = cJSON_Parse(message_buffer + 8);
        if (root != NULL) {
            if (message_buffer[0] == 's' && message_buffer[1] == 'u' && message_buffer[2] == 'b' && message_buffer[3] == 'v') {
                __on_subtitle_message_received(root);
            }
            cJSON_Delete(root);
        }
      }
}

int main(){
    static char config_buf[1024] = {0};

    snprintf(config_buf, sizeof(config_buf), CONV_AI_CONFIG_FORMAT,
             CONFIG_VOLC_INSTANCE_ID,
             CONFIG_VOLC_PRODUCT_KEY,
             CONFIG_VOLC_PRODUCT_SECRET,
             CONFIG_VOLC_DEVICE_NAME);

    volc_event_handler_t volc_event_handler = (volc_event_handler_t){
        .on_volc_event = __on_volc_event,
        .on_volc_audio_data = __on_volc_audio_data,
        .on_volc_message_data = __on_volc_message_data,
    };

    int error = volc_create(&engine, config_buf, &volc_event_handler, NULL);

    volc_opt_t opt = {
        .mode = VOLC_MODE_RTC,
        .bot_id = CONFIG_VOLC_BOT_ID,
    };

    error = volc_start(engine, &opt);

   for(int i = 0;i<5;i++){
        //  capture_audio_data();
        //   volc_send_audio_data(engine, data, len, &info);
        sleep(5);
        volc_send_text_to_agent(engine, "火山引擎测试", VOLC_AGENT_TYPE_TTS, 1);
    }
    volc_stop(engine);
    volc_destroy(engine);
    return 0;
}