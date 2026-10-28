#include <audio_math/math.h>
#include <audio_math/float_sqrt.h>
#include <audio_math/bits.h>
/* Shared floating KISS FFT: version adapters pass read-only plans without copying state. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <audio_math/fft_float32.h>
#include <audio_math/constants.h>
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) &&                           \
    (defined(_M_IX86) || defined(_M_X64))
#include <audio_math/float_trig.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Fixed-length forward/inverse twiddles are generated at build time without runtime initialization or mutable globals. */
#include <audio_math/fft_float32_tables.h>
#include "fft_float32.h"

#ifndef AUDIO_MATH_FFT_BITEXACT_SSE2
#ifdef AUDIO_MATH_FFT_512_BITEXACT_SSE2
#define AUDIO_MATH_FFT_BITEXACT_SSE2 AUDIO_MATH_FFT_512_BITEXACT_SSE2
#else
#define AUDIO_MATH_FFT_BITEXACT_SSE2 1
#endif
#endif

#if AUDIO_MATH_FFT_BITEXACT_SSE2 &&                                                                \
    (defined(__SSE2__) ||                                                                          \
     (defined(_MSC_VER) && (defined(_M_X64) || (!defined(_KERNEL_MODE) && defined(_M_IX86)))))
#define AUDIO_MATH_FFT_512_USE_SSE2 1
#if defined(__x86_64__) || defined(_M_X64)
/* MSVC Win32 register pressure makes generic mixed-radix butterflies slower than scalar code;
 * the fixed 512-point kernel still benefits. */
#define AUDIO_MATH_FFT_GENERIC_USE_SSE2 1
#endif
#include <emmintrin.h>

static inline __m128 audio_math_fft_twiddle_product_sse2(__m128 value, __m128 tw) {
    __m128 v_rr = _mm_shuffle_ps(value, value, _MM_SHUFFLE(2, 2, 0, 0));
    __m128 v_ii = _mm_shuffle_ps(value, value, _MM_SHUFFLE(3, 3, 1, 1));
    __m128 tw_ir = _mm_shuffle_ps(tw, tw, _MM_SHUFFLE(2, 3, 0, 1));

    __m128 p0 = _mm_mul_ps(v_rr, tw);
    __m128 p1 = _mm_mul_ps(v_ii, tw_ir);

    const __m128 sub_add_sign =
        _mm_castsi128_ps(_mm_setr_epi32((int)0x80000000, 0, (int)0x80000000, 0));
    p1 = _mm_xor_ps(p1, sub_add_sign);
    return _mm_add_ps(p0, p1);
}
#endif

/* Merge two subtransforms with a radix-two butterfly. */
static inline void audio_math_fft_bfly2(AudioMathComplex32 *Fout, size_t fstride, int32_t m,
                                        const AudioMathFftPlan *st) {
    const AudioMathComplex32 *tw1 = st->twiddles;
    /* k = 0: tw1[0] = {1.0f, 0.0f}; eliminate the complex twiddle multiplication. */
    {
        AudioMathComplex32 t = Fout[m];
        float imag = Fout->i;
        Fout[m].r = Fout->r - t.r;
        Fout[m].i = imag - t.i;
        Fout->r = t.r + Fout->r;
        Fout->i = t.i + Fout->i;
        ++Fout;
        tw1 += fstride;
    }
#if defined(AUDIO_MATH_FFT_GENERIC_USE_SSE2)
    int32_t k = 1;
    for (; k + 1 < m; k += 2) {
        __m128 vf0 = _mm_loadu_ps((const float *)Fout);
        __m128 vfm = _mm_loadu_ps((const float *)(Fout + m));
        __m128 vtw;
        if (fstride == 1) {
            vtw = _mm_loadu_ps((const float *)tw1);
        } else {
            AudioMathComplex32 tw_arr[2] = {tw1[0], tw1[fstride]};
            vtw = _mm_loadu_ps((const float *)tw_arr);
        }
        __m128 vt = audio_math_fft_twiddle_product_sse2(vfm, vtw);
        _mm_storeu_ps((float *)(Fout + m), _mm_sub_ps(vf0, vt));
        _mm_storeu_ps((float *)Fout, _mm_add_ps(vt, vf0));
        Fout += 2;
        tw1 += 2 * fstride;
    }
    for (; k < m; ++k, ++Fout, tw1 += fstride) {
        AudioMathComplex32 t = audio_math_fft_twiddle_product(Fout[m], *tw1);
        float imag = Fout->i;
        Fout[m].r = Fout->r - t.r;
        Fout[m].i = imag - t.i;
        Fout->r = t.r + Fout->r;
        Fout->i = t.i + Fout->i;
    }
#else
    for (int32_t k = 1; k < m; ++k, ++Fout, tw1 += fstride) {
        AudioMathComplex32 t = audio_math_fft_twiddle_product(Fout[m], *tw1);
        float imag = Fout->i;
        Fout[m].r = Fout->r - t.r;
        Fout[m].i = imag - t.i;
        Fout->r = t.r + Fout->r;
        Fout->i = t.i + Fout->i;
    }
#endif
}

/* Merge three subtransforms using cube roots of unity in a radix-three butterfly. */
static inline void audio_math_fft_bfly3(AudioMathComplex32 *Fout, size_t fstride, int32_t m,
                                        const AudioMathFftPlan *st) {
    const AudioMathComplex32 *tw1 = st->twiddles, *tw2 = st->twiddles;
    float epi3 = st->twiddles[m * fstride].i;
    /* k = 0: tw1[0] = tw2[0] = {1.0f, 0.0f}. */
    {
        AudioMathComplex32 scratch1 = Fout[m];
        AudioMathComplex32 scratch2 = Fout[2 * m];
        float imag = Fout->i;
        AudioMathComplex32 sum = {scratch1.r + scratch2.r, scratch1.i + scratch2.i};
        Fout[m].r = Fout->r - sum.r * 0.5f;
        Fout[m].i = imag - sum.i * 0.5f;
        imag = Fout->i;
        float difference_imag = epi3 * (scratch1.i - scratch2.i);
        float difference_real = epi3 * (scratch1.r - scratch2.r);
        Fout->r = sum.r + Fout->r;
        Fout->i = sum.i + imag;
        imag = Fout[m].i;
        Fout[2 * m].r = difference_imag + Fout[m].r;
        Fout[2 * m].i = imag - difference_real;
        imag = Fout[m].i;
        Fout[m].r = Fout[m].r - difference_imag;
        Fout[m].i = difference_real + imag;
        ++Fout;
        tw1 += fstride;
        tw2 += 2 * fstride;
    }
    for (int32_t k = 1; k < m; ++k, ++Fout, tw1 += fstride, tw2 += 2 * fstride) {
        AudioMathComplex32 scratch1 = audio_math_fft_twiddle_product(Fout[m], *tw1);
        AudioMathComplex32 scratch2 = audio_math_fft_twiddle_product(Fout[2 * m], *tw2);
        float imag = Fout->i;
        AudioMathComplex32 sum = {scratch1.r + scratch2.r, scratch1.i + scratch2.i};
        Fout[m].r = Fout->r - sum.r * 0.5f;
        Fout[m].i = imag - sum.i * 0.5f;
        imag = Fout->i;
        float difference_imag = epi3 * (scratch1.i - scratch2.i);
        float difference_real = epi3 * (scratch1.r - scratch2.r);
        Fout->r = sum.r + Fout->r;
        Fout->i = sum.i + imag;
        imag = Fout[m].i;
        Fout[2 * m].r = difference_imag + Fout[m].r;
        Fout[2 * m].i = imag - difference_real;
        imag = Fout[m].i;
        Fout[m].r = Fout[m].r - difference_imag;
        Fout[m].i = difference_real + imag;
    }
}

/* Merge four subtransforms with a direction-aware radix-four butterfly. */
static inline void audio_math_fft_bfly4(AudioMathComplex32 *Fout, size_t fstride, int32_t m,
                                        const AudioMathFftPlan *st) {
    const AudioMathComplex32 *tw1 = st->twiddles, *tw2 = st->twiddles, *tw3 = st->twiddles;
    if (st->inverse == 0) {
        /* k = 0: twiddles are always {1.0f, 0.0f}; eliminate 12 multiplications. */
        {
            AudioMathComplex32 scratch0 = Fout[m];
            AudioMathComplex32 scratch1 = Fout[2 * m];
            AudioMathComplex32 scratch2 = Fout[3 * m];
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r + odd_difference.i;
            Fout[m].i = even_difference.i - odd_difference.r;
            Fout[3 * m].r = even_difference.r - odd_difference.i;
            Fout[3 * m].i = even_difference.i + odd_difference.r;
            ++Fout;
            tw1 += fstride;
            tw2 += 2 * fstride;
            tw3 += 3 * fstride;
        }
#if defined(AUDIO_MATH_FFT_GENERIC_USE_SSE2)
        const __m128 v_rot_sign =
            _mm_castsi128_ps(_mm_setr_epi32(0, (int)0x80000000, 0, (int)0x80000000));
        int32_t k = 1;
        for (; k + 1 < m; k += 2) {
            __m128 vf0 = _mm_loadu_ps((const float *)Fout);
            __m128 vf1 = _mm_loadu_ps((const float *)(Fout + m));
            __m128 vf2 = _mm_loadu_ps((const float *)(Fout + 2 * m));
            __m128 vf3 = _mm_loadu_ps((const float *)(Fout + 3 * m));

            __m128 vtw1, vtw2, vtw3;
            if (fstride == 1) {
                vtw1 = _mm_loadu_ps((const float *)tw1);
                AudioMathComplex32 tw2_arr[2] = {tw2[0], tw2[2]};
                AudioMathComplex32 tw3_arr[2] = {tw3[0], tw3[3]};
                vtw2 = _mm_loadu_ps((const float *)tw2_arr);
                vtw3 = _mm_loadu_ps((const float *)tw3_arr);
            } else {
                AudioMathComplex32 tw1_arr[2] = {tw1[0], tw1[fstride]};
                AudioMathComplex32 tw2_arr[2] = {tw2[0], tw2[2 * fstride]};
                AudioMathComplex32 tw3_arr[2] = {tw3[0], tw3[3 * fstride]};
                vtw1 = _mm_loadu_ps((const float *)tw1_arr);
                vtw2 = _mm_loadu_ps((const float *)tw2_arr);
                vtw3 = _mm_loadu_ps((const float *)tw3_arr);
            }

            __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, vtw1);
            __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, vtw2);
            __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, vtw3);

            __m128 even_sum = _mm_add_ps(s1, vf0);
            __m128 odd_sum = _mm_add_ps(s0, s2);
            __m128 even_difference = _mm_sub_ps(vf0, s1);
            __m128 odd_difference = _mm_sub_ps(s0, s2);

            _mm_storeu_ps((float *)Fout, _mm_add_ps(even_sum, odd_sum));
            _mm_storeu_ps((float *)(Fout + 2 * m), _mm_sub_ps(even_sum, odd_sum));

            __m128 od_rot = _mm_shuffle_ps(odd_difference, odd_difference, _MM_SHUFFLE(2, 3, 0, 1));
            od_rot = _mm_xor_ps(od_rot, v_rot_sign);

            _mm_storeu_ps((float *)(Fout + m), _mm_add_ps(even_difference, od_rot));
            _mm_storeu_ps((float *)(Fout + 3 * m), _mm_sub_ps(even_difference, od_rot));

            Fout += 2;
            tw1 += 2 * fstride;
            tw2 += 4 * fstride;
            tw3 += 6 * fstride;
        }
        for (; k < m; ++k, ++Fout, tw1 += fstride, tw2 += 2 * fstride, tw3 += 3 * fstride) {
            AudioMathComplex32 scratch0 = audio_math_fft_twiddle_product(Fout[m], *tw1);
            AudioMathComplex32 scratch1 = audio_math_fft_twiddle_product(Fout[2 * m], *tw2);
            AudioMathComplex32 scratch2 = audio_math_fft_twiddle_product(Fout[3 * m], *tw3);
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r + odd_difference.i;
            Fout[m].i = even_difference.i - odd_difference.r;
            Fout[3 * m].r = even_difference.r - odd_difference.i;
            Fout[3 * m].i = even_difference.i + odd_difference.r;
        }
#else
        for (int32_t k = 1; k < m;
             ++k, ++Fout, tw1 += fstride, tw2 += 2 * fstride, tw3 += 3 * fstride) {
            AudioMathComplex32 scratch0 = audio_math_fft_twiddle_product(Fout[m], *tw1);
            AudioMathComplex32 scratch1 = audio_math_fft_twiddle_product(Fout[2 * m], *tw2);
            AudioMathComplex32 scratch2 = audio_math_fft_twiddle_product(Fout[3 * m], *tw3);
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r + odd_difference.i;
            Fout[m].i = even_difference.i - odd_difference.r;
            Fout[3 * m].r = even_difference.r - odd_difference.i;
            Fout[3 * m].i = even_difference.i + odd_difference.r;
        }
#endif
    } else {
        /* k = 0 */
        {
            AudioMathComplex32 scratch0 = Fout[m];
            AudioMathComplex32 scratch1 = Fout[2 * m];
            AudioMathComplex32 scratch2 = Fout[3 * m];
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r - odd_difference.i;
            Fout[m].i = even_difference.i + odd_difference.r;
            Fout[3 * m].r = even_difference.r + odd_difference.i;
            Fout[3 * m].i = even_difference.i - odd_difference.r;
            ++Fout;
            tw1 += fstride;
            tw2 += 2 * fstride;
            tw3 += 3 * fstride;
        }
#if defined(AUDIO_MATH_FFT_GENERIC_USE_SSE2)
        const __m128 v_rot_sign =
            _mm_castsi128_ps(_mm_setr_epi32((int)0x80000000, 0, (int)0x80000000, 0));
        int32_t k = 1;
        for (; k + 1 < m; k += 2) {
            __m128 vf0 = _mm_loadu_ps((const float *)Fout);
            __m128 vf1 = _mm_loadu_ps((const float *)(Fout + m));
            __m128 vf2 = _mm_loadu_ps((const float *)(Fout + 2 * m));
            __m128 vf3 = _mm_loadu_ps((const float *)(Fout + 3 * m));

            __m128 vtw1, vtw2, vtw3;
            if (fstride == 1) {
                vtw1 = _mm_loadu_ps((const float *)tw1);
                AudioMathComplex32 tw2_arr[2] = {tw2[0], tw2[2]};
                AudioMathComplex32 tw3_arr[2] = {tw3[0], tw3[3]};
                vtw2 = _mm_loadu_ps((const float *)tw2_arr);
                vtw3 = _mm_loadu_ps((const float *)tw3_arr);
            } else {
                AudioMathComplex32 tw1_arr[2] = {tw1[0], tw1[fstride]};
                AudioMathComplex32 tw2_arr[2] = {tw2[0], tw2[2 * fstride]};
                AudioMathComplex32 tw3_arr[2] = {tw3[0], tw3[3 * fstride]};
                vtw1 = _mm_loadu_ps((const float *)tw1_arr);
                vtw2 = _mm_loadu_ps((const float *)tw2_arr);
                vtw3 = _mm_loadu_ps((const float *)tw3_arr);
            }

            __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, vtw1);
            __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, vtw2);
            __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, vtw3);

            __m128 even_sum = _mm_add_ps(s1, vf0);
            __m128 odd_sum = _mm_add_ps(s0, s2);
            __m128 even_difference = _mm_sub_ps(vf0, s1);
            __m128 odd_difference = _mm_sub_ps(s0, s2);

            _mm_storeu_ps((float *)Fout, _mm_add_ps(even_sum, odd_sum));
            _mm_storeu_ps((float *)(Fout + 2 * m), _mm_sub_ps(even_sum, odd_sum));

            __m128 od_rot = _mm_shuffle_ps(odd_difference, odd_difference, _MM_SHUFFLE(2, 3, 0, 1));
            od_rot = _mm_xor_ps(od_rot, v_rot_sign);

            _mm_storeu_ps((float *)(Fout + m), _mm_add_ps(even_difference, od_rot));
            _mm_storeu_ps((float *)(Fout + 3 * m), _mm_sub_ps(even_difference, od_rot));

            Fout += 2;
            tw1 += 2 * fstride;
            tw2 += 4 * fstride;
            tw3 += 6 * fstride;
        }
        for (; k < m; ++k, ++Fout, tw1 += fstride, tw2 += 2 * fstride, tw3 += 3 * fstride) {
            AudioMathComplex32 scratch0 = audio_math_fft_twiddle_product(Fout[m], *tw1);
            AudioMathComplex32 scratch1 = audio_math_fft_twiddle_product(Fout[2 * m], *tw2);
            AudioMathComplex32 scratch2 = audio_math_fft_twiddle_product(Fout[3 * m], *tw3);
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r - odd_difference.i;
            Fout[m].i = even_difference.i + odd_difference.r;
            Fout[3 * m].r = even_difference.r + odd_difference.i;
            Fout[3 * m].i = even_difference.i - odd_difference.r;
        }
#else
        for (int32_t k = 1; k < m;
             ++k, ++Fout, tw1 += fstride, tw2 += 2 * fstride, tw3 += 3 * fstride) {
            AudioMathComplex32 scratch0 = audio_math_fft_twiddle_product(Fout[m], *tw1);
            AudioMathComplex32 scratch1 = audio_math_fft_twiddle_product(Fout[2 * m], *tw2);
            AudioMathComplex32 scratch2 = audio_math_fft_twiddle_product(Fout[3 * m], *tw3);
            AudioMathComplex32 even_sum = {scratch1.r + Fout->r, scratch1.i + Fout->i};
            AudioMathComplex32 odd_sum = {scratch0.r + scratch2.r, scratch0.i + scratch2.i};
            AudioMathComplex32 even_difference = {Fout->r - scratch1.r, Fout->i - scratch1.i};
            AudioMathComplex32 odd_difference = {scratch0.r - scratch2.r, scratch0.i - scratch2.i};
            Fout->r = even_sum.r + odd_sum.r;
            Fout->i = even_sum.i + odd_sum.i;
            Fout[2 * m].r = even_sum.r - odd_sum.r;
            Fout[2 * m].i = even_sum.i - odd_sum.i;
            Fout[m].r = even_difference.r - odd_difference.i;
            Fout[m].i = even_difference.i + odd_difference.r;
            Fout[3 * m].r = even_difference.r + odd_difference.i;
            Fout[3 * m].i = even_difference.i - odd_difference.r;
        }
#endif
    }
}

/* Merge five subtransforms using fifth roots of unity in a radix-five butterfly. */
static inline void audio_math_fft_bfly5(AudioMathComplex32 *Fout, size_t fstride, int32_t m,
                                        const AudioMathFftPlan *st) {
    AudioMathComplex32 ya = st->twiddles[m * fstride], yb = st->twiddles[2 * m * fstride];
    /* u = 0: twiddles are always {1.0f, 0.0f}; eliminate 16 multiplications. */
    {
        AudioMathComplex32 scratch0 = *Fout;
        AudioMathComplex32 scratch1 = Fout[m];
        AudioMathComplex32 scratch2 = Fout[2 * m];
        AudioMathComplex32 scratch3 = Fout[3 * m];
        AudioMathComplex32 scratch4 = Fout[4 * m];
        AudioMathComplex32 scratch7 = {scratch1.r + scratch4.r, scratch1.i + scratch4.i};
        AudioMathComplex32 scratch8 = {scratch2.r + scratch3.r, scratch2.i + scratch3.i};
        AudioMathComplex32 scratch9 = {scratch2.r - scratch3.r, scratch2.i - scratch3.i};
        AudioMathComplex32 scratch10 = {scratch1.r - scratch4.r, scratch1.i - scratch4.i};
        float cross_imag = -(scratch10.r * ya.i) - yb.i * scratch9.r;
        float sum_imag = scratch0.i + ya.r * scratch7.i + yb.r * scratch8.i;
        float sum_real = scratch0.r + ya.r * scratch7.r + yb.r * scratch8.r;
        float cross_real = ya.i * scratch10.i + yb.i * scratch9.i;
        Fout[m].i = sum_imag - cross_imag;
        float second_cross_imag = yb.i * scratch10.r - ya.i * scratch9.r;
        float second_cross_real = ya.i * scratch9.i - yb.i * scratch10.i;
        float second_sum_imag = scratch0.i + yb.r * scratch7.i + ya.r * scratch8.i;
        Fout[m].r = sum_real - cross_real;
        Fout[4 * m].r = cross_real + sum_real;
        Fout[2 * m].i = second_cross_imag + second_sum_imag;
        Fout[3 * m].i = second_sum_imag - second_cross_imag;
        float second_sum_real = scratch0.r + yb.r * scratch7.r + ya.r * scratch8.r;
        Fout->i = scratch0.i + (scratch8.i + scratch7.i);
        Fout->r = scratch0.r + (scratch8.r + scratch7.r);
        Fout[4 * m].i = cross_imag + sum_imag;
        Fout[2 * m].r = second_cross_real + second_sum_real;
        Fout[3 * m].r = second_sum_real - second_cross_real;
        ++Fout;
    }
    for (int32_t u = 1; u < m; ++u, ++Fout) {
        AudioMathComplex32 scratch0 = *Fout;
        AudioMathComplex32 scratch1 =
            audio_math_fft_twiddle_product(Fout[m], st->twiddles[u * fstride]);
        AudioMathComplex32 scratch2 =
            audio_math_fft_twiddle_product(Fout[2 * m], st->twiddles[2 * u * fstride]);
        AudioMathComplex32 scratch3 =
            audio_math_fft_twiddle_product(Fout[3 * m], st->twiddles[3 * u * fstride]);
        AudioMathComplex32 scratch4 =
            audio_math_fft_twiddle_product(Fout[4 * m], st->twiddles[4 * u * fstride]);
        AudioMathComplex32 scratch7 = {scratch1.r + scratch4.r, scratch1.i + scratch4.i};
        AudioMathComplex32 scratch8 = {scratch2.r + scratch3.r, scratch2.i + scratch3.i};
        AudioMathComplex32 scratch9 = {scratch2.r - scratch3.r, scratch2.i - scratch3.i};
        AudioMathComplex32 scratch10 = {scratch1.r - scratch4.r, scratch1.i - scratch4.i};
        float cross_imag = -(scratch10.r * ya.i) - yb.i * scratch9.r;
        float sum_imag = scratch0.i + ya.r * scratch7.i + yb.r * scratch8.i;
        float sum_real = scratch0.r + ya.r * scratch7.r + yb.r * scratch8.r;
        float cross_real = ya.i * scratch10.i + yb.i * scratch9.i;
        Fout[m].i = sum_imag - cross_imag;
        float second_cross_imag = yb.i * scratch10.r - ya.i * scratch9.r;
        float second_cross_real = ya.i * scratch9.i - yb.i * scratch10.i;
        float second_sum_imag = scratch0.i + yb.r * scratch7.i + ya.r * scratch8.i;
        Fout[m].r = sum_real - cross_real;
        Fout[4 * m].r = cross_real + sum_real;
        Fout[2 * m].i = second_cross_imag + second_sum_imag;
        Fout[3 * m].i = second_sum_imag - second_cross_imag;
        float second_sum_real = scratch0.r + yb.r * scratch7.r + ya.r * scratch8.r;
        Fout->i = scratch0.i + (scratch8.i + scratch7.i);
        Fout->r = scratch0.r + (scratch8.r + scratch7.r);
        Fout[4 * m].i = cross_imag + sum_imag;
        Fout[2 * m].r = second_cross_real + second_sum_real;
        Fout[3 * m].r = second_sum_real - second_cross_real;
    }
}

/* Apply generic complex merging for radices without dedicated butterflies. */
static void audio_math_fft_bfly_generic(AudioMathComplex32 *Fout, size_t fstride, int32_t m,
                                        int32_t p, const AudioMathFftPlan *st) {
    AudioMathComplex32 *scratch = st->generic_scratch;
    if (!scratch)
        return;
    for (int32_t u = 0; u < m; ++u) {
        for (int32_t q = 0; q < p; ++q)
            scratch[q] = Fout[u + q * m];
        for (int32_t q1 = 0; q1 < p; ++q1) {
            int32_t k = u + q1 * m, twidx = 0;
            Fout[k] = scratch[0];
            for (int32_t q = 1; q < p; ++q) {
                twidx += (int32_t)(k * fstride);
                if (twidx >= st->length)
                    twidx -= st->length;
                AudioMathComplex32 tw = st->twiddles[twidx];
                Fout[k].r = Fout[k].r + (scratch[q].r * tw.r - scratch[q].i * tw.i);
                Fout[k].i = Fout[k].i + (tw.r * scratch[q].i + scratch[q].r * tw.i);
            }
        }
    }
}

#if AUDIO_MATH_FFT_ITERATIVE_STATIC
/* Gather leaf-ordered input and merge stagewise, preserving butterfly operation order. */
void audio_math_fft_work(AudioMathComplex32 *output, const AudioMathComplex32 *input,
                         size_t twiddle_stride, int32_t input_stride, const int32_t *factors,
                         const AudioMathFftPlan *plan) {
    const int32_t length = factors[0] * factors[1];
    int stages = 0;
    do {
        ++stages;
    } while (factors[2 * stages - 1] != 1);
    /* Gather leaf inputs in mixed-radix digit-reversed order. */
    const uint16_t *order = audio_math_fft_permutation(length, factors);
    if (order) {
        const ptrdiff_t step = (ptrdiff_t)twiddle_stride * input_stride;
        if (step == 1) {
            for (int32_t out = 0; out < length; ++out)
                output[out] = input[order[out]];
        } else {
            for (int32_t out = 0; out < length; ++out)
                output[out] = input[(ptrdiff_t)order[out] * step];
        }
    } else {
        for (int32_t out = 0; out < length; ++out) {
            int32_t remaining = out;
            size_t source = 0, stride = twiddle_stride;
            for (int stage = 0; stage < stages; ++stage) {
                int32_t sublength = factors[2 * stage + 1];
                source += (size_t)(remaining / sublength) * stride;
                remaining %= sublength;
                stride *= factors[2 * stage];
            }
            output[out] = input[(ptrdiff_t)source * input_stride];
        }
    }
    /* Blocks within one level are independent; finish that level before its parent without recursion frames. */
    for (int stage = stages - 1; stage >= 0; --stage) {
        int32_t radix = factors[2 * stage], sublength = factors[2 * stage + 1];
        int32_t block = radix * sublength;
        size_t stride = twiddle_stride * (size_t)(length / block);
        for (int32_t base = 0; base < length; base += block) {
            AudioMathComplex32 *out = output + base;
            switch (radix) {
            case 2:
                audio_math_fft_bfly2(out, stride, sublength, plan);
                break;
            case 3:
                audio_math_fft_bfly3(out, stride, sublength, plan);
                break;
            case 4:
                audio_math_fft_bfly4(out, stride, sublength, plan);
                break;
            case 5:
                if (sublength > 0)
                    audio_math_fft_bfly5(out, stride, sublength, plan);
                break;
            default:
                if (plan->allow_generic_radix)
                    audio_math_fft_bfly_generic(out, stride, sublength, radix, plan);
                break;
            }
        }
    }
}

#else
/* Recursively compute subtransforms and merge by radix into natural bin order. */
void audio_math_fft_work(AudioMathComplex32 *output, const AudioMathComplex32 *input,
                         size_t twiddle_stride, int32_t input_stride, const int32_t *factors,
                         const AudioMathFftPlan *plan) {
    const int32_t radix = factors[0], subtransform_length = factors[1];
    if (subtransform_length == 1) {
        for (int32_t sample = 0; sample < radix; ++sample) {
            output[sample] = *input;
            input += twiddle_stride * input_stride;
        }
    } else {
        for (int32_t branch = 0; branch < radix; ++branch) {
            audio_math_fft_work(output + branch * subtransform_length, input,
                                twiddle_stride * radix, input_stride, factors + 2, plan);
            input += twiddle_stride * input_stride;
        }
    }
    switch (radix) {
    case 2:
        audio_math_fft_bfly2(output, twiddle_stride, subtransform_length, plan);
        break;
    case 3:
        audio_math_fft_bfly3(output, twiddle_stride, subtransform_length, plan);
        break;
    case 4:
        audio_math_fft_bfly4(output, twiddle_stride, subtransform_length, plan);
        break;
    case 5:
        if (subtransform_length > 0)
            audio_math_fft_bfly5(output, twiddle_stride, subtransform_length, plan);
        break;
    default:
        /* This adapter omits merges for other radices; its caller selects the appropriate path. */
        if (plan->allow_generic_radix)
            audio_math_fft_bfly_generic(output, twiddle_stride, subtransform_length, radix, plan);
        break;
    }
}

#endif

/* Record stage radices/remainders in order 4,2,3,then successive odd numbers. */
void audio_math_fft_factor(int32_t length, int32_t threshold, int32_t *factors) {
    int32_t radix = 4;
    do {
        while (length % radix) {
            radix = radix == 4 ? 2 : radix == 2 ? 3 : radix + 2;
            if (radix > threshold)
                radix = length;
        }
        length /= radix;
        *factors++ = radix;
        *factors++ = length;
    } while (length > 1);
}

/* Extract power-of-two real-FFT half-spectrum reconstruction coefficients from read-only complex twiddles. */
int audio_math_fftr_copy_twiddles(int32_t length, int inverse, AudioMathComplex32 *twiddles) {
    if (length < 4 || length > 512 || (length & (length - 1)) || !twiddles)
        return 0;
    size_t start = (size_t)length / 4 + 1;
    size_t bytes = (size_t)(length / 4) * sizeof *twiddles;
    if (length == 512) {
        memcpy(twiddles, audio_math_fft_512_twiddles[inverse != 0] + start, bytes);
        return 1;
    }
    const uint32_t *bits = audio_math_fft_static_twiddles(length, inverse != 0);
    if (!bits)
        return 0;
    memcpy(twiddles, bits + 2 * start, bytes);
    return 1;
}

/* Copy forward/inverse twiddle tables for covered lengths and generate other lengths from the
 * phase formula. Coefficient storage is independent of recursive/iterative dispatch and
 * preserves near-zero terms and signed zero. */
void audio_math_fft_twiddles(int32_t length, int32_t inverse, AudioMathComplex32 *twiddles) {
    if (length == 512) {
        memcpy(twiddles, audio_math_fft_512_twiddles[inverse != 0],
               sizeof audio_math_fft_512_twiddles[0]);
        return;
    }
    const uint32_t *bits = audio_math_fft_static_twiddles(length, inverse != 0);
    if (bits) {
        memcpy(twiddles, bits, (size_t)length * sizeof *twiddles);
        return;
    }
    for (int32_t sample = 0; sample < length; ++sample) {
        double phase = (-AUDIO_MATH_TWO_PI_F64 * (double)sample) / (double)length;
        if (inverse)
            phase = -phase;
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) &&                           \
    (defined(_M_IX86) || defined(_M_X64))
        audio_math_sincos_bounded_f64_to_f32_soft(phase, &twiddles[sample].i, &twiddles[sample].r);
#else
        double sine, cosine;
        audio_math_sincos(phase, &sine, &cosine);
        twiddles[sample].r = (float)cosine;
        twiddles[sample].i = (float)sine;
#endif
    }
}

/* Find the next FFT length with prime factors only 2/3/5. */
int32_t audio_math_fft_next_fast_size(int32_t n) {
    for (;; ++n) {
        int32_t m = n;
        while (!(m & 1))
            m /= 2;
        while (m % 3 == 0)
            m /= 3;
        while (m % 5 == 0)
            m /= 5;
        if (m <= 1)
            return n;
    }
}

/* Merge four subtransforms with the radix-four butterfly core. */
static inline void audio_math_fft_512_bfly4_core(AudioMathComplex32 *Fout, int32_t m,
                                                 const AudioMathComplex32 *tw1,
                                                 const AudioMathComplex32 *tw2,
                                                 const AudioMathComplex32 *tw3, int inverse) {
    AudioMathComplex32 *f0 = Fout;
    AudioMathComplex32 *f1 = f0 + m;
    AudioMathComplex32 *f2 = f1 + m;
    AudioMathComplex32 *f3 = f2 + m;
    if (!inverse) {
        /* k = 0: twiddles are always {1.0f, 0.0f}; save 12 floating-point multiplications. */
        {
            AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
            AudioMathComplex32 even_sum = {s1.r + f0->r, s1.i + f0->i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0->r - s1.r, f0->i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0->r = even_sum.r + odd_sum.r;
            f0->i = even_sum.i + odd_sum.i;
            f2->r = even_sum.r - odd_sum.r;
            f2->i = even_sum.i - odd_sum.i;
            f1->r = even_diff.r + odd_diff.i;
            f1->i = even_diff.i - odd_diff.r;
            f3->r = even_diff.r - odd_diff.i;
            f3->i = even_diff.i + odd_diff.r;
        }
#if defined(AUDIO_MATH_FFT_512_USE_SSE2)
        if (m >= 2) {
            /* k = 1 (handle the odd index in scalar code). */
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], tw1[1]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], tw2[1]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], tw3[1]);
            AudioMathComplex32 even_sum = {s1.r + f0[1].r, s1.i + f0[1].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[1].r - s1.r, f0[1].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0[1].r = even_sum.r + odd_sum.r;
            f0[1].i = even_sum.i + odd_sum.i;
            f2[1].r = even_sum.r - odd_sum.r;
            f2[1].i = even_sum.i - odd_sum.i;
            f1[1].r = even_diff.r + odd_diff.i;
            f1[1].i = even_diff.i - odd_diff.r;
            f3[1].r = even_diff.r - odd_diff.i;
            f3[1].i = even_diff.i + odd_diff.r;

            const __m128 v_rot_sign =
                _mm_castsi128_ps(_mm_setr_epi32(0, (int)0x80000000, 0, (int)0x80000000));
            for (int32_t k = 2; k < m; k += 2) {
                __m128 vf0 = _mm_loadu_ps((const float *)(f0 + k));
                __m128 vf1 = _mm_loadu_ps((const float *)(f1 + k));
                __m128 vf2 = _mm_loadu_ps((const float *)(f2 + k));
                __m128 vf3 = _mm_loadu_ps((const float *)(f3 + k));
                __m128 vtw1 = _mm_loadu_ps((const float *)(tw1 + k));
                __m128 vtw2 = _mm_loadu_ps((const float *)(tw2 + k));
                __m128 vtw3 = _mm_loadu_ps((const float *)(tw3 + k));

                __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, vtw1);
                __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, vtw2);
                __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, vtw3);

                __m128 es = _mm_add_ps(s1, vf0);
                __m128 os = _mm_add_ps(s0, s2);
                __m128 ed = _mm_sub_ps(vf0, s1);
                __m128 od = _mm_sub_ps(s0, s2);

                _mm_storeu_ps((float *)(f0 + k), _mm_add_ps(es, os));
                _mm_storeu_ps((float *)(f2 + k), _mm_sub_ps(es, os));

                __m128 od_rot = _mm_shuffle_ps(od, od, _MM_SHUFFLE(2, 3, 0, 1));
                od_rot = _mm_xor_ps(od_rot, v_rot_sign);
                _mm_storeu_ps((float *)(f1 + k), _mm_add_ps(ed, od_rot));
                _mm_storeu_ps((float *)(f3 + k), _mm_sub_ps(ed, od_rot));
            }
        }
#else
        for (int32_t k = 1; k < m; ++k) {
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[k], tw1[k]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[k], tw2[k]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[k], tw3[k]);
            AudioMathComplex32 even_sum = {s1.r + f0[k].r, s1.i + f0[k].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[k].r - s1.r, f0[k].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0[k].r = even_sum.r + odd_sum.r;
            f0[k].i = even_sum.i + odd_sum.i;
            f2[k].r = even_sum.r - odd_sum.r;
            f2[k].i = even_sum.i - odd_sum.i;
            f1[k].r = even_diff.r + odd_diff.i;
            f1[k].i = even_diff.i - odd_diff.r;
            f3[k].r = even_diff.r - odd_diff.i;
            f3[k].i = even_diff.i + odd_diff.r;
        }
#endif
    } else {
        /* k = 0 */
        {
            AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
            AudioMathComplex32 even_sum = {s1.r + f0->r, s1.i + f0->i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0->r - s1.r, f0->i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0->r = even_sum.r + odd_sum.r;
            f0->i = even_sum.i + odd_sum.i;
            f2->r = even_sum.r - odd_sum.r;
            f2->i = even_sum.i - odd_sum.i;
            f1->r = even_diff.r - odd_diff.i;
            f1->i = even_diff.i + odd_diff.r;
            f3->r = even_diff.r + odd_diff.i;
            f3->i = even_diff.i - odd_diff.r;
        }
#if defined(AUDIO_MATH_FFT_512_USE_SSE2)
        if (m >= 2) {
            /* k = 1 (handle the odd index in scalar code). */
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], tw1[1]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], tw2[1]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], tw3[1]);
            AudioMathComplex32 even_sum = {s1.r + f0[1].r, s1.i + f0[1].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[1].r - s1.r, f0[1].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0[1].r = even_sum.r + odd_sum.r;
            f0[1].i = even_sum.i + odd_sum.i;
            f2[1].r = even_sum.r - odd_sum.r;
            f2[1].i = even_sum.i - odd_sum.i;
            f1[1].r = even_diff.r - odd_diff.i;
            f1[1].i = even_diff.i + odd_diff.r;
            f3[1].r = even_diff.r + odd_diff.i;
            f3[1].i = even_diff.i - odd_diff.r;

            const __m128 v_rot_sign =
                _mm_castsi128_ps(_mm_setr_epi32((int)0x80000000, 0, (int)0x80000000, 0));
            for (int32_t k = 2; k < m; k += 2) {
                __m128 vf0 = _mm_loadu_ps((const float *)(f0 + k));
                __m128 vf1 = _mm_loadu_ps((const float *)(f1 + k));
                __m128 vf2 = _mm_loadu_ps((const float *)(f2 + k));
                __m128 vf3 = _mm_loadu_ps((const float *)(f3 + k));
                __m128 vtw1 = _mm_loadu_ps((const float *)(tw1 + k));
                __m128 vtw2 = _mm_loadu_ps((const float *)(tw2 + k));
                __m128 vtw3 = _mm_loadu_ps((const float *)(tw3 + k));

                __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, vtw1);
                __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, vtw2);
                __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, vtw3);

                __m128 es = _mm_add_ps(s1, vf0);
                __m128 os = _mm_add_ps(s0, s2);
                __m128 ed = _mm_sub_ps(vf0, s1);
                __m128 od = _mm_sub_ps(s0, s2);

                _mm_storeu_ps((float *)(f0 + k), _mm_add_ps(es, os));
                _mm_storeu_ps((float *)(f2 + k), _mm_sub_ps(es, os));

                __m128 od_rot = _mm_shuffle_ps(od, od, _MM_SHUFFLE(2, 3, 0, 1));
                od_rot = _mm_xor_ps(od_rot, v_rot_sign);
                _mm_storeu_ps((float *)(f1 + k), _mm_add_ps(ed, od_rot));
                _mm_storeu_ps((float *)(f3 + k), _mm_sub_ps(ed, od_rot));
            }
        }
#else
        for (int32_t k = 1; k < m; ++k) {
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[k], tw1[k]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[k], tw2[k]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[k], tw3[k]);
            AudioMathComplex32 even_sum = {s1.r + f0[k].r, s1.i + f0[k].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[k].r - s1.r, f0[k].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            f0[k].r = even_sum.r + odd_sum.r;
            f0[k].i = even_sum.i + odd_sum.i;
            f2[k].r = even_sum.r - odd_sum.r;
            f2[k].i = even_sum.i - odd_sum.i;
            f1[k].r = even_diff.r - odd_diff.i;
            f1[k].i = even_diff.i + odd_diff.r;
            f3[k].r = even_diff.r + odd_diff.i;
            f3[k].i = even_diff.i - odd_diff.r;
        }
#endif
    }
}

/* Dedicated 512-point complex FFT eliminating recursion, factor parsing, and intermediate copies. */
void audio_math_fft_512(float *interleaved, int inverse, AudioMathFft512Work *work) {
    if (!interleaved || !work)
        return;

    const AudioMathComplex32 *twiddles = audio_math_fft_512_twiddles[inverse != 0];
    const AudioMathComplex32 *in = (const AudioMathComplex32 *)interleaved;
    AudioMathComplex32 *out = work->output;

    /* Stage 0: fuse input permutation with radix-two butterflies, avoiding a 4 KiB input copy.
     * All 256 radix-two blocks have twiddle {1.0f, 0.0f} and require no floating-point multiply. */
    const uint16_t *order = audio_math_fft_permutation_512;
    for (int32_t base = 0; base < 512; base += 2) {
        AudioMathComplex32 in0 = in[order[base]];
        AudioMathComplex32 in1 = in[order[base + 1]];
        out[base].r = in1.r + in0.r;
        out[base].i = in1.i + in0.i;
        out[base + 1].r = in0.r - in1.r;
        out[base + 1].i = in0.i - in1.i;
    }

    /* Stage 1: radix four, m=2 (64 eight-point blocks, stride 64).
     * k=0 requires no multiply; the three k=1 twiddles are fixed across all blocks. */
    {
        AudioMathComplex32 tw1_1 = twiddles[64];
        AudioMathComplex32 tw2_1 = twiddles[128];
        AudioMathComplex32 tw3_1 = twiddles[192];
        if (!inverse) {
            for (int32_t base = 0; base < 512; base += 8) {
                AudioMathComplex32 *f0 = out + base;
                AudioMathComplex32 *f1 = f0 + 2;
                AudioMathComplex32 *f2 = f1 + 2;
                AudioMathComplex32 *f3 = f2 + 2;
                {
                    AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
                    AudioMathComplex32 es = {s1.r + f0->r, s1.i + f0->i};
                    AudioMathComplex32 os = {s0.r + s2.r, s0.i + s2.i};
                    AudioMathComplex32 ed = {f0->r - s1.r, f0->i - s1.i};
                    AudioMathComplex32 od = {s0.r - s2.r, s0.i - s2.i};
                    f0->r = es.r + os.r;
                    f0->i = es.i + os.i;
                    f2->r = es.r - os.r;
                    f2->i = es.i - os.i;
                    f1->r = ed.r + od.i;
                    f1->i = ed.i - od.r;
                    f3->r = ed.r - od.i;
                    f3->i = ed.i + od.r;
                }
                {
                    AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], tw1_1);
                    AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], tw2_1);
                    AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], tw3_1);
                    AudioMathComplex32 es = {s1.r + f0[1].r, s1.i + f0[1].i};
                    AudioMathComplex32 os = {s0.r + s2.r, s0.i + s2.i};
                    AudioMathComplex32 ed = {f0[1].r - s1.r, f0[1].i - s1.i};
                    AudioMathComplex32 od = {s0.r - s2.r, s0.i - s2.i};
                    f0[1].r = es.r + os.r;
                    f0[1].i = es.i + os.i;
                    f2[1].r = es.r - os.r;
                    f2[1].i = es.i - os.i;
                    f1[1].r = ed.r + od.i;
                    f1[1].i = ed.i - od.r;
                    f3[1].r = ed.r - od.i;
                    f3[1].i = ed.i + od.r;
                }
            }
        } else {
            for (int32_t base = 0; base < 512; base += 8) {
                AudioMathComplex32 *f0 = out + base;
                AudioMathComplex32 *f1 = f0 + 2;
                AudioMathComplex32 *f2 = f1 + 2;
                AudioMathComplex32 *f3 = f2 + 2;
                {
                    AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
                    AudioMathComplex32 es = {s1.r + f0->r, s1.i + f0->i};
                    AudioMathComplex32 os = {s0.r + s2.r, s0.i + s2.i};
                    AudioMathComplex32 ed = {f0->r - s1.r, f0->i - s1.i};
                    AudioMathComplex32 od = {s0.r - s2.r, s0.i - s2.i};
                    f0->r = es.r + os.r;
                    f0->i = es.i + os.i;
                    f2->r = es.r - os.r;
                    f2->i = es.i - os.i;
                    f1->r = ed.r - od.i;
                    f1->i = ed.i + od.r;
                    f3->r = ed.r + od.i;
                    f3->i = ed.i - od.r;
                }
                {
                    AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], tw1_1);
                    AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], tw2_1);
                    AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], tw3_1);
                    AudioMathComplex32 es = {s1.r + f0[1].r, s1.i + f0[1].i};
                    AudioMathComplex32 os = {s0.r + s2.r, s0.i + s2.i};
                    AudioMathComplex32 ed = {f0[1].r - s1.r, f0[1].i - s1.i};
                    AudioMathComplex32 od = {s0.r - s2.r, s0.i - s2.i};
                    f0[1].r = es.r + os.r;
                    f0[1].i = es.i + os.i;
                    f2[1].r = es.r - os.r;
                    f2[1].i = es.i - os.i;
                    f1[1].r = ed.r - od.i;
                    f1[1].i = ed.i + od.r;
                    f3[1].r = ed.r + od.i;
                    f3[1].i = ed.i - od.r;
                }
            }
        }
    }

    /* Consecutive twiddles for stages 2/3 reuse the instance input workspace. Because the
     * dedicated path no longer copies input, these 120 complex values neither enlarge the
     * instance nor require an approximately 1 KiB local stack frame. */
    AudioMathComplex32 *twiddle_scratch = work->input;
    AudioMathComplex32 *s2_tw1 = twiddle_scratch;
    AudioMathComplex32 *s2_tw2 = s2_tw1 + 8;
    AudioMathComplex32 *s2_tw3 = s2_tw2 + 8;
    AudioMathComplex32 *s3_tw1 = s2_tw3 + 8;
    AudioMathComplex32 *s3_tw2 = s3_tw1 + 32;
    AudioMathComplex32 *s3_tw3 = s3_tw2 + 32;

    /* Stage 2: radix four, m=8 (16 32-point blocks, stride 16). */
    for (int32_t k = 1; k < 8; ++k) {
        s2_tw1[k] = twiddles[k * 16];
        s2_tw2[k] = twiddles[k * 32];
        s2_tw3[k] = twiddles[k * 48];
    }
    for (int32_t base = 0; base < 512; base += 32) {
        audio_math_fft_512_bfly4_core(out + base, 8, s2_tw1, s2_tw2, s2_tw3, inverse != 0);
    }

    /* Stage 3: radix four, m=32 (four 128-point blocks, stride 4). */
    for (int32_t k = 1; k < 32; ++k) {
        s3_tw1[k] = twiddles[k * 4];
        s3_tw2[k] = twiddles[k * 8];
        s3_tw3[k] = twiddles[k * 12];
    }
    for (int32_t base = 0; base < 512; base += 128) {
        audio_math_fft_512_bfly4_core(out + base, 32, s3_tw1, s3_tw2, s3_tw3, inverse != 0);
    }

    /* Stage 4: radix four, m=128 (one 512-point block, stride 1). Write directly to
     * interleaved; inverse operation fuses the 1/512 scale to avoid another copy/scale pass. */
    const int32_t m = 128;
    AudioMathComplex32 *f0 = out;
    AudioMathComplex32 *f1 = f0 + m;
    AudioMathComplex32 *f2 = f1 + m;
    AudioMathComplex32 *f3 = f2 + m;
    AudioMathComplex32 *dst0 = (AudioMathComplex32 *)interleaved;
    AudioMathComplex32 *dst1 = dst0 + m;
    AudioMathComplex32 *dst2 = dst1 + m;
    AudioMathComplex32 *dst3 = dst2 + m;
    if (!inverse) {
        /* k = 0 */
        {
            AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
            AudioMathComplex32 even_sum = {s1.r + f0->r, s1.i + f0->i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0->r - s1.r, f0->i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0->r = even_sum.r + odd_sum.r;
            dst0->i = even_sum.i + odd_sum.i;
            dst2->r = even_sum.r - odd_sum.r;
            dst2->i = even_sum.i - odd_sum.i;
            dst1->r = even_diff.r + odd_diff.i;
            dst1->i = even_diff.i - odd_diff.r;
            dst3->r = even_diff.r - odd_diff.i;
            dst3->i = even_diff.i + odd_diff.r;
        }
#if defined(AUDIO_MATH_FFT_512_USE_SSE2)
        {
            /* k = 1 (scalar). */
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], twiddles[1]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], twiddles[2]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], twiddles[3]);
            AudioMathComplex32 even_sum = {s1.r + f0[1].r, s1.i + f0[1].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[1].r - s1.r, f0[1].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0[1].r = even_sum.r + odd_sum.r;
            dst0[1].i = even_sum.i + odd_sum.i;
            dst2[1].r = even_sum.r - odd_sum.r;
            dst2[1].i = even_sum.i - odd_sum.i;
            dst1[1].r = even_diff.r + odd_diff.i;
            dst1[1].i = even_diff.i - odd_diff.r;
            dst3[1].r = even_diff.r - odd_diff.i;
            dst3[1].i = even_diff.i + odd_diff.r;
        }
        const __m128 v_rot_sign =
            _mm_castsi128_ps(_mm_setr_epi32(0, (int)0x80000000, 0, (int)0x80000000));
        for (int32_t k = 2; k < m; k += 2) {
            __m128 vf0 = _mm_loadu_ps((const float *)(f0 + k));
            __m128 vf1 = _mm_loadu_ps((const float *)(f1 + k));
            __m128 vf2 = _mm_loadu_ps((const float *)(f2 + k));
            __m128 vf3 = _mm_loadu_ps((const float *)(f3 + k));

            __m128 tw1 = _mm_loadu_ps((const float *)(twiddles + k));
            AudioMathComplex32 tw2_arr[2] = {twiddles[2 * k], twiddles[2 * (k + 1)]};
            AudioMathComplex32 tw3_arr[2] = {twiddles[3 * k], twiddles[3 * (k + 1)]};
            __m128 tw2 = _mm_loadu_ps((const float *)tw2_arr);
            __m128 tw3 = _mm_loadu_ps((const float *)tw3_arr);

            __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, tw1);
            __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, tw2);
            __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, tw3);

            __m128 es = _mm_add_ps(s1, vf0);
            __m128 os = _mm_add_ps(s0, s2);
            __m128 ed = _mm_sub_ps(vf0, s1);
            __m128 od = _mm_sub_ps(s0, s2);

            _mm_storeu_ps((float *)(dst0 + k), _mm_add_ps(es, os));
            _mm_storeu_ps((float *)(dst2 + k), _mm_sub_ps(es, os));

            __m128 od_rot = _mm_shuffle_ps(od, od, _MM_SHUFFLE(2, 3, 0, 1));
            od_rot = _mm_xor_ps(od_rot, v_rot_sign);
            _mm_storeu_ps((float *)(dst1 + k), _mm_add_ps(ed, od_rot));
            _mm_storeu_ps((float *)(dst3 + k), _mm_sub_ps(ed, od_rot));
        }
#else
        for (int32_t k = 1; k < m; ++k) {
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[k], twiddles[k]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[k], twiddles[2 * k]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[k], twiddles[3 * k]);
            AudioMathComplex32 even_sum = {s1.r + f0[k].r, s1.i + f0[k].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[k].r - s1.r, f0[k].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0[k].r = even_sum.r + odd_sum.r;
            dst0[k].i = even_sum.i + odd_sum.i;
            dst2[k].r = even_sum.r - odd_sum.r;
            dst2[k].i = even_sum.i - odd_sum.i;
            dst1[k].r = even_diff.r + odd_diff.i;
            dst1[k].i = even_diff.i - odd_diff.r;
            dst3[k].r = even_diff.r - odd_diff.i;
            dst3[k].i = even_diff.i + odd_diff.r;
        }
#endif
    } else {
        const float scale = 1.0f / 512.0f;
        /* k = 0 */
        {
            AudioMathComplex32 s0 = *f1, s1 = *f2, s2 = *f3;
            AudioMathComplex32 even_sum = {s1.r + f0->r, s1.i + f0->i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0->r - s1.r, f0->i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0->r = (even_sum.r + odd_sum.r) * scale;
            dst0->i = (even_sum.i + odd_sum.i) * scale;
            dst2->r = (even_sum.r - odd_sum.r) * scale;
            dst2->i = (even_sum.i - odd_sum.i) * scale;
            dst1->r = (even_diff.r - odd_diff.i) * scale;
            dst1->i = (even_diff.i + odd_diff.r) * scale;
            dst3->r = (even_diff.r + odd_diff.i) * scale;
            dst3->i = (even_diff.i - odd_diff.r) * scale;
        }
#if defined(AUDIO_MATH_FFT_512_USE_SSE2)
        {
            /* k = 1 (scalar). */
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[1], twiddles[1]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[1], twiddles[2]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[1], twiddles[3]);
            AudioMathComplex32 even_sum = {s1.r + f0[1].r, s1.i + f0[1].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[1].r - s1.r, f0[1].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0[1].r = (even_sum.r + odd_sum.r) * scale;
            dst0[1].i = (even_sum.i + odd_sum.i) * scale;
            dst2[1].r = (even_sum.r - odd_sum.r) * scale;
            dst2[1].i = (even_sum.i - odd_sum.i) * scale;
            dst1[1].r = (even_diff.r - odd_diff.i) * scale;
            dst1[1].i = (even_diff.i + odd_diff.r) * scale;
            dst3[1].r = (even_diff.r + odd_diff.i) * scale;
            dst3[1].i = (even_diff.i - odd_diff.r) * scale;
        }
        const __m128 v_rot_sign =
            _mm_castsi128_ps(_mm_setr_epi32((int)0x80000000, 0, (int)0x80000000, 0));
        const __m128 vscale = _mm_set1_ps(scale);
        for (int32_t k = 2; k < m; k += 2) {
            __m128 vf0 = _mm_loadu_ps((const float *)(f0 + k));
            __m128 vf1 = _mm_loadu_ps((const float *)(f1 + k));
            __m128 vf2 = _mm_loadu_ps((const float *)(f2 + k));
            __m128 vf3 = _mm_loadu_ps((const float *)(f3 + k));

            __m128 tw1 = _mm_loadu_ps((const float *)(twiddles + k));
            AudioMathComplex32 tw2_arr[2] = {twiddles[2 * k], twiddles[2 * (k + 1)]};
            AudioMathComplex32 tw3_arr[2] = {twiddles[3 * k], twiddles[3 * (k + 1)]};
            __m128 tw2 = _mm_loadu_ps((const float *)tw2_arr);
            __m128 tw3 = _mm_loadu_ps((const float *)tw3_arr);

            __m128 s0 = audio_math_fft_twiddle_product_sse2(vf1, tw1);
            __m128 s1 = audio_math_fft_twiddle_product_sse2(vf2, tw2);
            __m128 s2 = audio_math_fft_twiddle_product_sse2(vf3, tw3);

            __m128 es = _mm_add_ps(s1, vf0);
            __m128 os = _mm_add_ps(s0, s2);
            __m128 ed = _mm_sub_ps(vf0, s1);
            __m128 od = _mm_sub_ps(s0, s2);

            _mm_storeu_ps((float *)(dst0 + k), _mm_mul_ps(_mm_add_ps(es, os), vscale));
            _mm_storeu_ps((float *)(dst2 + k), _mm_mul_ps(_mm_sub_ps(es, os), vscale));

            __m128 od_rot = _mm_shuffle_ps(od, od, _MM_SHUFFLE(2, 3, 0, 1));
            od_rot = _mm_xor_ps(od_rot, v_rot_sign);
            _mm_storeu_ps((float *)(dst1 + k), _mm_mul_ps(_mm_add_ps(ed, od_rot), vscale));
            _mm_storeu_ps((float *)(dst3 + k), _mm_mul_ps(_mm_sub_ps(ed, od_rot), vscale));
        }
#else
        for (int32_t k = 1; k < m; ++k) {
            AudioMathComplex32 s0 = audio_math_fft_twiddle_product(f1[k], twiddles[k]);
            AudioMathComplex32 s1 = audio_math_fft_twiddle_product(f2[k], twiddles[2 * k]);
            AudioMathComplex32 s2 = audio_math_fft_twiddle_product(f3[k], twiddles[3 * k]);
            AudioMathComplex32 even_sum = {s1.r + f0[k].r, s1.i + f0[k].i};
            AudioMathComplex32 odd_sum = {s0.r + s2.r, s0.i + s2.i};
            AudioMathComplex32 even_diff = {f0[k].r - s1.r, f0[k].i - s1.i};
            AudioMathComplex32 odd_diff = {s0.r - s2.r, s0.i - s2.i};
            dst0[k].r = (even_sum.r + odd_sum.r) * scale;
            dst0[k].i = (even_sum.i + odd_sum.i) * scale;
            dst2[k].r = (even_sum.r - odd_sum.r) * scale;
            dst2[k].i = (even_sum.i - odd_sum.i) * scale;
            dst1[k].r = (even_diff.r - odd_diff.i) * scale;
            dst1[k].i = (even_diff.i + odd_diff.r) * scale;
            dst3[k].r = (even_diff.r + odd_diff.i) * scale;
            dst3[k].i = (even_diff.i - odd_diff.r) * scale;
        }
#endif
    }
}
