#include <audio_math/integer.h>
#include <stdio.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL line=%d expression=%s\n", __LINE__, #expression);               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    CHECK(audio_math_clamp_i32(-8, -4, 6) == -4);
    CHECK(audio_math_clamp_i32(2, -4, 6) == 2);
    CHECK(audio_math_clamp_i32(9, -4, 6) == 6);
    CHECK(audio_math_clamp_u32(1, 3, 9) == 3);
    CHECK(audio_math_clamp_u32(12, 3, 9) == 9);

    CHECK(audio_math_saturate_i64_to_i32(INT64_MIN) == INT32_MIN);
    CHECK(audio_math_saturate_i64_to_i32(INT32_C(-7)) == -7);
    CHECK(audio_math_saturate_i64_to_i32(INT64_MAX) == INT32_MAX);
    CHECK(audio_math_saturate_i64_to_i24(INT64_MIN) == INT32_C(-8388608));
    CHECK(audio_math_saturate_i64_to_i24(INT32_C(17)) == 17);
    CHECK(audio_math_saturate_i64_to_i24(INT64_MAX) == INT32_C(8388607));
    CHECK(audio_math_add_saturate_i32(INT32_MAX, 1) == INT32_MAX);
    CHECK(audio_math_add_saturate_i32(INT32_MIN, -1) == INT32_MIN);
    CHECK(audio_math_subtract_saturate_i32(INT32_MIN, 1) == INT32_MIN);
    CHECK(audio_math_subtract_saturate_i32(INT32_MAX, -1) == INT32_MAX);
    CHECK(audio_math_double_saturate_i64(INT64_MAX) == INT64_MAX);
    CHECK(audio_math_double_saturate_i64(INT64_MIN) == INT64_MIN);
    CHECK(audio_math_double_saturate_i64(INT64_C(-7)) == INT64_C(-14));

    CHECK(audio_math_sign_extend_i32(UINT32_C(0x7fffff), 24) == INT32_C(8388607));
    CHECK(audio_math_sign_extend_i32(UINT32_C(0x800000), 24) == INT32_C(-8388608));
    CHECK(audio_math_sign_extend_i32(UINT32_C(0xffffff), 24) == -1);
    CHECK(audio_math_sign_extend_i32(1, 1) == -1);
    CHECK(audio_math_sign_extend_i32(UINT32_C(0x80000000), 32) == INT32_MIN);

    CHECK(audio_math_round_shift_even_i64(1, 1) == 0);
    CHECK(audio_math_round_shift_even_i64(3, 1) == 2);
    CHECK(audio_math_round_shift_even_i64(5, 1) == 2);
    CHECK(audio_math_round_shift_even_i64(7, 1) == 4);
    CHECK(audio_math_round_shift_even_i64(-1, 1) == 0);
    CHECK(audio_math_round_shift_even_i64(-3, 1) == -2);
    CHECK(audio_math_round_shift_even_i64(-5, 1) == -2);
    CHECK(audio_math_round_shift_even_i64(-7, 1) == -4);
    CHECK(audio_math_round_shift_even_i64(INT64_MAX, 1) == INT64_C(4611686018427387904));
    CHECK(audio_math_round_shift_even_i64(INT64_MIN, 1) == INT64_C(-4611686018427387904));
    CHECK(audio_math_round_shift_even_i64(INT64_MIN, 64) == 0);
    CHECK(audio_math_round_shift_even_i64(INT64_MAX, 64) == 0);
    CHECK(audio_math_round_shift_even_i32(INT32_MAX, 1) == INT32_C(1073741824));
    CHECK(audio_math_round_shift_even_i32(INT32_MIN, 1) == INT32_C(-1073741824));
    CHECK(audio_math_round_shift_even_i32(INT32_MIN, 32) == 0);
    CHECK(audio_math_round_shift_even_i32(INT32_MAX, 32) == 0);

    puts("PASS integer contracts");
    return 0;
}
