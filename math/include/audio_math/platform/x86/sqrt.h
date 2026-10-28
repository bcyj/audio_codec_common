#ifndef AUDIO_MATH_X86_SQRT_H
#define AUDIO_MATH_X86_SQRT_H
#if defined(_M_X64) ||                                                                             \
    (!defined(_KERNEL_MODE) && ((defined(_M_IX86_FP) && _M_IX86_FP >= 1) || defined(__SSE__)))
#include <xmmintrin.h>
#define AUDIO_MATH_X86_NATIVE_SQRT 1
/* Use baseline SSE float square root without FMA, AVX, or runtime dispatch. */
static inline float audio_math_x86_sqrt_f32(float value) {
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
}
#endif
#if defined(_M_X64) ||                                                                             \
    (!defined(_KERNEL_MODE) && ((defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)))
#include <emmintrin.h>
#define AUDIO_MATH_X86_NATIVE_SQRT_F64 1
/* Use baseline SSE2 double square root without AVX/FMA. */
static inline double audio_math_x86_sqrt_f64(double value) {
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(value)));
}
#endif
#endif
