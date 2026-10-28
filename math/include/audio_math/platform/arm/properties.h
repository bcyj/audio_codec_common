#ifndef AUDIO_MATH_ARM_PROPERTIES_H
#define AUDIO_MATH_ARM_PROPERTIES_H
/* The ARM32 hard-float path uses an inline VFP backend without a math runtime.
 * ARM32 without hardware floating point, ARM64, and other hosts use shared bit operations.
 */
#if !defined(_WIN32) && defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 12) == 12 &&          \
    (defined(__GNUC__) || defined(__clang__))
#define AUDIO_MATH_ARM_NATIVE_PROPERTIES 1

/* Tests a float for NaN with native classification. */
static inline int audio_math_arm_is_nan_f32(float value) {
    return !!__builtin_isnan(value);
}
/* Tests a double for NaN with native classification. */
static inline int audio_math_arm_is_nan_f64(double value) {
    return !!__builtin_isnan(value);
}
/* Tests a float for infinity with native classification. */
static inline int audio_math_arm_is_infinite_f32(float value) {
    return !!__builtin_isinf(value);
}
/* Tests a double for infinity with native classification. */
static inline int audio_math_arm_is_infinite_f64(double value) {
    return !!__builtin_isinf(value);
}
/* Tests whether a float is finite with native classification. */
static inline int audio_math_arm_is_finite_f32(float value) {
    return !!__builtin_isfinite(value);
}
/* Tests whether a double is finite with native classification. */
static inline int audio_math_arm_is_finite_f64(double value) {
    return !!__builtin_isfinite(value);
}
/* Uses VFP float absolute value and preserves native NaN behavior. */
static inline float audio_math_arm_abs_f32(float value) {
    return __builtin_fabsf(value);
}
/* Uses VFP double absolute value without a math-library call. */
static inline double audio_math_arm_abs_f64(double value) {
    return __builtin_fabs(value);
}
#endif
#endif
