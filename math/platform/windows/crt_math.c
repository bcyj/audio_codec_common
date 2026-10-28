/* User-mode compatibility uses DLL loading and is excluded from Windows kernel builds.
 * Kernel code defines only compiler FP-use markers, not user-mode loading services. */
#if !defined(_KERNEL_MODE)

/* Supply MinGW with the same double trig as native MSVC.
 * Depend only on Windows system UCRT, without installing/linking the MSVC compiler toolchain. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <math.h>
#include <audio_math/platform/windows/crt.h>
/* Process-wide function-address cache without audio state, read-only after InitOnceExecuteOnce publication.
 * Resolve once; all instances share pure system-CRT math functions. */
static INIT_ONCE audio_math_math_once = INIT_ONCE_STATIC_INIT;
static audio_math_windows_unary_math_fn audio_math_crt_sin, audio_math_crt_cos;

/* Load UCRT from the system directory and resolve trig functions thread-safely. */
static BOOL CALLBACK audio_math_resolve_math(PINIT_ONCE once, PVOID parameter, PVOID *context) {
    (void)once;
    (void)parameter;
    (void)context;
    HMODULE module = GetModuleHandleW(L"ucrtbase.dll");
    if (!module)
        module = LoadLibraryExW(L"ucrtbase.dll", NULL, 0x00000800);
    if (!module)
        return FALSE;
    audio_math_crt_sin = (audio_math_windows_unary_math_fn)GetProcAddress(module, "sin");
    audio_math_crt_cos = (audio_math_windows_unary_math_fn)GetProcAddress(module, "cos");
    return audio_math_crt_sin && audio_math_crt_cos;
}

/* Query the system trig backend, allowing later retries after resolution failure. */
int audio_math_windows_math_ready(void) {
    return InitOnceExecuteOnce(&audio_math_math_once, audio_math_resolve_math, NULL, NULL) != 0;
}

/* Return NaN if unavailable without terminating the host or switching to differently rounded approximations. */
double audio_math_windows_sin(double value) {
    if (!audio_math_windows_math_ready())
        return NAN;
    return audio_math_crt_sin(value);
}

/* Compute double cosine through UCRT. */
double audio_math_windows_cos(double value) {
    if (!audio_math_windows_math_ready())
        return NAN;
    return audio_math_crt_cos(value);
}

#else

/* MSVC floating objects require this link marker; it has no math or instance state.
 * C external linkage, 32-bit read-only storage, and the toolchain-prescribed marker value. */
const int _fltused = 0x9875;

#endif
