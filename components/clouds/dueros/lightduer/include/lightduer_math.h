/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: Su Hao (suhao@baidu.com)
//
// Description: Encode & decode the int8 into base16/32/64.

#ifndef BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_MATH_H
#define BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_MATH_H

#include "lightduer_types.h"

#if defined(FIXED_POINT)
#if (FIXED_POINT == 16)
#define DUER_FIXED_POINT    (FIXED_POINT)
#else
#warning "Not supported FIXED_POINT bits!!!"
#endif
#endif

#define DUER_QCONST_BITS(_x, _bits)     (int)(.1 + (_x) * (((int)1) << ((_bits) - 1)))
#define DUER_FCONST_BITS(_x, _bits)     (double)(1. * (_x) / (((int)1) << ((_bits) - 1)))

#if defined(DUER_FIXED_POINT) && (DUER_FIXED_POINT > 0)

typedef int duer_coef_t;
typedef long long duer_large_coef_t;

#define DUER_INT_MIN            (-1 << (DUER_FIXED_POINT - 1))
#define DUER_INT_MAX            (~(-1 << DUER_FIXED_POINT))

#define DUER_QCONST(_x)         DUER_QCONST_BITS(_x, DUER_FIXED_POINT)
#define DUER_FCONST(_x)         DUER_FCONST_BITS(_x, DUER_FIXED_POINT)
#define DUER_COEF_MUL(_x, _y)   (duer_coef_t)(((((duer_large_coef_t)(_x)) * ((duer_large_coef_t)(_y))) + (1 << (DUER_FIXED_POINT - 2))) >> (DUER_FIXED_POINT - 1))
#define DUER_SIN(_x)            duer_coef_sin(_x)
#define DUER_SAT(_x)            (((_x) > DUER_INT_MAX) ? DUER_INT_MAX : (((_x) < DUER_INT_MIN) ? DUER_INT_MIN : (_x)))

#define DUER_ABS(_x)            (((_x) < 0) ? (0 - (_x)) : (_x))

#define DUER_COEF_F             "%d"

#else

#include <math.h>

typedef float duer_coef_t;

#define DUER_QCONST(_x)         (_x)
#define DUER_FCONST(_x)         (_x)
#define DUER_COEF_MUL(_x, _y)   ((_x) * (_y))
#define DUER_SIN(_x)            sin(_x)
#define DUER_SAT(_x)            (_x)
#define DUER_ABS(_x)            abs(_x)

#define DUER_COEF_F             "%f"

#endif

#define DUER_DIV_CEIL(_den, _mol)       (((_den) + (_mol) - 1) / (_mol))
#define DUER_MAX(_a, _b)                (((_a) > (_b)) ? (_a) : (_b))

#ifdef __cplusplus
extern "C" {
#endif

DUER_INT const duer_coef_t PI;
DUER_INT const duer_coef_t PI_DOUBLE;
DUER_INT const duer_coef_t PI_HALF;

/**
 * Taylor:
 *     sin(x) = SIGMA((-1) ^ n) * (x ^ (2 * n + 1)) / ( 2 * n + 1)!)    {n >= 0}
 *
 * @param  x radian
 * @return   sin(x) value
 */
DUER_INT duer_coef_t duer_coef_sin(duer_coef_t x);

/**
 * Convolution fomular:
 *     y(n) = ∑x(i)h(j)   ; i ∈ {0, 1, 2 ... N}
 *                        ; j ∈ {0, 1, 2 ... M}
 *                        ; n = i + j, n ∈ {0, 1, 2 ... N + M - 1}
 *                        
 * @param  y The output values
 * @param  O y array length, O = N + M - 1
 * @param  x The input values
 * @param  N x array length
 * @param  h The coefficient values
 * @param  M The coefficient array length
 * @return   Calculate status.
 */
DUER_INT duer_status_t duer_conv(duer_coef_t *y, duer_size_t O, int ystart,
                                 const duer_coef_t *x, duer_size_t N, int xstart,
                                 const duer_coef_t *h, duer_size_t M);

/**
 * w(n) = ∑[k:0->(L-1)]a(k)*cos(2*k*PI*n*(N-1))
 *         ; k = 0, 1, 2 ... (L - 1)
 *         ; n = 0, 1, 2 ... (N - 1)
 * 
 * @param  n    The index of the window
 * @param  N    The window length
 * @param  a    The cofficient array
 * @param  L    The array length
 * @return The result for add window
 */
DUER_INT duer_coef_t duer_window_cos(int n, int N, const duer_coef_t a[], duer_size_t L);

/**
 * w(i) = a0
 *      - a1 * cos(2 * PI * i / (N - 1))
 *
 *      ; a0 = 0.54
 *      ; a1 = 0.46
 * 
 * @param  index  The i value in the fomular
 * @param  length The N value in the fomular
 * @return        The result for hamming
 */
DUER_INT duer_coef_t duer_window_hamming(int index, int length);

/**
 * w(i) = a0
 *      - a1 * cos(2 * PI * i / (N - 1))
 *
 *      ; a0 = 0.5
 *      ; a1 = 0.5
 * 
 * @param  index  The i value in the fomular
 * @param  length The N value in the fomular
 * @return        The result for hanning
 */
DUER_INT duer_coef_t duer_window_hanning(int index, int length);

/**
 * w(i) = a0
 *      - a1 * cos(2 * PI * i / (N - 1))
 *      + a2 * cos(4 * PI * i / (N - 1))
 *
 *      ; a0 = 0.62
 *      ; a1 = 0.48
 *      ; a2 = 0.38
 * 
 * @param  index  The i value in the fomular
 * @param  length The N value in the fomular
 * @return        The result for hanning
 */
DUER_INT duer_coef_t duer_window_blackman(int index, int length);

/**
 * w(i) = a0
 *      - a1 * cos(2 * PI * i / (N - 1))
 *      + a2 * cos(4 * PI * i / (N - 1))
 *      - a3 * cos(6 * PI * i / (N - 1))
 *
 *      ; a0 = 0.35875
 *      ; a1 = 0.48829
 *      ; a2 = 0.14128
 *      ; a3 = 0.01168
 * 
 * @param  index  The i value in the fomular
 * @param  length The N value in the fomular
 * @return        The result for hanning
 */
DUER_INT duer_coef_t duer_window_blackman_harris(int index, int length);

/**
 * w(i) = a0
 *      - a1 * cos(2 * PI * i / (N - 1))
 *      + a2 * cos(4 * PI * i / (N - 1))
 *      - a3 * cos(6 * PI * i / (N - 1))
 *
 *      ; a0 = 0.3635819
 *      ; a1 = 0.4891775
 *      ; a2 = 0.1365995
 *      ; a3 = 0.0106411
 * 
 * @param  index  The i value in the fomular
 * @param  length The N value in the fomular
 * @return        The result for hanning
 */
DUER_INT duer_coef_t duer_window_blackman_nuttall(int index, int length);

DUER_INT duer_coef_t duer_sample_generate(int index, int freq, duer_size_t length, int fs);

DUER_INT duer_coef_t duer_short2coef(short val);

DUER_INT short duer_coef2short(duer_coef_t val);

DUER_INT duer_coef_t *duer_short2coef_array(duer_coef_t dst[], const short src[], duer_size_t length);

DUER_INT short *duer_coef2short_array(short src[], const duer_coef_t dst[], duer_size_t length);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_MATH_H
