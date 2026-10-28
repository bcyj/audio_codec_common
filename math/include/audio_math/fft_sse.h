/* Optional split-radix SSE/SSE2 FFT interface.
 * AUDIO_MATH_FFT_SPLIT_RADIX_SSE defaults to 0; vector kernels and read-only tables are
 * compiled only when it is enabled.
 */
#ifndef AUDIO_MATH_FFT_SSE_H
#define AUDIO_MATH_FFT_SSE_H

#include "audio_math/fft_float32.h"
#include <stdint.h>

#ifndef AUDIO_MATH_FFT_SPLIT_RADIX_SSE
#define AUDIO_MATH_FFT_SPLIT_RADIX_SSE 0
#endif

#if AUDIO_MATH_FFT_SPLIT_RADIX_SSE &&                                                              \
    (defined(__x86_64__) || defined(_M_X64) ||                                                     \
     (!defined(_KERNEL_MODE) && (defined(__i386__) || defined(_M_IX86)))) &&                      \
    (defined(__SSE2__) || defined(_MSC_VER))
#define AUDIO_MATH_HAS_FFT_SSE 1
#else
#define AUDIO_MATH_HAS_FFT_SSE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Return whether length is a supported power-of-two transform from 4 through 1024 points. */
int audio_math_fft_sse_supported(int32_t length);

/* Perform an in-place power-of-two FFT on complex data in natural order (length 4..1024).
 * z: input/in-place output buffer containing at least length complex values.
 * tmpbuf: scratch buffer containing at least length complex values; it must not overlap z.
 * length: power-of-two length (4, 8, 16, 32, 64, 128, 256, 512, or 1024).
 * inverse: 0 for forward, 1 for inverse without division by length.
 * Return 1 on success, or 0 for unsupported lengths/platforms.
 */
int audio_math_fft_sse_transform(AudioMathComplex32 *z, AudioMathComplex32 *tmpbuf, int32_t length,
                                 int inverse);

/* General power-of-two complex FFT entry point supporting out-of-place or in-place operation.
 * output: output buffer; it may equal input for in-place operation.
 * input: input complex buffer.
 * scratch: scratch buffer containing at least length complex values.
 * length: power-of-two length from 4 through 1024.
 * inverse: 0 for forward and 1 for inverse.
 * Return 1 on success and 0 on failure.
 */
int audio_math_fft_power2(AudioMathComplex32 *output, const AudioMathComplex32 *input,
                          AudioMathComplex32 *scratch, int32_t length, int inverse);

#ifdef __cplusplus
}
#endif

#endif
