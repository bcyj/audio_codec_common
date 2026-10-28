#ifndef AUDIO_MATH_ANDROID_MATH_H
#define AUDIO_MATH_ANDROID_MATH_H
#ifdef __ANDROID__
#ifdef __cplusplus
extern "C" {
#endif
/* Declare Bionic joint sine/cosine under a project name bound directly to libm sincos.
 * No math.h extension declaration, caller feature-macro changes, or runtime forwarding.
 */
void audio_math_android_sincos(double phase, double *sine, double *cosine) __asm__("sincos");
#ifdef __cplusplus
}
#endif
#define AUDIO_MATH_PLATFORM_HAS_SINCOS 1
/* Android retains Bionic joint evaluation rather than splitting into two trig calls. */
static inline void audio_math_sincos(double x, double *sine, double *cosine) {
    audio_math_android_sincos(x, sine, cosine);
}
#endif
#endif
