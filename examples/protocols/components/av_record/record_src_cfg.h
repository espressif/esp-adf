
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
#define USB_CAM_PIN_PWDN     CONFIG_USB_CAMERA_POWER_GPIO
#define USB_CAM_FRAME_BUFFER_SIZE  CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE
/* USB Camera Descriptors Related MACROS,
the quick demo skip the standred get descriptors process,
users need to get params from camera descriptors from PC side,
eg. run `lsusb -v` in linux,
then hardcode the related MACROS below
*/

#ifdef __cplusplus
}
#endif

#endif
