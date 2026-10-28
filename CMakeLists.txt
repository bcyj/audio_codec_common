cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(audio_codec_common LANGUAGES C)
endif()

option(AUDIO_COMMON_BUILD_TESTS "Build audio_codec_common tests" OFF)
option(AUDIO_COMMON_NO_CRT "Build common components for a freestanding host" OFF)
option(AUDIO_COMMON_DRIVER_RESOURCES "Use bounded stack/resource checks" OFF)

if(AUDIO_COMMON_BUILD_TESTS)
  # Enable testing at the top level before adding subdirectories so every
  # common-library test is visible from the root build directory.
  enable_testing()
  set(AUDIO_MATH_BUILD_TESTS ON CACHE BOOL
    "Build independent numeric contract tests" FORCE)
endif()

add_subdirectory(core)

# During migration an older codec superproject may already have added its
# bundled audio_math target. Reuse it instead of defining a second copy; once
# every codec consumes this repository, the normal path below owns the target.
if(TARGET audio_math)
  if(NOT TARGET audio_common::math_headers)
    add_library(audio_common_math_headers INTERFACE)
    add_library(audio_common::math_headers ALIAS audio_common_math_headers)
    target_include_directories(audio_common_math_headers INTERFACE
      "${CMAKE_CURRENT_SOURCE_DIR}/math/include")
  endif()
  if(NOT TARGET audio_common::math)
    add_library(audio_common::math ALIAS audio_math)
  endif()
else()
  add_subdirectory(math)
endif()

if(AUDIO_COMMON_BUILD_TESTS)
  add_subdirectory(core/tests)
endif()
