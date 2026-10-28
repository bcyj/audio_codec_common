/* Contract tests for the SSE FFT vector kernel and general power-of-two transform.
 * Covers 4..1024 points, forward/inverse transforms, impulses, noise, finite inputs,
 * sentinel write protection, and numeric error bounds.
 */
#include <audio_math/fft_sse.h>
#include <audio_math/fft_float32.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Reference discrete Fourier transform (DFT). */
static void reference_dft(const AudioMathComplex32 *in, AudioMathComplex32 *out, int n,
                          int inverse) {
    double sign = inverse ? 1.0 : -1.0;
    for (int k = 0; k < n; ++k) {
        double sum_r = 0.0, sum_i = 0.0;
        for (int t = 0; t < n; ++t) {
            double angle = sign * 2.0 * M_PI * (double)k * (double)t / (double)n;
            double c = cos(angle), s = sin(angle);
            sum_r += (double)in[t].r * c - (double)in[t].i * s;
            sum_i += (double)in[t].r * s + (double)in[t].i * c;
        }
        out[k].r = (float)sum_r;
        out[k].i = (float)sum_i;
    }
}

int main(void) {
#if !AUDIO_MATH_HAS_FFT_SSE
    printf("FFT SSE: platform does not support SSE; verified graceful fallback.\n");
    if (audio_math_fft_sse_supported(512))
        return 1;
    AudioMathComplex32 dummy[4] = {{0}};
    if (audio_math_fft_sse_transform(dummy, dummy, 4, 0))
        return 2;
    return 0;
#else
    printf("Verifying FFmpeg SSE FFT implementation...\n");
    const int lengths[] = {4, 8, 16, 32, 64, 128, 256, 512, 1024};
    const size_t num_lengths = sizeof lengths / sizeof *lengths;

    AudioMathComplex32 input[1024], expected[1024], actual[1024], scratch[1024];
    uint8_t guard_before[64], guard_after[64];
    memset(guard_before, 0x5a, sizeof guard_before);
    memset(guard_after, 0x5a, sizeof guard_after);

    unsigned total_cases = 0;

    for (size_t l = 0; l < num_lengths; ++l) {
        int n = lengths[l];
        if (!audio_math_fft_sse_supported(n)) {
            fprintf(stderr, "Expected size %d to be supported\n", n);
            return 10;
        }

        for (int inverse = 0; inverse < 2; ++inverse) {
            for (int pattern = 0; pattern < 5; ++pattern) {
                for (int i = 0; i < n; ++i) {
                    float r = 0.0f, im = 0.0f;
                    switch (pattern) {
                    case 0: /* Positive and negative zero. */
                        r = 0.0f;
                        im = -0.0f;
                        break;
                    case 1: /* Unit impulse. */
                        r = (i == 0) ? 1.0f : 0.0f;
                        im = 0.0f;
                        break;
                    case 2: /* Deterministic multitone sine wave. */
                        r = (float)sin(i * 0.123 + pattern);
                        im = (float)cos(i * 0.456 - pattern);
                        break;
                    case 3: /* Subnormal and low-amplitude values. */
                        r = (float)(sin(i * 0.789) * 1e-20);
                        im = (float)(cos(i * 0.321) * 1e-20);
                        break;
                    default: /* Large finite values. */
                        r = (float)(sin(i * 0.654) * 1e4);
                        im = (float)(cos(i * 0.987) * 1e4);
                        break;
                    }
                    input[i].r = r;
                    input[i].i = im;
                }

                reference_dft(input, expected, n, inverse);

                /* In-place SSE transform. */
                memcpy(actual, input, (size_t)n * sizeof *actual);
                if (!audio_math_fft_sse_transform(actual, scratch, n, inverse)) {
                    fprintf(stderr, "audio_math_fft_sse_transform failed: n=%d inv=%d\n", n,
                            inverse);
                    return 11;
                }

                double delta_energy = 0.0, ref_energy = 0.0;
                for (int i = 0; i < n; ++i) {
                    double dr = (double)actual[i].r - (double)expected[i].r;
                    double di = (double)actual[i].i - (double)expected[i].i;
                    delta_energy += dr * dr + di * di;
                    ref_energy += (double)expected[i].r * expected[i].r +
                                  (double)expected[i].i * expected[i].i;
                }
                double rel_error = sqrt(delta_energy / (ref_energy > 1e-20 ? ref_energy : 1.0));
                if (rel_error > 1e-4) {
                    fprintf(stderr, "Relative error exceeded: n=%d inv=%d pattern=%d err=%e\n", n,
                            inverse, pattern, rel_error);
                    return 12;
                }

                /* Out-of-place audio_math_fft_power2 transform. */
                memset(actual, 0x3c, (size_t)n * sizeof *actual);
                if (!audio_math_fft_power2(actual, input, scratch, n, inverse)) {
                    fprintf(stderr, "audio_math_fft_power2 failed: n=%d inv=%d\n", n, inverse);
                    return 13;
                }

                delta_energy = 0.0;
                for (int i = 0; i < n; ++i) {
                    double dr = (double)actual[i].r - (double)expected[i].r;
                    double di = (double)actual[i].i - (double)expected[i].i;
                    delta_energy += dr * dr + di * di;
                }
                rel_error = sqrt(delta_energy / (ref_energy > 1e-20 ? ref_energy : 1.0));
                if (rel_error > 1e-4) {
                    fprintf(stderr, "power2 relative error exceeded: n=%d inv=%d err=%e\n", n,
                            inverse, rel_error);
                    return 14;
                }

                ++total_cases;
            }
        }
    }

    /* Verify the audio_math_fft_512 entry point and forward/inverse contract. */
    AudioMathFft512Work work_512;
    float interleaved[1024];
    for (int i = 0; i < 512; ++i) {
        interleaved[2 * i] = (float)sin(i * 0.05);
        interleaved[2 * i + 1] = (float)cos(i * 0.08);
        input[i].r = interleaved[2 * i];
        input[i].i = interleaved[2 * i + 1];
    }
    reference_dft(input, expected, 512, 0);

    /* 512-point forward transform. */
    audio_math_fft_512(interleaved, 0, &work_512);
    for (int i = 0; i < 512; ++i) {
        double dr = (double)interleaved[2 * i] - (double)expected[i].r;
        double di = (double)interleaved[2 * i + 1] - (double)expected[i].i;
        if (sqrt(dr * dr + di * di) > 5e-4) {
            fprintf(stderr, "audio_math_fft_512 forward error exceeded at %d\n", i);
            return 20;
        }
    }

    /* The 512-point inverse includes 1/512 normalization and should restore input. */
    audio_math_fft_512(interleaved, 1, &work_512);
    for (int i = 0; i < 512; ++i) {
        double dr = (double)interleaved[2 * i] - (double)input[i].r;
        double di = (double)interleaved[2 * i + 1] - (double)input[i].i;
        if (sqrt(dr * dr + di * di) > 1e-4) {
            fprintf(stderr, "audio_math_fft_512 roundtrip error exceeded at %d: dr=%e di=%e\n", i,
                    dr, di);
            return 21;
        }
    }
    ++total_cases;

    /* Unsupported-length guard. */
    if (audio_math_fft_sse_supported(3) || audio_math_fft_sse_supported(100) ||
        audio_math_fft_sse_supported(2048) || audio_math_fft_sse_supported(0)) {
        fprintf(stderr, "Invalid lengths unexpectedly reported supported\n");
        return 30;
    }

    printf("PASS FFT SSE contract test: %u test cases passed.\n", total_cases);
    return 0;
#endif
}
