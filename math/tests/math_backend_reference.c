#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include <audio_math/math.h>

/* Compile the pre-change host evaluation path independently; Android explicitly enables NDK extension declarations. */
void lhdc_test_reference_sincos(double phase, double *sine, double *cosine) {
#ifdef __ANDROID__
    sincos(phase, sine, cosine);
#else
    *sine = audio_math_sin(phase);
    *cosine = audio_math_cos(phase);
#endif
}
