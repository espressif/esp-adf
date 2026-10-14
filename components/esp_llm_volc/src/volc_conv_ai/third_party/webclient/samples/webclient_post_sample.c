/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-08-03    chenyong      the first version
 */

#include <string.h>

#include "agora_log.h"
#define TAG "webclient_postsample"

#include <webclient.h>

#define POST_RESP_BUFSZ 1024
#define POST_HEADER_BUFSZ 1024

#define POST_LOCAL_URI "https://www.rt-thread.com/service/echo"

const char *post_data = "RT-Thread is an open source IoT operating system from China!";

/* send HTTP POST request by common request interface, it used to receive longer data */
static int webclient_post_comm(const char *uri, const void *post_data, size_t data_len)
{
  struct webclient_session *session = NULL;
  unsigned char *buffer = NULL;
  int ret = 0;
  int bytes_read, resp_status;

  buffer = (unsigned char *)calloc(POST_RESP_BUFSZ, sizeof(unsigned char));
  if (buffer == NULL) {
    AGO_LOGD(TAG, "no memory for receive response buffer.");
    ret = -1;
    goto __exit;
  }

  /* create webclient session and set header response size */
  session = webclient_session_create(POST_HEADER_BUFSZ);
  if (session == NULL) {
    ret = -1;
    goto __exit;
  }

  /* build header for upload */
  webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));
  webclient_header_fields_add(session, "Content-Type: application/octet-stream\r\n");

  /* send POST request by default header */
  if ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200) {
    AGO_LOGE(TAG, "webclient POST request failed, response(%d) error.", resp_status);
    ret = -RT_ERROR;
    goto __exit;
  }

  AGO_LOGD(TAG, "webclient post response data: ");
  do {
    bytes_read = webclient_read(session, buffer, POST_RESP_BUFSZ);
    if (bytes_read <= 0) {
      break;
    }
    AGO_LOGD(TAG, "%s", buffer);
  } while (1);

__exit:
  if (session) {
    webclient_close(session);
  }
  if (buffer) {
    free(buffer);
  }

  return ret;
}

/* send HTTP POST request by simplify request interface, it used to received shorter data */
static int webclient_post_smpl(const char *uri, const char *post_data, size_t data_len)
{
  char *response = NULL;
  char *header = NULL;
  size_t resp_len = 0;

  webclient_request_header_add(&header, "Content-Length: %d\r\n", strlen(post_data));
  webclient_request_header_add(&header, "Content-Type: application/octet-stream\r\n");

  if (webclient_request(uri, header, post_data, data_len, (void **)&response, &resp_len) < 0) {
    AGO_LOGE(TAG, "webclient send post request failed.");
    free(header);
    return -RT_ERROR;
  }

  AGO_LOGD(TAG, "webclient send post request by simplify request interface.");
  AGO_LOGD(TAG, "webclient post response data: ");
  AGO_LOGD(TAG, "%s", response);
  AGO_LOGD(TAG, "\n");

  if (header) {
    free(header);
  }
  if (response) {
    free(response);
  }

  return 0;
}

int main(int argc, char **argv)
{
  char *uri = NULL;

  if (argc == 1) {
    uri = strdup(POST_LOCAL_URI);
    if (uri == NULL) {
      AGO_LOGD(TAG, "no memory for create post request uri buffer.");
      return -1;
    }

    webclient_post_comm(uri, (void *)post_data, strlen(post_data));
  } else if (argc == 2) {
    if (strcmp(argv[1], "-s") == 0) {
      uri = strdup(POST_LOCAL_URI);
      if (uri == NULL) {
        AGO_LOGD(TAG, "no memory for create post request uri buffer.");
        return -1;
      }

      webclient_post_smpl(uri, (void *)post_data, strlen(post_data));
    } else {
      uri = strdup(argv[1]);
      if (uri == NULL) {
        AGO_LOGD(TAG, "no memory for create post request uri buffer.");
        return -1;
      }
      webclient_post_comm(uri, (void *)post_data, strlen(post_data));
    }
  } else if (argc == 3 && strcmp(argv[1], "-s") == 0) {
    uri = strdup(argv[2]);
    if (uri == NULL) {
      AGO_LOGD(TAG, "no memory for create post request uri buffer.");
      return -1;
    }

    webclient_post_smpl(uri, (void *)post_data, strlen(post_data));
  } else {
    AGO_LOGD(TAG, "web_post_test [uri]     - webclient post request test.");
    AGO_LOGD(TAG, "web_post_test -s [uri]  - webclient simplify post request test.");
    return -1;
  }

  if (uri) {
    free(uri);
  }

  return 0;
}
