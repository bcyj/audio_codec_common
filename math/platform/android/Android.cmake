# The math module can be built independently of LHDC.
include_guard(GLOBAL)
function(audio_math_android_compile_settings target)
  if(ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_options(${target} PRIVATE -mthumb -mfpu=vfpv3-d16 -mfloat-abi=softfp)
  endif()
endfunction()
