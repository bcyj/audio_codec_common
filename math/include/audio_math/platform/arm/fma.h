#ifndef AUDIO_MATH_ARM_FMA_H
#define AUDIO_MATH_ARM_FMA_H
#include <audio_math/numeric_bits.h>

/* Uses native ARM fused multiply-add while preserving the existing non-finite policy. */
static inline float audio_math_platform_fma_f32(float a, float b, float c) {
    if ((audio_math_float32_bits(a) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) ||
        (audio_math_float32_bits(b) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) ||
        (audio_math_float32_bits(c) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return a * b + c;
    return __builtin_fmaf(a, b, c);
}

/* Uses native double-precision ARM FMA while preserving non-finite evaluation order. */
static inline double audio_math_platform_fma_f64(double a, double b, double c) {
    if ((audio_math_float64_bits(a) & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) ||
        (audio_math_float64_bits(b) & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) ||
        (audio_math_float64_bits(c) & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000))
        return a * b + c;
    return __builtin_fma(a, b, c);
}
#endif
