cmake_minimum_required(VERSION 3.19)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(audio_math LANGUAGES C)
endif()
option(AUDIO_MATH_BUILD_TESTS "Build independent numeric contract tests" OFF)
option(AUDIO_MATH_FFT_ITERATIVE_STATIC "Use allocation-free iterative FFT scheduling" OFF)
if(DEFINED AUDIO_MATH_FFT_512_BITEXACT_SSE2 AND NOT DEFINED AUDIO_MATH_FFT_BITEXACT_SSE2)
  set(AUDIO_MATH_FFT_BITEXACT_SSE2 ${AUDIO_MATH_FFT_512_BITEXACT_SSE2}
    CACHE BOOL "Use bit-exact SSE2 acceleration for FFT butterflies")
endif()
option(AUDIO_MATH_FFT_BITEXACT_SSE2
  "Use bit-exact SSE2 acceleration for FFT butterflies" ON)
option(AUDIO_MATH_FFT_SPLIT_RADIX_SSE
  "Build the optional split-radix SSE FFT backend" OFF)
if(DEFINED LHDC_KISS_FFT_ITERATIVE_STATIC)
  set(AUDIO_MATH_FFT_ITERATIVE_STATIC ${LHDC_KISS_FFT_ITERATIVE_STATIC})
endif()

add_library(audio_math_headers INTERFACE)
add_library(audio_common::math_headers ALIAS audio_math_headers)
add_library(audio_math::headers ALIAS audio_math_headers)
target_include_directories(audio_math_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(audio_math_sources src/fma_soft.c src/float_exp.c src/float_sine.c
  src/fft_float32.c src/fft_float32_tables.c src/fft_sse.c)
if(AUDIO_MATH_FFT_SPLIT_RADIX_SSE)
  list(APPEND audio_math_sources src/fft_sse_tables.c)
endif()
add_library(audio_math_objects OBJECT ${audio_math_sources})
target_compile_definitions(audio_math_objects PRIVATE
  AUDIO_MATH_NO_CRT=$<BOOL:${AUDIO_COMMON_NO_CRT}>
  AUDIO_MATH_FFT_ITERATIVE_STATIC=$<BOOL:${AUDIO_MATH_FFT_ITERATIVE_STATIC}>
  AUDIO_MATH_FFT_BITEXACT_SSE2=$<BOOL:${AUDIO_MATH_FFT_BITEXACT_SSE2}>
  AUDIO_MATH_FFT_512_BITEXACT_SSE2=$<BOOL:${AUDIO_MATH_FFT_BITEXACT_SSE2}>
  AUDIO_MATH_FFT_SPLIT_RADIX_SSE=$<BOOL:${AUDIO_MATH_FFT_SPLIT_RADIX_SSE}>)
if(WIN32)
  target_sources(audio_math_objects PRIVATE platform/windows/float_math.c
    platform/windows/math_tables.c platform/windows/crt_math.c)
endif()
target_include_directories(audio_math_objects PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(audio_math_objects PRIVATE c_std_11)
set_target_properties(audio_math_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(audio_math_objects PRIVATE /O2 /fp:strict /utf-8
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:/arch:SSE2>)
else()
  target_compile_options(audio_math_objects PRIVATE -O2 -ffp-contract=off -fno-fast-math
    -Wunused-variable -Wunused-function -Wunused-const-variable -Wparentheses -Wuninitialized)
  if(ANDROID)
    include(platform/android/Android.cmake)
    audio_math_android_compile_settings(audio_math_objects)
  endif()
endif()
add_library(audio_math STATIC $<TARGET_OBJECTS:audio_math_objects>)
add_library(audio_math::math ALIAS audio_math)
add_library(audio_common::math ALIAS audio_math)
target_include_directories(audio_math PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(audio_math PUBLIC
  AUDIO_MATH_FFT_SPLIT_RADIX_SSE=$<BOOL:${AUDIO_MATH_FFT_SPLIT_RADIX_SSE}>)
if(NOT MSVC)
  target_link_libraries(audio_math PUBLIC m)
endif()

if(AUDIO_MATH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
