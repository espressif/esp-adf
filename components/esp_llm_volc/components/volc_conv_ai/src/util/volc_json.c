// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "volc_osal.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

static int _get_array_index(char *fmt)
{
  char *start, *end;
  int num;
  start = strchr(fmt, '[');
  if (NULL != start) {
    end = strchr(fmt, ']');
    if (NULL != end) {
      *start = *end = '\0';
      start++;
      num = atoi(start);
      if (num >= 0) {
        return num;
      }
    }
  }
  return -1;
}

static cJSON *_read_item_object(cJSON *root, const char *fmt)
{
  cJSON *parent = root;
  cJSON *child = NULL;
  int fmt_size = strlen(fmt) + 1;
  char *buf = (char *)volc_osal_malloc(fmt_size);
  char *buf_p = buf;
  int offset = 0;
  int arr_index;
  int i = 0;
  if (NULL == buf) {
    LOGE("malloc failed !");
    return NULL;
  }
  /* 1. split */
  memcpy(buf, fmt, fmt_size);
  for (; i < fmt_size; i++) {
    if ('.' == buf[i]) {
      buf[i] = '\0';
    }
  }
  /* 2. parse */
  while (fmt_size > 0) {
    offset = strlen(buf) + 1;
    arr_index = _get_array_index(buf);
    child = cJSON_GetObjectItem(parent, buf);
    if (NULL == child) {
      goto L_ERROR;
    }
    if (arr_index >= 0) {
      child = cJSON_GetArrayItem(child, arr_index);
      if (NULL == child) {
        goto L_ERROR;
      }
    }
    parent = child;
    buf = buf + offset;
    fmt_size -= offset;
  }

L_ERROR:
  HAL_SAFE_FREE(buf_p);
  return child;
}

static int _read_item_int(cJSON *root, const char *fmt, int *dst)
{
  if (NULL == root || NULL == fmt) {
    LOGW("invalid input root %p fmt %p", root, fmt);
    return -1;
  }
  cJSON *obj = _read_item_object(root, fmt);
  if (!cJSON_IsNumber(obj)) {
    LOGD("the value of the key(%s) is not the INT type", fmt);
    return -1;
  }
  if (NULL != dst) {
    *dst = (int)cJSON_GetNumberValue(obj);
  }
  return 0;
}

static int _read_item_double(cJSON *root, const char *fmt, double *dst)
{
  if (NULL == root || NULL == fmt) {
    LOGW("invalid input root %p fmt %p", root, fmt);
    return -1;
  }
  cJSON *obj = _read_item_object(root, fmt);
  if (!cJSON_IsNumber(obj)) {
    LOGD("the value of the key(%s) is not the DOUBLE type", fmt);
    return -1;
  }
  if (NULL != dst) {
    *dst = obj->valuedouble;
  }
  return 0;
}

static int _read_item_string(cJSON *root, const char *fmt, char **dst)
{
  if (NULL == root || NULL == fmt) {
    LOGW("invalid input root %p fmt %p", root, fmt);
    return -1;
  }
  cJSON *obj = _read_item_object(root, fmt);
  if (!cJSON_IsString(obj)) {
    LOGD("the value of the key(%s) is not the STRING type", fmt);
    return -1;
  }
  if (NULL != dst) {
    *dst = (char *)volc_osal_malloc(strlen(obj->valuestring) + 1);
    if (NULL == *dst) {
      LOGE("memory alloc failed");
      return -1;
    }
    strncpy(*dst, obj->valuestring, strlen(obj->valuestring));
    (*dst)[strlen(obj->valuestring)] = '\0';
  }
  return 0;
}

static int _read_item_bool(cJSON *root, const char *fmt, bool *dst)
{
  if (!root || !fmt) {
    LOGW("invalid arguments, root: %p, fmt: %p", root, fmt);
    return -1;
  }
  cJSON *obj = _read_item_object(root, fmt);
  if (!cJSON_IsBool(obj)) {
    LOGW("the value of the key(%s) is not the BOOL type", fmt);
    return -1;
  }
  if (NULL != dst) {
    *dst = cJSON_IsTrue(obj) ? true: false;
  }
  return 0;
}

int volc_json_read_int(cJSON *root, const char *fmt, int *dst) {
  return _read_item_int(root, fmt, dst);
}

int volc_json_read_double(cJSON *root, const char *fmt, double *dst) {
  return _read_item_double(root, fmt, dst);
}

int volc_json_read_string(cJSON *root, const char *fmt, char **dst) {
  return _read_item_string(root, fmt, dst);
}

int volc_json_read_bool(cJSON *root, const char *fmt, bool *dst)
{
  return _read_item_bool(root, fmt, dst);
}

int volc_json_read_object(cJSON *root, const char *fmt, cJSON **dst) {
  cJSON *obj;
  if (NULL == root || NULL == fmt) {
    LOGW("invalid input root %p fmt %p", root, fmt);
    return -1;
  }
  obj = _read_item_object(root, fmt);
  if (NULL == obj) {
    LOGD("parse error, fmt=%s", fmt);
    return -1;
  }
  if (NULL != dst) {
    *dst = cJSON_Duplicate(obj, 1);
    if (NULL == *dst) {
      LOGE("memory alloc failed");
      return -1;
    }
  }
  return 0;
}

int volc_json_check_int(cJSON *root, const char *fmt)
{
  return _read_item_int(root, fmt, NULL);
}

int volc_json_check_double(cJSON *root, const char *fmt)
{
  return _read_item_double(root, fmt, NULL);
}

int volc_json_check_string(cJSON *root, const char *fmt)
{
  return _read_item_string(root, fmt, NULL);
}

int volc_json_check_bool(cJSON *root, const char *fmt)
{
  return _read_item_bool(root, fmt, NULL);
}
