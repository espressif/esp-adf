
/* Data queue utility to buffer data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _RECORD_SRC_CFG_H_
#define _RECORD_SRC_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Following section configure for USB camera
 */
#define USB_CAM_PIN_PWDN                        GPIO_NUM_48

/* USB Camera Descriptors Related MACROS,
the quick demo skip the standred get descriptors process,
users need to get params from camera descriptors from PC side,
eg. run `lsusb -v` in linux,
then hardcode the related MACROS below
*/
#define DESCRIPTOR_CONFIGURATION_INDEX          1
#define DESCRIPTOR_FORMAT_MJPEG_INDEX           2
#define DESCRIPTOR_FRAME_640_480_INDEX          1
#define DESCRIPTOR_FRAME_480_320_INDEX          2
#define DESCRIPTOR_FRAME_352_288_INDEX          3
#define DESCRIPTOR_FRAME_320_240_INDEX          4

#define DESCRIPTOR_STREAM_INTERFACE_INDEX       1
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_128 1
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_256 2
#define DESCRIPTOR_STREAM_INTERFACE_ALT_MPS_512 3
#define DESCRIPTOR_STREAM_ISOC_ENDPOINT_ADDR    0x81

#define USB_CAM_ISOC_EP_MPS                     512

#ifdef __cplusplus
}
#endif

#endif
