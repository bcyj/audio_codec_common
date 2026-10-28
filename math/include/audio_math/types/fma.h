#ifndef AUDIO_MATH_FMA_TYPES_H
#define AUDIO_MATH_FMA_TYPES_H
#include <stdint.h>
/* 128-bit significand for software binary64 FMA without compiler __int128. */
typedef struct {
    uint64_t high;
    uint64_t low;
} AudioMathFmaWide;
#endif
