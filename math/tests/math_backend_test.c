#include <math.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#ifdef _GNU_SOURCE
#define LHDC_TEST_GNU_BEFORE 1
#else
#define LHDC_TEST_GNU_BEFORE 0
#endif
#include <audio_math/math.h>
#include <audio_math/numeric_bits.h>
#include <audio_math/constants.h>
#ifdef _GNU_SOURCE
#if !LHDC_TEST_GNU_BEFORE
#error Public math headers must not enable _GNU_SOURCE.
#endif
#else
#if LHDC_TEST_GNU_BEFORE
#error Public math headers must not disable _GNU_SOURCE.
#endif
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

void lhdc_test_reference_sincos(double phase, double *sine, double *cosine);
static void (*volatile functions[2])(double, double *, double *) = {lhdc_test_reference_sincos,
                                                                    audio_math_sincos};
static uint64_t state = UINT64_C(0x753ba12d1945678f);
static uint64_t checks;
static volatile uint64_t consumed;
static double inputs[1024];

/* Generate reproducible double input patterns. */
static uint64_t random_bits(void) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}
/* Compare both outputs bitwise, including zero signs and NaN payloads. */
static int check_value(uint64_t bits) {
    double sine[2], cosine[2];
    double value = audio_math_float64_from_bits(bits);
    functions[0](value, &sine[0], &cosine[0]);
    functions[1](value, &sine[1], &cosine[1]);
    ++checks;
    if (audio_math_float64_bits(sine[0]) != audio_math_float64_bits(sine[1]) ||
        audio_math_float64_bits(cosine[0]) != audio_math_float64_bits(cosine[1])) {
        printf("FAIL phase=%016" PRIx64 " sine=%016" PRIx64 "/%016" PRIx64 " cosine=%016" PRIx64
               "/%016" PRIx64 "\n",
               bits, audio_math_float64_bits(sine[0]), audio_math_float64_bits(sine[1]),
               audio_math_float64_bits(cosine[0]), audio_math_float64_bits(cosine[1]));
        return 0;
    }
    return 1;
}
/* Read the host monotonic clock independently of product implementation. */
static double now_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER ticks, frequency;
    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&frequency);
    return (double)ticks.QuadPart / frequency.QuadPart;
#else
    struct timespec ticks;
    clock_gettime(CLOCK_MONOTONIC, &ticks);
    return ticks.tv_sec + ticks.tv_nsec * 1e-9;
#endif
}
/* Time FFT phases, ordinary angles, and large angles separately, alternating order and checking output checksums. */
static int benchmark(void) {
#ifdef _WIN32
    if (!SetThreadAffinityMask(GetCurrentThread(), 1))
        return 1;
#endif
    for (unsigned range = 0; range < 3; ++range) {
        for (unsigned i = 0; i < 1024; ++i) {
            uint64_t bits = random_bits();
            inputs[i] = range == 0 ? ((double)i - 512.0) * AUDIO_MATH_TWO_PI_F64 / 512.0
                        : range == 1
                            ? ((double)i - 512.0) * 16.0
                            : audio_math_float64_from_bits((bits & UINT64_C(0x800fffffffffffff)) |
                                                     ((uint64_t)(1100 + i % 947) << 52));
        }
        for (unsigned round = 0; round < 7; ++round) {
            double times[2];
            uint64_t sums[2];
            for (unsigned visit = 0; visit < 2; ++visit) {
                unsigned side = visit ^ (round & 1);
                uint64_t sum = 0;
                double start = now_seconds();
                for (unsigned i = 0; i < 65536; ++i) {
                    double sine, cosine;
                    functions[side](inputs[i & 1023], &sine, &cosine);
                    sum += audio_math_float64_bits(sine) ^ audio_math_float64_bits(cosine);
                }
                times[side] = now_seconds() - start;
                sums[side] = sum;
                consumed = sum;
            }
            if (sums[0] != sums[1])
                return 1;
            printf("{\"range\":%u,\"round\":%u,\"operations\":65536,\"before\":%.12g,"
                   "\"after\":%.12g,\"checksum\":\"%016" PRIx64 "\"}\n",
                   range, round, times[0], times[1], sums[0]);
        }
    }
    return 0;
}
/* Cover special values, all exponents, FFT phase grids, and random double inputs. */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--bench") == 0)
        return benchmark();
    const uint64_t values[] = {0,
                               UINT64_C(0x8000000000000000),
                               1,
                               UINT64_C(0x8000000000000001),
                               UINT64_C(0x000fffffffffffff),
                               UINT64_C(0x0010000000000000),
                               UINT64_C(0x3ff0000000000000),
                               UINT64_C(0xbff0000000000000),
                               UINT64_C(0x400921fb54442d18),
                               UINT64_C(0xc00921fb54442d18),
                               UINT64_C(0x7fefffffffffffff),
                               UINT64_C(0xffefffffffffffff),
                               UINT64_C(0x7ff0000000000000),
                               UINT64_C(0xfff0000000000000),
                               UINT64_C(0x7ff8123456789abc),
                               UINT64_C(0xfff8123456789abc),
                               UINT64_C(0x7ff0123456789abc),
                               UINT64_C(0xfff0123456789abc),
                               UINT64_C(0x7ff8abcdef123456),
                               UINT64_C(0xfff0abcdef123456)};
    for (unsigned i = 0; i < sizeof values / sizeof *values; ++i)
        if (!check_value(values[i]))
            return 1;
    const uint64_t fractions[] = {0, 1, UINT64_C(0x0007ffffffffffff), UINT64_C(0x000fffffffffffff)};
    for (unsigned exponent = 0; exponent < 2048; ++exponent)
        for (unsigned variant = 0; variant < 8; ++variant)
            if (!check_value(((uint64_t)exponent << 52) | fractions[variant & 3] |
                             (variant & 4 ? UINT64_C(0x8000000000000000) : 0)))
                return 1;
    for (int index = -4096; index <= 4096; ++index)
        if (!check_value(audio_math_float64_bits((double)index * AUDIO_MATH_TWO_PI_F64 / 4096.0)))
            return 1;
    for (unsigned i = 0; i < 262144; ++i)
        if (!check_value(random_bits()))
            return 1;
    printf("PASS sincos_checks=%" PRIu64 "\n", checks);
    return 0;
}
