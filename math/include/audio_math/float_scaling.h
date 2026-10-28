#ifndef AUDIO_MATH_DSP_FLOAT_SCALING_H
#define AUDIO_MATH_DSP_FLOAT_SCALING_H
#include <audio_math/bits.h>
#include <audio_math/numeric_bits.h>

/* Return the single-precision frexp exponent; zero/infinity/NaN return zero.
 * Finite nonzero values satisfy 0.5 <= abs(value * 2^-exponent) < 1, including subnormals.
 * Extract only the exponent without promotion to double or runtime math calls.
 */
static inline int audio_math_frexp_exponent_f32(float value) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    uint32_t magnitude = audio_math_float32_bits_sse2(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
#else
    uint32_t magnitude = audio_math_float32_bits(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
#endif
    unsigned exponent = magnitude >> 23;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Return normal values first; the unsigned interval includes only exponent fields 1..254. */
    if (exponent - 1u < 254u)
        return (int)exponent - 126;
    if (magnitude == 0 || exponent == 255)
        return 0;
#else
    if (exponent == 255 || magnitude == 0)
        return 0;
    if (exponent != 0)
        return (int)exponent - 126;
#endif
    return -117 - (int)audio_math_count_leading_zeros32(magnitude);
}

/* Copy a double sign bit without changing magnitude, negative zero, or NaN payload. */
static inline double audio_math_copy_sign_f64(double magnitude, double sign_source) {
    const uint64_t sign_mask = UINT64_C(0x8000000000000000);
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Copy binary64 register sign/magnitude bits without quieting signaling NaNs. */
    return audio_math_float64_from_bits_sse2((audio_math_float64_bits_sse2(magnitude) & ~sign_mask) |
                                  (audio_math_float64_bits_sse2(sign_source) & sign_mask));
#else
    return audio_math_float64_from_bits((audio_math_float64_bits(magnitude) & ~sign_mask) |
                                  (audio_math_float64_bits(sign_source) & sign_mask));
#endif
}

/* Scale double by 2^power with nearest-even rounding, rounding subnormal results only once at the end.
 * Support every int exponent, preserve signed zero, overflow to signed infinity, and quiet NaNs with payload retained.
 * Return values only; no libm errno, exceptions, or nondefault rounding modes.
 */
static inline double audio_math_scale_pow2_f64_fallback(double value, int power) {
    const uint64_t fraction_mask = UINT64_C(0x000fffffffffffff);
    const uint64_t hidden_bit = UINT64_C(0x0010000000000000);
    uint64_t bits = audio_math_float64_bits(value);
    uint64_t sign = bits & UINT64_C(0x8000000000000000);
    uint64_t fraction = bits & fraction_mask;
    int exponent = (int)((bits >> 52) & 2047);
    if (exponent == 2047) {
        if (fraction)
            bits |= UINT64_C(0x0008000000000000);
        return audio_math_float64_from_bits(bits);
    }
    if (exponent == 0) {
        if (fraction == 0)
            return value;
        unsigned shift = audio_math_count_leading_zeros64(fraction) - 11;
        /* Normalize nonzero subnormal significands with shifts 1..52. */
        fraction = audio_math_shift_left_bounded_u64(fraction, shift);
        exponent = 1 - (int)shift;
    } else {
        fraction |= hidden_bit;
    }

    int64_t scaled_exponent = (int64_t)exponent + power;
    if (scaled_exponent >= 2047)
        return audio_math_float64_from_bits(sign | UINT64_C(0x7ff0000000000000));
    if (scaled_exponent > 0)
        return audio_math_float64_from_bits(sign | ((uint64_t)scaled_exponent << 52) |
                                      (fraction & fraction_mask));
    if (scaled_exponent < -52)
        return audio_math_float64_from_bits(sign);

    unsigned shift = (unsigned)(1 - scaled_exponent);
    /* scaled_exponent is -52..0; the two shift counts are 1..53 and 0..52. */
    uint64_t retained = audio_math_shift_right_bounded_u64(fraction, shift);
    uint64_t halfway = audio_math_shift_left_bounded_u64(UINT64_C(1), shift - 1);
    uint64_t discarded = fraction & ((halfway << 1) - 1);
    if (discarded > halfway || (discarded == halfway && (retained & 1)))
        ++retained;
    /* Rounding the largest subnormal upward naturally makes retained the smallest normal bit pattern. */
    return audio_math_float64_from_bits(sign | retained);
}

/* Adjust only the exponent for normal-to-normal scaling; other inputs use full boundary handling.
 * Unsigned addition is modular. Input exponents are at most 2046; no int exponent can wrap the entire
 * unsigned range and return to 1..2046. Both interval checks therefore establish normal input/output,
 * without overflowing signed addition, normalization, or significand decomposition.
 */
static inline double audio_math_scale_pow2_f64(double value, int power) {
    const uint64_t exponent_mask = UINT64_C(0x7ff0000000000000);
    uint64_t bits = audio_math_float64_bits(value);
    unsigned exponent = (unsigned)((bits >> 52) & 2047);
    unsigned scaled_exponent = exponent + (unsigned)power;
    if (exponent - 1u < 2046u && scaled_exponent - 1u < 2046u)
        return audio_math_float64_from_bits((bits & ~exponent_mask) | ((uint64_t)scaled_exponent << 52));
    return audio_math_scale_pow2_f64_fallback(value, power);
}

/* Scale double exactly within caller-proven bounds, preserving zero's sign.
 * Nonzero input and its 2^power-scaled result must both be normal. Modify the exponent only,
 * without rounding, subnormal/infinity/NaN handling, or variable 64-bit shifts.
 */
static inline double audio_math_scale_pow2_normal_f64(double value, int power) {
    const uint64_t exponent_mask = UINT64_C(0x7ff0000000000000);
    uint64_t bits = audio_math_float64_bits(value);
    if ((bits & UINT64_C(0x7fffffffffffffff)) == 0)
        return value;
    unsigned exponent = (unsigned)((bits >> 52) & 2047);
    unsigned scaled_exponent = exponent + (unsigned)power;
    return audio_math_float64_from_bits((bits & ~exponent_mask) | ((uint64_t)scaled_exponent << 52));
}
#endif
