#ifndef AUDIO_MATH_DSP_FLOAT_ROUNDING_H
#define AUDIO_MATH_DSP_FLOAT_ROUNDING_H
#include <audio_math/numeric_bits.h>
#include <audio_math/compiler.h>
#include <audio_math/bits.h>
#include <audio_math/platform/arm/rounding.h>

/* Return values only; no rounding-mode access or libm errno/exception guarantees. */
typedef enum AudioMathIntegralRounding {
    AUDIO_MATH_INTEGRAL_TRUNCATE,
    AUDIO_MATH_INTEGRAL_FLOOR,
    AUDIO_MATH_INTEGRAL_CEIL,
    AUDIO_MATH_INTEGRAL_NEAREST_AWAY
} AudioMathIntegralRounding;

/* Round binary32/binary64 in the specified direction; format/mode are compile-time call-site constants.
 * Preserve negative zero/infinity; quiet NaNs while retaining sign/payload, without numeric integer conversion.
 */
static AUDIO_MATH_FORCE_INLINE uint64_t audio_math_integral_round_bits(uint64_t bits, unsigned fraction_bits,
                                                           unsigned exponent_bits, unsigned bias,
                                                           AudioMathIntegralRounding mode) {
    uint64_t sign_mask = audio_math_shift_left_bounded_u64(UINT64_C(1), fraction_bits + exponent_bits);
    uint64_t sign = bits & sign_mask;
    uint64_t magnitude = bits & (sign_mask - 1);
    uint64_t fraction_mask = audio_math_shift_left_bounded_u64(UINT64_C(1), fraction_bits) - 1;
    unsigned exponent_mask = (1u << exponent_bits) - 1;
    unsigned exponent = (unsigned)audio_math_shift_right_bounded_u64(bits, fraction_bits) & exponent_mask;

    if (exponent >= bias + fraction_bits) {
        if (exponent == exponent_mask && (bits & fraction_mask))
            return bits | audio_math_shift_left_bounded_u64(UINT64_C(1), fraction_bits - 1);
        return bits;
    }
    if (exponent < bias) {
        if (!magnitude)
            return bits;
        int increase = (mode == AUDIO_MATH_INTEGRAL_FLOOR && sign) ||
                       (mode == AUDIO_MATH_INTEGRAL_CEIL && !sign) ||
                       (mode == AUDIO_MATH_INTEGRAL_NEAREST_AWAY && exponent == bias - 1);
        return sign | (increase ? audio_math_shift_left_bounded_u64(bias, fraction_bits) : 0);
    }

    unsigned fractional_places = fraction_bits - (exponent - bias);
    /* Here bias <= exponent < bias + fraction_bits, giving counts 1..23/52. */
    uint64_t unit = audio_math_shift_left_bounded_u64(UINT64_C(1), fractional_places);
    uint64_t discarded = bits & (unit - 1);
    uint64_t integral = bits & ~(unit - 1);
    if (!discarded)
        return bits;
    int increase = (mode == AUDIO_MATH_INTEGRAL_FLOOR && sign) || (mode == AUDIO_MATH_INTEGRAL_CEIL && !sign) ||
                   (mode == AUDIO_MATH_INTEGRAL_NEAREST_AWAY && discarded >= unit / 2);
    return integral + (increase ? unit : 0);
}

/* Shared rounding algorithm; register bit transfers are native x64 kernel only. */
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#define audio_math_rounding_f32_bits audio_math_float32_bits_sse2
#define audio_math_rounding_f32_from_bits audio_math_float32_from_bits_sse2
#define audio_math_rounding_f64_bits audio_math_float64_bits_sse2
#define audio_math_rounding_f64_from_bits audio_math_float64_from_bits_sse2
#else
#define audio_math_rounding_f32_bits audio_math_float32_bits
#define audio_math_rounding_f32_from_bits audio_math_float32_from_bits
#define audio_math_rounding_f64_bits audio_math_float64_bits
#define audio_math_rounding_f64_from_bits audio_math_float64_from_bits
#endif

/* Truncate single precision toward zero, returning single precision. */
static inline float audio_math_trunc_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_trunc_f32(value);
#else
    return audio_math_rounding_f32_from_bits((uint32_t)audio_math_integral_round_bits(
        audio_math_rounding_f32_bits(value), 23, 8, 127, AUDIO_MATH_INTEGRAL_TRUNCATE));
#endif
}
/* Round single precision toward negative infinity. */
static inline float audio_math_floor_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_floor_f32(value);
#else
    return audio_math_rounding_f32_from_bits((uint32_t)audio_math_integral_round_bits(audio_math_rounding_f32_bits(value), 23,
                                                                     8, 127, AUDIO_MATH_INTEGRAL_FLOOR));
#endif
}
/* Round single precision toward positive infinity. */
static inline float audio_math_ceil_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_ceil_f32(value);
#else
    return audio_math_rounding_f32_from_bits((uint32_t)audio_math_integral_round_bits(audio_math_rounding_f32_bits(value), 23,
                                                                     8, 127, AUDIO_MATH_INTEGRAL_CEIL));
#endif
}
/* Round single precision to nearest, with ties away from zero. */
static inline float audio_math_round_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_round_f32(value);
#else
    return audio_math_rounding_f32_from_bits((uint32_t)audio_math_integral_round_bits(
        audio_math_rounding_f32_bits(value), 23, 8, 127, AUDIO_MATH_INTEGRAL_NEAREST_AWAY));
#endif
}
/* Truncate double precision toward zero, returning double precision. */
static inline double audio_math_trunc_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_trunc_f64(value);
#else
    return audio_math_rounding_f64_from_bits(
        audio_math_integral_round_bits(audio_math_rounding_f64_bits(value), 52, 11, 1023, AUDIO_MATH_INTEGRAL_TRUNCATE));
#endif
}
/* Round double precision toward negative infinity. */
static inline double audio_math_floor_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_floor_f64(value);
#else
    return audio_math_rounding_f64_from_bits(
        audio_math_integral_round_bits(audio_math_rounding_f64_bits(value), 52, 11, 1023, AUDIO_MATH_INTEGRAL_FLOOR));
#endif
}
/* Round double precision toward positive infinity. */
static inline double audio_math_ceil_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_ceil_f64(value);
#else
    return audio_math_rounding_f64_from_bits(
        audio_math_integral_round_bits(audio_math_rounding_f64_bits(value), 52, 11, 1023, AUDIO_MATH_INTEGRAL_CEIL));
#endif
}
/* Round double precision to nearest, with ties away from zero. */
static inline double audio_math_round_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_ROUNDING
    return audio_math_arm_round_f64(value);
#else
    return audio_math_rounding_f64_from_bits(audio_math_integral_round_bits(audio_math_rounding_f64_bits(value), 52, 11, 1023,
                                                           AUDIO_MATH_INTEGRAL_NEAREST_AWAY));
#endif
}
#undef audio_math_rounding_f32_bits
#undef audio_math_rounding_f32_from_bits
#undef audio_math_rounding_f64_bits
#undef audio_math_rounding_f64_from_bits
#endif
