#ifndef AUDIO_MATH_ARM_SQRT_H
#define AUDIO_MATH_ARM_SQRT_H
#if (defined(__aarch64__) || (defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 4))) &&         \
    (defined(__GNUC__) || defined(__clang__))
#define AUDIO_MATH_ARM_NATIVE_SQRT 1
/* Executes native float square root with the hardware zero, infinity, and NaN rules. */
static inline float audio_math_arm_sqrt_f32(float value) {
    float result;
#if defined(__aarch64__)
    __asm__("fsqrt %s0, %s1" : "=w"(result) : "w"(value));
#else
    __asm__("vsqrt.f32 %0, %1" : "=t"(result) : "t"(value));
#endif
    return result;
}
#endif
#if (defined(__aarch64__) || (defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 8))) &&         \
    (defined(__GNUC__) || defined(__clang__))
#define AUDIO_MATH_ARM_NATIVE_SQRT_F64 1
/* Uses native double square root only when the declared ISA supports it. */
static inline double audio_math_arm_sqrt_f64(double value) {
    double result;
#if defined(__aarch64__)
    __asm__("fsqrt %d0, %d1" : "=w"(result) : "w"(value));
#else
    __asm__("vsqrt.f64 %P0, %P1" : "=w"(result) : "w"(value));
#endif
    return result;
}
#endif
#endif
