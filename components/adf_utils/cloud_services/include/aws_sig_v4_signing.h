/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#ifndef _AWS_SIG_V4_SIGNING_H_
#define _AWS_SIG_V4_SIGNING_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Get Amazon Polly Signature V4 signing HTTP header
 *
 * @param[in]  payload       Request data
 * @param[in]  region_name   The region name
 * @param[in]  content_type  The content type
 * @param[in]  secret_key    The secret key
 * @param[in]  access_key    The access key
 * @param[in]  amz_date      The amz_date, full timestamp, format as `%Y%m%dT%H%M%SZ`
 * @param[in]  date_stamp    The date_stamp, only date data format as `%Y%m%d`
 *
 * @return     HTTP authentication header
 */
char *aws_polly_authentication_header(const char *payload,
                                      const char *region_name,
                                      const char *content_type,
                                      const char *secret_key,
                                      const char *access_key,
                                      const char *amz_date,
                                      const char *date_stamp);

#ifdef __cplusplus
}
#endif

#endif
