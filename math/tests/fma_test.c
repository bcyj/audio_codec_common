#include <audio_math/fma_native.h>
#include <audio_math/numeric_bits.h>
#include <math.h>
#include <stdio.h>

/* Check known exact results for cancellation, double rounding, signed zero, and underflow. */
int main(void) {
    static const uint32_t vectors[][4] = {{0x3fc00000, 0x3f800001, 0x97800000, 0x3fc00001},
                                          {0x3f800001, 0x3f7ffffe, 0xbf800000, 0xa8800000},
                                          {0x7f7fffff, 0x40000000, 0xff7fffff, 0x7f7fffff},
                                          {0x00000001, 0x3f000000, 0x00000000, 0x00000000},
                                          {0x00000003, 0x3f000000, 0x00000000, 0x00000002},
                                          {0x80000001, 0x3f000000, 0x80000000, 0x80000000},
                                          {0x00000000, 0xbf800000, 0x80000000, 0x80000000},
                                          {0x80000000, 0xbf800000, 0x80000000, 0x00000000},
                                          {0x3f800000, 0x3f800000, 0xbf800000, 0x00000000},
                                          {0x00800000, 0x3f000000, 0x00000001, 0x00400001},
                                          {0x7f7fffff, 0x40000000, 0x00000000, 0x7f800000}};
    for (unsigned i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        float a = audio_math_float32_from_bits(vectors[i][0]);
        float b = audio_math_float32_from_bits(vectors[i][1]);
        float c = audio_math_float32_from_bits(vectors[i][2]);
        uint32_t actual = audio_math_float32_bits(audio_math_fma_f32(a, b, c));
        uint32_t soft = audio_math_float32_bits(audio_math_fma_soft_f32(a, b, c));
        if (actual != vectors[i][3] || soft != actual) {
            fprintf(stderr, "FMA vector %u: %08x / %08x != %08x\n", i, actual, soft, vectors[i][3]);
            return 1;
        }
    }
    if (audio_math_fma_f64(0x1.0000000000001p0, 0x1.ffffffffffffep-1, -1.0) != -0x1p-104)
        return 2;
    static const uint64_t double_vectors[][4] = {
        {UINT64_C(0x0000000000000001), UINT64_C(0x3fe0000000000000), 0, 0},
        {UINT64_C(0x0000000000000003), UINT64_C(0x3fe0000000000000), 0, 2},
        {UINT64_C(0x0010000000000000), UINT64_C(0x3fe0000000000000), 1,
         UINT64_C(0x0008000000000001)},
        {UINT64_C(0x7fefffffffffffff), UINT64_C(0x4000000000000000), UINT64_C(0xffefffffffffffff),
         UINT64_C(0x7fefffffffffffff)},
        {UINT64_C(0x3ff0000000000000), UINT64_C(0x3ff0000000000000), UINT64_C(0xbff0000000000000),
         0},
        {UINT64_C(0x8000000000000001), UINT64_C(0x3fe0000000000000), UINT64_C(0x8000000000000000),
         UINT64_C(0x8000000000000000)}};
    for (unsigned i = 0; i < sizeof double_vectors / sizeof *double_vectors; ++i) {
        double a = audio_math_float64_from_bits(double_vectors[i][0]);
        double b = audio_math_float64_from_bits(double_vectors[i][1]);
        double c = audio_math_float64_from_bits(double_vectors[i][2]);
        if (audio_math_float64_bits(audio_math_fma_soft_f64(a, b, c)) != double_vectors[i][3] ||
            audio_math_float64_bits(audio_math_fma_f64(a, b, c)) != double_vectors[i][3])
            return 6;
    }
    if (!isnan(audio_math_fma_f32(INFINITY, 0.0f, 1.0f)))
        return 3;
    if (audio_math_fma_f32(INFINITY, 1.0f, 1.0f) != INFINITY)
        return 4;
    uint32_t returned_nan = audio_math_float32_bits(audio_math_float32_from_return_bits(UINT32_C(0x7f800001)));
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    if (returned_nan != UINT32_C(0x7fc00001))
        return 5;
#else
    if (returned_nan != UINT32_C(0x7f800001))
        return 5;
#endif
    puts("FMA: exact rounding, cancellation, subnormal, signed zero, nonfinite passed");
    return 0;
}
