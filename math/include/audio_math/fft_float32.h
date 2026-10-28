/* Shared floating KISS FFT backend, independent of V4/V5 state layouts and data namespaces. */
#ifndef AUDIO_MATH_FFT_FLOAT32_H
#define AUDIO_MATH_FFT_FLOAT32_H
#include <stdint.h>
#include <stddef.h>
typedef struct AudioMathComplex32 { float r; float i; } AudioMathComplex32;

/* Default retains recursion; one selects iterative scheduling and static mixed-radix permutations.
 * Both schedules share read-only twiddles for covered lengths; other lengths use dynamic formulas.
 * Plans, workspace, and public signatures are unchanged; uncovered lengths also support generic iterative transforms.
 */
#ifndef AUDIO_MATH_FFT_ITERATIVE_STATIC
#define AUDIO_MATH_FFT_ITERATIVE_STATIC 0
#endif

/* Read-only per-call plan view; version instances own twiddles and workspace. */
typedef struct AudioMathFftPlan {
    int32_t length;
    int32_t inverse;
    const AudioMathComplex32 *twiddles;
    int allow_generic_radix;
    /* Radices outside 2/3/4/5 require scratch capacity at least as large as the largest radix.
     * The common kernel allocates nothing; LHDC adapters provide fallback allocation for interfaces without scratch. */
    AudioMathComplex32 *generic_scratch;
} AudioMathFftPlan;

#ifdef __cplusplus
extern "C" {
#endif
void audio_math_fft_work(AudioMathComplex32 *output, const AudioMathComplex32 *input, size_t twiddle_stride,
                        int32_t input_stride, const int32_t *factors, const AudioMathFftPlan *plan);
void audio_math_fft_factor(int32_t length, int32_t threshold, int32_t *factors);
void audio_math_fft_twiddles(int32_t length, int32_t inverse, AudioMathComplex32 *twiddles);
/* Copy reconstruction coefficients for power-of-two real FFT lengths 4..512; return 1 on success.
 * Unsupported lengths/null output return 0 without writes. Capacity must hold length/4 complex values. */
int audio_math_fftr_copy_twiddles(int32_t length, int inverse, AudioMathComplex32 *twiddles);
int32_t audio_math_fft_next_fast_size(int32_t length);

/* Instance workspace for AR's fixed 512-point transform; twiddles/factors are shared read-only constants. */
typedef struct AudioMathFft512Work {
    AudioMathComplex32 input[512];
    AudioMathComplex32 output[512];
} AudioMathFft512Work;
void audio_math_fft_512(float *interleaved, int inverse, AudioMathFft512Work *work);
#ifdef __cplusplus
}
#endif
#endif
