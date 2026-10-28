#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <audio_math/float_remainder.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static float (*volatile reference)(float, float) = fmodf;
static float (*volatile candidate)(float, float) = audio_math_fmod_f32;
static uint32_t state = UINT32_C(0x713a2d45);
static uint64_t checks;
/* Generate deterministic full-width input patterns. */
static uint32_t random_bits(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
/* Match system fmodf bitwise; additionally check valid finite inputs against double, magnitude, and sign references. */
static int check_pair(uint32_t x_bits, uint32_t y_bits) {
    float x = audio_math_float32_from_bits(x_bits), y = audio_math_float32_from_bits(y_bits);
    uint32_t expected = audio_math_float32_bits(reference(x, y));
    uint32_t actual = audio_math_float32_bits(candidate(x, y));
    ++checks;
    if (expected != actual) {
        printf("FAIL x=%08" PRIx32 " y=%08" PRIx32 " expected=%08" PRIx32 " actual=%08" PRIx32 "\n",
               x_bits, y_bits, expected, actual);
        return 0;
    }
    uint32_t ax = x_bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    uint32_t ay = y_bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if (ax < AUDIO_MATH_FLOAT32_INFINITY_BITS && ay != 0 && ay <= AUDIO_MATH_FLOAT32_INFINITY_BITS) {
        uint32_t wide = audio_math_float32_bits((float)fmod((double)x, (double)y));
        if (wide != actual || (actual & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) >= ay ||
            ((actual ^ x_bits) & AUDIO_MATH_FLOAT32_SIGN_BITS))
            return 0;
    }
    return 1;
}

/* Provide a host math-runtime entry for benchmarks. */
static float baseline_remainder(float x, float y) {
    return fmodf(x, y);
}
static float (*volatile functions[2])(float, float) = {baseline_remainder, audio_math_fmod_f32};
static float numerators[1024], divisors[1024];
static volatile uint64_t consumed;
/* Read host monotonic time without product participation. */
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
/* Time fast returns, ordinary angles, and huge angles separately, alternating order and checking checksums. */
static int benchmark(void) {
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), 1);
#endif
    for (unsigned range = 0; range < 3; ++range) {
        for (unsigned i = 0; i < 1024; ++i) {
            numerators[i] = range == 0 ? ((float)i - 512.0f) * 0.5f
                            : range == 1
                                ? ((float)i - 512.0f) * 16.0f
                                : audio_math_float32_from_bits((random_bits() & UINT32_C(0x807fffff)) |
                                                         (((random_bits() % 119) + 136) << 23));
            divisors[i] = 360.0f;
        }
        for (unsigned round = 0; round < 9; ++round) {
            double times[2];
            uint64_t sums[2];
            for (unsigned visit = 0; visit < 2; ++visit) {
                unsigned side = visit ^ (round & 1);
                uint64_t sum = 0;
                double start = now_seconds();
                for (unsigned i = 0; i < 131072; ++i)
                    sum += audio_math_float32_bits(
                        functions[side](numerators[i & 1023], divisors[i & 1023]));
                times[side] = now_seconds() - start;
                sums[side] = sum;
                consumed = sum;
            }
            if (sums[0] != sums[1])
                return 1;
            printf("{\"range\":%u,\"round\":%u,\"operations\":131072,\"before\":%.12g,"
                   "\"after\":%.12g,\"checksum\":\"%016" PRIx64 "\"}\n",
                   range, round, times[0], times[1], sums[0]);
        }
    }
    return 0;
}

/* Cover exceptional values, all exponents, subnormals, a million random inputs, and dual-NaN payload selection. */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--bench") == 0)
        return benchmark();
    const uint32_t values[] = {0,           0x80000000u, 1,           0x80000001u, 0x007fffffu,
                               0x00800000u, 0x3f800000u, 0xbf800000u, 0x43b40000u, 0xc3b40000u,
                               0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u,
                               0xffc12345u, 0x7f812345u, 0xff812345u, 0x7fc54321u, 0xff854321u};
    for (unsigned i = 0; i < sizeof values / sizeof *values; ++i)
        for (unsigned j = 0; j < sizeof values / sizeof *values; ++j)
            if (!check_pair(values[i], values[j]))
                return 1;
    const uint32_t fractions[] = {0, 1, 0x003fffffu, 0x007fffffu};
    for (unsigned exponent = 0; exponent < 256; ++exponent)
        for (unsigned variant = 0; variant < 8; ++variant) {
            uint32_t x =
                (exponent << 23) | fractions[variant & 3] | ((variant & 4) ? 0x80000000u : 0);
            for (unsigned j = 0; j < sizeof values / sizeof *values; ++j)
                if (!check_pair(x, values[j]) || !check_pair(values[j], x))
                    return 1;
        }
    for (uint32_t value = 0; value < 65536; ++value)
        for (unsigned j = 0; j < 6; ++j)
            if (!check_pair(value, values[j]))
                return 1;
    for (unsigned i = 0; i < 1000000; ++i)
        if (!check_pair(random_bits(), random_bits()))
            return 1;
    for (unsigned i = 0; i < 20000; ++i) {
        uint32_t x = (random_bits() & UINT32_C(0x807fffff)) | UINT32_C(0x7f800001);
        uint32_t y = (random_bits() & UINT32_C(0x807fffff)) | UINT32_C(0x7f800001);
        if (!check_pair(x, y))
            return 1;
    }
    printf("PASS remainder_checks=%" PRIu64 "\n", checks);
    return 0;
}
