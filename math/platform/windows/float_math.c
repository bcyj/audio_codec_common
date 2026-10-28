#include <audio_math/float_rounding.h>
#include <audio_math/float_convert.h>
/* Shared Windows floating-point compatibility functions.
 * Copyright (c) 2017-2025, Arm Limited. SPDX-License-Identifier: MIT
 * See NOTICE.math.txt for provenance and the complete license. */
#include <audio_math/fma_native.h>
#include <audio_math/platform/windows/constants.h>
#include <audio_math/constants.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <audio_math/platform/windows/tables.h>

/* Select reduced sine signs by quadrant, shared by joint and single-component entries. */
static const double audio_math_win_sine_quadrant_sign[4] = {1.0, -1.0, -1.0, 1.0};

#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
/* Return the single-precision natural exponential through an SSE2 64-segment cubic model
 * without CRT calls. It rounds to nearest-even and preserves subnormals; only the result is
 * specified, without emulating errno or exception flags. The two AR callers use 1-result;
 * that final weight is separately validated against the FMA host implementation. */
float audio_math_windows_expf_sse2_soft(float input) {
    uint32_t bits = audio_math_float32_bits_sse2(input);
    uint32_t magnitude = bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if (magnitude >= AUDIO_MATH_FLOAT32_INFINITY_BITS) {
        if (magnitude > AUDIO_MATH_FLOAT32_INFINITY_BITS)
            return audio_math_float32_from_bits(bits | UINT32_C(0x00400000));
        return bits & AUDIO_MATH_FLOAT32_SIGN_BITS ? 0.0f : input;
    }
    double scaled = (double)input * AUDIO_MATH_WIN_EXPF_REDUCTION_FACTOR_F64;
    if (scaled >= AUDIO_MATH_WIN_EXPF_OVERFLOW_SCALED_F64)
        return audio_math_float32_from_bits(AUDIO_MATH_FLOAT32_INFINITY_BITS);
    if (scaled < AUDIO_MATH_WIN_EXPF_UNDERFLOW_SCALED_F64)
        return 0.0f;
    /* x64-mandatory SSE2 generates the bounded segment index directly under MXCSR
     * nearest-even mode. Like the remaining operations, this requires default rounding
     * and gradual underflow. */
    int nearest = _mm_cvtsd_si32(_mm_set_sd(scaled));
    unsigned index = (unsigned)nearest & (AUDIO_MATH_WIN_EXPF_SEGMENTS - 1u);
    int exponent = (nearest - (int)index) / AUDIO_MATH_WIN_EXPF_SEGMENTS;
    double reduced = (double)input - (double)nearest * AUDIO_MATH_WIN_EXPF_REDUCTION_STEP_F64;
    double polynomial = reduced * AUDIO_MATH_WIN_EXPF_CUBIC_F64 + 0.5;
    polynomial = (reduced * reduced) * polynomial + reduced;
    double value =
        polynomial * audio_math_win_expf_sse2_table[index] + audio_math_win_expf_sse2_table[index];
    double scale = audio_math_float64_from_bits((uint64_t)(unsigned)(exponent + 1023) << 52);
    return (float)(value * scale);
}
#endif

/* Read IEEE-754 float bits without aliasing undefined behavior. */
uint32_t audio_math_win_float_bits(float value) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    return audio_math_float32_bits_sse2(value);
#else
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
#endif
}

/* Reduce arbitrary finite large float angles using a 192-bit 4/pi table. */
static inline double audio_math_win_reduce_large(uint32_t bits, int *quadrant) {
    const uint32_t *table = audio_math_win_inv_pi_over_four + ((bits >> 26) & 15);
    uint32_t mantissa = ((bits & 0xffffff) | 0x800000) << ((bits >> 23) & 7);
    uint64_t high = (uint32_t)(mantissa * table[0]);
    uint64_t middle = (uint64_t)mantissa * table[4];
    uint64_t low = (uint64_t)mantissa * table[8];
    uint64_t remainder = ((low >> 32) | (high << 32)) + middle;
    uint64_t n = (remainder + (UINT64_C(1) << 61)) >> 62;
    remainder -= n << 62;
    *quadrant = (int)n;
    return (double)(int64_t)remainder * AUDIO_MATH_HALF_PI_Q62_SCALE_F64;
}

/* Reproduce baseline libm trig reduction, quadrant selection, and fused polynomial evaluation. */
void audio_math_win_sincosf(float input, float *sine, float *cosine) {
    uint32_t bits = audio_math_win_float_bits(input), top = (bits >> 20) & 0x7ff;
    double x = input;
    int n = 0;
    if (top < 0x398) {
        *sine = input;
        *cosine = 1.0f;
        return;
    }
    if (top >= 0x3f4) {
        if (top < 0x42f) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
            /* Products in this branch cannot land on a half integer, so nearest-even and
             * away-from-zero rules agree. Constraints are 0.75 <= |input| < 120, the fixed
             * 2/pi bit pattern, and default rounding. */
            n = _mm_cvtsd_si32(_mm_set_sd(x * AUDIO_MATH_TWO_OVER_PI_F64));
            double nearest = (double)n;
#else
            double nearest = audio_math_round_f64(x * AUDIO_MATH_TWO_OVER_PI_F64);
            /* Here |input| < 120, so the nearest quadrant index magnitude is at most 76. */
            n = audio_math_truncate_i32_f64(nearest);
#endif
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
            x = (x - nearest * AUDIO_MATH_WIN_REDUCE_HALF_PI_HIGH_F64) -
                nearest * AUDIO_MATH_WIN_REDUCE_HALF_PI_LOW_F64;
#else
            x = audio_math_fma_f64(-AUDIO_MATH_HALF_PI_F64, nearest, x);
#endif
        } else if (top < 0x7f8) {
            x = audio_math_win_reduce_large(bits, &n);
        } else {
            *sine = *cosine = input - input;
            return;
        }
    }
    /* Large-argument reduction extracts input sign and selects output by quadrant. */
    int quadrant = top >= 0x42f ? n + (int)(bits >> 31) : n;
    double square = x * x;
    x *= audio_math_win_sine_quadrant_sign[quadrant & 3];
    double fourth = square * square, cube = square * x;
    double fifth = square * cube, sixth = square * fourth;
    /* Skip the fused polynomial when a conservative interval determines both float bit patterns;
     * near either rounding boundary, use the fused polynomial without changing reduction/quadrant. */
    double fast_sin_low = cube * AUDIO_MATH_WIN_SIN_C3_F64 + x;
    double fast_sin_high = square * AUDIO_MATH_WIN_SIN_C7_F64 + AUDIO_MATH_WIN_SIN_C5_F64;
    double fast_cosine_sign = (quadrant & 2) ? -1.0 : 1.0;
    double fast_cos_low =
        square * (fast_cosine_sign * AUDIO_MATH_WIN_COS_C2_F64) + fast_cosine_sign;
    fast_cos_low = fourth * (fast_cosine_sign * AUDIO_MATH_WIN_COS_C4_F64) + fast_cos_low;
    double fast_cos_high = square * (fast_cosine_sign * AUDIO_MATH_WIN_COS_C8_F64) +
                           fast_cosine_sign * AUDIO_MATH_WIN_COS_C6_F64;
    double sin_estimate = fifth * fast_sin_high + fast_sin_low;
    double cos_estimate = sixth * fast_cos_high + fast_cos_low;
    float sin_lower = (float)(sin_estimate - AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
    float sin_upper = (float)(sin_estimate + AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
    float cos_lower = (float)(cos_estimate - AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
    float cos_upper = (float)(cos_estimate + AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
    if (audio_math_win_float_bits(sin_lower) == audio_math_win_float_bits(sin_upper) &&
        audio_math_win_float_bits(cos_lower) == audio_math_win_float_bits(cos_upper)) {
        *sine = (n & 1) ? cos_lower : sin_lower;
        *cosine = (n & 1) ? sin_lower : cos_lower;
        return;
    }
    double sin_low = audio_math_fma_f64(cube, AUDIO_MATH_WIN_SIN_C3_F64, x);
    double sin_high =
        audio_math_fma_f64(square, AUDIO_MATH_WIN_SIN_C7_F64, AUDIO_MATH_WIN_SIN_C5_F64);
    double cosine_sign = (quadrant & 2) ? -1.0 : 1.0;
    double cos_low =
        audio_math_fma_f64(square, cosine_sign * AUDIO_MATH_WIN_COS_C2_F64, cosine_sign);
    cos_low = audio_math_fma_f64(fourth, cosine_sign * AUDIO_MATH_WIN_COS_C4_F64, cos_low);
    double cos_high = audio_math_fma_f64(square, cosine_sign * AUDIO_MATH_WIN_COS_C8_F64,
                                         cosine_sign * AUDIO_MATH_WIN_COS_C6_F64);
    float s = (float)audio_math_fma_f64(fifth, sin_high, sin_low);
    float c = (float)audio_math_fma_f64(sixth, cos_high, cos_low);
    *sine = (n & 1) ? c : s;
    *cosine = (n & 1) ? s : c;
}

/* Compute only the required float trig component, retaining joint-entry reduction/rounding policy. */
static float trig_single_f32(float input, unsigned cosine) {
    uint32_t bits = audio_math_win_float_bits(input), top = (bits >> 20) & 0x7ff;
    if (top < 0x398)
        return cosine ? 1.0f : input;
    double x = input;
    int n = 0;
    if (top >= 0x3f4) {
        if (top < 0x42f) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
            n = _mm_cvtsd_si32(_mm_set_sd(x * AUDIO_MATH_TWO_OVER_PI_F64));
            double nearest = (double)n;
            x = (x - nearest * AUDIO_MATH_WIN_REDUCE_HALF_PI_HIGH_F64) -
                nearest * AUDIO_MATH_WIN_REDUCE_HALF_PI_LOW_F64;
#else
            double nearest = audio_math_round_f64(x * AUDIO_MATH_TWO_OVER_PI_F64);
            n = audio_math_truncate_i32_f64(nearest);
            x = audio_math_fma_f64(-AUDIO_MATH_HALF_PI_F64, nearest, x);
#endif
        } else if (top < 0x7f8) {
            x = audio_math_win_reduce_large(bits, &n);
        } else {
            return input - input;
        }
    }
    int quadrant = top >= 0x42f ? n + (int)(bits >> 31) : n;
    double square = x * x;
    double estimate, low, high, multiplier;
    if (((unsigned)n & 1u) ^ cosine) {
        double sign = (quadrant & 2) ? -1.0 : 1.0;
        double fourth = square * square;
        multiplier = square * fourth;
        low = square * (sign * AUDIO_MATH_WIN_COS_C2_F64) + sign;
        low = fourth * (sign * AUDIO_MATH_WIN_COS_C4_F64) + low;
        high = square * (sign * AUDIO_MATH_WIN_COS_C8_F64) + sign * AUDIO_MATH_WIN_COS_C6_F64;
        estimate = multiplier * high + low;
        float lower = (float)(estimate - AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
        float upper = (float)(estimate + AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
        if (audio_math_win_float_bits(lower) == audio_math_win_float_bits(upper))
            return lower;
        low = audio_math_fma_f64(square, sign * AUDIO_MATH_WIN_COS_C2_F64, sign);
        low = audio_math_fma_f64(fourth, sign * AUDIO_MATH_WIN_COS_C4_F64, low);
        high = audio_math_fma_f64(square, sign * AUDIO_MATH_WIN_COS_C8_F64,
                                  sign * AUDIO_MATH_WIN_COS_C6_F64);
    } else {
        x *= audio_math_win_sine_quadrant_sign[quadrant & 3];
        double cube = square * x;
        multiplier = square * cube;
        low = cube * AUDIO_MATH_WIN_SIN_C3_F64 + x;
        high = square * AUDIO_MATH_WIN_SIN_C7_F64 + AUDIO_MATH_WIN_SIN_C5_F64;
        estimate = multiplier * high + low;
        float lower = (float)(estimate - AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
        float upper = (float)(estimate + AUDIO_MATH_WIN_TRIG_ROUND_GUARD_F64);
        if (audio_math_win_float_bits(lower) == audio_math_win_float_bits(upper))
            return lower;
        low = audio_math_fma_f64(cube, AUDIO_MATH_WIN_SIN_C3_F64, x);
        high = audio_math_fma_f64(square, AUDIO_MATH_WIN_SIN_C7_F64, AUDIO_MATH_WIN_SIN_C5_F64);
    }
    return (float)audio_math_fma_f64(multiplier, high, low);
}

/* Output float sine with the baseline platform's evaluation order. */
float audio_math_win_sinf(float input) {
    return trig_single_f32(input, 0);
}

/* Output float cosine with the baseline platform's evaluation order. */
float audio_math_win_cosf(float input) {
    return trig_single_f32(input, 1);
}
/* Construct floating-point values from bit patterns. */
static inline float audio_math_win_float_value(uint32_t bits) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    return audio_math_float32_from_bits_sse2(bits);
#else
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
#endif
}

/* Reproduce exp2f bits with rounding intervals; retain three fused evaluations near boundaries. */
float audio_math_win_exp2f(float input) {
    uint32_t bits = audio_math_win_float_bits(input);
    uint32_t top = (bits >> 20) & 0x7ff;
    if (top >= 0x430) {
        if (bits == 0xff800000)
            return 0.0f;
        if (top >= 0x7f8)
            return input + input;
        if (input > 0.0f)
            return INFINITY;
        if (input <= -150.0f)
            return 0.0f;
    }
    double shifted = (double)input + AUDIO_MATH_WIN_INTEGER_ROUND_BIAS_F64;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    uint64_t encoded = audio_math_float64_bits_sse2(shifted);
#else
    uint64_t encoded;
    memcpy(&encoded, &shifted, sizeof encoded);
#endif
    double reduced = (double)input - (shifted - AUDIO_MATH_WIN_INTEGER_ROUND_BIAS_F64);
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Even entries of the 64-segment exponential table are 2^(i/32). Split the low five bits
     * from encoded, add the integer exponent bits directly, and perform encoded-table arithmetic
     * modulo 2^64. */
    unsigned index = (unsigned)encoded & 31u;
    uint64_t scale_bits;
    memcpy(&scale_bits, &audio_math_win_expf_sse2_table[2u * index], sizeof scale_bits);
    scale_bits += (encoded & ~UINT64_C(31)) << 47;
#else
    uint64_t scale_bits = audio_math_win_exp2_table[encoded & 31] + (encoded << 47);
#endif
    double scale;
    memcpy(&scale, &scale_bits, sizeof scale);
    /* The ordinary and fused polynomial values differ by less than 2^-48. Accept the ordinary
     * result only when both scaled interval endpoints round to the same float; otherwise use FMA. */
    double fast_upper = AUDIO_MATH_WIN_EXP2_C3_F64 * reduced + AUDIO_MATH_WIN_EXP2_C2_F64;
    double fast_lower = reduced * AUDIO_MATH_WIN_EXP2_C1_F64 + 1.0;
    double estimate = (reduced * reduced) * fast_upper + fast_lower;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Compute the two double endpoints in parallel using the same add/subtract/scale order; the
     * low lane is the lower bound. CVTPD2PS rounds each lane, and bit comparison preserves signed
     * zero. An indeterminate interval falls back to fused evaluation. */
    __m128d interval =
        _mm_add_pd(_mm_set1_pd(estimate), _mm_set_pd(AUDIO_MATH_WIN_EXP2_ROUND_GUARD_F64,
                                                     -AUDIO_MATH_WIN_EXP2_ROUND_GUARD_F64));
    interval = _mm_mul_pd(interval, _mm_set1_pd(scale));
    __m128 rounded = _mm_cvtpd_ps(interval);
    uint64_t endpoints = (uint64_t)_mm_cvtsi128_si64(_mm_castps_si128(rounded));
    if ((uint32_t)endpoints == (uint32_t)(endpoints >> 32))
        return _mm_cvtss_f32(rounded);
#else
    float rounded_lower = (float)((estimate - AUDIO_MATH_WIN_EXP2_ROUND_GUARD_F64) * scale);
    float rounded_upper = (float)((estimate + AUDIO_MATH_WIN_EXP2_ROUND_GUARD_F64) * scale);
    if (audio_math_win_float_bits(rounded_lower) == audio_math_win_float_bits(rounded_upper))
        return rounded_lower;
#endif
    double upper =
        audio_math_fma_f64(AUDIO_MATH_WIN_EXP2_C3_F64, reduced, AUDIO_MATH_WIN_EXP2_C2_F64);
    double lower = audio_math_fma_f64(reduced, AUDIO_MATH_WIN_EXP2_C1_F64, 1.0);
    return (float)(audio_math_fma_f64(reduced * reduced, upper, lower) * scale);
}

/* Compute a single-precision base-2 logarithm. Return directly when a conservative double interval
 * determines float rounding; use fused reduction and polynomial near rounding boundaries. */
float audio_math_win_log2f(float input) {
    uint32_t bits = audio_math_win_float_bits(input);
    if (bits == 0x3f800000)
        return 0.0f;
    if (bits - 0x00800000u >= 0x7f800000u - 0x00800000u) {
        if ((bits << 1) == 0)
            return -INFINITY;
        if (bits == 0x7f800000)
            return input;
        if ((bits & 0x80000000) || (bits << 1) >= 0xff000000)
            return input != input ? input + input : audio_math_win_float_value(0x7fc00000);
        bits = audio_math_win_float_bits(input * AUDIO_MATH_POW2_23_F32) - (23u << 23);
    }
    uint32_t relative = bits - 0x3f330000;
    unsigned index = (relative >> 19) & 15;
    int exponent = (int32_t)relative >> 23;
    double normalized = audio_math_win_float_value(bits - (relative & 0xff800000));
    /* Ordinary double evaluation has absolute error below 2^-42. Accept it only when both interval
     * endpoints round to the same float; otherwise use fused evaluation. Rounding is nearest-even. */
    double fast_reduced = audio_math_win_log2_table[index][0] * normalized - 1.0;
    double fast_square = fast_reduced * fast_reduced;
    double fast_polynomial = AUDIO_MATH_WIN_LOG2_C3_F64 * fast_reduced + AUDIO_MATH_WIN_LOG2_C2_F64;
    fast_polynomial = AUDIO_MATH_WIN_LOG2_C4_F64 * fast_square + fast_polynomial;
    double fast_base = audio_math_win_log2_table[index][1] + exponent;
    double fast_linear = AUDIO_MATH_WIN_LOG2_C1_F64 * fast_reduced + fast_base;
    double estimate = fast_square * fast_polynomial + fast_linear;
    float lower = (float)(estimate - AUDIO_MATH_WIN_LOG2_ROUND_GUARD_F64);
    float upper = (float)(estimate + AUDIO_MATH_WIN_LOG2_ROUND_GUARD_F64);
    if (audio_math_win_float_bits(lower) == audio_math_win_float_bits(upper))
        return lower;
    double reduced = audio_math_fma_f64(audio_math_win_log2_table[index][0], normalized, -1.0);
    double base = audio_math_win_log2_table[index][1] + exponent;
    double square = reduced * reduced;
    double polynomial =
        audio_math_fma_f64(AUDIO_MATH_WIN_LOG2_C3_F64, reduced, AUDIO_MATH_WIN_LOG2_C2_F64);
    polynomial = audio_math_fma_f64(AUDIO_MATH_WIN_LOG2_C4_F64, square, polynomial);
    double linear = audio_math_fma_f64(AUDIO_MATH_WIN_LOG2_C1_F64, reduced, base);
    return (float)audio_math_fma_f64(square, polynomial, linear);
}
