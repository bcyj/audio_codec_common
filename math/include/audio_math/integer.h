#ifndef AUDIO_MATH_INTEGER_H
#define AUDIO_MATH_INTEGER_H
#include <audio_math/bits.h>
#include <audio_math/numeric_bits.h>

/* Unsigned quotient/remainder without a hardware divide or runtime helper.
 * The caller supplies a nonzero divisor; zero is handled deterministically.
 * Normalize by leading zeros so the loop visits only quotient bits. */
static inline uint32_t audio_math_divide_u32(uint32_t numerator, uint32_t divisor,
                                             uint32_t *remainder) {
    uint32_t quotient = 0;
    if (divisor && numerator >= divisor) {
        unsigned shift =
            audio_math_count_leading_zeros32(divisor) - audio_math_count_leading_zeros32(numerator);
        divisor <<= shift;
        do {
            quotient <<= 1;
            if (numerator >= divisor) {
                numerator -= divisor;
                quotient |= 1;
            }
            divisor >>= 1;
        } while (shift--);
    }
    if (remainder)
        *remainder = numerator;
    return quotient;
}

/* Return an all-one mask for negatives, otherwise zero, without negative right shifts. */
static inline uint32_t audio_math_sign_mask_i32(int32_t value) {
    return 0u - ((uint32_t)value >> 31);
}

/* Add modulo 2^32 and return the signed bit interpretation. */
static inline int32_t audio_math_add_wrap_i32(int32_t a, int32_t b) {
    return audio_math_int32_from_bits((uint32_t)a + (uint32_t)b);
}
/* Subtract modulo 2^32 without signed overflow or out-of-range conversion. */
static inline int32_t audio_math_subtract_wrap_i32(int32_t a, int32_t b) {
    return audio_math_int32_from_bits((uint32_t)a - (uint32_t)b);
}
/* Unfold Rice values to 0,-1,1,-2,... across the complete int32 bit range. */
static inline int32_t audio_math_rice_unfold_u32(uint32_t value) {
    return audio_math_int32_from_bits((value >> 1) ^ (0u - (value & 1u)));
}
/* Multiply modulo 2^32 without relying on signed overflow. */
static inline int32_t audio_math_multiply_wrap_i32(int32_t a, int32_t b) {
    return audio_math_int32_from_bits((uint32_t)a * (uint32_t)b);
}
/* Restrict a signed or unsigned 32-bit value to an inclusive interval. */
static inline int32_t audio_math_clamp_i32(int32_t value, int32_t minimum, int32_t maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
static inline uint32_t audio_math_clamp_u32(uint32_t value, uint32_t minimum, uint32_t maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
/* Convert wider fixed-point intermediates with signed saturation. */
static inline int32_t audio_math_saturate_i64_to_i32(int64_t value) {
    return value < INT32_MIN ? INT32_MIN : (value > INT32_MAX ? INT32_MAX : (int32_t)value);
}
static inline int32_t audio_math_saturate_i64_to_i24(int64_t value) {
    return value < INT32_C(-8388608)
               ? INT32_C(-8388608)
               : (value > INT32_C(8388607) ? INT32_C(8388607) : (int32_t)value);
}
static inline int32_t audio_math_add_saturate_i32(int32_t a, int32_t b) {
    return audio_math_saturate_i64_to_i32((int64_t)a + b);
}
static inline int32_t audio_math_subtract_saturate_i32(int32_t a, int32_t b) {
    return audio_math_saturate_i64_to_i32((int64_t)a - b);
}
/* Double with saturation instead of overflowing at the signed 64-bit rails. */
static inline int64_t audio_math_double_saturate_i64(int64_t value) {
    return value > INT64_MAX / 2
               ? INT64_MAX
               : (value < INT64_MIN / 2 ? INT64_MIN : value * INT64_C(2));
}
/* Return the low 64 product bits of two uint64 values without requiring the full product to fit.
 * Win32 kernel builds use 32-bit partial products to avoid CRT wide multiplication; others use
 * native multiplication. Cross terms retain only low 32 bits; a_high*b_high contributes nothing
 * modulo 2^64.
 */
static AUDIO_MATH_FORCE_INLINE uint64_t audio_math_multiply_wrap_u64(uint64_t a, uint64_t b) {
#if (defined(_KERNEL_MODE) || defined(AUDIO_MATH_NO_CRT)) && defined(_MSC_VER) && defined(_M_IX86)
    uint32_t a_low = (uint32_t)a;
    uint32_t b_low = (uint32_t)b;
    /* Volatile high words prevent MSVC x86 from recognizing the partial-product
     * expansion and folding it back into the forbidden __allmul helper. */
    volatile uint32_t a_high = (uint32_t)(a >> 32);
    volatile uint32_t b_high = (uint32_t)(b >> 32);
    uint64_t low = __emulu(a_low, b_low);
    uint32_t high = (uint32_t)(low >> 32) + a_high * b_low + a_low * b_high;
    return ((uint64_t)high << 32) | (uint32_t)low;
#else
    return a * b;
#endif
}
/* Implement arithmetic right shift with unsigned shifts only; oversized counts return sign bits. */
static inline int32_t audio_math_shift_right_portable_i32(int32_t value, unsigned count) {
    if (count >= 32)
        return value < 0 ? -1 : 0;
    if (!count)
        return value;
    uint32_t bits = (uint32_t)value;
    return audio_math_int32_from_bits((bits >> count) |
                                      (audio_math_sign_mask_i32(value) << (32 - count)));
}

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
/* These compilers sign-extend negative right shifts; assert target behavior to prevent silent
 * semantic changes. */
#if defined(__cplusplus)
static_assert((INT32_MIN >> 1) == -1073741824 && (-1 >> 31) == -1 &&
                  (INT64_MIN >> 1) == INT64_MIN / 2 && (INT64_C(-1) >> 63) == -1,
              "Native signed right shift must extend the sign bit");
#else
_Static_assert((INT32_MIN >> 1) == -1073741824 && (-1 >> 31) == -1 &&
                   (INT64_MIN >> 1) == INT64_MIN / 2 && (INT64_C(-1) >> 63) == -1,
               "Native signed right shift must extend the sign bit");
#endif
#endif

/* Arithmetic right shift for proven counts 0..31, after caller checks outside loops. */
static inline int32_t audio_math_shift_right_bounded_i32(int32_t value, unsigned count) {
#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
    return value >> count;
#else
    return audio_math_shift_right_portable_i32(value, count);
#endif
}

/* Arithmetic 64-bit right shift for guaranteed counts 0..63.
 * Supported compilers retain checked native semantics; Win32 kernel/other compilers use explicit
 * two's-complement fallback.
 */
static AUDIO_MATH_FORCE_INLINE int64_t audio_math_shift_right_bounded_i64(int64_t value,
                                                                          unsigned count) {
#if (defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)) &&                              \
    !((defined(_KERNEL_MODE) || defined(AUDIO_MATH_NO_CRT)) && defined(_MSC_VER) &&                \
      defined(_M_IX86))
    return value >> count;
#elif (defined(_KERNEL_MODE) || defined(AUDIO_MATH_NO_CRT)) && defined(_MSC_VER) && defined(_M_IX86)
    if (!count)
        return value;
    uint64_t value_bits = (uint64_t)value;
    int32_t high = audio_math_int32_from_bits((uint32_t)(value_bits >> 32));
    if (count >= 32)
        return (int64_t)audio_math_shift_right_bounded_i32(high, count - 32);
    uint32_t low = ((uint32_t)value_bits >> count) | ((uint32_t)high << (32 - count));
    uint32_t shifted_high = (uint32_t)audio_math_shift_right_bounded_i32(high, count);
    uint64_t result_bits = ((uint64_t)shifted_high << 32) | low;
    int64_t result;
    memcpy(&result, &result_bits, sizeof result);
    return result;
#else
    uint64_t bits = audio_math_shift_arithmetic_sign64((uint64_t)value, count);
    int64_t result;
    memcpy(&result, &bits, sizeof result);
    return result;
#endif
}

/* Arithmetic right shift for arbitrary counts; oversized counts act as shift 31, retaining only
 * sign. Supported compilers use assertion-checked native operations; others use unsigned fallback.
 */
static inline int32_t audio_math_shift_right_i32(int32_t value, unsigned count) {
    return audio_math_shift_right_bounded_i32(value, count < 32 ? count : 31);
}
/* Sign-extend the low width bits. The caller supplies a width in 1..32. */
static inline int32_t audio_math_sign_extend_i32(uint32_t value, unsigned width) {
    if (width == 32)
        return audio_math_int32_from_bits(value);
    unsigned shift = 32u - width;
    return audio_math_shift_right_bounded_i32(audio_math_int32_from_bits(value << shift), shift);
}
/* Divide by 2^count with round-to-nearest, ties-to-even. Counts are 1..64 and 1..32. */
static inline int64_t audio_math_round_shift_even_i64(int64_t value, unsigned count) {
    uint64_t value_bits = (uint64_t)value;
    uint64_t quotient_bits = audio_math_shift_arithmetic_sign64(value_bits, count);
    int64_t quotient = audio_math_int64_from_bits(quotient_bits);
    uint64_t half = audio_math_shift_left_bounded_u64(UINT64_C(1), count - 1);
    uint64_t mask = count == 64 ? UINT64_MAX
                                : audio_math_shift_left_bounded_u64(UINT64_C(1), count) - 1;
    uint64_t remainder = value_bits & mask;
    return quotient + (remainder > half || (remainder == half && (quotient_bits & 1)));
}
static inline int32_t audio_math_round_shift_even_i32(int32_t value, unsigned count) {
    uint32_t value_bits = (uint32_t)value;
    int32_t quotient =
        count == 32 ? (value < 0 ? -1 : 0) : audio_math_shift_right_bounded_i32(value, count);
    uint32_t half = (uint32_t)audio_math_shift_left_bounded_u64(UINT64_C(1), count - 1);
    uint32_t mask = count == 32 ? UINT32_MAX : (UINT32_C(1) << count) - 1;
    uint32_t remainder = value_bits & mask;
    return quotient + (remainder > half || (remainder == half && ((uint32_t)quotient & 1)));
}
/* Extract the shifted low word of a modulo-2^64 product; count is modulo 64. */
static inline int32_t audio_math_multiply_shift_wrap_i32(int64_t a, int32_t b, unsigned count) {
    uint64_t product = audio_math_multiply_wrap_u64((uint64_t)a, (uint64_t)(int64_t)b);
    return audio_math_int32_from_bits(
        (uint32_t)audio_math_shift_arithmetic_sign64(product, count & 63u));
}
/* Extract Q31 middle bits of a modulo-2^64 multiply-add, preserving truncation/modulo-2^32
 * behavior. */
static inline int32_t audio_math_q31_truncate_wrap_i64(uint64_t product_bits) {
    return audio_math_int32_from_bits((uint32_t)(product_bits >> 31));
}
/* Truncate a two-word arithmetic shift to low 32 bits, retaining per-word low-eight-bit count
 * conventions. Counts 0..63 match 64-bit arithmetic shifts; other counts also have defined results.
 */
static inline uint32_t audio_math_shift_pair_right_low8_u32(uint64_t bits, uint32_t shift) {
    uint32_t lo = (uint32_t)bits, hi = (uint32_t)(bits >> 32);
    uint32_t value = audio_math_shift_right_low8_zero32(lo, shift) |
                     audio_math_shift_left_low8_zero32(hi, 32u - shift);
    if (audio_math_int32_from_bits(shift - 32u) >= 0)
        value |= audio_math_shift_arithmetic_low8_sign32(hi, shift - 32u);
    return value;
}
#endif
