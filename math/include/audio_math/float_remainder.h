#ifndef AUDIO_MATH_DSP_FLOAT_REMAINDER_H
#define AUDIO_MATH_DSP_FLOAT_REMAINDER_H
#include <audio_math/numeric_bits.h>
#include <audio_math/bits.h>
#ifdef _WIN32
#include <audio_math/platform/windows/remainder.h>
#endif

/* Only native MSVC x64 kernel builds transfer bits via registers; aliases are header-local. */
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#define audio_math_remainder_f32_bits audio_math_float32_bits_sse2
#define audio_math_remainder_f32_from_bits audio_math_float32_from_bits_sse2
#else
#define audio_math_remainder_f32_bits audio_math_float32_bits
#define audio_math_remainder_f32_from_bits audio_math_float32_from_bits
#endif

/* Exceptional remainder return policy without errno/exception emulation. */
static inline uint32_t audio_math_remainder_special_f32_bits(uint32_t numerator, uint32_t divisor) {
#ifdef _WIN32
    return audio_math_windows_remainder_special_bits(numerator, divisor);
#else
    if ((numerator & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return numerator | UINT32_C(0x00400000);
    if ((divisor & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return divisor | UINT32_C(0x00400000);
    return UINT32_C(0x7fc00000);
#endif
}

/* Return exact binary32 x-trunc(x/y)*y with the dividend's sign.
 * Avoid overflowing quotients with normalized-significand binary long division, requiring at most 276 reductions.
 * Ordinary results are exactly representable without rounding; preserve signed zero and platform exceptional-value policy.
 */
static inline float audio_math_fmod_f32(float numerator, float divisor) {
    uint32_t x_bits = audio_math_remainder_f32_bits(numerator);
    uint32_t y_bits = audio_math_remainder_f32_bits(divisor);
    uint32_t x = x_bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    uint32_t y = y_bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    uint32_t sign = x_bits & AUDIO_MATH_FLOAT32_SIGN_BITS;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* When y is at most infinity and x < y, x is finite and y nonzero, so return x unchanged.
     * Integer comparisons preserve negative zero without prematurely returning NaN divisors or infinite dividends. */
    if (x < y && y <= AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return numerator;
#endif
    if (x >= AUDIO_MATH_FLOAT32_INFINITY_BITS || y > AUDIO_MATH_FLOAT32_INFINITY_BITS || y == 0)
        return audio_math_remainder_f32_from_bits(audio_math_remainder_special_f32_bits(x_bits, y_bits));
    if (x < y)
        return numerator;
    if (x == y)
        return audio_math_remainder_f32_from_bits(sign);

    int x_exponent = (int)(x >> 23) - 127;
    int y_exponent = (int)(y >> 23) - 127;
    if (x >> 23) {
        x = (x & UINT32_C(0x007fffff)) | UINT32_C(0x00800000);
    } else {
        unsigned shift = audio_math_count_leading_zeros32(x) - 8;
        x <<= shift;
        x_exponent = -126 - (int)shift;
    }
    if (y >> 23) {
        y = (y & UINT32_C(0x007fffff)) | UINT32_C(0x00800000);
    } else {
        unsigned shift = audio_math_count_leading_zeros32(y) - 8;
        y <<= shift;
        y_exponent = -126 - (int)shift;
    }
    while (x_exponent > y_exponent) {
        if (x >= y)
            x -= y;
        if (x == 0)
            return audio_math_remainder_f32_from_bits(sign);
        x <<= 1;
        --x_exponent;
    }
    if (x >= y)
        x -= y;
    if (x == 0)
        return audio_math_remainder_f32_from_bits(sign);

    /* Subnormal divisors retain trailing significand zeros, so shifting back to binary32 units loses no bits. */
    if (y_exponent < -126)
        return audio_math_remainder_f32_from_bits(sign | (x >> (unsigned)(-126 - y_exponent)));
    unsigned shift = audio_math_count_leading_zeros32(x) - 8;
    int result_exponent = y_exponent - (int)shift;
    if (result_exponent < -126)
        return audio_math_remainder_f32_from_bits(sign | (x << (unsigned)(y_exponent + 126)));
    return audio_math_remainder_f32_from_bits(sign | ((uint32_t)(result_exponent + 127) << 23) |
                                  ((x << shift) & UINT32_C(0x007fffff)));
}
#undef audio_math_remainder_f32_bits
#undef audio_math_remainder_f32_from_bits
#endif
