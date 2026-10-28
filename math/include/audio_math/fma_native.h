#ifndef AUDIO_MATH_DSP_FMA_NATIVE_H
#define AUDIO_MATH_DSP_FMA_NATIVE_H
#include <audio_math/fma_soft.h>
#include <audio_math/platform/fma.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Prefer the target's native fused multiply-add, otherwise inline the software fast path. */
static inline float audio_math_fma_f32(float a, float b, float c) {
#if AUDIO_MATH_PLATFORM_NATIVE_FMA
    return audio_math_platform_fma_f32(a, b, c);
#else
    return audio_math_fma_soft_f32(a, b, c);
#endif
}

/* Prefer native double-precision fused multiply-add, otherwise use the common integer backend. */
static inline double audio_math_fma_f64(double a, double b, double c) {
#if AUDIO_MATH_PLATFORM_NATIVE_FMA
    return audio_math_platform_fma_f64(a, b, c);
#else
    return audio_math_fma_soft_f64(a, b, c);
#endif
}
#ifdef __cplusplus
}
#endif
#endif
