#include "math_host_reference.h"
#include <audio_math/numeric_bits.h>
#include <audio_math/float_rounding.h>
#include <math.h>
#include <audio_math/math.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* Return observable bits of the logf expression. */
static uint64_t current_logf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_logf(x));
}

/* Return observable bits of the log10f expression. */
static uint64_t current_log10f(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_log10f(x));
}

/* Return observable bits of the expf expression. */
static uint64_t current_expf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_expf(x));
}

/* Return observable bits of the asinf expression. */
static uint64_t current_asinf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_asinf(x));
}

/* Return observable bits of the powf expression. */
static uint64_t current_powf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_powf(x, y));
}

/* Return observable bits of the log expression. */
static uint64_t current_log(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(audio_math_log(x));
}

/* Return observable bits of the log10 expression. */
static uint64_t current_log10(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(audio_math_log10(x));
}

/* Return observable bits of the exp2 expression. */
static uint64_t current_exp2(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(audio_math_exp2(x));
}

/* Return observable bits of the pow expression. */
static uint64_t current_pow(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(audio_math_pow(x, y));
}

/* Return observable bits of the gain_index expression. */
static uint64_t current_gain_index(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float64_bits(audio_math_ceil_f64((double)audio_math_log10f(x) * 28.0));
}

/* Return observable bits of the power10f expression. */
static uint64_t current_power10f(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_powf(10.0f, x / 28.0f));
}

/* Return observable bits of the ar_decay expression. */
static uint64_t current_ar_decay(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(audio_math_expf(-2000.0f / (x * y)));
}

static LhdcMathHostFunction volatile current_functions[LHDC_TEST_MATH_HOST_OPERATIONS] = {
    current_logf, current_log10f,     current_expf,     current_asinf,
    current_powf, current_log,        current_log10,    current_exp2,
    current_pow,  current_gain_index, current_power10f, current_ar_decay};

static uint32_t state = UINT32_C(0x17430692);
static uint64_t cases, checks;
static uint64_t input_x[1024], input_y[1024];
static volatile uint64_t consumed;

/* Generate deterministic full-width floating-point bit patterns. */
static uint32_t random_bits(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

/* Compare nine math entries and three actual expressions bitwise, including NaN payloads and negative zero. */
static int check_all(uint64_t x, uint64_t y) {
    ++cases;
    for (unsigned op = 0; op < LHDC_TEST_MATH_HOST_OPERATIONS; ++op) {
        uint64_t before = lhdc_test_math_host_reference[op](x, y);
        uint64_t after = current_functions[op](x, y);
        ++checks;
        if (before != after) {
            printf("FAIL op=%u x=%016" PRIx64 " y=%016" PRIx64 " before=%016" PRIx64
                   " after=%016" PRIx64 "\n",
                   op, x, y, before, after);
            return 0;
        }
    }
    return 1;
}

/* Read the host monotonic clock for testing only. */
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

/* Time finite and wide-bit-pattern domains per operation, alternating order and checking checksums. */
static int benchmark(void) {
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), 1);
#endif
    for (unsigned op = 0; op < LHDC_TEST_MATH_HOST_OPERATIONS; ++op) {
        for (unsigned group = 0; group < 2; ++group) {
            for (unsigned i = 0; i < 1024; ++i) {
                if (group) {
                    input_x[i] = ((uint64_t)random_bits() << 32) | random_bits();
                    input_y[i] = ((uint64_t)random_bits() << 32) | random_bits();
                } else {
                    double x = op == 3 ? (double)(i % 1000) / 500.0 - 1.0
                                       : 0.25 + (double)(i % 1000) / 100.0;
                    double y = 0.125 + (double)(i % 17) / 8.0;
                    int wide = op >= 5 && op <= 8;
                    input_x[i] = wide ? audio_math_float64_bits(x) : audio_math_float32_bits((float)x);
                    input_y[i] = wide ? audio_math_float64_bits(y) : audio_math_float32_bits((float)y);
                }
            }
            for (unsigned round = 0; round < 7; ++round) {
                double times[2];
                uint64_t sums[2];
                for (unsigned visit = 0; visit < 2; ++visit) {
                    unsigned side = (visit + round) & 1;
                    LhdcMathHostFunction fn =
                        side ? current_functions[op] : lhdc_test_math_host_reference[op];
                    uint64_t sum = 0;
                    double start = now_seconds();
                    for (unsigned i = 0; i < 65536; ++i)
                        sum += fn(input_x[i & 1023], input_y[i & 1023]);
                    times[side] = now_seconds() - start;
                    sums[side] = sum;
                    consumed = sum;
                }
                if (sums[0] != sums[1])
                    return 1;
                printf("{\"op\":%u,\"range\":%u,\"round\":%u,\"operations\":65536,"
                       "\"before\":%.12g,\"after\":%.12g,\"checksum\":\"%016" PRIx64 "\"}\n",
                       op, group, round, times[0], times[1], sums[0]);
            }
        }
    }
    return 0;
}

/* Cover exceptions, every exponent, quantization/gain ranges, and 250,000 random input pairs. */
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--bench") == 0)
        return benchmark();
    const uint32_t f32[] = {0,           0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f800001u,
                            0x3f7fffffu, 0xbf800001u, 0xbf7fffffu, 0x00800000u, 0x007fffffu,
                            1,           0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
                            0x7fc12345u, 0xffc12345u, 0x7f812345u, 0xff812345u, 0x40000000u};
    const uint64_t f64[] = {0,
                            UINT64_C(0x8000000000000000),
                            UINT64_C(0x3ff0000000000000),
                            UINT64_C(0xbff0000000000000),
                            UINT64_C(0x3ff0000000000001),
                            UINT64_C(0x3fefffffffffffff),
                            UINT64_C(0xbff0000000000001),
                            UINT64_C(0xbfefffffffffffff),
                            UINT64_C(0x0010000000000000),
                            UINT64_C(0x000fffffffffffff),
                            1,
                            UINT64_C(0x7fefffffffffffff),
                            UINT64_C(0xffefffffffffffff),
                            UINT64_C(0x7ff0000000000000),
                            UINT64_C(0xfff0000000000000),
                            UINT64_C(0x7ff8123456789abc),
                            UINT64_C(0xfff8123456789abc),
                            UINT64_C(0x7ff0123456789abc),
                            UINT64_C(0xfff0123456789abc),
                            UINT64_C(0x4000000000000000)};
    for (unsigned i = 0; i < 20; ++i)
        for (unsigned j = 0; j < 20; ++j)
            if (!check_all(f32[i], f32[j]) || !check_all(f64[i], f64[j]))
                return 1;
    const uint32_t fraction32[] = {0, 1, 0x003fffffu, 0x007fffffu};
    for (unsigned exponent = 0; exponent < 256; ++exponent)
        for (unsigned variant = 0; variant < 8; ++variant) {
            uint32_t x =
                (exponent << 23) | fraction32[variant & 3] | (variant & 4 ? 0x80000000u : 0);
            if (!check_all(x, variant & 1 ? 0x3f000000u : 0xbf800000u))
                return 1;
        }
    const uint64_t fraction64[] = {0, 1, UINT64_C(0x0007ffffffffffff),
                                   UINT64_C(0x000fffffffffffff)};
    for (unsigned exponent = 0; exponent < 2048; ++exponent)
        for (unsigned variant = 0; variant < 8; ++variant) {
            uint64_t x = ((uint64_t)exponent << 52) | fraction64[variant & 3] |
                         (variant & 4 ? UINT64_C(0x8000000000000000) : 0);
            if (!check_all(x, variant & 1 ? UINT64_C(0x3fe0000000000000)
                                          : UINT64_C(0xbff0000000000000)))
                return 1;
        }
    for (unsigned i = 0; i < 2048; ++i) {
        double x = (double)i / 16.0 - 64.0, y = (double)i / 64.0 - 16.0;
        if (!check_all(audio_math_float32_bits((float)x), audio_math_float32_bits((float)y)) ||
            !check_all(audio_math_float64_bits(x), audio_math_float64_bits(y)))
            return 1;
    }
    for (unsigned i = 0; i < 250000; ++i) {
        uint64_t x = ((uint64_t)random_bits() << 32) | random_bits();
        uint64_t y = ((uint64_t)random_bits() << 32) | random_bits();
        if (!check_all(x, y))
            return 1;
    }
    printf("PASS math_host_cases=%" PRIu64 " checks=%" PRIu64 " operations=12\n", cases, checks);
    return 0;
}
