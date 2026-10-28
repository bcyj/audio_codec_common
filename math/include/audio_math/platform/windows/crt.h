/* Windows 10+ UCRT double-math entries. */
#ifndef AUDIO_MATH_WINDOWS_CRT_H
#define AUDIO_MATH_WINDOWS_CRT_H
typedef double(__cdecl *audio_math_windows_unary_math_fn)(double);
#ifdef __cplusplus
extern "C" {
#endif
double audio_math_windows_sin(double value);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
double audio_math_windows_cos(double value);
#ifdef __cplusplus
}
#endif
/* Backend availability: failed resolution may retry; both sin/cos report unavailability with NaN. */
#ifdef __cplusplus
extern "C" {
#endif
int audio_math_windows_math_ready(void);
#ifdef __cplusplus
}
#endif
#endif
