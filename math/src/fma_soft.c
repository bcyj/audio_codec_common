#include <audio_math/fma_native.h>
#include <audio_math/numeric_bits.h>
#include <audio_math/bits.h>
#include <audio_math/types/fma.h>
#include <math.h>

/* Right-shift and merge discarded nonzero bits into a sticky bit. */
static inline uint64_t audio_math_fma_shift_jam(uint64_t value, unsigned shift) {
    if (!shift)
        return value;
    if (shift >= 64)
        return value != 0;
    return audio_math_shift_right_bounded_u64(value, shift) |
           (audio_math_shift_left_bounded_u64(value, 64 - shift) != 0);
}

/* Round an unsigned significand nearest-even, including tiny subnormal results. */
static inline uint64_t audio_math_fma_round_even(uint64_t value, unsigned shift) {
    if (!shift)
        return value;
    if (shift > 64)
        return 0;
    if (shift == 64)
        return value > UINT64_C(0x8000000000000000);
    uint64_t result = audio_math_shift_right_bounded_u64(value, shift);
    uint64_t half = audio_math_shift_left_bounded_u64(UINT64_C(1), shift - 1);
    uint64_t tail = value & (audio_math_shift_left_bounded_u64(UINT64_C(1), shift) - 1);
    return result + (tail > half || (tail == half && (result & 1)));
}

/* Implement binary32 single rounding with an exact 48-bit product and a 64-bit guard/sticky significand.
 * Shared from the validated decoder software implementation; no allocation, recursion, lookup, or mutable globals.
 */
float audio_math_fma_exact_f32(float a, float b, float c) {
    uint32_t a_bits = audio_math_float32_bits(a);
    uint32_t b_bits = audio_math_float32_bits(b);
    uint32_t c_bits = audio_math_float32_bits(c);
    unsigned a_field = (a_bits >> 23) & 255;
    unsigned b_field = (b_bits >> 23) & 255;
    unsigned c_field = (c_bits >> 23) & 255;
    if (a_field == 255 || b_field == 255 || c_field == 255)
        return a * b + c;

    unsigned product_sign = (a_bits ^ b_bits) & UINT32_C(0x80000000);
    unsigned addend_sign = c_bits & UINT32_C(0x80000000);
    uint32_t a_mantissa = a_bits & UINT32_C(0x7fffff);
    uint32_t b_mantissa = b_bits & UINT32_C(0x7fffff);
    uint32_t c_mantissa = c_bits & UINT32_C(0x7fffff);
    if ((!a_field && !a_mantissa) || (!b_field && !b_mantissa)) {
        if (!c_field && !c_mantissa)
            return audio_math_float32_from_bits(product_sign & addend_sign);
        return c;
    }

    /* Restore the implicit bit; normalize subnormals first while retaining their true binary exponent. */
    int a_exponent = (int)a_field - 127;
    int b_exponent = (int)b_field - 127;
    int c_exponent = (int)c_field - 127;
    if (a_field)
        a_mantissa |= UINT32_C(0x800000);
    else {
        unsigned shift = audio_math_count_leading_zeros32(a_mantissa) - 8u;
        a_mantissa <<= shift;
        a_exponent = -126 - (int)shift;
    }
    if (b_field)
        b_mantissa |= UINT32_C(0x800000);
    else {
        unsigned shift = audio_math_count_leading_zeros32(b_mantissa) - 8u;
        b_mantissa <<= shift;
        b_exponent = -126 - (int)shift;
    }

    uint64_t product = (uint64_t)a_mantissa * b_mantissa;
    unsigned leading_bit = product >> 47 ? 47 : 46;
    int product_exponent = a_exponent + b_exponent - 46 + (int)leading_bit;
    int exponent = product_exponent;
    uint64_t product_aligned = audio_math_shift_left_bounded_u64(product, 62 - leading_bit);
    uint64_t addend_aligned = 0;
    if (c_field || c_mantissa) {
        if (c_field)
            c_mantissa |= UINT32_C(0x800000);
        else {
            unsigned shift = audio_math_count_leading_zeros32(c_mantissa) - 8u;
            c_mantissa <<= shift;
            c_exponent = -126 - (int)shift;
        }
        addend_aligned = (uint64_t)c_mantissa << 39;
        if (c_exponent > exponent)
            exponent = c_exponent;
        addend_aligned = audio_math_fma_shift_jam(addend_aligned, (unsigned)(exponent - c_exponent));
    }
    product_aligned = audio_math_fma_shift_jam(product_aligned, (unsigned)(exponent - product_exponent));

    /* Add same-sign values and subtract opposite signs by magnitude; exact cancellation produces positive zero. */
    uint64_t magnitude;
    unsigned sign = product_sign;
    if (product_sign == addend_sign)
        magnitude = product_aligned + addend_aligned;
    else if (product_aligned >= addend_aligned)
        magnitude = product_aligned - addend_aligned;
    else {
        magnitude = addend_aligned - product_aligned;
        sign = addend_sign;
    }
    if (!magnitude)
        return audio_math_float32_from_bits(0);
    leading_bit = 63u - audio_math_count_leading_zeros64(magnitude);
    int result_exponent = exponent - 62 + (int)leading_bit;
    if (result_exponent > 127)
        return audio_math_float32_from_bits(sign | UINT32_C(0x7f800000));

    /* Pack normal/subnormal results separately, rounding the final significand once. */
    uint64_t rounded;
    if (result_exponent >= -126) {
        rounded = leading_bit > 23 ? audio_math_fma_round_even(magnitude, leading_bit - 23)
                                   : audio_math_shift_left_bounded_u64(magnitude, 23 - leading_bit);
        if (rounded >= UINT64_C(0x1000000)) {
            rounded >>= 1;
            ++result_exponent;
        }
        if (result_exponent > 127)
            return audio_math_float32_from_bits(sign | UINT32_C(0x7f800000));
        return audio_math_float32_from_bits(sign | ((unsigned)(result_exponent + 127) << 23) |
                                      ((uint32_t)rounded & UINT32_C(0x7fffff)));
    }
    int subnormal_shift = -(exponent + 87);
    rounded = subnormal_shift > 0 ? audio_math_fma_round_even(magnitude, (unsigned)subnormal_shift)
                                  : audio_math_shift_left_bounded_u64(magnitude, (unsigned)-subnormal_shift);
    return audio_math_float32_from_bits(sign | (uint32_t)rounded);
}

/* Compare two-part unsigned significands without floating conversion. */
static inline int wide_compare(AudioMathFmaWide a, AudioMathFmaWide b) {
    if (a.high != b.high)
        return a.high > b.high ? 1 : -1;
    return a.low > b.low ? 1 : a.low < b.low ? -1 : 0;
}

/* Left-shift a 128-bit significand without word-width/oversized C shifts. */
static inline AudioMathFmaWide wide_left(AudioMathFmaWide value, unsigned shift) {
    if (!shift)
        return value;
    if (shift >= 128)
        return (AudioMathFmaWide){0, 0};
    if (shift >= 64)
        return (AudioMathFmaWide){audio_math_shift_left_bounded_u64(value.low, shift - 64), 0};
    return (AudioMathFmaWide){audio_math_shift_left_bounded_u64(value.high, shift) |
                            audio_math_shift_right_bounded_u64(value.low, 64 - shift),
                        audio_math_shift_left_bounded_u64(value.low, shift)};
}

/* Right-shift a 128-bit significand without rounding yet. */
static inline AudioMathFmaWide wide_right(AudioMathFmaWide value, unsigned shift) {
    if (!shift)
        return value;
    if (shift >= 128)
        return (AudioMathFmaWide){0, 0};
    if (shift >= 64)
        return (AudioMathFmaWide){0, audio_math_shift_right_bounded_u64(value.high, shift - 64)};
    return (AudioMathFmaWide){audio_math_shift_right_bounded_u64(value.high, shift),
                        audio_math_shift_right_bounded_u64(value.low, shift) |
                            audio_math_shift_left_bounded_u64(value.high, 64 - shift)};
}

/* Test whether low count bits are nonzero for alignment/rounding sticky bits. */
static inline int wide_low_nonzero(AudioMathFmaWide value, unsigned count) {
    if (!count)
        return 0;
    if (count >= 128)
        return value.high != 0 || value.low != 0;
    if (count >= 64) {
        unsigned high_count = count - 64;
        return value.low != 0 ||
               (high_count && audio_math_shift_left_bounded_u64(value.high, 64 - high_count) != 0);
    }
    return audio_math_shift_left_bounded_u64(value.low, 64 - count) != 0;
}

/* Right-shift and merge all discarded nonzero bits into the least-significant bit. */
static inline AudioMathFmaWide wide_right_jam(AudioMathFmaWide value, unsigned shift) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* One split computes quotient/sticky together; double-word shift intrinsics are used only for 1..63. */
    if (!shift)
        return value;
    if (shift < 64) {
        uint64_t discarded = value.low << (64 - shift);
        return (AudioMathFmaWide){value.high >> shift,
                            __shiftright128(value.low, value.high, (unsigned char)shift) |
                                (discarded != 0)};
    }
    if (shift < 128) {
        unsigned high_shift = shift - 64;
        uint64_t discarded = value.low != 0 ||
                             (high_shift && (value.high << (64 - high_shift)) != 0);
        return (AudioMathFmaWide){0, (value.high >> high_shift) | discarded};
    }
    return (AudioMathFmaWide){0, value.high != 0 || value.low != 0};
#else
    AudioMathFmaWide result = wide_right(value, shift);
    result.low |= wide_low_nonzero(value, shift);
    return result;
#endif
}

/* Add significands and propagate low-half carry; aligned sums are guaranteed to fit 128 bits. */
static inline AudioMathFmaWide wide_add(AudioMathFmaWide a, AudioMathFmaWide b) {
    uint64_t low = a.low + b.low;
    return (AudioMathFmaWide){a.high + b.high + (low < a.low), low};
}

/* Subtract the smaller significand from the larger and propagate borrow. */
static inline AudioMathFmaWide wide_subtract(AudioMathFmaWide a, AudioMathFmaWide b) {
    return (AudioMathFmaWide){a.high - b.high - (a.low < b.low), a.low - b.low};
}

/* Compute the exact 106-bit product of two 53-bit significands. */
static inline AudioMathFmaWide wide_product53(uint64_t a, uint64_t b) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Full 106-bit product of two 53-bit significands; baseline x64 MUL needs no FMA3/BMI2. */
    AudioMathFmaWide product;
    product.low = _umul128(a, b, &product.high);
    return product;
#elif defined(_KERNEL_MODE) && defined(_MSC_VER) && defined(_M_IX86)
    /* Inputs have only 53 significant bits; explicit 32x32-to-64 products map directly to x86 MUL. */
    uint32_t a_low = (uint32_t)a, a_high = (uint32_t)(a >> 32);
    uint32_t b_low = (uint32_t)b, b_high = (uint32_t)(b >> 32);
    uint64_t low = (uint64_t)a_low * b_low;
    uint64_t middle = (low >> 32) + (uint64_t)a_high * b_low + (uint64_t)a_low * b_high;
    return (AudioMathFmaWide){(uint64_t)a_high * b_high + (middle >> 32),
                        (middle << 32) | (uint32_t)low};
#else
    uint64_t low = (uint64_t)(uint32_t)a * (uint32_t)b;
    uint64_t middle = (low >> 32) + (a >> 32) * (uint32_t)b + (uint32_t)a * (b >> 32);
    return (AudioMathFmaWide){(a >> 32) * (b >> 32) + (middle >> 32), (middle << 32) | (uint32_t)low};
#endif
}

/* Locate the highest bit of a nonzero double-word significand using common CLZ. */
static inline unsigned wide_top(AudioMathFmaWide value) {
    return value.high ? 127u - audio_math_count_leading_zeros64(value.high)
                      : 63u - audio_math_count_leading_zeros64(value.low);
}

/* Round finally to at most 53 bits, selecting an even significand on ties. */
static inline uint64_t wide_round_even(AudioMathFmaWide value, unsigned shift) {
#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) && defined(_M_X64)
    /* Align discarded high bits to half_word while retaining lower sticky state. */
    const uint64_t half_word = UINT64_C(0x8000000000000000);
    if (!shift)
        return value.low;
    if (shift > 128)
        return 0;
    if (shift < 64) {
        uint64_t result = __shiftright128(value.low, value.high, (unsigned char)shift);
        uint64_t tail = value.low << (64 - shift);
        return result + (tail > half_word || (tail == half_word && (result & 1)));
    }
    if (shift == 64)
        return value.high +
               (value.low > half_word || (value.low == half_word && (value.high & 1)));
    if (shift < 128) {
        unsigned high_shift = shift - 64;
        uint64_t result = value.high >> high_shift;
        uint64_t tail = value.high << (64 - high_shift);
        return result + (tail > half_word ||
                         (tail == half_word && (value.low != 0 || (result & 1))));
    }
    /* shift==128: quotient is zero and an exact midpoint rounds to even zero. */
    return value.high > half_word || (value.high == half_word && value.low != 0);
#else
    if (!shift)
        return value.low;
    if (shift > 128)
        return 0;
    uint64_t result = wide_right(value, shift).low;
    unsigned half_index = shift - 1;
    unsigned half =
        (unsigned)(half_index >= 64
                       ? audio_math_shift_right_bounded_u64(value.high, half_index - 64) & 1
                       : audio_math_shift_right_bounded_u64(value.low, half_index) & 1);
    return result + (half && (wide_low_nonzero(value, half_index) || (result & 1)));
#endif
}

/* Implement binary64 FMA with exact integer products, wide alignment, and one rounding.
 * Share f32 rules to avoid CRT performance/rounding issues; no allocation, recursion, or mutable globals.
 */
double audio_math_fma_soft_f64(double a, double b, double c) {
    uint64_t a_bits = audio_math_fma_f64_bits(a), b_bits = audio_math_fma_f64_bits(b),
             c_bits = audio_math_fma_f64_bits(c);
    unsigned a_field = (unsigned)(a_bits >> 52) & 2047;
    unsigned b_field = (unsigned)(b_bits >> 52) & 2047;
    unsigned c_field = (unsigned)(c_bits >> 52) & 2047;
    if (a_field == 2047 || b_field == 2047 || c_field == 2047)
        return a * b + c;
    const uint64_t hidden = UINT64_C(0x0010000000000000);
    const uint64_t sign_mask = UINT64_C(0x8000000000000000);
    const uint64_t infinity = UINT64_C(0x7ff0000000000000);
    uint64_t product_sign = (a_bits ^ b_bits) & sign_mask, addend_sign = c_bits & sign_mask;
    uint64_t a_mantissa = a_bits & (hidden - 1), b_mantissa = b_bits & (hidden - 1),
             c_mantissa = c_bits & (hidden - 1);
    if ((!a_field && !a_mantissa) || (!b_field && !b_mantissa)) {
        if (!c_field && !c_mantissa)
            return audio_math_fma_f64_from_bits(product_sign & addend_sign);
        return c;
    }
    int a_exponent = (int)a_field - 1023, b_exponent = (int)b_field - 1023,
        c_exponent = (int)c_field - 1023;
    if (a_field)
        a_mantissa |= hidden;
    else {
        unsigned shift = audio_math_count_leading_zeros64(a_mantissa) - 11u;
        a_mantissa = audio_math_shift_left_bounded_u64(a_mantissa, shift);
        a_exponent = -1022 - (int)shift;
    }
    if (b_field)
        b_mantissa |= hidden;
    else {
        unsigned shift = audio_math_count_leading_zeros64(b_mantissa) - 11u;
        b_mantissa = audio_math_shift_left_bounded_u64(b_mantissa, shift);
        b_exponent = -1022 - (int)shift;
    }

    AudioMathFmaWide product = wide_product53(a_mantissa, b_mantissa);
    unsigned top = (product.high >> 41) ? 105 : 104;
    int product_exponent = a_exponent + b_exponent - 104 + (int)top, exponent = product_exponent;
    AudioMathFmaWide p = wide_left(product, 126 - top), q = {0, 0};
    if (c_field || c_mantissa) {
        if (c_field)
            c_mantissa |= hidden;
        else {
            unsigned shift = audio_math_count_leading_zeros64(c_mantissa) - 11u;
            c_mantissa = audio_math_shift_left_bounded_u64(c_mantissa, shift);
            c_exponent = -1022 - (int)shift;
        }
        q = wide_left((AudioMathFmaWide){0, c_mantissa}, 74);
        if (c_exponent > exponent)
            exponent = c_exponent;
        q = wide_right_jam(q, (unsigned)(exponent - c_exponent));
    }
    p = wide_right_jam(p, (unsigned)(exponent - product_exponent));
    AudioMathFmaWide magnitude;
    uint64_t sign = product_sign;
    if (product_sign == addend_sign)
        magnitude = wide_add(p, q);
    else if (wide_compare(p, q) >= 0)
        magnitude = wide_subtract(p, q);
    else {
        magnitude = wide_subtract(q, p);
        sign = addend_sign;
    }
    if (!magnitude.high && !magnitude.low)
        return audio_math_fma_f64_from_bits(0);
    top = wide_top(magnitude);
    int result_exponent = exponent - 126 + (int)top;
    if (result_exponent > 1023)
        return audio_math_fma_f64_from_bits(sign | infinity);
    uint64_t rounded;
    if (result_exponent >= -1022) {
        rounded = top > 52 ? wide_round_even(magnitude, top - 52)
                          : audio_math_shift_left_bounded_u64(magnitude.low, 52 - top);
        if (rounded >= (hidden << 1)) {
            rounded >>= 1;
            ++result_exponent;
        }
        if (result_exponent > 1023)
            return audio_math_fma_f64_from_bits(sign | infinity);
        return audio_math_fma_f64_from_bits(sign | ((uint64_t)(result_exponent + 1023) << 52) |
                                      (rounded & (hidden - 1)));
    }
    int shift = -(exponent + 948);
    rounded =
        shift > 0 ? wide_round_even(magnitude, (unsigned)shift)
                  : audio_math_shift_left_bounded_u64(magnitude.low, (unsigned)-shift);
    return audio_math_fma_f64_from_bits(sign | rounded);
}
