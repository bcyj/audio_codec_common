#ifndef AUDIO_MATH_BITS_H
#define AUDIO_MATH_BITS_H
#include <audio_math/compiler.h>
#include <stdint.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* Swap an integer's four bytes without pretending MSVC provides GCC builtins. */
static inline uint32_t audio_math_byte_swap32(uint32_t value) {
#if defined(_MSC_VER)
    return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(value);
#else
    return (value >> 24) | ((value >> 8) & UINT32_C(0xff00)) | ((value << 8) & UINT32_C(0xff0000)) |
           (value << 24);
#endif
}
/* Count leading zeros in 32 bits; zero explicitly returns 32. */
static AUDIO_MATH_FORCE_INLINE unsigned audio_math_count_leading_zeros32(uint32_t value) {
#if defined(_MSC_VER)
    unsigned long bit;
    return _BitScanReverse(&bit, value) ? 31u - bit : 32u;
#elif defined(__GNUC__) || defined(__clang__)
    return value ? (unsigned)__builtin_clz(value) : 32u;
#else
    unsigned count = 0;
    if (!value)
        return 32;
    while (!(value & UINT32_C(0x80000000))) {
        ++count;
        value <<= 1;
    }
    return count;
#endif
}
/* Count leading zeros in 64 bits; zero returns 64, with a two-word option on 32-bit platforms. */
static AUDIO_MATH_FORCE_INLINE unsigned audio_math_count_leading_zeros64(uint64_t value) {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    unsigned long bit;
    return _BitScanReverse64(&bit, value) ? 63u - bit : 64u;
#elif defined(__GNUC__) || defined(__clang__)
    return value ? (unsigned)__builtin_clzll(value) : 64u;
#else
    uint32_t high = (uint32_t)(value >> 32);
    return high ? audio_math_count_leading_zeros32(high)
                : 32u + audio_math_count_leading_zeros32((uint32_t)value);
#endif
}
/* Logical left shift with a guaranteed count below 64.
 * MSVC Win32 kernel builds use two 32-bit words to avoid CRT helpers for variable 64-bit shifts.
 * Other targets use native expressions; callers guarantee the count, including valid zero.
 */
static AUDIO_MATH_FORCE_INLINE uint64_t audio_math_shift_left_bounded_u64(uint64_t value,
                                                                          unsigned count) {
#if (defined(_KERNEL_MODE) || defined(AUDIO_MATH_NO_CRT)) && defined(_MSC_VER) && defined(_M_IX86)
    uint32_t low = (uint32_t)value, high = (uint32_t)(value >> 32);
    if (!count)
        return value;
    if (count >= 32)
        return (uint64_t)(low << (count - 32)) << 32;
    return ((uint64_t)((high << count) | (low >> (32 - count))) << 32) | (low << count);
#else
    return value << count;
#endif
}
/* Logical right shift with count below 64, using native or CRT-free two-word implementations. */
static AUDIO_MATH_FORCE_INLINE uint64_t audio_math_shift_right_bounded_u64(uint64_t value,
                                                                           unsigned count) {
#if (defined(_KERNEL_MODE) || defined(AUDIO_MATH_NO_CRT)) && defined(_MSC_VER) && defined(_M_IX86)
    uint32_t low = (uint32_t)value, high = (uint32_t)(value >> 32);
    if (!count)
        return value;
    if (count >= 32)
        return high >> (count - 32);
    return ((uint64_t)(high >> count) << 32) | ((low >> count) | (high << (32 - count)));
#else
    return value >> count;
#endif
}
/* Left shift by the low eight count bits; oversized shifts return zero. */
static inline uint32_t audio_math_shift_left_low8_zero32(uint32_t value, unsigned shift) {
    shift &= 255;
    return shift < 32 ? value << shift : 0;
}
/* Logical right shift by low eight count bits; oversized shifts return zero. */
static inline uint32_t audio_math_shift_right_low8_zero32(uint32_t value, unsigned shift) {
    shift &= 255;
    return shift < 32 ? value >> shift : 0;
}
/* Arithmetic right shift by low eight count bits; oversized shifts retain only the sign. */
static inline uint32_t audio_math_shift_arithmetic_low8_sign32(uint32_t value, unsigned shift) {
    shift &= 255;
    if (!shift)
        return value;
    if (shift >= 32)
        return value >> 31 ? UINT32_MAX : 0;
    return (value >> shift) | (value >> 31 ? UINT32_MAX << (32 - shift) : 0);
}
/* Left shift by low five count bits, explicitly defining modulo-32 counts. */
static inline uint32_t audio_math_shift_left_mod32(uint32_t value, unsigned shift) {
    return value << (shift & 31u);
}
/* Logical right shift by low five count bits. */
static inline uint32_t audio_math_shift_right_mod32(uint32_t value, unsigned shift) {
    return value >> (shift & 31u);
}
/* Arithmetic right shift of 64-bit two's-complement bits; oversized counts return all sign bits. */
static inline uint64_t audio_math_shift_arithmetic_sign64(uint64_t bits, unsigned shift) {
    if (!shift)
        return bits;
    if (shift >= 64u)
        return bits >> 63 ? UINT64_MAX : 0;
    return audio_math_shift_right_bounded_u64(bits, shift) |
           (bits >> 63 ? audio_math_shift_left_bounded_u64(UINT64_MAX, 64u - shift) : 0);
}
/* Compute integer ceil(log2) for positive values, zero for zero, without floating math. */
static inline unsigned audio_math_ceil_log2_u32(uint32_t value) {
    return value > 1u ? 32u - audio_math_count_leading_zeros32(value - 1u) : 0u;
}

/* Return floor(sqrt(uint32_t)), including zero and UINT32_MAX.
 * Extract binary digit pairs in at most sixteen steps without floating point, division, recursion,
 * or scratch.
 */
static inline uint32_t audio_math_sqrt_floor_u32(uint32_t value) {
    if (value < 2)
        return value;
    unsigned top_pair = (31u - audio_math_count_leading_zeros32(value)) & ~1u;
    uint32_t place = UINT32_C(1) << top_pair;
    uint32_t root = 0;
    while (place) {
        uint32_t trial = root + place;
        if (value >= trial) {
            value -= trial;
            root = (root >> 1) + place;
        } else {
            root >>= 1;
        }
        place >>= 2;
    }
    return root;
}
#endif
