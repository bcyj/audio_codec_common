#include "math_host_reference.h"
#include <audio_math/numeric_bits.h>
#include <audio_math/float_rounding.h>
#include <math.h>

/* Return observable bits of the logf expression. */
static uint64_t reference_logf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(logf(x));
}

/* Return observable bits of the log10f expression. */
static uint64_t reference_log10f(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(log10f(x));
}

/* Return observable bits of the expf expression. */
static uint64_t reference_expf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(expf(x));
}

/* Return observable bits of the asinf expression. */
static uint64_t reference_asinf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(asinf(x));
}

/* Return observable bits of the powf expression. */
static uint64_t reference_powf(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(powf(x, y));
}

/* Return observable bits of the log expression. */
static uint64_t reference_log(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(log(x));
}

/* Return observable bits of the log10 expression. */
static uint64_t reference_log10(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(log10(x));
}

/* Return observable bits of the exp2 expression. */
static uint64_t reference_exp2(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(exp2(x));
}

/* Return observable bits of the pow expression. */
static uint64_t reference_pow(uint64_t a, uint64_t b) {
    double x = audio_math_float64_from_bits(a);
    double y = audio_math_float64_from_bits(b);
    (void)y;
    return audio_math_float64_bits(pow(x, y));
}

/* Return observable bits of the gain_index expression. */
static uint64_t reference_gain_index(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float64_bits(audio_math_ceil_f64((double)log10f(x) * 28.0));
}

/* Return observable bits of the power10f expression. */
static uint64_t reference_power10f(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(powf(10.0f, x / 28.0f));
}

/* Return observable bits of the ar_decay expression. */
static uint64_t reference_ar_decay(uint64_t a, uint64_t b) {
    float x = audio_math_float32_from_bits((uint32_t)a);
    float y = audio_math_float32_from_bits((uint32_t)b);
    (void)y;
    return audio_math_float32_bits(expf(-2000.0f / (x * y)));
}

LhdcMathHostFunction const lhdc_test_math_host_reference[LHDC_TEST_MATH_HOST_OPERATIONS] = {
    reference_logf, reference_log10f,     reference_expf,     reference_asinf,
    reference_powf, reference_log,        reference_log10,    reference_exp2,
    reference_pow,  reference_gain_index, reference_power10f, reference_ar_decay};
