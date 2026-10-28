#ifndef AUDIO_MATH_DSP_FLOAT_MDCT_H
#define AUDIO_MATH_DSP_FLOAT_MDCT_H
#include <audio_math/float_trig.h>
/* Return double sine/cosine over [-2,2] for MDCT post-scaling and nested windows.
 * Callers guarantee finite bounded input, nearest-even rounding, preserved subnormals, and no contraction.
 * Do not narrow early to float or treat this as a full-domain/correctly-rounded libm replacement. */
static inline void audio_math_sincos_quarter_turn_f64_soft(double input, double *sine, double *cosine) {
    double magnitude = input < 0 ? -input : input;
    int quadrant = magnitude < AUDIO_MATH_HALF_PI_F64 * 0.5 ? 0 : (input < 0 ? -1 : 1);
    double r = (input - quadrant * AUDIO_MATH_HALF_PI_F64) - quadrant * AUDIO_MATH_HALF_PI_LOW_F64;
    double square = r * r;
    double s = 2.8114572543455206e-15;
    s = -7.647163731819816e-13 + square * s;
    s = 1.6059043836821613e-10 + square * s;
    s = -2.505210838544172e-08 + square * s;
    s = 2.7557319223985893e-06 + square * s;
    s = -0.0001984126984126984 + square * s;
    s = 0.008333333333333333 + square * s;
    s = -0.16666666666666666 + square * s;
    s = (1.0 + square * s) * r;
    double c = 4.779477332387385e-14;
    c = -1.1470745597729725e-11 + square * c;
    c = 2.08767569878681e-09 + square * c;
    c = -2.755731922398589e-07 + square * c;
    c = 2.48015873015873e-05 + square * c;
    c = -0.001388888888888889 + square * c;
    c = 0.041666666666666664 + square * c;
    c = -0.5 + square * c;
    c = 1.0 + square * c;
    *sine = quadrant == 0 ? s : quadrant > 0 ? c : -c;
    *cosine = quadrant == 0 ? c : quadrant > 0 ? -s : s;
}
/* Retain inner double sine, squaring, and multiplication by double pi/2, then output a float Vorbis window.
 * Positive overlap lengths produce phases in (0,pi/2). */
static inline float audio_math_vorbis_window_bounded_f64_to_f32_soft(double phase) {
    double sine, ignored;
    audio_math_sincos_quarter_turn_f64_soft(phase, &sine, &ignored);
    audio_math_sincos_quarter_turn_f64_soft(sine * sine * AUDIO_MATH_HALF_PI_F64, &sine, &ignored);
    return (float)sine;
}
#endif
