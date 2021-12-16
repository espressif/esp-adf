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
#include "esp_dlna_avt.h"
#include "esp_log.h"
#include <sys/time.h>
static const char *TAG = "DLNA_AVT";

void esp_dlna_avt_register_service(upnp_handle_t upnp)
{
    extern const uint8_t AVTransport_start[]        asm("_binary_AVTransport_xml_start");
    extern const uint8_t AVTransport_end[]          asm("_binary_AVTransport_xml_end");

    extern const uint8_t EventLastChange_start[]    asm("_binary_EventLastChange_xml_start");
    extern const uint8_t EventLastChange_end[]      asm("_binary_EventLastChange_xml_end");

    ESP_LOGD(TAG, "Registering AVTransport service");
    const service_action_t avt_action[] = {
        {
            .name = "GetTransportInfo",
            .num_attrs = 3,
            .attrs = (const upnp_attr_t[])
            {
                { .name = "CurrentTransportState",  .val = ATTR_CB(esp_dlna_upnp_attr_cb),  .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRANS_STATE)   },
                { .name = "CurrentTransportStatus", .val = ATTR_CB(esp_dlna_upnp_attr_cb),  .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRANS_STATUS)  },
                { .name = "CurrentSpeed",           .val = ATTR_CB(esp_dlna_upnp_attr_cb),  .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_PLAY_SPEED)    },
            },
            .callback = NULL
        },
        {
            .name = "GetMediaInfo",
            .num_attrs = 9,
            .attrs = (upnp_attr_t[])
            {
                {   /* 1 */ .name = "NrTracks",           .val = (void *)1,                      .type = (ATTR_TYPE_INT | ATTR_RESPONSE | ATTR_CONST )            },
                {   /* 2 */ .name = "CurrentURI",         .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_URI)      },
                {   /* 3 */ .name = "MediaDuration",      .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_MEDIA_DURATION) },
                {   /* 4 */ .name = "CurrentURIMetaData", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_METADATA) },
                {   /* 5 */ .name = "NextURI",            .val = CONST_STR(""),                  .type = (ATTR_TYPE_STR | ATTR_RESPONSE | ATTR_CONST)             },
                {   /* 6 */ .name = "NextURIMetaData",    .val = CONST_STR(""),                  .type = (ATTR_TYPE_STR | ATTR_RESPONSE | ATTR_CONST)             },
                {   /* 7 */ .name = "PlayMedium",         .val = CONST_STR("NONE"),              .type = (ATTR_TYPE_STR | ATTR_RESPONSE | ATTR_CONST)             },
                {   /* 8 */ .name = "WriteStatus",        .val = CONST_STR("NOT_IMPLEMENTED"),   .type = (ATTR_TYPE_STR | ATTR_RESPONSE | ATTR_CONST)             },
                {   /* 9 */ .name = "RecordMedium",       .val = CONST_STR("NOT_IMPLEMENTED"),   .type = (ATTR_TYPE_STR | ATTR_RESPONSE | ATTR_CONST)             },
            },
            .callback = NULL,
        },
        {
            .name = "GetPositionInfo",
            .num_attrs = 8,
            .attrs = (upnp_attr_t[])
            {
                {   /* 1 */ .name = "Track",         .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_NO)        },
                {   /* 2 */ .name = "TrackDuration", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_DURATION), },
                {   /* 3 */ .name = "TrackMetaData", .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_METADATA)  },
                {   /* 4 */ .name = "TrackURI",      .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE | AVT_GET_TRACK_URI)       },
                {   /* 5 */ .name = "RelTime",       .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE)| AVT_GET_POS_RELTIME      },
                {   /* 6 */ .name = "AbsTime",       .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE)| AVT_GET_POS_ABSTIME      },
                {   /* 7 */ .name = "RelCount",      .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE)| AVT_GET_POS_RELCOUNT     },
                {   /* 8 */ .name = "AbsCount",      .val = ATTR_CB(esp_dlna_upnp_attr_cb), .type = (ATTR_CALLBACK | ATTR_RESPONSE)| AVT_GET_POS_ABSCOUNT     },
            },
            .callback = NULL,
        },
        { .name = "SetAVTransportURI",      ONE_ATTR_CB_TYPE("CurrentURI", esp_dlna_upnp_attr_cb, AVT_SET_TRACK_URI), .callback = NULL },
        { .name = "Play",                   ONE_ATTR_CB_TYPE("Speed",      esp_dlna_upnp_attr_cb, AVT_PLAY),          .callback = NULL },
        { .name = "Stop",                   ONE_ATTR_CB_TYPE("InstanceID", esp_dlna_upnp_attr_cb, AVT_STOP),          .callback = NULL },
        { .name = "Pause",                  ONE_ATTR_CB_TYPE("InstanceID", esp_dlna_upnp_attr_cb, AVT_PAUSE),         .callback = NULL },
        { .name = "Next",                   ONE_ATTR_CB_TYPE("InstanceID", esp_dlna_upnp_attr_cb, AVT_NEXT),          .callback = NULL },
        { .name = "Previous",               ONE_ATTR_CB_TYPE("InstanceID", esp_dlna_upnp_attr_cb, AVT_PREV),          .callback = NULL },
        { .name = "Seek",                   ONE_ATTR_CB_TYPE("Target", esp_dlna_upnp_attr_cb, AVT_SEEK),              .callback = NULL },
        { .name = "GetDeviceCapabilities",  .callback = NULL },
        { .name = "GetTransportSettings",   .callback = NULL },
        { .name = "SetPlayMode",            .callback = NULL },

        { .name = NULL, /* End marker */ },
    };

    const service_notify_t avt_notify[] = {
        { .name = "TransportState",                 ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRANS_STATE)      },
        { .name = "CurrentTransportActions",        ONE_ATTR_CONST("val", "Play,Pause,Stop,Seek,Next,Previous")              },
        { .name = "TransportStatus",                ONE_ATTR_CONST("val", "OK")                                              },
        { .name = "CurrentPlayMode",                ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_PLAY_MODE)        },
        { .name = "CurrentTrackDuration",           ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRACK_DURATION)   },
        // { .name = "CurrentTrackURI",                ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRACK_URI)        },
        // { .name = "CurrentMediaDuration",           ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_MEDIA_DURATION)   },
        // { .name = "AVTransportURI",                 ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRACK_URI)        },
        // { .name = "CurrentTrackMetaData",           ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRACK_METADATA)   },
        // { .name = "RelativeTimePosition",           ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_POS_RELTIME)      },
        // { .name = "AbsoluteTimePosition",           ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_POS_ABSTIME)      },
        // { .name = "RelativeCounterPosition",        ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_POS_RELCOUNT)     },
        // { .name = "AbsoluteCounterPosition",        ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_POS_ABSCOUNT)     },
        // { .name = "CurrentTrack",                   ONE_ATTR_CB_TYPE("val", esp_dlna_upnp_attr_cb, AVT_GET_TRACK_NO)         },
        // { .name = "LastChange",                     ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "NextAVTransportURI",             ONE_ATTR_CONST("val", "")                                                },
        // { .name = "CurrentRecordQualityMode",       ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "PossibleRecordQualityModes",     ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "NextAVTransportURIMetaData",     ONE_ATTR_CONST("val", "")                                                },
        // { .name = "PlaybackStorageMedium",          ONE_ATTR_CONST("val", "NONE")                                            },
        // { .name = "RecordMediumWriteStatus",        ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "RecordStorageMedium",            ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "PossiblePlaybackStorageMedia",   ONE_ATTR_CONST("val", "NONE,NETWORK,HDD,CD-DA,UNKNOWN")                  },
        // { .name = "AVTransportURIMetaData",         ONE_ATTR_CONST("val", "")                                                },
        // { .name = "NumberOfTracks",                 ONE_ATTR_CONST("val", "0")                                               },
        // { .name = "PossibleRecordStorageMedia",     ONE_ATTR_CONST("val", "NOT_IMPLEMENTED")                                 },
        // { .name = "TransportPlaySpeed",             ONE_ATTR_CONST("val", "1")                                               },
        { .name = NULL,  /* End marker */                                                                                    },
    };

    const upnp_service_item_t avt_service = {
        .name = "AVTransport",
        .version = "1",
        .service_desc = {
            .mime_type  = "text/xml",
            .data       = (const char *)AVTransport_start,
            .size       = AVTransport_end - AVTransport_start,
        },
        .notify_template = {
            .mime_type  = "text/xml",
            .data = (const char *)EventLastChange_start,
            .size = EventLastChange_end - EventLastChange_start,
            .path = "/AVT/"

        },
    };
    esp_upnp_register_service(upnp, &avt_service, avt_action, avt_notify);
}