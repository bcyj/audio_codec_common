#ifndef AUDIO_MATH_DSP_FLOAT_TRIG_Q31_H
#define AUDIO_MATH_DSP_FLOAT_TRIG_Q31_H
#include <audio_math/float_convert.h>
#include <audio_math/float_rounding.h>
#include <audio_math/float_trig.h>
#define AUDIO_MATH_TRIG_Q31_SPLITTER_F64 134217729.0
#define AUDIO_MATH_TRIG_Q31_INTEGER_GUARD_F64 0x1p-12
/* Two doubles hold high parts/rounding residuals for recomputation near Q31 boundaries only. */
typedef struct {
    double high, low;
} AudioMathTrigQ31Extended;

/* Add and normalize high/low parts with nearest-even rounding and sequential double operations. */
static inline AudioMathTrigQ31Extended audio_math_trig_q31_add(AudioMathTrigQ31Extended a, AudioMathTrigQ31Extended b) {
    double high = a.high + b.high;
    double part = high - a.high;
    double low = (a.high - (high - part)) + (b.high - part);
    low = low + a.low + b.low;
    double sum = high + low;
    return (AudioMathTrigQ31Extended){sum, low - (sum - high)};
}

/* Preserve product residuals by split multiplication without software/hardware FMA. */
static inline AudioMathTrigQ31Extended audio_math_trig_q31_multiply(AudioMathTrigQ31Extended a,
                                                         AudioMathTrigQ31Extended b) {
    double high = a.high * b.high;
    double left = a.high * AUDIO_MATH_TRIG_Q31_SPLITTER_F64;
    double ah = left - (left - a.high);
    double al = a.high - ah;
    double right = b.high * AUDIO_MATH_TRIG_Q31_SPLITTER_F64;
    double bh = right - (right - b.high);
    double bl = b.high - bh;
    double low = ((ah * bh - high) + ah * bl + al * bh) + al * bl;
    low = low + a.high * b.low;
    low = low + a.low * b.high;
    double sum = high + low;
    return (AudioMathTrigQ31Extended){sum, low - (sum - high)};
}

/* Evaluate the sine polynomial with high/low coefficients; callers multiply the reduced phase separately. */
static inline AudioMathTrigQ31Extended audio_math_trig_q31_sine_extended(AudioMathTrigQ31Extended square) {
    AudioMathTrigQ31Extended polynomial =
        (AudioMathTrigQ31Extended){6.446950284384474e-26, -1.9330404233703465e-42};
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-3.868170170630684e-23, 8.843177655482344e-40});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){1.9572941063391263e-20, -1.3643503830087908e-36});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-8.22063524662433e-18, -2.2141894119604265e-34});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){2.8114572543455206e-15, 1.6508842730861433e-31});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-7.647163731819816e-13, -7.03872877733453e-30});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){1.6059043836821613e-10, 1.2585294588752098e-26});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-2.505210838544172e-08, 1.448814070935912e-24});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){2.7557319223985893e-06, -1.858393274046472e-22});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-0.0001984126984126984, -1.7209558293420705e-22});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){0.008333333333333333, 1.1564823173178714e-19});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-0.16666666666666666, -9.25185853854297e-18});
    polynomial = audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                                   (AudioMathTrigQ31Extended){1.0, 0.0});
    return polynomial;
}

/* Evaluate the high/low cosine polynomial for integer-boundary recomputation. */
static inline AudioMathTrigQ31Extended audio_math_trig_q31_cosine_extended(AudioMathTrigQ31Extended square) {
    AudioMathTrigQ31Extended polynomial =
        (AudioMathTrigQ31Extended){1.6117375710961184e-24, -3.6846573564509766e-41};
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-8.896791392450574e-22, 7.911402614872376e-38});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){4.110317623312165e-19, 1.4412973378659527e-36});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-1.5619206968586225e-16, -1.1910679660273754e-32});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){4.779477332387385e-14, 4.399205485834081e-31});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-1.1470745597729725e-11, -2.0655512752830745e-28});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){2.08767569878681e-09, -1.20734505911326e-25});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-2.755731922398589e-07, -2.3767714622250297e-23});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){2.48015873015873e-05, 2.1511947866775882e-23});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){-0.001388888888888889, 5.300543954373577e-20});
    polynomial =
        audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                          (AudioMathTrigQ31Extended){0.041666666666666664, 2.3129646346357427e-18});
    polynomial = audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                                   (AudioMathTrigQ31Extended){-0.5, 0.0});
    polynomial = audio_math_trig_q31_add(audio_math_trig_q31_multiply(polynomial, square),
                                   (AudioMathTrigQ31Extended){1.0, 0.0});
    return polynomial;
}

/* Recompute trig near Q31 boundaries, rounding to double before scaling, adding half, and flooring.
 * Do not scale high/low parts directly to integers; retain existing binary64 intermediate rounding. */
static inline void audio_math_trig_q31_refine(double input, int32_t *imaginary, int32_t *real) {
    double magnitude = input < 0.0 ? -input : input;
    int quadrant = magnitude < AUDIO_MATH_HALF_PI_F64 * 0.5   ? 0
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 1.5 ? 1
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 2.5 ? 2
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 3.5 ? 3
                                                             : 4;
    if (input < 0.0)
        quadrant = -quadrant;
    AudioMathTrigQ31Extended reduced =
        audio_math_trig_q31_add((AudioMathTrigQ31Extended){input, 0},
                          audio_math_trig_q31_multiply((AudioMathTrigQ31Extended){-(double)quadrant, 0},
                                                 (AudioMathTrigQ31Extended){AUDIO_MATH_HALF_PI_F64,
                                                                       AUDIO_MATH_HALF_PI_LOW_F64}));
    AudioMathTrigQ31Extended square = audio_math_trig_q31_multiply(reduced, reduced);
    AudioMathTrigQ31Extended sv = audio_math_trig_q31_multiply(reduced, audio_math_trig_q31_sine_extended(square));
    AudioMathTrigQ31Extended cv = audio_math_trig_q31_cosine_extended(square);
    double s = sv.high, c = cv.high, sine, cosine;
    if (quadrant == 1 || quadrant == -3) {
        sine = c;
        cosine = -s;
    } else if (quadrant == -1 || quadrant == 3) {
        sine = -c;
        cosine = s;
    } else if (quadrant == 2 || quadrant == -2) {
        sine = -s;
        cosine = -c;
    } else {
        sine = s;
        cosine = c;
    }
    *imaginary = audio_math_truncate_sat_i32_f64(audio_math_floor_f64(sine * AUDIO_MATH_INT32_MAX_F64 + 0.5));
    *real = audio_math_truncate_sat_i32_f64(audio_math_floor_f64(cosine * AUDIO_MATH_INT32_MAX_F64 + 0.5));
}
/* Convert double phase in [-7,7] to sine/cosine integers scaled by INT32_MAX.
 * Use double polynomials normally and high/low recomputation near integer boundaries, never float.
 * Require nearest-even rounding, preserved subnormals, and no contraction. Finite FFT-formula samples are verified,
 * without claiming full-domain correct rounding or every CRT policy. Outputs may alias; cosine is retained last.
 * Return 1 for valid input; return 0 for nonfinite/out-of-range input without output writes. */
static inline int audio_math_sincos_bounded_f64_to_q31_soft(double input, int32_t *imaginary,
                                                      int32_t *real) {
    if ((audio_math_float64_bits(input) & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS) >
        audio_math_float64_bits(AUDIO_MATH_TRIG_MAX_PHASE_F64))
        return 0;
    double magnitude = input < 0.0 ? -input : input;
    int quadrant = magnitude < AUDIO_MATH_HALF_PI_F64 * 0.5   ? 0
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 1.5 ? 1
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 2.5 ? 2
                   : magnitude < AUDIO_MATH_HALF_PI_F64 * 3.5 ? 3
                                                             : 4;
    if (input < 0.0)
        quadrant = -quadrant;
    double reduced = (input - (double)quadrant * AUDIO_MATH_HALF_PI_F64) -
                     (double)quadrant * AUDIO_MATH_HALF_PI_LOW_F64;
    double square = reduced * reduced;
    double s = 2.8114572543455206e-15;
    s = -7.647163731819816e-13 + square * s;
    s = 1.6059043836821613e-10 + square * s;
    s = -2.505210838544172e-08 + square * s;
    s = 2.7557319223985893e-06 + square * s;
    s = -0.0001984126984126984 + square * s;
    s = 0.008333333333333333 + square * s;
    s = -0.16666666666666666 + square * s;
    s = (1.0 + square * s) * reduced;
    double c = 4.779477332387385e-14;
    c = -1.1470745597729725e-11 + square * c;
    c = 2.08767569878681e-09 + square * c;
    c = -2.755731922398589e-07 + square * c;
    c = 2.48015873015873e-05 + square * c;
    c = -0.001388888888888889 + square * c;
    c = 0.041666666666666664 + square * c;
    c = -0.5 + square * c;
    c = 1.0 + square * c;
    double sine, cosine;
    if (quadrant == 1 || quadrant == -3) {
        sine = c;
        cosine = -s;
    } else if (quadrant == -1 || quadrant == 3) {
        sine = -c;
        cosine = s;
    } else if (quadrant == 2 || quadrant == -2) {
        sine = -s;
        cosine = -c;
    } else {
        sine = s;
        cosine = c;
    }
    double scaled_sine = sine * AUDIO_MATH_INT32_MAX_F64 + 0.5;
    double scaled_cosine = cosine * AUDIO_MATH_INT32_MAX_F64 + 0.5;
    double floor_sine = audio_math_floor_f64(scaled_sine);
    double floor_cosine = audio_math_floor_f64(scaled_cosine);
    double fraction_sine = scaled_sine - floor_sine;
    double fraction_cosine = scaled_cosine - floor_cosine;
    if (fraction_sine < AUDIO_MATH_TRIG_Q31_INTEGER_GUARD_F64 ||
        fraction_sine > 1.0 - AUDIO_MATH_TRIG_Q31_INTEGER_GUARD_F64 ||
        fraction_cosine < AUDIO_MATH_TRIG_Q31_INTEGER_GUARD_F64 ||
        fraction_cosine > 1.0 - AUDIO_MATH_TRIG_Q31_INTEGER_GUARD_F64) {
        audio_math_trig_q31_refine(input, imaginary, real);
    } else {
        *imaginary = audio_math_truncate_sat_i32_f64(floor_sine);
        *real = audio_math_truncate_sat_i32_f64(floor_cosine);
    }
    return 1;
}
#endif
