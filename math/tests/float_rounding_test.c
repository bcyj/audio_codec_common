#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <audio_math/float_rounding.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static float (*volatile reference32[4])(float) = {truncf, floorf, ceilf, roundf};
static double (*volatile reference64[4])(double) = {trunc, floor, ceil, round};
static float (*volatile candidate32[4])(float) = {audio_math_trunc_f32, audio_math_floor_f32, audio_math_ceil_f32,
                                                  audio_math_round_f32};
static double (*volatile candidate64[4])(double) = {audio_math_trunc_f64, audio_math_floor_f64, audio_math_ceil_f64,
                                                    audio_math_round_f64};

/* Retain pre-change standard-library expressions behind equal C call boundaries to avoid an extra candidate-only call. */
static float baseline_trunc_f32(float value) {
    return truncf(value);
}
/* Call host float floor for verification/benchmarking. */
static float baseline_floor_f32(float value) {
    return floorf(value);
}
/* Call host float ceiling for verification/benchmarking. */
static float baseline_ceil_f32(float value) {
    return ceilf(value);
}
/* Call host float nearest rounding for verification/benchmarking. */
static float baseline_round_f32(float value) {
    return roundf(value);
}
/* Call host double truncation for verification/benchmarking. */
static double baseline_trunc_f64(double value) {
    return trunc(value);
}
/* Call host double floor for verification/benchmarking. */
static double baseline_floor_f64(double value) {
    return floor(value);
}
/* Call host double ceiling for verification/benchmarking. */
static double baseline_ceil_f64(double value) {
    return ceil(value);
}
/* Call host double nearest rounding for verification/benchmarking. */
static double baseline_round_f64(double value) {
    return round(value);
}
static float (*volatile baseline32[4])(float) = {baseline_trunc_f32, baseline_floor_f32,
                                                 baseline_ceil_f32, baseline_round_f32};
static double (*volatile baseline64[4])(double) = {baseline_trunc_f64, baseline_floor_f64,
                                                   baseline_ceil_f64, baseline_round_f64};

static uint64_t random_state = UINT64_C(0x3141592653589793);
static uint64_t comparisons;

/* Generate reproducible unsigned test patterns. */
static uint64_t random_bits(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}

/* Compare every binary32 rounding mode; finite/infinite results match system functions bitwise. */
static int check32(uint32_t bits) {
    float value = audio_math_float32_from_bits(bits);
    for (unsigned mode = 0; mode < 4; ++mode) {
        uint32_t expected = audio_math_float32_bits(reference32[mode](value));
        uint32_t actual = audio_math_float32_bits(candidate32[mode](value));
        uint32_t generic =
            (uint32_t)audio_math_integral_round_bits(bits, 23, 8, 127, (AudioMathIntegralRounding)mode);
        if ((bits & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS) > AUDIO_MATH_FLOAT32_INFINITY_BITS) {
            expected = bits | UINT32_C(0x00400000);
        }
        if (expected != actual || expected != generic) {
            printf("FAIL32 mode=%u input=%08" PRIx32 " expected=%08" PRIx32 " actual=%08" PRIx32
                   "\n",
                   mode, bits, expected, actual);
            return 0;
        }
        comparisons += 2;
    }
    return 1;
}

/* Compare binary64 system results; verify NaNs using common payload-preserving quieting rules. */
static int check64(uint64_t bits) {
    double value = audio_math_float64_from_bits(bits);
    for (unsigned mode = 0; mode < 4; ++mode) {
        uint64_t expected = audio_math_float64_bits(reference64[mode](value));
        uint64_t actual = audio_math_float64_bits(candidate64[mode](value));
        uint64_t generic = audio_math_integral_round_bits(bits, 52, 11, 1023, (AudioMathIntegralRounding)mode);
        if ((bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000))
            expected = bits | UINT64_C(0x0008000000000000);
        if (expected != actual || expected != generic) {
            printf("FAIL64 mode=%u input=%016" PRIx64 " expected=%016" PRIx64 " actual=%016" PRIx64
                   "\n",
                   mode, bits, expected, actual);
            return 0;
        }
        comparisons += 2;
    }
    return 1;
}

/* Read monotonic time, keeping test clocks separate from library runtime dependencies. */
static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER value, frequency;
    QueryPerformanceCounter(&value);
    QueryPerformanceFrequency(&frequency);
    return (double)value.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec + (double)value.tv_nsec * 1e-9;
#endif
}

/* Benchmark rounding entries with identical input/indirect calls and retained checksums. */
static int benchmark(void) {
#ifdef _WIN32
    DWORD_PTR allowed, system;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &allowed, &system) ||
        !SetProcessAffinityMask(GetCurrentProcess(), allowed & (0 - allowed)))
        return 2;
#endif
    float *input32 = malloc(4096 * sizeof *input32);
    double *input64 = malloc(4096 * sizeof *input64);
    if (!input32 || !input64)
        return 3;
    for (unsigned i = 0; i < 4096; ++i) {
        uint64_t bits = random_bits();
        input32[i] = audio_math_float32_from_bits((uint32_t)(bits & UINT32_C(0x807fffff)) |
                                            ((119 + i % 35) << 23));
        input64[i] = audio_math_float64_from_bits((bits & UINT64_C(0x800fffffffffffff)) |
                                            ((uint64_t)(1015 + i % 68) << 52));
    }
    for (unsigned precision = 32; precision <= 64; precision += 32) {
        for (unsigned mode = 0; mode < 4; ++mode) {
            for (unsigned round = 0; round < 9; ++round) {
                double elapsed[2];
                uint64_t checksums[2];
                for (unsigned order = 0; order < 2; ++order) {
                    unsigned side = (order + round) & 1;
                    uint64_t checksum = 0;
                    double start = now();
                    for (unsigned repeat = 0; repeat < 32; ++repeat) {
                        for (unsigned i = 0; i < 4096; ++i) {
                            if (precision == 32)
                                checksum += audio_math_float32_bits(
                                    (side ? candidate32[mode] : baseline32[mode])(input32[i]));
                            else
                                checksum += audio_math_float64_bits(
                                    (side ? candidate64[mode] : baseline64[mode])(input64[i]));
                        }
                    }
                    elapsed[side] = now() - start;
                    checksums[side] = checksum;
                }
                if (checksums[0] != checksums[1])
                    return 4;
                printf(
                    "{\"precision\":%u,\"mode\":%u,\"round\":%u,\"operations\":131072,\"before\":%.9f,\"after\":%.9f,\"checksum\":\"%016" PRIx64
                    "\"}\n",
                    precision, mode, round, elapsed[0], elapsed[1], checksums[0]);
            }
        }
    }
    free(input32);
    free(input64);
    return 0;
}

/* Cover every exponent, signed zeros, subnormals, half-integer neighborhoods, and deterministic random bits. */
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--bench"))
        return benchmark();
    const uint64_t fractions[] = {0,
                                  1,
                                  UINT64_C(0x7ffffffffffff),
                                  UINT64_C(0x8000000000000),
                                  UINT64_C(0x8000000000001),
                                  UINT64_C(0xfffffffffffff)};
    for (unsigned exponent = 0; exponent < 2048; ++exponent) {
        for (unsigned sign = 0; sign < 2; ++sign) {
            for (unsigned i = 0; i < sizeof fractions / sizeof *fractions; ++i) {
                uint64_t bits = ((uint64_t)sign << 63) | ((uint64_t)exponent << 52) | fractions[i];
                if (!check64(bits))
                    return 1;
                if (exponent < 256 &&
                    !check32((sign << 31) | (exponent << 23) | (uint32_t)(fractions[i] >> 29)))
                    return 1;
            }
        }
    }
    for (int i = -10000; i <= 10000; ++i) {
        double value = (double)i + 0.5;
        uint64_t bits64 = audio_math_float64_bits(value);
        uint32_t bits32 = audio_math_float32_bits((float)value);
        for (int adjacent = -1; adjacent <= 1; ++adjacent) {
            if (!check64(bits64 + (uint64_t)adjacent) || !check32(bits32 + (uint32_t)adjacent))
                return 1;
        }
    }
    for (unsigned i = 0; i < 262144; ++i) {
        if (!check32((uint32_t)random_bits()) || !check64(random_bits()))
            return 1;
    }
    printf("PASS comparisons=%" PRIu64 " finite_and_infinity=exact NaN=quiet_payload_policy\n",
           comparisons);
    return 0;
}
