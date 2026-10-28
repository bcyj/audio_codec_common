#include <audio_math/float_sine.h>

#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) &&                           \
    (defined(_M_IX86) || defined(_M_X64))
#include <audio_math/float_convert.h>
#include <audio_math/float_trig_q31.h>

/* floor((2/pi)*2^256), least-significant word first; read-only with one translation-unit definition. */
static const uint32_t reciprocal_pi[8] = {0xdebbc561u, 0xfe5163abu, 0x3c439041u, 0xdb629599u,
                                          0xf534ddc0u, 0xfc2757d1u, 0x4e441529u, 0xa2f9836eu};

/* Extract 32 consecutive product bits beginning at bit. */
static uint32_t product_word(const uint32_t *product, unsigned bit) {
    unsigned word = bit / 32;
    unsigned shift = bit % 32;
    uint32_t value = product[word] >> shift;
    if (shift && word + 1 < 10)
        value |= product[word + 1] << (32 - shift);
    return value;
}

/* Reduce 26 < |phase| <= 2^134 to the nearest pi/2 quadrant, retaining 96 fractional bits. */
static AudioMathTrigQ31Extended reduce_large(uint64_t magnitude, unsigned *quadrant) {
    uint32_t mantissa[2] = {(uint32_t)magnitude,
                            ((uint32_t)(magnitude >> 32) & 0xfffffu) | 0x100000u};
    uint32_t product[10] = {0};
    for (unsigned i = 0; i < 2; ++i) {
        uint64_t carry = 0;
        for (unsigned j = 0; j < 8; ++j) {
            uint64_t value = (uint64_t)mantissa[i] * reciprocal_pi[j] + product[i + j] + carry;
            product[i + j] = (uint32_t)value;
            carry = value >> 32;
        }
        product[i + 8] = (uint32_t)carry;
    }
    unsigned exponent = (unsigned)(magnitude >> 52) - 1023u;
    unsigned point = 308u - exponent;
    unsigned index = product_word(product, point) & 3u;
    uint32_t a = product_word(product, point - 32);
    uint32_t b = product_word(product, point - 64);
    uint32_t c = product_word(product, point - 96);
    double high = (double)a * 0x1p-32;
    if (a & 0x80000000u) {
        high -= 1.0;
        ++index;
    }
    AudioMathTrigQ31Extended fraction = audio_math_trig_q31_add((AudioMathTrigQ31Extended){high, 0},
                                                     (AudioMathTrigQ31Extended){(double)b * 0x1p-64, 0});
    fraction = audio_math_trig_q31_add(fraction, (AudioMathTrigQ31Extended){(double)c * 0x1p-96, 0});
    *quadrant = index & 3u;
    return audio_math_trig_q31_multiply(
        fraction, (AudioMathTrigQ31Extended){AUDIO_MATH_HALF_PI_F64, AUDIO_MATH_HALF_PI_LOW_F64});
}

/* Evaluate sine/cosine on [-pi/4,pi/4] in double Horner order. */
static double sine_in_quadrant(double reduced, unsigned quadrant) {
    double square = reduced * reduced;
    double result;
    if (quadrant & 1u) {
        double p = 4.779477332387385e-14;
        p = -1.1470745597729725e-11 + square * p;
        p = 2.08767569878681e-09 + square * p;
        p = -2.755731922398589e-07 + square * p;
        p = 2.48015873015873e-05 + square * p;
        p = -0.001388888888888889 + square * p;
        p = 0.041666666666666664 + square * p;
        p = -0.5 + square * p;
        result = 1.0 + square * p;
    } else {
        double p = 2.8114572543455206e-15;
        p = -7.647163731819816e-13 + square * p;
        p = 1.6059043836821613e-10 + square * p;
        p = -2.505210838544172e-08 + square * p;
        p = 2.7557319223985893e-06 + square * p;
        p = -0.0001984126984126984 + square * p;
        p = 0.008333333333333333 + square * p;
        p = -0.16666666666666666 + square * p;
        result = (1.0 + square * p) * reduced;
    }
    return quadrant & 2u ? -result : result;
}

/* The small-phase reduction high part has 33 significant bits and multiplies |quadrant| <= 17 exactly. */
static double sine_small(double input) {
    double magnitude = input < 0.0 ? -input : input;
    if (magnitude < 1e-9)
        return input;
    int quadrant = audio_math_truncate_i32_f64(magnitude / AUDIO_MATH_HALF_PI_F64 + 0.5);
    if (input < 0.0)
        quadrant = -quadrant;
    double reduced = (input - (double)quadrant * 0x1.921fb54400000p+0) -
                     (double)quadrant * 6.077100506506192e-11;
    return sine_in_quadrant(reduced, (unsigned)quadrant);
}

/* Software double sine specialized for V4 sinc, not a full binary64-domain libm replacement. */
double audio_math_sine_wide_f64_soft(double input) {
    uint64_t bits = audio_math_float64_bits(input);
    uint64_t magnitude = bits & AUDIO_MATH_FLOAT64_MAGNITUDE_BITS;
    if (magnitude <= audio_math_float64_bits(26.0))
        return sine_small(input);
    if (magnitude >= AUDIO_MATH_FLOAT64_INFINITY_BITS)
        return input - input;
    if (magnitude > audio_math_float64_bits(0x1p134))
        return audio_math_float64_from_bits(UINT64_C(0x7ff8000000000000));
    unsigned quadrant;
    AudioMathTrigQ31Extended reduced = reduce_large(magnitude, &quadrant);
    double result = sine_in_quadrant(reduced.high + reduced.low, quadrant);
    return bits & AUDIO_MATH_FLOAT64_SIGN_BITS ? -result : result;
}
#endif
