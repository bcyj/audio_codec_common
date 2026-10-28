/* Controlled SSE/SSE2 intrinsics implementation of a split-radix FFT vector algorithm.
 * It uses T4/T8 butterflies and PASS_BIG dispatch, has no CRT dependency, and stores all
 * trigonometric and bit-reversal data in read-only tables for MSVC /kernel compatibility.
 * Unaligned vector accesses support complex buffers aligned to either 8 or 16 bytes.
 */
#include "audio_math/fft_sse.h"
#include "fft_sse_tables.h"
#include <string.h>

#if AUDIO_MATH_HAS_FFT_SSE
#include <xmmintrin.h>
#include <emmintrin.h>

#define M_SQRT1_2 0.70710678118654752440f

/* T4_SSE: vectorized four-point transform over four complex values.
 * Input: a_ptr={r0,i0,r2,i2}, b_ptr={r1,i1,r3,i3}.
 * Output: a_ptr={r0,r1,r2,r3}, b_ptr={i0,i1,i2,i3}.
 */
static inline void audio_math_t4_sse(float *a_ptr, float *b_ptr) {
    const __m128 ps_p1p1m1p1 = _mm_castsi128_ps(_mm_setr_epi32(0, 0, (int)0x80000000, 0));
    __m128 a = _mm_loadu_ps(a_ptr);
    __m128 b = _mm_loadu_ps(b_ptr);
    __m128 t = _mm_sub_ps(a, b);
    a = _mm_add_ps(a, b);
    t = _mm_xor_ps(t, ps_p1p1m1p1);
    b = _mm_shuffle_ps(a, t, _MM_SHUFFLE(2, 3, 3, 2));
    a = _mm_shuffle_ps(a, t, _MM_SHUFFLE(1, 0, 1, 0));
    t = _mm_sub_ps(a, b);
    a = _mm_add_ps(a, b);
    b = _mm_shuffle_ps(a, t, _MM_SHUFFLE(3, 1, 3, 1));
    a = _mm_shuffle_ps(a, t, _MM_SHUFFLE(2, 0, 2, 0));
    _mm_storeu_ps(a_ptr, a);
    _mm_storeu_ps(b_ptr, b);
}

/* T8_SSE: eight-point core combining the four-point result with sqrt(2)/2 rotations.
 * Input: z0={r0..r3}, z1={i0..i3}, z2={r4,i4,r6,i6}, z3={r5,i5,r7,i7}.
 * Output: z0={r0..r3}, z1={i0..i3}, z2={r4..r7}, z3={i4..i7}.
 */
static inline void audio_math_t8_sse(float *z0_ptr, float *z1_ptr, float *z2_ptr, float *z3_ptr) {
    const __m128 ps_root2mppm = _mm_setr_ps(-M_SQRT1_2, M_SQRT1_2, M_SQRT1_2, -M_SQRT1_2);
    const __m128 ps_root2 = _mm_set1_ps(M_SQRT1_2);
    __m128 z0 = _mm_loadu_ps(z0_ptr);
    __m128 z1 = _mm_loadu_ps(z1_ptr);
    __m128 z2 = _mm_loadu_ps(z2_ptr);
    __m128 z3 = _mm_loadu_ps(z3_ptr);
    __m128 t6 = _mm_add_ps(z2, z3);
    z2 = _mm_sub_ps(z2, z3);
    z3 = _mm_shuffle_ps(z2, z2, _MM_SHUFFLE(2, 3, 0, 1));
    z2 = _mm_mul_ps(z2, ps_root2mppm);
    z3 = _mm_mul_ps(z3, ps_root2);
    z2 = _mm_add_ps(z2, z3);
    z3 = _mm_shuffle_ps(t6, z2, _MM_SHUFFLE(2, 1, 3, 0));
    t6 = _mm_shuffle_ps(t6, z2, _MM_SHUFFLE(0, 3, 1, 2));
    z2 = _mm_sub_ps(t6, z3);
    t6 = _mm_add_ps(t6, z3);
    __m128 t5 = _mm_shuffle_ps(t6, z2, _MM_SHUFFLE(2, 0, 3, 1));
    t6 = _mm_shuffle_ps(t6, z2, _MM_SHUFFLE(3, 1, 2, 0));
    z2 = _mm_sub_ps(z0, t6);
    z0 = _mm_add_ps(z0, t6);
    z3 = _mm_sub_ps(z1, t5);
    z1 = _mm_add_ps(z1, t5);
    _mm_storeu_ps(z0_ptr, z0);
    _mm_storeu_ps(z1_ptr, z1);
    _mm_storeu_ps(z2_ptr, z2);
    _mm_storeu_ps(z3_ptr, z3);
}

/* Four-point base kernel. */
static inline void audio_math_fft4_sse(float *z) {
    audio_math_t4_sse(z, z + 4);
}

/* Eight-point base kernel; output remains in four split vectors. */
static inline void audio_math_fft8_sse(float *z) {
    audio_math_t4_sse(z, z + 4);
    audio_math_t8_sse(z, z + 4, z + 8, z + 12);
}

/* Sixteen-point base kernel: one eight-point lower transform, two four-point upper transforms,
 * then PASS_SMALL with cos_16. */
static inline void audio_math_fft16_sse(float *z, const float *cos_16) {
    audio_math_t4_sse(z, z + 4);
    audio_math_t8_sse(z, z + 4, z + 8, z + 12);
    audio_math_t4_sse(z + 16, z + 20);
    audio_math_t4_sse(z + 24, z + 28);
    __m128 m4 = _mm_loadu_ps(z + 16);
    __m128 m5 = _mm_loadu_ps(z + 20);
    __m128 m0 = _mm_loadu_ps(cos_16);
    __m128 m1 = _mm_loadu_ps(cos_16 + 4);
    __m128 m2 = _mm_mul_ps(m4, m0);
    __m128 m6 = _mm_loadu_ps(z + 24);
    __m128 m3 = _mm_mul_ps(m5, m1);
    __m128 m7 = _mm_loadu_ps(z + 28);
    m4 = _mm_mul_ps(m4, m1);
    m5 = _mm_mul_ps(m5, m0);
    m2 = _mm_add_ps(m2, m3);
    m3 = _mm_mul_ps(m1, m7);
    m5 = _mm_sub_ps(m5, m4);
    m1 = _mm_mul_ps(m1, m6);
    m4 = _mm_mul_ps(m0, m6);
    m0 = _mm_mul_ps(m0, m7);
    m4 = _mm_sub_ps(m4, m3);
    __m128 z0 = _mm_loadu_ps(z);
    m0 = _mm_add_ps(m0, m1);
    m1 = _mm_sub_ps(m4, m2);
    m4 = _mm_add_ps(m4, m2);
    __m128 m3_val = _mm_sub_ps(z0, m4);
    m4 = _mm_add_ps(m4, z0);
    __m128 z2 = _mm_loadu_ps(z + 8);
    _mm_storeu_ps(z + 16, m3_val);
    _mm_storeu_ps(z, m4);
    m3_val = _mm_sub_ps(m5, m0);
    m4 = _mm_sub_ps(z2, m3_val);
    m3_val = _mm_add_ps(m3_val, z2);
    _mm_storeu_ps(z + 24, m4);
    _mm_storeu_ps(z + 8, m3_val);
    __m128 z3 = _mm_loadu_ps(z + 12);
    m3_val = _mm_add_ps(m5, m0);
    __m128 m2_val = _mm_sub_ps(z3, m1);
    __m128 z1 = _mm_loadu_ps(z + 4);
    m1 = _mm_add_ps(m1, z3);
    _mm_storeu_ps(z + 28, m2_val);
    _mm_storeu_ps(z + 12, m1);
    m4 = _mm_sub_ps(z1, m3_val);
    m3_val = _mm_add_ps(m3_val, z1);
    _mm_storeu_ps(z + 20, m4);
    _mm_storeu_ps(z + 4, m3_val);
}

/* Internal PASS_BIG dispatch retaining split vectors (separate real/imaginary vectors per quadrant).
 * n is the total length of the current subtransform.
 */
static void audio_math_pass_sse(float *z, const float *w, int n) {
    int n_iter = n / 16;
    int o1 = 8 * n_iter;
    int o2 = 16 * n_iter;
    int o3 = 24 * n_iter;
    int w_im_offset = n / 4;
    for (int i = 0; i < n_iter; ++i) {
        float *p4 = z + o2 + 8 * i;
        float *p5 = z + o2 + 8 * i + 4;
        float *p6 = z + o3 + 8 * i;
        float *p7 = z + o3 + 8 * i + 4;
        float *p0 = z + 8 * i;
        float *p1 = z + 8 * i + 4;
        float *p2 = z + o1 + 8 * i;
        float *p3 = z + o1 + 8 * i + 4;

        __m128 m4 = _mm_loadu_ps(p4);
        __m128 m5 = _mm_loadu_ps(p5);
        __m128 m0 = _mm_loadu_ps(w + 4 * i);
        __m128 m1 = _mm_loadu_ps(w + w_im_offset + 4 * i);
        __m128 m2 = _mm_mul_ps(m4, m0);
        __m128 m6 = _mm_loadu_ps(p6);
        __m128 m3 = _mm_mul_ps(m5, m1);
        __m128 m7 = _mm_loadu_ps(p7);
        m4 = _mm_mul_ps(m4, m1);
        m5 = _mm_mul_ps(m5, m0);
        m2 = _mm_add_ps(m2, m3);
        m3 = _mm_mul_ps(m1, m7);
        m5 = _mm_sub_ps(m5, m4);
        m1 = _mm_mul_ps(m1, m6);
        m4 = _mm_mul_ps(m0, m6);
        m0 = _mm_mul_ps(m0, m7);
        m4 = _mm_sub_ps(m4, m3);
        __m128 z0 = _mm_loadu_ps(p0);
        m0 = _mm_add_ps(m0, m1);
        m1 = _mm_sub_ps(m4, m2);
        m4 = _mm_add_ps(m4, m2);

        __m128 new_r0 = _mm_add_ps(m4, z0);
        __m128 new_r2 = _mm_sub_ps(z0, m4);

        __m128 z2 = _mm_loadu_ps(p2);
        __m128 diff_m5_m0 = _mm_sub_ps(m5, m0);
        __m128 new_r3 = _mm_sub_ps(z2, diff_m5_m0);
        __m128 new_r1 = _mm_add_ps(diff_m5_m0, z2);

        __m128 z3 = _mm_loadu_ps(p3);
        __m128 new_i3 = _mm_sub_ps(z3, m1);
        __m128 new_i1 = _mm_add_ps(m1, z3);

        __m128 z1 = _mm_loadu_ps(p1);
        __m128 sum_m5_m0 = _mm_add_ps(m5, m0);
        __m128 new_i2 = _mm_sub_ps(z1, sum_m5_m0);
        __m128 new_i0 = _mm_add_ps(sum_m5_m0, z1);

        _mm_storeu_ps(p0, new_r0);
        _mm_storeu_ps(p1, new_i0);
        _mm_storeu_ps(p2, new_r1);
        _mm_storeu_ps(p3, new_i1);
        _mm_storeu_ps(p4, new_r2);
        _mm_storeu_ps(p5, new_i2);
        _mm_storeu_ps(p6, new_r3);
        _mm_storeu_ps(p7, new_i3);
    }
}

/* Final PASS_BIG dispatch computes the last butterflies and converts split vectors back to
 * standard interleaved complex order {r0,i0,r1,i1,...}. */
static void audio_math_pass_interleave_sse(float *z, const float *w, int n) {
    int n_iter = n / 16;
    int o1 = 8 * n_iter;
    int o2 = 16 * n_iter;
    int o3 = 24 * n_iter;
    int w_im_offset = n / 4;
    for (int i = 0; i < n_iter; ++i) {
        float *p4 = z + o2 + 8 * i;
        float *p5 = z + o2 + 8 * i + 4;
        float *p6 = z + o3 + 8 * i;
        float *p7 = z + o3 + 8 * i + 4;
        float *p0 = z + 8 * i;
        float *p1 = z + 8 * i + 4;
        float *p2 = z + o1 + 8 * i;
        float *p3 = z + o1 + 8 * i + 4;

        __m128 m4 = _mm_loadu_ps(p4);
        __m128 m5 = _mm_loadu_ps(p5);
        __m128 m0 = _mm_loadu_ps(w + 4 * i);
        __m128 m1 = _mm_loadu_ps(w + w_im_offset + 4 * i);
        __m128 m2 = _mm_mul_ps(m4, m0);
        __m128 m6 = _mm_loadu_ps(p6);
        __m128 m3 = _mm_mul_ps(m5, m1);
        __m128 m7 = _mm_loadu_ps(p7);
        m4 = _mm_mul_ps(m4, m1);
        m5 = _mm_mul_ps(m5, m0);
        m2 = _mm_add_ps(m2, m3);
        m3 = _mm_mul_ps(m1, m7);
        m5 = _mm_sub_ps(m5, m4);
        m1 = _mm_mul_ps(m1, m6);
        m4 = _mm_mul_ps(m0, m6);
        m0 = _mm_mul_ps(m0, m7);
        m4 = _mm_sub_ps(m4, m3);
        __m128 z0 = _mm_loadu_ps(p0);
        m0 = _mm_add_ps(m0, m1);
        m1 = _mm_sub_ps(m4, m2);
        m4 = _mm_add_ps(m4, m2);

        __m128 new_r0 = _mm_add_ps(m4, z0);
        __m128 new_r2 = _mm_sub_ps(z0, m4);

        __m128 z2 = _mm_loadu_ps(p2);
        __m128 diff_m5_m0 = _mm_sub_ps(m5, m0);
        __m128 new_r3 = _mm_sub_ps(z2, diff_m5_m0);
        __m128 new_r1 = _mm_add_ps(diff_m5_m0, z2);

        __m128 z3 = _mm_loadu_ps(p3);
        __m128 new_i3 = _mm_sub_ps(z3, m1);
        __m128 new_i1 = _mm_add_ps(m1, z3);

        __m128 z1 = _mm_loadu_ps(p1);
        __m128 sum_m5_m0 = _mm_add_ps(m5, m0);
        __m128 new_i2 = _mm_sub_ps(z1, sum_m5_m0);
        __m128 new_i0 = _mm_add_ps(sum_m5_m0, z1);

        _mm_storeu_ps(p0, _mm_unpacklo_ps(new_r0, new_i0));
        _mm_storeu_ps(p1, _mm_unpackhi_ps(new_r0, new_i0));
        _mm_storeu_ps(p2, _mm_unpacklo_ps(new_r1, new_i1));
        _mm_storeu_ps(p3, _mm_unpackhi_ps(new_r1, new_i1));
        _mm_storeu_ps(p4, _mm_unpacklo_ps(new_r2, new_i2));
        _mm_storeu_ps(p5, _mm_unpackhi_ps(new_r2, new_i2));
        _mm_storeu_ps(p6, _mm_unpacklo_ps(new_r3, new_i3));
        _mm_storeu_ps(p7, _mm_unpackhi_ps(new_r3, new_i3));
    }
}

/* FFT kernels for each statically expanded recursion level. */
static void audio_math_fft32_sse(float *z, const float *cos_16, const float *cos_32) {
    audio_math_fft16_sse(z, cos_16);
    audio_math_fft8_sse(z + 32);
    audio_math_fft8_sse(z + 48);
    audio_math_pass_sse(z, cos_32, 32);
}

static void audio_math_fft64_sse(float *z, const float *const *cos_tabs) {
    audio_math_fft32_sse(z, cos_tabs[4], cos_tabs[5]);
    audio_math_fft16_sse(z + 64, cos_tabs[4]);
    audio_math_fft16_sse(z + 96, cos_tabs[4]);
    audio_math_pass_sse(z, cos_tabs[6], 64);
}

static void audio_math_fft128_sse(float *z, const float *const *cos_tabs) {
    audio_math_fft64_sse(z, cos_tabs);
    audio_math_fft32_sse(z + 128, cos_tabs[4], cos_tabs[5]);
    audio_math_fft32_sse(z + 192, cos_tabs[4], cos_tabs[5]);
    audio_math_pass_sse(z, cos_tabs[7], 128);
}

static void audio_math_fft256_sse(float *z, const float *const *cos_tabs) {
    audio_math_fft128_sse(z, cos_tabs);
    audio_math_fft64_sse(z + 256, cos_tabs);
    audio_math_fft64_sse(z + 384, cos_tabs);
    audio_math_pass_sse(z, cos_tabs[8], 256);
}

static void audio_math_fft512_sse(float *z, const float *const *cos_tabs) {
    audio_math_fft256_sse(z, cos_tabs);
    audio_math_fft128_sse(z + 512, cos_tabs);
    audio_math_fft128_sse(z + 768, cos_tabs);
    audio_math_pass_sse(z, cos_tabs[9], 512);
}

/* Unified dispatcher for transform lengths from 4 through 1024. */
static void audio_math_fft_calc_sse_dispatch(float *z, int nbits, const float *const *cos_tabs) {
    switch (nbits) {
    case 2:
        audio_math_fft4_sse(z);
        {
            __m128 r = _mm_loadu_ps(z);
            __m128 im = _mm_loadu_ps(z + 4);
            _mm_storeu_ps(z, _mm_unpacklo_ps(r, im));
            _mm_storeu_ps(z + 4, _mm_unpackhi_ps(r, im));
        }
        break;
    case 3:
        audio_math_fft8_sse(z);
        for (int i = 0; i < 16; i += 8) {
            __m128 r = _mm_loadu_ps(z + i);
            __m128 im = _mm_loadu_ps(z + i + 4);
            _mm_storeu_ps(z + i, _mm_unpacklo_ps(r, im));
            _mm_storeu_ps(z + i + 4, _mm_unpackhi_ps(r, im));
        }
        break;
    case 4:
        audio_math_fft16_sse(z, cos_tabs[4]);
        for (int i = 0; i < 32; i += 8) {
            __m128 r = _mm_loadu_ps(z + i);
            __m128 im = _mm_loadu_ps(z + i + 4);
            _mm_storeu_ps(z + i, _mm_unpacklo_ps(r, im));
            _mm_storeu_ps(z + i + 4, _mm_unpackhi_ps(r, im));
        }
        break;
    case 5:
        audio_math_fft16_sse(z, cos_tabs[4]);
        audio_math_fft8_sse(z + 32);
        audio_math_fft8_sse(z + 48);
        audio_math_pass_interleave_sse(z, cos_tabs[5], 32);
        break;
    case 6:
        audio_math_fft32_sse(z, cos_tabs[4], cos_tabs[5]);
        audio_math_fft16_sse(z + 64, cos_tabs[4]);
        audio_math_fft16_sse(z + 96, cos_tabs[4]);
        audio_math_pass_interleave_sse(z, cos_tabs[6], 64);
        break;
    case 7:
        audio_math_fft64_sse(z, cos_tabs);
        audio_math_fft32_sse(z + 128, cos_tabs[4], cos_tabs[5]);
        audio_math_fft32_sse(z + 192, cos_tabs[4], cos_tabs[5]);
        audio_math_pass_interleave_sse(z, cos_tabs[7], 128);
        break;
    case 8:
        audio_math_fft128_sse(z, cos_tabs);
        audio_math_fft64_sse(z + 256, cos_tabs);
        audio_math_fft64_sse(z + 384, cos_tabs);
        audio_math_pass_interleave_sse(z, cos_tabs[8], 256);
        break;
    case 9:
        audio_math_fft256_sse(z, cos_tabs);
        audio_math_fft128_sse(z + 512, cos_tabs);
        audio_math_fft128_sse(z + 768, cos_tabs);
        audio_math_pass_interleave_sse(z, cos_tabs[9], 512);
        break;
    case 10:
        audio_math_fft512_sse(z, cos_tabs);
        audio_math_fft256_sse(z + 1024, cos_tabs);
        audio_math_fft256_sse(z + 1536, cos_tabs);
        audio_math_pass_interleave_sse(z, cos_tabs[10], 1024);
        break;
    default:
        break;
    }
}

/* Bit-reversal and low-bit swap permutation supporting arbitrary float-buffer alignment. */
static void audio_math_fft_permute_sse(AudioMathComplex32 *z, const uint16_t *revtab,
                                       AudioMathComplex32 *tmpbuf, int n) {
    for (int i = 0; i < n; i += 2) {
        uint16_t j0 = revtab[i];
        uint16_t j1 = revtab[i + 1];
        tmpbuf[j0] = z[i];
        tmpbuf[j1] = z[i + 1];
    }
    memcpy(z, tmpbuf, (size_t)n * sizeof(AudioMathComplex32));
}

#endif /* AUDIO_MATH_HAS_FFT_SSE */

int audio_math_fft_sse_supported(int32_t length) {
#if AUDIO_MATH_HAS_FFT_SSE
    return length >= 4 && length <= 1024 && (length & (length - 1)) == 0;
#else
    (void)length;
    return 0;
#endif
}

int audio_math_fft_sse_transform(AudioMathComplex32 *z, AudioMathComplex32 *tmpbuf, int32_t length,
                                 int inverse) {
#if AUDIO_MATH_HAS_FFT_SSE
    if (!z || !tmpbuf || !audio_math_fft_sse_supported(length))
        return 0;

    int nbits = 0;
    for (int32_t s = length; s > 1; s >>= 1)
        ++nbits;

    const uint16_t *revtab = audio_math_fft_sse_get_revtab(nbits, inverse);
    if (!revtab)
        return 0;

    audio_math_fft_permute_sse(z, revtab, tmpbuf, length);
    audio_math_fft_calc_sse_dispatch((float *)z, nbits, audio_math_fft_sse_cos_tabs);
    return 1;
#else
    (void)z;
    (void)tmpbuf;
    (void)length;
    (void)inverse;
    return 0;
#endif
}

int audio_math_fft_power2(AudioMathComplex32 *output, const AudioMathComplex32 *input,
                          AudioMathComplex32 *scratch, int32_t length, int inverse) {
#if AUDIO_MATH_HAS_FFT_SSE
    if (!output || !input || !scratch || !audio_math_fft_sse_supported(length))
        return 0;
    if (output != input)
        memcpy(output, input, (size_t)length * sizeof *output);
    return audio_math_fft_sse_transform(output, scratch, length, inverse);
#else
    (void)output;
    (void)input;
    (void)scratch;
    (void)length;
    (void)inverse;
    return 0;
#endif
}
