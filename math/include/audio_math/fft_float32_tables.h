#ifndef AUDIO_MATH_FFT_FLOAT32_TABLES_H
#define AUDIO_MATH_FFT_FLOAT32_TABLES_H
#include <audio_math/fft_float32.h>
#ifdef __cplusplus
extern "C" {
#endif
extern const int32_t audio_math_fft_512_factors[10];
extern const AudioMathComplex32 audio_math_fft_512_twiddles[2][512];
extern const uint16_t audio_math_fft_permutation_512[512];
const uint32_t *audio_math_fft_static_twiddles(int32_t length, int inverse);
#if AUDIO_MATH_FFT_ITERATIVE_STATIC
const uint16_t *audio_math_fft_permutation(int n, const int32_t *factors);
#endif
#ifdef __cplusplus
}
#endif
#endif
