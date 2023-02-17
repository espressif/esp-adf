/* RTMP source application to fetch data from server and callback in FLV container

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _RTMP_SRC_APP_H
#define _RTMP_SRC_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief        Run RTMP source application
 * @param        url: RTMP server url pattern rtmp://ipaddress:port/app_name/stream_name
 * @param        duration: Total run duration for RTMP source (unit ms)
 * @return       - 0: On success
 *               - Others: Fail to run
 */
int rtmp_src_app_run(char *url, uint32_t duration);

#ifdef __cplusplus
}
#endif

#endif
