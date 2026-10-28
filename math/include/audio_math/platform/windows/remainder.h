#ifndef AUDIO_MATH_WINDOWS_REMAINDER_H
#define AUDIO_MATH_WINDOWS_REMAINDER_H
#include <stdint.h>

/* Preserve exceptional Windows binary32 remainder bits without user-mode CRT calls.
 * x86 retains x87 larger-payload NaN selection and positive-sign preference for ties; x64 prioritizes infinite dividends.
 * Callers have established a NaN operand, infinite dividend, or zero divisor.
 */
static inline uint32_t audio_math_windows_remainder_special_bits(uint32_t numerator, uint32_t divisor) {
    uint32_t x = numerator & UINT32_C(0x7fffffff);
    uint32_t y = divisor & UINT32_C(0x7fffffff);
    const uint32_t infinity = UINT32_C(0x7f800000);
    const uint32_t quiet = UINT32_C(0x00400000);
#if defined(_M_IX86) || defined(__i386__)
    if (x > infinity && y > infinity) {
        numerator |= quiet;
        divisor |= quiet;
        x |= quiet;
        y |= quiet;
        return x > y ? numerator : y > x ? divisor : numerator & divisor;
    }
#else
    if (x == infinity)
        return UINT32_C(0xffc00000);
#endif
    if (x > infinity)
        return numerator | quiet;
    if (y > infinity)
        return divisor | quiet;
    return UINT32_C(0xffc00000);
}
#endif
