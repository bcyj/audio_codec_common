#ifndef AUDIO_MATH_ANDROID_FMA_H
#define AUDIO_MATH_ANDROID_FMA_H
/* Android ARM32 uses Bionic even without hardware FMA; ARM64 may emit compiler-selected instructions. */
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__arm__) || defined(__aarch64__))
#define AUDIO_MATH_PLATFORM_NATIVE_FMA 1
#include <audio_math/platform/arm/fma.h>
#else
#define AUDIO_MATH_PLATFORM_NATIVE_FMA 0
#endif
#endif
