/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_dlna.h"
#include "esp_dlna_rcs.h"
#include "esp_log.h"

static const char *TAG = "DLNA_RCS";

void esp_dlna_rcs_register_service(upnp_handle_t upnp)
{
    extern const uint8_t RenderingControl_start[]   asm("_binary_RenderingControl_xml_start");
    extern const uint8_t RenderingControl_end[]     asm("_binary_RenderingControl_xml_end");

    extern const uint8_t EventLastChange_start[]    asm("_binary_EventLastChange_xml_start");
    extern const uint8_t EventLastChange_end[]      asm("_binary_EventLastChange_xml_end");

    ESP_LOGD(TAG, "Registering RenderingControl service");
    const service_action_t rcs_action[] = {
        {
            .name = "SetMute",
            .num_attrs = 3,
            .attrs = (const upnp_attr_t[])
            {
                { .name = "Instance",  },
                { .name = "Channel",   },
                { .name = "DesiredMute", .val =  ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | RCS_SET_MUTE), },
            },
            .callback = NULL,
        },
        {
            .name = "GetMute",
            .num_attrs = 3,
            .attrs = (const upnp_attr_t[])
            {
                { .name = "Instance", },
                { .name = "Channel",  },
                { .name = "CurrentMute", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | RCS_GET_MUTE), },
            },
            .callback = NULL,
        },
        {
            .name = "SetVolume",
            .num_attrs = 3,
            .attrs = (const upnp_attr_t[])
            {
                { .name = "Instance",  },
                { .name = "Channel",   },
                { .name = "DesiredVolume", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | RCS_SET_VOL) },
            },
            .callback = NULL,
        },
        {
            .name = "GetVolume",
            .num_attrs = 3,
            .attrs = (const upnp_attr_t[])
            {
                { .name = "Instance", },
                { .name = "Channel",  },
                { .name = "CurrentVolume", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | RCS_GET_VOL) },
            },
            .callback = NULL,
        },

        { .name = NULL  },
    };

    const service_notify_t rcs_notify[] = {

        { .name = "Volume",             ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, RCS_GET_VOL)     },
        { .name = "Mute",               ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, RCS_GET_MUTE)    },
        { .name = "PresetNameList",     ONE_ATTR_CONST("val", "FactoryDefaults")                        },
        { .name = "LastChange",         ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                        },
        { .name = NULL  },
    };

    const upnp_service_item_t rcs_service = {
        .name = "RenderingControl",
        .version = "1",
        .service_desc = {
            .mime_type  = "text/xml",
            .data       = (const char *)RenderingControl_start,
            .size       = RenderingControl_end - RenderingControl_start,
        },
        .notify_template = {
            .mime_type  = "text/xml",
            .data = (const char *)EventLastChange_start,
            .size = EventLastChange_end - EventLastChange_start,
            .path = "/RCS/"
        },
    };
    esp_upnp_register_service(upnp, &rcs_service, rcs_action, rcs_notify);
}
