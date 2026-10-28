#ifndef AUDIO_MATH_NUMERIC_BITS_H
#define AUDIO_MATH_NUMERIC_BITS_H
#include <float.h>
#include <stdint.h>
#include <string.h>
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#include <audio_math/compiler.h>
#include <emmintrin.h>
#endif

/* IEEE binary32 fields/conversion-boundary bits for classification without floating arithmetic. */
#define AUDIO_MATH_FLOAT32_SIGN_BITS UINT32_C(0x80000000)
#define AUDIO_MATH_FLOAT32_MAGNITUDE_BITS UINT32_C(0x7fffffff)
#define AUDIO_MATH_FLOAT32_INFINITY_BITS UINT32_C(0x7f800000)
#define AUDIO_MATH_FLOAT32_POW2_31_BITS UINT32_C(0x4f000000)
#define AUDIO_MATH_FLOAT32_POW2_32_BITS UINT32_C(0x4f800000)
#define AUDIO_MATH_FLOAT64_SIGN_BITS UINT64_C(0x8000000000000000)
#define AUDIO_MATH_FLOAT64_MAGNITUDE_BITS UINT64_C(0x7fffffffffffffff)
#define AUDIO_MATH_FLOAT64_INFINITY_BITS UINT64_C(0x7ff0000000000000)
#define AUDIO_MATH_FLOAT64_POW2_31_BITS UINT64_C(0x41e0000000000000)
/* Bits of binary32 finite-normal endpoints promoted exactly to binary64. */
#define AUDIO_MATH_FLOAT64_FLOAT32_MIN_NORMAL_BITS UINT64_C(0x3810000000000000)
#define AUDIO_MATH_FLOAT64_FLOAT32_MAX_FINITE_BITS UINT64_C(0x47efffffe0000000)
#if defined(__cplusplus)
static_assert(sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                  DBL_MANT_DIG == 53,
              "IEEE binary32/binary64 required");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   DBL_MANT_DIG == 53,
               "IEEE binary32/binary64 required");
#else
/* Public headers retain compile-time format checks for MSVC/C99 callers without C11. */
typedef char
    AudioMathIeeeBinaryFormatRequired[(sizeof(float) == 4 && sizeof(double) == 8 &&
                                       FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53)
                                          ? 1
                                          : -1];
#endif

/* Reinterpret 32-bit integer bits without numeric conversion. */
static inline int32_t audio_math_int32_from_bits(uint32_t bits) {
    int32_t value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
/* Reinterpret 64-bit integer bits without implementation-defined conversion. */
static inline int64_t audio_math_int64_from_bits(uint64_t bits) {
    int64_t value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
/* Construct float from IEEE-754 bits without floating arithmetic. */
static inline float audio_math_float32_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
/* Read a float's IEEE-754 bits. */
static inline uint32_t audio_math_float32_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
/* Construct double from IEEE-754 bits. */
static inline double audio_math_float64_from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}
/* Read a double's IEEE-754 bits. */
static inline uint64_t audio_math_float64_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
/* Read binary32 register bits directly without numeric conversion.
 * Native MSVC x64 kernel only; retain NaN payloads, subnormals, and signed zero. */
static AUDIO_MATH_FORCE_INLINE uint32_t audio_math_float32_bits_sse2(float value) {
    return (uint32_t)_mm_cvtsi128_si32(_mm_castps_si128(_mm_set_ss(value)));
}
/* Copy integer bits into a binary32 register without floating arithmetic.
 * Signed arguments are interpreted as raw bits only on this restricted MSVC x64 target. */
static AUDIO_MATH_FORCE_INLINE float audio_math_float32_from_bits_sse2(uint32_t bits) {
    return _mm_cvtss_f32(_mm_castsi128_ps(_mm_cvtsi32_si128((int)bits)));
}
/* Read binary64 register bits directly in native MSVC x64 kernel builds.
 * MOVQ performs no conversion, preserving NaN payloads, subnormals, and signed zero bitwise.
 * Separate from generic memcpy so address-based table reads can remain integer memory loads. */
static AUDIO_MATH_FORCE_INLINE uint64_t audio_math_float64_bits_sse2(double value) {
    return (uint64_t)_mm_cvtsi128_si64(_mm_castpd_si128(_mm_set_sd(value)));
}
/* Copy integer bits directly into binary64 registers without precision/rounding changes.
 * Out-of-signed-range argument interpretation relies only on this restricted MSVC x64 target. */
static AUDIO_MATH_FORCE_INLINE double audio_math_float64_from_bits_sse2(uint64_t bits) {
    return _mm_cvtsd_f64(_mm_castsi128_pd(_mm_cvtsi64_si128((__int64)bits)));
}
#endif

/* Construct function-return bits, retaining Win32 x87 quieting of signaling NaNs.
 * Raw interpretation still uses audio_math_float32_from_bits; this entry applies FP-return format
 * conventions.
 */
static inline float audio_math_float32_from_return_bits(uint32_t bits) {
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) && (bits & UINT32_C(0x007fffff)))
        bits |= UINT32_C(0x00400000);
#endif
    return audio_math_float32_from_bits(bits);
}
#endif
