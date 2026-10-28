/* Windows math adaptation: floating trig and large-argument reduction.
 * Copyright (c) 2018-2024, Arm Limited. SPDX-License-Identifier: MIT
 * See platform/windows/NOTICE.math.txt for the full license. Changes: memcpy bit access,
 * explicit single-rounding fused operations, and MSVC/MinGW support.
 */
#ifndef AUDIO_MATH_WINDOWS_TRIG_H
#define AUDIO_MATH_WINDOWS_TRIG_H
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t audio_math_win_float_bits(float value);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void audio_math_win_sincosf(float input, float *sine, float *cosine);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_win_sinf(float input);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_win_cosf(float input);
#ifdef __cplusplus
}
#endif
#endif
