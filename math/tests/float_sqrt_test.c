#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <audio_math/float_sqrt.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static float (*volatile reference)(float) = sqrtf;
/* Retain the system square-root entry for speed/local-stack comparison. */
static float baseline_sqrt(float value) {
    return sqrtf(value);
}
static float (*volatile functions[3])(float) = {baseline_sqrt, audio_math_sqrt_f32, audio_math_sqrt_soft_f32};
static uint32_t state = UINT32_C(0x572b194d);
static uint64_t checks;
static volatile uint64_t consumed;
static float inputs[1024];

/* Generate reproducible full-width input patterns. */
static uint32_t random_bits(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
/* Compare system, common hardware, and pure-C outputs bitwise, with additional double references for valid inputs. */
static int check_value(uint32_t bits) {
    float value = audio_math_float32_from_bits(bits);
    uint32_t expected = audio_math_float32_bits(reference(value));
    uint32_t actual = audio_math_float32_bits(functions[1](value));
    uint32_t software = audio_math_float32_bits(functions[2](value));
    ++checks;
    if (expected != actual || expected != software) {
        printf("FAIL input=%08" PRIx32 " expected=%08" PRIx32 " actual=%08" PRIx32
               " software=%08" PRIx32 "\n",
               bits, expected, actual, software);
        return 0;
    }
    if (!(bits & AUDIO_MATH_FLOAT32_SIGN_BITS) && bits < AUDIO_MATH_FLOAT32_INFINITY_BITS &&
        expected != audio_math_float32_bits((float)sqrt((double)value)))
        return 0;
    return 1;
}
/* Read host monotonic time outside product implementation. */
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
/* Time DCT lengths, wide-exponent positives, and subnormals, rotating three paths and checking checksums. */
static int benchmark(void) {
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), 1);
#endif
    const unsigned lengths[] = {60, 110, 120, 128, 220, 240, 256, 440, 480, 512, 960, 1024, 1920};
    for (unsigned range = 0; range < 3; ++range) {
        for (unsigned i = 0; i < 1024; ++i) {
            uint32_t bits = random_bits();
            inputs[i] =
                range == 0 ? (float)lengths[i % (sizeof lengths / sizeof *lengths)]
                : range == 1
                    ? audio_math_float32_from_bits((bits % UINT32_C(0x7f000000)) + UINT32_C(0x00800000))
                    : audio_math_float32_from_bits((bits & UINT32_C(0x007fffff)) | 1u);
        }
        for (unsigned round = 0; round < 7; ++round) {
            double times[3];
            uint64_t sums[3];
            for (unsigned visit = 0; visit < 3; ++visit) {
                unsigned side = (visit + round) % 3;
                uint64_t sum = 0;
                double start = now_seconds();
                for (unsigned i = 0; i < 65536; ++i)
                    sum += audio_math_float32_bits(functions[side](inputs[i & 1023]));
                times[side] = now_seconds() - start;
                sums[side] = sum;
                consumed = sum;
            }
            if (sums[0] != sums[1] || sums[0] != sums[2])
                return 1;
            printf("{\"range\":%u,\"round\":%u,\"operations\":65536,\"before\":%.12g,"
                   "\"after\":%.12g,\"software\":%.12g,\"checksum\":\"%016" PRIx64 "\"}\n",
                   range, round, times[0], times[1], times[2], sums[0]);
        }
    }
    return 0;
}
/* Cover exceptional inputs, all exponents, square boundaries, subnormals, and a million random inputs.
 * Optionally exhaust all 24-bit normal significands at both exponent parities and all positive subnormal patterns.
 */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--bench") == 0)
        return benchmark();
    if (argc == 2 && strcmp(argv[1], "--exhaustive") == 0) {
        for (uint32_t fraction = 0; fraction < UINT32_C(0x00800000); ++fraction) {
            if (!check_value(UINT32_C(0x3f800000) | fraction) ||
                !check_value(UINT32_C(0x40000000) | fraction))
                return 1;
            if (fraction && !check_value(fraction))
                return 1;
        }
    } else {
        const uint32_t values[] = {0,           0x80000000u, 1,           0x80000001u, 0x007fffffu,
                                   0x00800000u, 0x3f800000u, 0xbf800000u, 0x40000000u, 0x40800000u,
                                   0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u,
                                   0xffc12345u, 0x7f812345u, 0xff812345u, 0x7fc54321u, 0xff854321u};
        for (unsigned i = 0; i < sizeof values / sizeof *values; ++i)
            if (!check_value(values[i]))
                return 1;
        const uint32_t fractions[] = {0, 1, 0x003fffffu, 0x007fffffu};
        for (unsigned exponent = 0; exponent < 256; ++exponent)
            for (unsigned variant = 0; variant < 8; ++variant)
                if (!check_value((exponent << 23) | fractions[variant & 3] |
                                 (variant & 4 ? AUDIO_MATH_FLOAT32_SIGN_BITS : 0)))
                    return 1;
        for (uint32_t bits = 0; bits < 65536; ++bits)
            if (!check_value(bits))
                return 1;
        for (unsigned integer = 1; integer < 65536; ++integer) {
            uint32_t square = audio_math_float32_bits((float)((double)integer * integer));
            for (unsigned neighbor = 0; neighbor < 3; ++neighbor)
                if (!check_value(square + neighbor - 1))
                    return 1;
        }
        for (unsigned i = 0; i < 1000000; ++i)
            if (!check_value(random_bits()))
                return 1;
    }
    printf("PASS sqrt_checks=%" PRIu64 "\n", checks);
    return 0;
}
