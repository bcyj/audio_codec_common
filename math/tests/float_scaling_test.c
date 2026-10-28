#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <audio_math/float_scaling.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static double (*volatile crt_scale)(double, int) = ldexp;
static double (*volatile crt_frexp)(double, int *) = frexp;
static double (*volatile crt_sign)(double, double) = copysign;
static uint64_t comparisons;
static uint64_t crt_underflow_differences;
static uint64_t random_state = UINT64_C(0x123456789abcdef1);

/* Generate a fixed sequence independently of host PRNG implementations. */
static uint64_t random_bits(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}

/* Provide an independent reference using staged exact power-of-two multiplication; nonzero subnormals round only at the final step.
 * Earlier underflow guarantees the remaining shrink produces zero without changing final rounding.
 * Round each stage to binary64 so x87 excess precision cannot conceal final rounding; do not call ldexp.
 */
static double arithmetic_scale(double value, int power) {
    uint64_t bits = audio_math_float64_bits(value);
    uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
    if (magnitude == 0 || magnitude >= UINT64_C(0x7ff0000000000000))
        return value;
    if (power > 4096)
        power = 4096;
    if (power < -4096)
        power = -4096;
    volatile double scaled = value;
    while (power > 1023) {
        scaled = scaled * 0x1p1023;
        power -= 1023;
    }
    while (power < -1022) {
        scaled = scaled * 0x1p-969;
        power += 969;
    }
    double factor = audio_math_float64_from_bits((uint64_t)(power + 1023) << 52);
    scaled = scaled * factor;
    return scaled;
}

/* Compare double scaling bitwise, recording CRT subnormal truncation differences while always checking independent arithmetic. */
static int check_scale(uint64_t bits, int power) {
    double input = audio_math_float64_from_bits(bits);
    uint64_t legacy = audio_math_float64_bits(crt_scale(input, power));
    uint64_t expected = audio_math_float64_bits(arithmetic_scale(input, power));
    uint64_t actual = audio_math_float64_bits(audio_math_scale_pow2_f64(input, power));
    uint64_t full = audio_math_float64_bits(audio_math_scale_pow2_f64_fallback(input, power));
    if ((bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000))
        expected = bits | UINT64_C(0x0008000000000000);
    if (legacy != expected) {
        uint64_t absolute = expected & UINT64_C(0x7fffffffffffffff);
        uint64_t legacy_absolute = legacy & UINT64_C(0x7fffffffffffffff);
        /* Record Windows CRT subnormal truncation separately without relaxing algorithm-output regressions. */
        if (absolute > UINT64_C(0x0010000000000000) || (absolute != legacy_absolute + 1) ||
            ((legacy ^ expected) >> 63)) {
            printf("FAIL unexpected CRT difference input=%016" PRIx64 " power=%d\n", bits, power);
            return 0;
        }
        ++crt_underflow_differences;
    }
    ++comparisons;
    if (expected != actual || actual != full) {
        printf("FAIL scale input=%016" PRIx64 " power=%d expected=%016" PRIx64 " actual=%016" PRIx64
               "\n",
               bits, power, expected, actual);
        return 0;
    }
    return 1;
}

/* Verify exponents/sign copying without relying on unspecified frexp exponents for nonfinite input. */
static int check_exponent(uint32_t bits) {
    float input = audio_math_float32_from_bits(bits);
    uint32_t magnitude = bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    int expected = 0;
    if (magnitude < AUDIO_MATH_FLOAT32_INFINITY_BITS)
        (void)crt_frexp((double)input, &expected);
    int actual = audio_math_frexp_exponent_f32(input);
    ++comparisons;
    if (actual != expected) {
        printf("FAIL exponent input=%08" PRIx32 " expected=%d actual=%d\n", bits, expected, actual);
        return 0;
    }
    return 1;
}

/* Compare all sign-copying bits, including negative zero and quiet NaNs. */
static int check_sign(uint64_t bits, uint64_t sign) {
    /* Signaling-NaN argument/return quieting is ABI-specific; verify bit contracts separately without CRT. */
    uint64_t magnitude = bits & UINT64_C(0x7fffffffffffffff);
    if (magnitude > UINT64_C(0x7ff0000000000000))
        bits |= UINT64_C(0x0008000000000000);
    double value = audio_math_float64_from_bits(bits);
    double source = audio_math_float64_from_bits(sign);
    uint64_t expected =
        (bits & UINT64_C(0x7fffffffffffffff)) | (sign & UINT64_C(0x8000000000000000));
    uint64_t actual = audio_math_float64_bits(audio_math_copy_sign_f64(value, source));
    ++comparisons;
    if (actual != expected || audio_math_float64_bits(crt_sign(value, source)) != expected) {
        printf("FAIL sign input=%016" PRIx64 "\n", bits);
        return 0;
    }
    return 1;
}

/* Call host exponent extraction for verification/benchmarking. */
static int baseline_exponent(float value) {
    int exponent = 0;
    (void)frexp((double)value, &exponent);
    return exponent;
}
/* Call host power-of-two scaling for verification/benchmarking. */
static double baseline_scale(double value, int power) {
    return ldexp(value, power);
}
/* Call host sign copying for verification/benchmarking. */
static double baseline_sign(double value, double sign) {
    return copysign(value, sign);
}
static int (*volatile exponent_functions[2])(float) = {baseline_exponent, audio_math_frexp_exponent_f32};
static double (*volatile scale_functions[2])(double, int) = {baseline_scale, audio_math_scale_pow2_f64};
static double (*volatile sign_functions[2])(double, double) = {baseline_sign, audio_math_copy_sign_f64};
static double samples[1024];
static volatile uint64_t consumed;

/* Read monotonic time for host testing only. */
static double now_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return value.tv_sec + value.tv_nsec * 1e-9;
#endif
}

/* Alternate timing over normal audio amplitudes without letting boundary cases dominate benchmark samples. */
static int benchmark(int compare_full) {
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), 1);
#endif
    for (unsigned k = 0; k < 1024; ++k)
        samples[k] = (double)((int)k - 511) * 0.03125 + 0.015625;
    if (compare_full)
        scale_functions[0] = audio_math_scale_pow2_f64_fallback;
    for (unsigned kind = compare_full ? 1 : 0; kind < (compare_full ? 2u : 3u); ++kind) {
        for (unsigned round = 0; round < 9; ++round) {
            double times[2];
            uint64_t sums[2];
            for (unsigned visit = 0; visit < 2; ++visit) {
                unsigned side = visit ^ (round & 1);
                uint64_t sum = 0;
                double start = now_seconds();
                for (unsigned k = 0; k < 262144; ++k) {
                    double value = samples[k & 1023];
                    if (kind == 0)
                        sum += (uint32_t)exponent_functions[side]((float)value);
                    else if (kind == 1)
                        sum += audio_math_float64_bits(scale_functions[side](value, (int)(k & 31) - 15));
                    else
                        sum += audio_math_float64_bits(sign_functions[side](0.5, value));
                }
                times[side] = now_seconds() - start;
                sums[side] = sum;
                consumed = sum;
            }
            if (sums[0] != sums[1])
                return 1;
            printf("{\"kind\":%u,\"round\":%u,\"operations\":262144,\"before\":%.12g,"
                   "\"after\":%.12g,\"checksum\":\"%016" PRIx64 "\"}\n",
                   kind, round, times[0], times[1], sums[0]);
        }
    }
    return 0;
}

/* Cover exponent boundaries, half-ULPs, subnormals, extreme int exponents, and reproducible random bits. */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--bench") == 0)
        return benchmark(0);
    if (argc == 2 && strcmp(argv[1], "--bench-full") == 0)
        return benchmark(1);
    const int powers[] = {INT_MIN, -4096, -2098, -1075, -1074, -1023, -1022, -53,  -52,  -1,
                          0,       1,     52,    53,    1022,  1023,  1074,  2098, 4096, INT_MAX};
    const uint64_t fractions[] = {0,
                                  1,
                                  2,
                                  3,
                                  UINT64_C(0x0007ffffffffffff),
                                  UINT64_C(0x0008000000000000),
                                  UINT64_C(0x000ffffffffffffe),
                                  UINT64_C(0x000fffffffffffff)};
    for (unsigned e = 0; e < 2048; ++e) {
        for (unsigned f = 0; f < sizeof fractions / sizeof *fractions; ++f) {
            for (unsigned sign = 0; sign < 2; ++sign) {
                uint64_t bits = ((uint64_t)sign << 63) | ((uint64_t)e << 52) | fractions[f];
                for (unsigned p = 0; p < sizeof powers / sizeof *powers; ++p) {
                    if (!check_scale(bits, powers[p]))
                        return 1;
                }
                /* Precisely scan shifts/rounding near normal/subnormal boundaries. */
                for (int target = -55; target <= 2; ++target) {
                    if (!check_scale(bits, target - (int)e))
                        return 1;
                }
            }
        }
    }
    /* Every positive binary32 subnormal, not just powers of two. */
    for (uint32_t bits = 0; bits < UINT32_C(0x00800000); ++bits) {
        if (!check_exponent(bits))
            return 1;
    }
    for (unsigned k = 0; k < 250000; ++k) {
        uint64_t bits = random_bits();
        if (!check_exponent((uint32_t)bits) ||
            !check_sign(bits, random_bits() & UINT64_C(0x8000000000000000)) ||
            !check_scale(bits, (int)(random_bits() % 4401) - 2200))
            return 1;
    }
    const uint32_t specials[] = {0x80000000u, 0x7f800000u, 0xff800000u,
                                 0x7fc12345u, 0xffc12345u, 0x7f812345u};
    for (unsigned k = 0; k < sizeof specials / sizeof *specials; ++k) {
        if (!check_exponent(specials[k]))
            return 1;
    }
    printf("PASS comparisons=%" PRIu64 " subnormal_f32=8388608 crt_underflow_differences=%" PRIu64
           "\n",
           comparisons, crt_underflow_differences);
    return 0;
}
