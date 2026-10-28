#include <audio_math/float_exp.h>
#include <audio_math/float_convert.h>

#define EXP2_OVERFLOW_INPUT 128.0f
#define EXP2_UNDERFLOW_INPUT (-150.0f)
#define EXP2_TINY_INPUT_BITS UINT32_C(0x32800000)

#define EXP_OVERFLOW_GUARD 89.0f
#define EXP_UNDERFLOW_GUARD (-104.0f)
#define EXP_TINY_INPUT_BITS UINT32_C(0x32800000)
#define EXP_LOG2_E 0x1.71547652b82fep+0

/* exp(r) coefficients 1/n!, n=0..18, rounded from exact rationals to binary64.
 * Degree-18 fallback retains Horner order; shorter forms group odd/even terms only under rounding guards.
 */
static const double exponential_coefficients[19] = {1.0,
                                                    1.0,
                                                    0.5,
                                                    0x1.5555555555555p-3,
                                                    0x1.5555555555555p-5,
                                                    0x1.1111111111111p-7,
                                                    0x1.6c16c16c16c17p-10,
                                                    0x1.a01a01a01a01ap-13,
                                                    0x1.a01a01a01a01ap-16,
                                                    0x1.71de3a556c734p-19,
                                                    0x1.27e4fb7789f5cp-22,
                                                    0x1.ae64567f544e4p-26,
                                                    0x1.1eed8eff8d898p-29,
                                                    0x1.6124613a86d09p-33,
                                                    0x1.93974a8c07c9dp-37,
                                                    0x1.ae7f3e733b81fp-41,
                                                    0x1.ae7f3e733b81fp-45,
                                                    0x1.952c77030ad4ap-49,
                                                    0x1.6827863b97d97p-53};

#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_IX86))
#include <audio_math/compiler.h>
#define EXP_FAST_ROUND_GUARD 0x1p-30
/* Evaluate a short polynomial in odd/even chains and determine float rounding; fall back to degree 18 if uncertain.
 * Callers guarantee |reduced| < 0.35 and a normal binary64 power-of-two scale. */
static AUDIO_MATH_FORCE_INLINE int exponential_round_fast(double reduced, int exponent, float *rounded) {
    double square = reduced * reduced;
    double even = exponential_coefficients[8];
    even = exponential_coefficients[6] + square * even;
    even = exponential_coefficients[4] + square * even;
    even = exponential_coefficients[2] + square * even;
    even = exponential_coefficients[0] + square * even;
    double odd = exponential_coefficients[7];
    odd = exponential_coefficients[5] + square * odd;
    odd = exponential_coefficients[3] + square * odd;
    odd = exponential_coefficients[1] + square * odd;
    double polynomial = even + reduced * odd;
    double scale = audio_math_float64_from_bits((uint64_t)(uint32_t)(exponent + 1023) << 52);
    float lower = (float)((polynomial - EXP_FAST_ROUND_GUARD) * scale);
    float upper = (float)((polynomial + EXP_FAST_ROUND_GUARD) * scale);
    if (audio_math_float32_bits(lower) != audio_math_float32_bits(upper))
        return 0;
    *rounded = lower;
    return 1;
}
#endif

/* Separate integer exponent, approximate fractional power on a small interval, and convert to float once at the end. */
float audio_math_exp2f_soft(float input) {
    uint32_t bits = audio_math_float32_bits(input);
    if ((bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return audio_math_float32_from_bits(bits | UINT32_C(0x00400000));
    if (input >= EXP2_OVERFLOW_INPUT)
        return audio_math_float32_from_bits(AUDIO_MATH_FLOAT32_INFINITY_BITS);
    if (input <= EXP2_UNDERFLOW_INPUT)
        return 0.0f;
    /* For |x| < 2^-26, 2^x is closer to one than either adjacent binary32 rounding midpoint. */
    if ((bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) < EXP2_TINY_INPUT_BITS)
        return 1.0f;

    int exponent = audio_math_truncate_i32_f64((double)input + (input < 0.0f ? -0.5 : 0.5));
    double reduced = ((double)input - exponent) * AUDIO_MATH_LN_TWO_F64;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_IX86))
    float rounded;
    if (exponential_round_fast(reduced, exponent, &rounded))
        return rounded;
#endif
    int degree = (int)(sizeof exponential_coefficients / sizeof exponential_coefficients[0]) - 1;
    double polynomial = exponential_coefficients[degree];
    for (int order = degree - 1; order >= 0; --order)
        polynomial = exponential_coefficients[order] + reduced * polynomial;

    /* exponent is -150..128, so 2^exponent is an exact normal binary64 value. */
    double scale = audio_math_float64_from_bits((uint64_t)(uint32_t)(exponent + 1023) << 52);
    return (float)(polynomial * scale);
}

/* Reduce natural exponent around the nearest power of two, reuse small-interval exp coefficients, and round to float once.
 * Split ln(2) into high/low parts to avoid cancellation in x-k*ln(2) at large exponents.
 * Coarse bounds lie beyond binary32 overflow/underflow; nearby inputs still use ordinary rounding.
 */
float audio_math_expf_soft(float input) {
    uint32_t bits = audio_math_float32_bits(input);
    if ((bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return audio_math_float32_from_bits(bits | UINT32_C(0x00400000));
    if (input >= EXP_OVERFLOW_GUARD)
        return audio_math_float32_from_bits(AUDIO_MATH_FLOAT32_INFINITY_BITS);
    if (input <= EXP_UNDERFLOW_GUARD)
        return 0.0f;
    if ((bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) < EXP_TINY_INPUT_BITS)
        return 1.0f;

    double value = input;
    int exponent = audio_math_truncate_i32_f64(value * EXP_LOG2_E + (input < 0.0f ? -0.5 : 0.5));
    double reduced =
        (value - exponent * AUDIO_MATH_LN_TWO_HIGH_F64) - exponent * AUDIO_MATH_LN_TWO_LOW_F64;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_IX86))
    float rounded;
    if (exponential_round_fast(reduced, exponent, &rounded))
        return rounded;
#endif
    int degree = (int)(sizeof exponential_coefficients / sizeof exponential_coefficients[0]) - 1;
    double polynomial = exponential_coefficients[degree];
    for (int order = degree - 1; order >= 0; --order)
        polynomial = exponential_coefficients[order] + reduced * polynomial;

    double scale = audio_math_float64_from_bits((uint64_t)(uint32_t)(exponent + 1023) << 52);
    return (float)(polynomial * scale);
}

#if defined(_KERNEL_MODE)
#define POW10_F32_OVERFLOW_GUARD 39.0
#define POW10_F32_UNDERFLOW_GUARD (-46.0)

/* Reduce base-ten powers to exp(r)*2^k using shared software-exponent coefficients.
 * Multiply high/low exponent and ln(10) parts to reduce cancellation error.
 * Both coarse bounds lie beyond float overflow/round-to-zero boundaries.
 */
float audio_math_pow10_to_f32_soft(double value) {
    uint64_t bits = audio_math_float64_bits(value);
    if ((bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000))
        return (float)(value + value);
    if (value >= POW10_F32_OVERFLOW_GUARD)
        return audio_math_float32_from_bits(AUDIO_MATH_FLOAT32_INFINITY_BITS);
    if (value <= POW10_F32_UNDERFLOW_GUARD)
        return 0.0f;
    int exponent = audio_math_truncate_i32_f64(value * AUDIO_MATH_LOG2_TEN_F64 +
                                        (value < 0.0 ? -0.5 : 0.5));
    double high = audio_math_float64_from_bits(bits & ~UINT64_C(0x7ffffff));
    double low = value - high;
    double reduced = (high * AUDIO_MATH_LN_TEN_HIGH_F64 -
                      exponent * AUDIO_MATH_LN_TWO_HIGH_F64) +
                     ((low * AUDIO_MATH_LN_TEN_HIGH_F64 + high * AUDIO_MATH_LN_TEN_LOW_F64 -
                       exponent * AUDIO_MATH_LN_TWO_LOW_F64) + low * AUDIO_MATH_LN_TEN_LOW_F64);
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_IX86))
    float rounded;
    if (exponential_round_fast(reduced, exponent, &rounded))
        return rounded;
#endif
    double polynomial = exponential_coefficients[18];
    for (int order = 17; order >= 0; --order)
        polynomial = exponential_coefficients[order] + reduced * polynomial;
    double scale = audio_math_float64_from_bits((uint64_t)(uint32_t)(exponent + 1023) << 52);
    return (float)(polynomial * scale);
}
#endif
