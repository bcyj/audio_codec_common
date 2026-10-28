# audio_codec_common

Shared, codec-independent C components used by bt audio codes.

- `core`: host allocator adapters, checked linear workspaces, and allocation-free containers.
- `math`: integer, floating-point, FFT and platform numeric backends.

Public headers use the `audio_common` or `audio_math` include namespaces.
The CMake targets are `audio_common::core`, `audio_common::math_headers`, and
`audio_common::math`. Codec-specific bitstream, state and policy code must stay
in its codec repository.

For a combined build, add this repository once before adding either codec:

```cmake
add_subdirectory(path/to/audio_codec_common audio_common_build)
add_subdirectory(path/to/lhdc lhdc-build)
add_subdirectory(path/to/aptx-adaptive aptx-build)
```

Standalone codec builds may set `AUDIO_COMMON_ROOT` to this source directory.
The allocator adapter never selects a process-global heap: codecs either supply host callbacks or
bind caller-owned workspace. Containers and processing paths remain allocation-free after instance
initialization.
