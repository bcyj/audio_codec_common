#ifndef AUDIO_MATH_DSP_FLOAT_WINDOW_H
#define AUDIO_MATH_DSP_FLOAT_WINDOW_H
#include <audio_math/float_trig.h>

/* Compute the float window sin((sin(x) * (double)HALF_PI_F32) * sin(x)).
 * Retain inner double sine and existing product order, storing float only after outer sine.
 * Valid range is [0,2]; invalid input yields quiet NaN. Not a general double sine.
 * Require nearest-even rounding, preserved subnormals, and no contraction; no CRT calls.
 */
static inline float audio_math_sine_squared_window_bounded_f64_to_f32_soft(double input) {
    uint64_t bits = audio_math_float64_bits(input) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS;
    if (input < 0.0 || bits > audio_math_float64_bits((double)AUDIO_MATH_TRIG_MAX_PHASE_F32))
        return audio_math_float32_from_bits(AUDIO_MATH_TRIG_QUIET_NAN_BITS);
    int quadrant = input < AUDIO_MATH_HALF_PI_F64 * 0.5 ? 0 : 1;
    double reduced = (input - (double)quadrant * AUDIO_MATH_HALF_PI_F64) -
                     (double)quadrant * AUDIO_MATH_HALF_PI_LOW_F64;
    double square = reduced * reduced;
    double sine;
    if (quadrant == 0) {
        double polynomial = 2.8114572543455206e-15;
        polynomial = -7.647163731819816e-13 + square * polynomial;
        polynomial = 1.6059043836821613e-10 + square * polynomial;
        polynomial = -2.505210838544172e-08 + square * polynomial;
        polynomial = 2.7557319223985893e-06 + square * polynomial;
        polynomial = -0.0001984126984126984 + square * polynomial;
        polynomial = 0.008333333333333333 + square * polynomial;
        polynomial = -0.16666666666666666 + square * polynomial;
        sine = (1.0 + square * polynomial) * reduced;
    } else {
        double polynomial = 4.779477332387385e-14;
        polynomial = -1.1470745597729725e-11 + square * polynomial;
        polynomial = 2.08767569878681e-09 + square * polynomial;
        polynomial = -2.755731922398589e-07 + square * polynomial;
        polynomial = 2.48015873015873e-05 + square * polynomial;
        polynomial = -0.001388888888888889 + square * polynomial;
        polynomial = 0.041666666666666664 + square * polynomial;
        polynomial = -0.5 + square * polynomial;
        sine = 1.0 + square * polynomial;
    }
    double phase = (sine * (double)AUDIO_MATH_HALF_PI_F32) * sine;
    float result, ignored;
    audio_math_sincos_bounded_f64_to_f32_soft(phase, &result, &ignored);
    return result;
}
#endif
