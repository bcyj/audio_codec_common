#ifndef AUDIO_MATH_DSP_FLOAT_SQRT_H
#define AUDIO_MATH_DSP_FLOAT_SQRT_H
#include <audio_math/numeric_bits.h>
#include <audio_math/bits.h>
#include <audio_math/platform/arm/sqrt.h>
#include <audio_math/platform/x86/sqrt.h>

/* Select quiet-NaN bits for negative square roots without errno/exception emulation. */
static inline uint32_t audio_math_sqrt_invalid_f32_bits(void) {
#if defined(_WIN32) || defined(__i386__) || defined(__x86_64__)
    return UINT32_C(0xffc00000);
#else
    return UINT32_C(0x7fc00000);
#endif
}

/* Exact common C binary32 square root with nearest-even rounding.
 * Extract 24 digit pairs; round upward when the remainder exceeds the integer root. Integer radicands cannot tie exactly.
 * Quiet NaNs with sign/payload retained; preserve negative zero. Finite nonzero results are always normal.
 */
static inline float audio_math_sqrt_soft_f32(float value) {
    uint32_t bits = audio_math_float32_bits(value);
    uint32_t magnitude = bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if (magnitude > AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return audio_math_float32_from_bits(bits | UINT32_C(0x00400000));
    if (!magnitude)
        return value;
    if (bits & AUDIO_MATH_FLOAT32_SIGN_BITS)
        return audio_math_float32_from_bits(audio_math_sqrt_invalid_f32_bits());
    if (magnitude == AUDIO_MATH_FLOAT32_INFINITY_BITS)
        return value;

    int exponent = (int)(bits >> 23) - 127;
    uint32_t significand;
    if (bits >> 23) {
        significand = (bits & UINT32_C(0x007fffff)) | UINT32_C(0x00800000);
    } else {
        unsigned shift = audio_math_count_leading_zeros32(bits) - 8;
        significand = bits << shift;
        exponent = -126 - (int)shift;
    }
    unsigned odd = (unsigned)exponent & 1u;
    int result_exponent = (exponent - (int)odd) / 2;
    /* Split the 48-bit radicand into high 16/low 32 bits to avoid wide-shift runtime calls on 32-bit hosts. */
    uint32_t radicand_high = significand >> (9 - odd);
    uint32_t radicand_low = significand << (23 + odd);
    uint32_t root = 0;
    uint32_t remainder = 0;
    for (unsigned pair = 0; pair < 24; ++pair) {
        remainder = (remainder << 2) | (radicand_high >> 14);
        radicand_high = ((radicand_high << 2) | (radicand_low >> 30)) & UINT32_C(0xffff);
        radicand_low <<= 2;
        uint32_t trial = (root << 2) | 1u;
        root <<= 1;
        if (remainder >= trial) {
            remainder -= trial;
            root |= 1u;
        }
    }
    if (remainder > root)
        ++root;
    if (root == UINT32_C(0x01000000)) {
        root >>= 1;
        ++result_exponent;
    }
    return audio_math_float32_from_bits(((uint32_t)(result_exponent + 127) << 23) |
                                  (root & UINT32_C(0x007fffff)));
}

/* Common square-root entry using known native ISA instructions or exact C fallback.
 * Guarantee default nearest-even returns with subnormal input preserved; do not change caller FP state.
 */
static inline float audio_math_sqrt_f32(float value) {
#if defined(AUDIO_MATH_X86_NATIVE_SQRT)
    return audio_math_x86_sqrt_f32(value);
#elif defined(AUDIO_MATH_ARM_NATIVE_SQRT)
    return audio_math_arm_sqrt_f32(value);
#else
    return audio_math_sqrt_soft_f32(value);
#endif
}

/* Exact common C binary64 square root with nearest-even rounding and subnormal support.
 * Split the 106-bit radicand into high 42/low 64 bits; 53 steps produce an integer root/exact remainder.
 * Midpoint squares have a quarter-unit tail, so integer radicands cannot fall exactly on a rounding midpoint.
 * No errno, exceptions, or nondefault-rounding emulation. */
static inline double audio_math_sqrt_soft_f64(double value) {
    const uint64_t fraction_mask = UINT64_C(0x000fffffffffffff);
    const uint64_t hidden_bit = UINT64_C(0x0010000000000000);
    const uint64_t infinity = UINT64_C(0x7ff0000000000000);
    uint64_t bits = audio_math_float64_bits(value);
    uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
    if (magnitude > infinity)
        return audio_math_float64_from_bits(bits | UINT64_C(0x0008000000000000));
    if (!magnitude)
        return value;
    if (bits >> 63) {
#if defined(_WIN32) || defined(__i386__) || defined(__x86_64__)
        return audio_math_float64_from_bits(UINT64_C(0xfff8000000000000));
#else
        return audio_math_float64_from_bits(UINT64_C(0x7ff8000000000000));
#endif
    }
    if (magnitude == infinity)
        return value;

    int exponent = (int)(bits >> 52) - 1023;
    uint64_t significand = bits & fraction_mask;
    if (bits >> 52) {
        significand |= hidden_bit;
    } else {
        exponent = -1022;
        /* Constant shifts prevent ARM32 runtime calls for variable wide-integer shifts. */
        while (!(significand & hidden_bit)) {
            significand <<= 1;
            --exponent;
        }
    }
    unsigned odd = (unsigned)exponent & 1u;
    int result_exponent = (exponent - (int)odd) / 2;
    uint64_t radicand_high = odd ? significand >> 11 : significand >> 12;
    uint64_t radicand_low = odd ? significand << 53 : significand << 52;
    uint64_t root = 0;
    uint64_t remainder = 0;
    for (unsigned pair = 0; pair < 53; ++pair) {
        remainder = (remainder << 2) | (radicand_high >> 40);
        radicand_high = ((radicand_high << 2) | (radicand_low >> 62)) & UINT64_C(0x3ffffffffff);
        radicand_low <<= 2;
        uint64_t trial = (root << 2) | 1u;
        root <<= 1;
        if (remainder >= trial) {
            remainder -= trial;
            root |= 1u;
        }
    }
    if (remainder > root)
        ++root;
    if (root == (hidden_bit << 1)) {
        root >>= 1;
        ++result_exponent;
    }
    return audio_math_float64_from_bits(((uint64_t)(result_exponent + 1023) << 52) |
                                  (root & fraction_mask));
}

/* Select native double square root by ISA or use exact C fallback.
 * Preserve caller FP state and double square-root precision. */
static inline double audio_math_sqrt_f64(double value) {
#if defined(AUDIO_MATH_X86_NATIVE_SQRT_F64)
    return audio_math_x86_sqrt_f64(value);
#elif defined(AUDIO_MATH_ARM_NATIVE_SQRT_F64)
    return audio_math_arm_sqrt_f64(value);
#else
    return audio_math_sqrt_soft_f64(value);
#endif
}
#endif
