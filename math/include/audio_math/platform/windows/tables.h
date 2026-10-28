#ifndef AUDIO_MATH_WINDOWS_TABLES_H
#define AUDIO_MATH_WINDOWS_TABLES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern const uint32_t audio_math_win_inv_pi_over_four[24];
#ifdef __cplusplus
}
#endif
#if !(defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64))
#ifdef __cplusplus
extern "C" {
#endif
extern const uint64_t audio_math_win_exp2_table[32];
#ifdef __cplusplus
}
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern const double audio_math_win_log2_table[16][2];
#ifdef __cplusplus
}
#endif
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
#ifdef __cplusplus
extern "C" {
#endif
extern const double audio_math_win_expf_sse2_table[64];
#ifdef __cplusplus
}
#endif
#endif
#endif
