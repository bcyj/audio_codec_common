#ifndef AUDIO_MATH_ARM_ROUNDING_H
#define AUDIO_MATH_ARM_ROUNDING_H
#if (defined(__arm__) || defined(__aarch64__)) && (defined(__GNUC__) || defined(__clang__))
#define AUDIO_MATH_ARM_NATIVE_ROUNDING 1

/* Lets the compiler select float truncation for the declared ARM ISA. */
static inline float audio_math_arm_trunc_f32(float value) {
    return __builtin_truncf(value);
}
/* Lets the compiler select float rounding toward negative infinity. */
static inline float audio_math_arm_floor_f32(float value) {
    return __builtin_floorf(value);
}
/* Lets the compiler select float rounding toward positive infinity. */
static inline float audio_math_arm_ceil_f32(float value) {
    return __builtin_ceilf(value);
}
/* Lets the compiler select float nearest rounding with halves away from zero. */
static inline float audio_math_arm_round_f32(float value) {
    return __builtin_roundf(value);
}
/* Lets the compiler select double truncation for the declared ARM ISA. */
static inline double audio_math_arm_trunc_f64(double value) {
    return __builtin_trunc(value);
}
/* Lets the compiler select double rounding toward negative infinity. */
static inline double audio_math_arm_floor_f64(double value) {
    return __builtin_floor(value);
}
/* Lets the compiler select double rounding toward positive infinity. */
static inline double audio_math_arm_ceil_f64(double value) {
    return __builtin_ceil(value);
}
/* Lets the compiler select double nearest rounding with halves away from zero. */
static inline double audio_math_arm_round_f64(double value) {
    return __builtin_round(value);
}
#endif
#endif
