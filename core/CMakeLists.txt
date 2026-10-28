add_library(audio_common_core STATIC src/memory.c src/ring_buffer.c)
add_library(audio_common::core ALIAS audio_common_core)

target_compile_features(audio_common_core PUBLIC c_std_11)
target_include_directories(audio_common_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
set_target_properties(audio_common_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  target_compile_options(audio_common_core PRIVATE /O2 /W4 /utf-8)
  if(AUDIO_COMMON_NO_CRT)
    target_compile_options(audio_common_core PRIVATE /GS- /Zl /Oi)
  endif()
else()
  target_compile_options(audio_common_core PRIVATE -O2 -Wall -Wextra -Wpedantic)
  if(AUDIO_COMMON_NO_CRT)
    target_compile_options(audio_common_core PRIVATE -fno-stack-protector)
  endif()
  if(AUDIO_COMMON_DRIVER_RESOURCES)
    target_compile_options(audio_common_core PRIVATE -fstack-usage -Werror=frame-larger-than=512)
  endif()
endif()
