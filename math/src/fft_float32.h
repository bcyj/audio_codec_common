#ifndef AUDIO_MATH_SRC_FFT_FLOAT32_H
#define AUDIO_MATH_SRC_FFT_FLOAT32_H
#include <audio_math/fft_float32.h>
/* Multiply a complex value by its twiddle, preserving floating multiply-add order. */
static inline AudioMathComplex32 audio_math_fft_twiddle_product(AudioMathComplex32 value, AudioMathComplex32 twiddle) {
    return (AudioMathComplex32){value.r * twiddle.r - value.i * twiddle.i,
                          value.r * twiddle.i + twiddle.r * value.i};
}
#endif
