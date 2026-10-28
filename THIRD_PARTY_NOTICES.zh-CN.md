# AOSP、KISS FFT 与 LZH 的引用及许可说明

[English](THIRD_PARTY_NOTICES.md) | 简体中文

| 本项目范围 | 上游项目 | 引用性质 | 上游许可 |
|---|---|---|---|
| 公共浮点 FFT、公共定点 FFT 的相关蝶形及混合基数组织 | KISS FFT，Mark Borgerding 等 | 实现参考及改写，随包保留上游版权与许可 | BSD-3-Clause |

## KISS FFT

上游项目：[mborgerding/kissfft](https://github.com/mborgerding/kissfft)。上游 [COPYING](https://github.com/mborgerding/kissfft/blob/master/COPYING) 记录 Mark Borgerding 的版权及 BSD-3-Clause 标识，具体条款见其 [LICENSES/BSD-3-Clause](https://github.com/mborgerding/kissfft/blob/master/LICENSES/BSD-3-Clause)。

```text
Copyright (c) 2003-2010 Mark Borgerding. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
```

本项目相关实现位置：

- `math/src/fft_float32.c`、`math/src/fft_float32.h`：单精度复数 FFT 蝶形、分解和调度。
- `math/include/audio_math/fft_float32.h`：公共浮点 FFT 计划及接口。

- [LICENSES/KISS-FFT-BSD-3-Clause.txt](LICENSES/KISS-FFT-BSD-3-Clause.txt)：KISS FFT 版权与 BSD 三条款全文。
