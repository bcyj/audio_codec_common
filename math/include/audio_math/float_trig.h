#ifndef AUDIO_MATH_DSP_FLOAT_TRIG_H
#define AUDIO_MATH_DSP_FLOAT_TRIG_H
#include <audio_math/numeric_bits.h>
#include <audio_math/constants.h>

#define AUDIO_MATH_TRIG_MAX_PHASE_F32 2.0f
#define AUDIO_MATH_TRIG_MAX_PHASE_F64 7.0
#define AUDIO_MATH_TRIG_SMALL_PHASE_F32 0x1p-12f
#define AUDIO_MATH_TRIG_QUIET_NAN_BITS UINT32_C(0x7fc00000)

/* Reduce phase with small integer quadrants, evaluate shared double polynomials, and restore quadrant-specific float sine/cosine. */
static inline void audio_math_sincos_quadrant_f64_to_f32_soft(double input, int quadrant, float *sine,
                                                        float *cosine) {
    double high = input - (double)quadrant * AUDIO_MATH_HALF_PI_F64;
    double low = -(double)quadrant * AUDIO_MATH_HALF_PI_LOW_F64;
    double reduced = high + low;
    double square = reduced * reduced;
    double sine_estimate = 2.8114572543455206e-15;
    sine_estimate = -7.647163731819816e-13 + square * sine_estimate;
    sine_estimate = 1.6059043836821613e-10 + square * sine_estimate;
    sine_estimate = -2.505210838544172e-08 + square * sine_estimate;
    sine_estimate = 2.7557319223985893e-06 + square * sine_estimate;
    sine_estimate = -0.0001984126984126984 + square * sine_estimate;
    sine_estimate = 0.008333333333333333 + square * sine_estimate;
    sine_estimate = -0.16666666666666666 + square * sine_estimate;
    sine_estimate = 1.0 + square * sine_estimate;
    sine_estimate *= reduced;
    double cosine_estimate = 4.779477332387385e-14;
    cosine_estimate = -1.1470745597729725e-11 + square * cosine_estimate;
    cosine_estimate = 2.08767569878681e-09 + square * cosine_estimate;
    cosine_estimate = -2.755731922398589e-07 + square * cosine_estimate;
    cosine_estimate = 2.48015873015873e-05 + square * cosine_estimate;
    cosine_estimate = -0.001388888888888889 + square * cosine_estimate;
    cosine_estimate = 0.041666666666666664 + square * cosine_estimate;
    cosine_estimate = -0.5 + square * cosine_estimate;
    cosine_estimate = 1.0 + square * cosine_estimate;
    float s = (float)sine_estimate;
    float c = (float)cosine_estimate;
    if (quadrant == 1 || quadrant == -3) {
        *sine = c;
        *cosine = -s;
    } else if (quadrant == -1 || quadrant == 3) {
        *sine = -c;
        *cosine = s;
    } else if (quadrant == 2 || quadrant == -2) {
        *sine = -s;
        *cosine = -c;
    } else {
        *sine = s;
        *cosine = c;
    }
}

/* Compute sine/cosine for float phase in [-2,2]; out-of-range/nonfinite inputs yield quiet NaN.
 * Reduce to about [-pi/4,pi/4], evaluate degree-17/16 double sine/cosine polynomials, then store float.
 * Require nearest-even rounding and preserved subnormals; do not change rounding or call CRT.
 * Outputs may alias, retaining cosine last; phases at most 2^-12 return input and one directly. */
static inline void audio_math_sincos_bounded_f32_soft(float input, float *sine, float *cosine) {
    uint32_t magnitude = audio_math_float32_bits(input) & AUDIO_MATH_FLOAT32_MAGNITUDE_BITS;
    if (magnitude > audio_math_float32_bits(AUDIO_MATH_TRIG_MAX_PHASE_F32)) {
        *sine = *cosine = audio_math_float32_from_bits(AUDIO_MATH_TRIG_QUIET_NAN_BITS);
        return;
    }
    if (magnitude <= audio_math_float32_bits(AUDIO_MATH_TRIG_SMALL_PHASE_F32)) {
        *sine = input;
        *cosine = 1.0f;
        return;
    }
    int quadrant =
        magnitude >= audio_math_float32_bits(AUDIO_MATH_QUARTER_PI_F32) ? (input < 0 ? -1 : 1) : 0;
    audio_math_sincos_quadrant_f64_to_f32_soft((double)input, quadrant, sine, cosine);
}

/* Compute float sine/cosine for double phase in [-7,7] without narrowing phase early.
 * Require nearest-even rounding, preserved subnormals, and no multiply-add reordering. Float output only, not double trig.
 * Out-of-range/nonfinite inputs yield quiet NaN; outputs may alias with cosine retained last. */
static inline void audio_math_sincos_bounded_f64_to_f32_soft(double input, float *sine, float *cosine) {
    uint64_t bits = audio_math_float64_bits(input) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS;
    if (bits > audio_math_float64_bits(AUDIO_MATH_TRIG_MAX_PHASE_F64)) {
        *sine = *cosine = audio_math_float32_from_bits(AUDIO_MATH_TRIG_QUIET_NAN_BITS);
        return;
    }
    if (bits == 0) {
        *sine = (float)input;
        *cosine = 1.0f;
        return;
    }
    double magnitude = input < 0.0 ? -input : input;
    int quadrant = magnitude < AUDIO_MATH_HALF_PI_F64 * 0.5   ? 0
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 1.5 ? 1
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 2.5 ? 2
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 3.5 ? 3
                                                             : 4;
    if (input < 0.0)
        quadrant = -quadrant;
    audio_math_sincos_quadrant_f64_to_f32_soft(input, quadrant, sine, cosine);
}
/* Compute bounded (1-cos(x))/2 retaining the double cancellation boundary after cosine.
 * Reduce [-7,7] around zero, pi, or 2pi, reconstruct double with a degree-26 cosine polynomial, then store float.
 * For exhaustively verified short analysis windows; not general double cos or full-domain correct rounding.
 * Require nearest-even rounding, preserved subnormals, and no multiply-add reordering; invalid inputs return NaN.
 */
static inline float audio_math_hann_bounded_f64_to_f32_soft(double input) {
    uint64_t bits = audio_math_float64_bits(input) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS;
    if (bits > audio_math_float64_bits(AUDIO_MATH_TRIG_MAX_PHASE_F64)) {
        if (bits > AUDIO_MATH_FLOAT64_INFINITY_BITS)
            return (float)(input + input);
        return audio_math_float32_from_bits(AUDIO_MATH_TRIG_QUIET_NAN_BITS);
    }
    double phase = input < 0.0 ? -input : input;
    int quadrant = phase < AUDIO_MATH_HALF_PI_F64         ? 0
                   : phase < 3.0 * AUDIO_MATH_HALF_PI_F64 ? 2
                                                         : 4;
    double reduced = (phase - (double)quadrant * AUDIO_MATH_HALF_PI_F64) -
                     (double)quadrant * AUDIO_MATH_HALF_PI_LOW_F64;
    double square = reduced * reduced;
    double polynomial = -2.4795962632247976e-27;
    polynomial = 1.6117375710961184e-24 + square * polynomial;
    polynomial = -8.896791392450574e-22 + square * polynomial;
    polynomial = 4.110317623312165e-19 + square * polynomial;
    polynomial = -1.5619206968586225e-16 + square * polynomial;
    polynomial = 4.779477332387385e-14 + square * polynomial;
    polynomial = -1.1470745597729725e-11 + square * polynomial;
    polynomial = 2.08767569878681e-09 + square * polynomial;
    polynomial = -2.755731922398589e-07 + square * polynomial;
    polynomial = 2.48015873015873e-05 + square * polynomial;
    polynomial = -0.001388888888888889 + square * polynomial;
    polynomial = 0.041666666666666664 + square * polynomial;
    polynomial = -0.5 + square * polynomial;
    double cosine = 1.0 + square * polynomial;
    if (quadrant == 2)
        cosine = -cosine;
    return (float)((1.0 - cosine) * 0.5);
}
#endif
