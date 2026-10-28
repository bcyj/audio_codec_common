# audio_codec_common

LHDC 与 aptX Adaptive 共用的、与具体编解码器无关的 C 组件。

- `core`：宿主分配器适配、受检线性工作区和无动态分配容器。
- `math`：整数、浮点、FFT 及平台数值后端。

公开头文件分别使用 `audio_common` 和 `audio_math` 命名空间；CMake 目标为
`audio_common::core`、`audio_common::math_headers` 和 `audio_common::math`。
码流格式、编解码状态与策略必须保留在各自的 codec 仓库中。

组合构建应只添加一次本仓库，再添加两个 codec：

```cmake
add_subdirectory(path/to/audio_codec_common audio_common_build)
add_subdirectory(path/to/lhdc lhdc-build)
add_subdirectory(path/to/aptx-adaptive aptx-build)
```

codec 独立构建时可用 `AUDIO_COMMON_ROOT` 指向本源码目录。公共分配器不选择进程级
全局堆：codec 必须显式提供宿主回调或绑定调用方工作区；实例初始化完成后的容器和
处理路径不再分配内存。
