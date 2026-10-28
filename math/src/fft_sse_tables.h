/* Read-only static twiddle and bit-reversal table declarations for the SSE FFT. */
#ifndef AUDIO_MATH_FFT_SSE_TABLES_H
#define AUDIO_MATH_FFT_SSE_TABLES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const float audio_math_fft_sse_cos_16[8];
extern const float audio_math_fft_sse_cos_32[16];
extern const float audio_math_fft_sse_cos_64[32];
extern const float audio_math_fft_sse_cos_128[64];
extern const float audio_math_fft_sse_cos_256[128];
extern const float audio_math_fft_sse_cos_512[256];
extern const float audio_math_fft_sse_cos_1024[512];

extern const float *const audio_math_fft_sse_cos_tabs[11];

extern const uint16_t audio_math_fft_sse_revtab_4[2][4];
extern const uint16_t audio_math_fft_sse_revtab_8[2][8];
extern const uint16_t audio_math_fft_sse_revtab_16[2][16];
extern const uint16_t audio_math_fft_sse_revtab_32[2][32];
extern const uint16_t audio_math_fft_sse_revtab_64[2][64];
extern const uint16_t audio_math_fft_sse_revtab_128[2][128];
extern const uint16_t audio_math_fft_sse_revtab_256[2][256];
extern const uint16_t audio_math_fft_sse_revtab_512[2][512];
extern const uint16_t audio_math_fft_sse_revtab_1024[2][1024];

const uint16_t *audio_math_fft_sse_get_revtab(int nbits, int inverse);

#ifdef __cplusplus
}
#endif

#endif
