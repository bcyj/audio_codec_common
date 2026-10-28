#ifndef AUDIO_MATH_CONSTANTS_H
#define AUDIO_MATH_CONSTANTS_H

/* Common mathematical constants. Literal suffixes select evaluation precision, while the
 * calling expression's types and operation order determine rounding. Hexadecimal literals
 * encode exact target binary values without nonstandard M_PI or runtime libm dependencies.
 */

/* Double-precision values. Do not replace division by reciprocal multiplication or reorder products/divisions. */
#define AUDIO_MATH_PI_F64 0x1.921fb54442d18p+1
#define AUDIO_MATH_TWO_PI_F64 (2.0 * AUDIO_MATH_PI_F64)
#define AUDIO_MATH_HALF_PI_F64 (0.5 * AUDIO_MATH_PI_F64)
/* Difference between true pi/2 and the binary64 high part above, used for small-integer quadrant reduction. */
#define AUDIO_MATH_HALF_PI_LOW_F64 6.123233995736766e-17
#define AUDIO_MATH_PI_OVER_32_F64 (0x1p-5 * AUDIO_MATH_PI_F64)
#define AUDIO_MATH_TWO_OVER_PI_F64 (2.0 / AUDIO_MATH_PI_F64)
#define AUDIO_MATH_LN_TWO_F64 0x1.62e42fefa39efp-1
/* High/low ln(2); small integers multiply the high part exactly for exponential/log reduction. */
#define AUDIO_MATH_LN_TWO_HIGH_F64 0x1.62e42fee00000p-1
#define AUDIO_MATH_LN_TWO_LOW_F64 0x1.a39ef35793c76p-33
#define AUDIO_MATH_LOG10_TWO_F64 0x1.34413509f79ffp-2
/* log10(e), converting extended-precision natural logs to decimal logs. */
#define AUDIO_MATH_LOG10_E_F64 0x1.bcb7b1526e50ep-2
/* Split log10(2); small integer exponents multiply the high part exactly. */
#define AUDIO_MATH_LOG10_TWO_HIGH_F64 0x1.3441350000000p-2
#define AUDIO_MATH_LOG10_TWO_LOW_F64 0x1.3ef3fde623e25p-31

/* The f suffix selects single precision without casts inside macros; exact binary scalings
 * share their base values. Angle conversion uses a specified float bit pattern and must not
 * be replaced directly with float pi / 180.
 */
#define AUDIO_MATH_PI_F32 0x1.921fb6p+1f
#define AUDIO_MATH_TWO_PI_F32 (2.0f * AUDIO_MATH_PI_F32)
#define AUDIO_MATH_HALF_PI_F32 (0.5f * AUDIO_MATH_PI_F32)
#define AUDIO_MATH_QUARTER_PI_F32 (0.25f * AUDIO_MATH_PI_F32)
#define AUDIO_MATH_SQRT_TWO_F32 0x1.6a09e6p+0f
#define AUDIO_MATH_INV_SQRT_TWO_F32 (0.5f * AUDIO_MATH_SQRT_TWO_F32)
#define AUDIO_MATH_LN_TWO_F32 0x1.62e430p-1f
#define AUDIO_MATH_LOG2_TEN_F64 0x1.a934f0979a371p+1
#define AUDIO_MATH_LN_TEN_HIGH_F64 0x1.26bb1b8000000p+1
#define AUDIO_MATH_LN_TEN_LOW_F64 0x1.daaa8ac16ea57p-26
#define AUDIO_MATH_RADIANS_PER_DEGREE_F32 0x1.1df46ap-6f
#define AUDIO_MATH_DEGREES_PER_RADIAN_F32 0x1.ca5dc2p+5f

/* Windows large-angle reduction: convert Q62 integer remainder to radians with (pi/2) * 2^-62. */
#define AUDIO_MATH_HALF_PI_Q62_SCALE_F64 (AUDIO_MATH_HALF_PI_F64 * 0x1p-62)

/* Use long double literals; target ABI determines precision without narrowing to double. */
#define AUDIO_MATH_TWO_PI_LONG_DOUBLE 0x1.921fb54442d18469898cc51701b8p+2L

/* Exact numeric constants; preserve literal types and values and express casts explicitly at use sites. */
#define AUDIO_MATH_EPSILON_F32 1.1920928955078125e-7f
#define AUDIO_MATH_EPSILON_F64 0x1p-52
#define AUDIO_MATH_MIN_NORMAL_F32 0x1p-126f
#define AUDIO_MATH_MAX_FINITE_F32 0x1.fffffep127f
#define AUDIO_MATH_INT32_MAX_F64 2147483647.0
#define AUDIO_MATH_INT24_MAX_F32 8388607.0f
#define AUDIO_MATH_INV_SQRT_EIGHT_F32 0.35355338f
#define AUDIO_MATH_ONE_TENTH_F32 0x1.99999ap-4f
#define AUDIO_MATH_POW2_NEG_32_F32 0x1p-32f
#define AUDIO_MATH_POW2_NEG_31_F64 0x1p-31
#define AUDIO_MATH_POW2_NEG_31_F32 0x1p-31f
#define AUDIO_MATH_POW2_NEG_30_F32 0x1p-30f
#define AUDIO_MATH_POW2_NEG_24_F32 0x1p-24f
#define AUDIO_MATH_POW2_NEG_20_F32 0x1p-20f
#define AUDIO_MATH_POW2_NEG_16_F32 0x1p-16f
#define AUDIO_MATH_POW2_NEG_11_F64 0x1p-11
#define AUDIO_MATH_POW2_NEG_10_F32 0x1p-10f
#define AUDIO_MATH_POW2_NEG_9_F32 0x1p-9f
#define AUDIO_MATH_POW2_NEG_8_F32 0x1p-8f
#define AUDIO_MATH_POW2_NEG_7_F32 0x1p-7f
#define AUDIO_MATH_POW2_20_F32 0x1p20f
#define AUDIO_MATH_POW2_23_F32 0x1p23f
#define AUDIO_MATH_POW2_31_F32 0x1p31f
#define AUDIO_MATH_POW2_31_F64 0x1p31
#define AUDIO_MATH_POW2_32_F32 0x1p32f
#define AUDIO_MATH_POW2_53_F32 0x1p53f
#define AUDIO_MATH_POW2_NEG_12_F32 0x1p-12f

#endif
