#ifndef AUDIO_MATH_COMPILER_H
#define AUDIO_MATH_COMPILER_H

/* Only for small common helpers with measured call overhead; do not force-inline large algorithms. */
#if defined(_MSC_VER)
#define AUDIO_MATH_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define AUDIO_MATH_FORCE_INLINE inline __attribute__((always_inline))
#else
#define AUDIO_MATH_FORCE_INLINE inline
#endif

#endif
