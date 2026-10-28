#ifndef AUDIO_MATH_WINDOWS_EXP_LOG_H
#define AUDIO_MATH_WINDOWS_EXP_LOG_H
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_win_exp2f(float input);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_win_log2f(float input);
#ifdef __cplusplus
}
#endif
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_windows_expf_sse2_soft(float input);
#ifdef __cplusplus
}
#endif
#endif
#endif
