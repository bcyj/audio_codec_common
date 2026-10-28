#ifndef AUDIO_MATH_X86_FMA_H
#define AUDIO_MATH_X86_FMA_H
#include <audio_math/numeric_bits.h>
#include <immintrin.h>

#if defined(_KERNEL_MODE)
#error "Hardware FMA is prohibited in _KERNEL_MODE due to VEX/XSTATE processor state constraints."
#endif

/* Native x86/x64 hardware FMA; nonfinite values retain the existing separate-operation NaN policy. */
static inline float audio_math_platform_fma_f32(float a, float b, float c) {
    if ((audio_math_float32_bits(a) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) ||
        (audio_math_float32_bits(b) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) ||
        (audio_math_float32_bits(c) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000))
        return a * b + c;
    return _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(a), _mm_set_ss(b), _mm_set_ss(c)));
}

/* Native double-precision x86/x64 FMA preserving nonfinite operand processing order. */
static inline double audio_math_platform_fma_f64(double a, double b, double c) {
    if ((audio_math_float64_bits(a) & UINT64_C(0x7ff0000000000000)) ==
            UINT64_C(0x7ff0000000000000) ||
        (audio_math_float64_bits(b) & UINT64_C(0x7ff0000000000000)) ==
            UINT64_C(0x7ff0000000000000) ||
        (audio_math_float64_bits(c) & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000))
        return a * b + c;
    return _mm_cvtsd_f64(_mm_fmadd_sd(_mm_set_sd(a), _mm_set_sd(b), _mm_set_sd(c)));
}
#endif
