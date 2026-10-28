#ifndef AUDIO_MATH_DSP_FLOAT_LOG_H
#define AUDIO_MATH_DSP_FLOAT_LOG_H
#include <audio_math/numeric_bits.h>
#include <audio_math/constants.h>

/* Compute an extended-precision natural log of binary32 without host math.
 * Reduce the significand to [sqrt(1/2), sqrt(2)] and evaluate the odd 2*atanh(z) series;
 * retain double output so callers can finish calculations before rounding to float.
 * Require nearest-even rounding and Win32 x87 53-bit precision; do not emulate errno/exceptions
 * or guarantee correct rounding for arbitrary binary64 logs. Quiet NaNs; negatives return NaN.
 */
static inline double audio_math_log_f32_wide_soft(float input) {
    uint32_t bits = audio_math_float32_bits(input);
    uint32_t magnitude = bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if (magnitude > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return (double)audio_math_float32_from_bits(bits | UINT32_C(0x00400000));
    if (magnitude == 0)
        return audio_math_float64_from_bits(UINT64_C(0xfff0000000000000));
    if (bits & AUDIO_MATH_FLOAT32_SIGN_BITS)
        return audio_math_float64_from_bits(UINT64_C(0x7ff8000000000000));
    if (bits == AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return (double)input;

    int exponent_adjustment = 0;
    if (bits < UINT32_C(0x00800000)) {
        bits = audio_math_float32_bits(input * AUDIO_MATH_POW2_23_F32);
        exponent_adjustment = -23;
    }
    int exponent = (int)(bits >> 23) - 127 + exponent_adjustment;
    uint32_t mantissa_bits = (bits & UINT32_C(0x007fffff)) | UINT32_C(0x3f800000);
    double mantissa = (double)audio_math_float32_from_bits(mantissa_bits);
    if (mantissa_bits > UINT32_C(0x3fb504f3)) {
        mantissa *= 0.5;
        ++exponent;
    }
    double reduced = (mantissa - 1.0) / (mantissa + 1.0);
    double square = reduced * reduced;
    /* ln(m) = 2*z*(1 + z^2/3 + ... + z^34/35).
     * |z| < 0.172 and omitted terms total less than 3e-30. Coefficients are nearest-even binary64
     * odd reciprocals expressed as exact-value literals to avoid runtime division under strict FP.
     * Preserve explicit Horner order without coefficient arrays or scratch. */
    double polynomial = 0.028571428571428571; /* 1/35 */
    polynomial = 0.030303030303030304 + square * polynomial; /* 1/33 */
    polynomial = 0.032258064516129031 + square * polynomial; /* 1/31 */
    polynomial = 0.034482758620689655 + square * polynomial; /* 1/29 */
    polynomial = 0.037037037037037035 + square * polynomial; /* 1/27 */
    polynomial = 0.040000000000000001 + square * polynomial; /* 1/25 */
    polynomial = 0.043478260869565216 + square * polynomial; /* 1/23 */
    polynomial = 0.047619047619047616 + square * polynomial; /* 1/21 */
    polynomial = 0.052631578947368418 + square * polynomial; /* 1/19 */
    polynomial = 0.058823529411764705 + square * polynomial; /* 1/17 */
    polynomial = 0.066666666666666666 + square * polynomial; /* 1/15 */
    polynomial = 0.076923076923076927 + square * polynomial; /* 1/13 */
    polynomial = 0.090909090909090912 + square * polynomial; /* 1/11 */
    polynomial = 0.1111111111111111 + square * polynomial; /* 1/9 */
    polynomial = 0.14285714285714285 + square * polynomial; /* 1/7 */
    polynomial = 0.20000000000000001 + square * polynomial; /* 1/5 */
    polynomial = 0.33333333333333331 + square * polynomial; /* 1/3 */
    double logarithm = (2.0 * reduced) * (1.0 + square * polynomial);
    return exponent * AUDIO_MATH_LN_TWO_HIGH_F64 + (logarithm + exponent * AUDIO_MATH_LN_TWO_LOW_F64);
}
/* Compose decimal log from separate exponent/significand terms and round to float last.
 * Negative finite values/infinity return negative quiet NaN; zero, positive infinity, and NaN follow natural log.
 * No errno/exception emulation; consumers verify any required CRT float-rounding match. */
static inline float audio_math_log10_f32_soft(float input) {
    uint32_t bits = audio_math_float32_bits(input);
    uint32_t magnitude = bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if ((bits & AUDIO_MATH_FLOAT32_SIGN_BITS) && magnitude != 0 &&
        magnitude <= AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return audio_math_float32_from_bits(UINT32_C(0xffc00000));
    if (magnitude == 0 || magnitude >= AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return (float)audio_math_log_f32_wide_soft(input);
    int adjustment = 0;
    if (bits < UINT32_C(0x00800000)) {
        bits = audio_math_float32_bits(input * AUDIO_MATH_POW2_23_F32);
        adjustment = -23;
    }
    int exponent = (int)(bits >> 23) - 127 + adjustment;
    uint32_t mantissa = (bits & UINT32_C(0x007fffff)) | UINT32_C(0x3f800000);
    if (mantissa > UINT32_C(0x3fb504f3)) {
        mantissa -= UINT32_C(0x00800000);
        ++exponent;
    }
    double logarithm = audio_math_log_f32_wide_soft(audio_math_float32_from_bits(mantissa));
    return (float)(exponent * AUDIO_MATH_LOG10_TWO_HIGH_F64 +
                   (logarithm * AUDIO_MATH_LOG10_E_F64 + exponent * AUDIO_MATH_LOG10_TWO_LOW_F64));
}
#endif
