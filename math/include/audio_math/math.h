#ifndef AUDIO_MATH_H
#define AUDIO_MATH_H
#include <math.h>
#ifdef __ANDROID__
#include <audio_math/platform/android/math.h>
#endif
#ifdef _WIN32
#include <audio_math/platform/windows/trig.h>
#include <audio_math/platform/windows/exp_log.h>
#ifdef __MINGW32__
#include <audio_math/platform/windows/crt.h>
#endif
#endif

/* Explicitly select verified platform math without redefining standard-library names. */
static inline float audio_math_sinf(float x) {
#ifdef _WIN32
    return audio_math_win_sinf(x);
#else
    return sinf(x);
#endif
}
/* Compute single-precision cosine under platform numeric policy. */
static inline float audio_math_cosf(float x) {
#ifdef _WIN32
    return audio_math_win_cosf(x);
#else
    return cosf(x);
#endif
}
/* Compute float sine/cosine together; outputs may alias and retain cosine last.
 * Windows reuses joint evaluation; other hosts retain separate-function return policies. */
static inline void audio_math_sincosf(float x, float *sine, float *cosine) {
#ifdef _WIN32
    audio_math_win_sincosf(x, sine, cosine);
#else
    float cosine_value = audio_math_cosf(x);
    float sine_value = audio_math_sinf(x);
    *sine = sine_value;
    *cosine = cosine_value;
#endif
}
/* Compute single-precision power of two under platform numeric policy. */
static inline float audio_math_exp2f(float x) {
#ifdef _WIN32
    return audio_math_win_exp2f(x);
#else
    return exp2f(x);
#endif
}
/* Compute single-precision binary logarithm under platform numeric policy. */
static inline float audio_math_log2f(float x) {
#ifdef _WIN32
    return audio_math_win_log2f(x);
#else
    return log2f(x);
#endif
}
/* Project-name mappings for direct host calls preserve builtin recognition and expression conversion boundaries.
 * Mappings contain no parameters/casts, add no evaluations, and do not redefine standard-library names.
 * These entries still depend on host math; later independent backends are selected centrally here. */
/* Compute single-precision natural logarithm. */
#define audio_math_logf logf
/* Compute single-precision decimal logarithm. */
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && defined(_M_IX86)
#include <audio_math/float_log.h>
#define audio_math_log10f audio_math_log10_f32_soft
#else
#define audio_math_log10f log10f
#endif
/* Compute single-precision natural exponent. */
#define audio_math_expf expf
/* Compute single-precision arcsine. */
#define audio_math_asinf asinf
/* Compute single-precision real power. */
#define audio_math_powf powf
/* Compute double-precision natural logarithm. */
#define audio_math_log log
/* Compute double-precision decimal logarithm. */
#define audio_math_log10 log10
/* Compute double-precision power of two. */
#define audio_math_exp2 exp2
/* Compute double-precision real power. */
#define audio_math_pow pow
/* Retain host double trig for existing dynamic-table initialization.
 * This does not apply MinGW's UCRT binding below; backend changes need separate coefficient/output validation. */
#define audio_math_host_sin sin
#define audio_math_host_cos cos
/* Select verified double trig; MinGW explicitly binds UCRT. */
static inline double audio_math_sin(double x) {
#if defined(_WIN32) && defined(__MINGW32__)
    return audio_math_windows_sin(x);
#else
    return sin(x);
#endif
}
/* Select double cosine with the same policy as audio_math_sin. */
static inline double audio_math_cos(double x) {
#if defined(_WIN32) && defined(__MINGW32__)
    return audio_math_windows_cos(x);
#else
    return cos(x);
#endif
}
#ifndef AUDIO_MATH_PLATFORM_HAS_SINCOS
/* Compute sine then cosine in existing order if no joint platform entry is available. */
static inline void audio_math_sincos(double x, double *sine, double *cosine) {
    *sine = audio_math_sin(x);
    *cosine = audio_math_cos(x);
}
#endif
#endif
