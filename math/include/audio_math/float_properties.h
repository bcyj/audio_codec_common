#ifndef AUDIO_MATH_DSP_FLOAT_PROPERTIES_H
#define AUDIO_MATH_DSP_FLOAT_PROPERTIES_H
#include <audio_math/numeric_bits.h>
#include <audio_math/platform/arm/properties.h>

/* Native x64 kernel builds transfer bits through registers; other builds retain generic copying.
 * Header-local aliases avoid extra call boundaries or changes to ordinary SDK compilation. */
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#define audio_math_properties_f32_bits audio_math_float32_bits_sse2
#define audio_math_properties_f64_bits audio_math_float64_bits_sse2
#define audio_math_properties_f64_from_bits audio_math_float64_from_bits_sse2
#else
#define audio_math_properties_f32_bits audio_math_float32_bits
#define audio_math_properties_f64_bits audio_math_float64_bits
#define audio_math_properties_f64_from_bits audio_math_float64_from_bits
#endif

/* Return numeric properties without reading/changing rounding mode or promising CRT errno/exception flags. */

/* Test single-precision NaN; return one for signaling and quiet NaNs. */
static inline int audio_math_is_nan_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_nan_f32(value);
#else
    return (audio_math_properties_f32_bits(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS;
#endif
}
/* Test double-precision NaN without floating comparisons. */
static inline int audio_math_is_nan_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_nan_f64(value);
#else
    return (audio_math_properties_f64_bits(value) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT64_INFINITY_BITS;
#endif
}
/* Test positive/negative single-precision infinity. */
static inline int audio_math_is_infinite_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_infinite_f32(value);
#else
    return (audio_math_properties_f32_bits(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) == AUDIO_MATH_FLOAT32_INFINITY_BITS;
#endif
}
/* Test positive/negative double-precision infinity. */
static inline int audio_math_is_infinite_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_infinite_f64(value);
#else
    return (audio_math_properties_f64_bits(value) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS) == AUDIO_MATH_FLOAT64_INFINITY_BITS;
#endif
}
/* Test finite single precision, including signed zeros and all subnormals. */
static inline int audio_math_is_finite_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_finite_f32(value);
#else
    return (audio_math_properties_f32_bits(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) < AUDIO_MATH_FLOAT32_INFINITY_BITS;
#endif
}
/* Test finite double precision independently of host classification runtimes. */
static inline int audio_math_is_finite_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_is_finite_f64(value);
#else
    return (audio_math_properties_f64_bits(value) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS) < AUDIO_MATH_FLOAT64_INFINITY_BITS;
#endif
}
/* Clear the float sign bit for absolute value, making negative zero positive and preserving magnitude bits. */
static inline float audio_math_abs_f32(float value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_abs_f32(value);
#else
    uint32_t magnitude = audio_math_properties_f32_bits(value) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
#ifdef _WIN32
    /* Both Windows word sizes quiet signaling NaNs. */
    if (magnitude > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        magnitude |= UINT32_C(0x00400000);
#endif
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Copy sign-cleared/quieted bits without floating arithmetic. */
    return audio_math_float32_from_bits_sse2(magnitude);
#else
    return audio_math_float32_from_bits(magnitude);
#endif
#endif
}
/* Clear the double sign bit for absolute value, retaining NaN payloads and host return boundaries. */
static inline double audio_math_abs_f64(double value) {
#ifdef AUDIO_MATH_ARM_NATIVE_PROPERTIES
    return audio_math_arm_abs_f64(value);
#else
    uint64_t magnitude = audio_math_properties_f64_bits(value) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS;
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    /* Win32 return handling quiets signaling NaNs. */
    if (magnitude > AUDIO_MATH_FLOAT64_INFINITY_BITS)
        magnitude |= UINT64_C(0x0008000000000000);
#endif
    return audio_math_properties_f64_from_bits(magnitude);
#endif
}
#undef audio_math_properties_f32_bits
#undef audio_math_properties_f64_bits
#undef audio_math_properties_f64_from_bits
#endif
