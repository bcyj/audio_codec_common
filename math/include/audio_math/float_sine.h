#ifndef AUDIO_MATH_DSP_FLOAT_SINE_H
#define AUDIO_MATH_DSP_FLOAT_SINE_H

#if defined(_KERNEL_MODE) && defined(_MSC_VER) && !defined(__clang__) &&                           \
    (defined(_M_IX86) || defined(_M_X64))
/* Finite |phase| <= 2^134 covers every V4 float-cutoff sinc phase.
 * Return double; callers convert to float only after division by the tap index. */
#ifdef __cplusplus
extern "C" {
#endif
double audio_math_sine_wide_f64_soft(double phase);
#ifdef __cplusplus
}
#endif

#endif

#endif
