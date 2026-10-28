#ifndef AUDIO_MATH_DSP_FLOAT_EXP_H
#define AUDIO_MATH_DSP_FLOAT_EXP_H

/* Compute binary32 power of two with binary32 output, independently of host math.
 * Use nearest-even rounding; Win32 x87 requires 53-bit precision. Only return values are specified,
 * without CRT errno, exception flags, or nondefault-rounding compatibility.
 */
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_exp2f_soft(float input);
#ifdef __cplusplus
}
#endif

/* Single-precision natural exponent with the same return policy/environment requirements as power of two above. */
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_expf_soft(float input);
#ifdef __cplusplus
}
#endif

/* Compute nonnegative integer powers by sequential double multiplication without a math runtime.
 * Callers bound the order/select result type; do not use exponentiation by squaring, which changes rounding order.
 * This multiplication model need not match arbitrary host pow approximations.
 */
static inline double audio_math_powu_f64(double base, unsigned exponent) {
    double result = 1.0;
    for (unsigned i = 0; i < exponent; ++i)
        result *= base;
    return result;
}

#if defined(_KERNEL_MODE)
/* Compute base-ten power retaining a double exponent until final float rounding.
 * For kernel custom presets/tilts; return values only, without errno or FP exception emulation.
 * Do not narrow the exponent early; environment requirements match software exponentiation above.
 */
#ifdef __cplusplus
extern "C" {
#endif
float audio_math_pow10_to_f32_soft(double exponent);
#ifdef __cplusplus
}
#endif
#endif

#endif
