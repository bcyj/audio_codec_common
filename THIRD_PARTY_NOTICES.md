# Third-Party Notices

English | [Simplified Chinese](THIRD_PARTY_NOTICES.zh-CN.md)

## KISS FFT

Upstream: [mborgerding/kissfft](https://github.com/mborgerding/kissfft). Its [COPYING](https://github.com/mborgerding/kissfft/blob/master/COPYING) records Mark Borgerding's copyright and BSD-3-Clause identifier; terms appear in [LICENSES/BSD-3-Clause](https://github.com/mborgerding/kissfft/blob/master/LICENSES/BSD-3-Clause).

```text
Copyright (c) 2003-2010 Mark Borgerding. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
```

Related implementation locations:

- `math/src/fft_float32.c`, `math/src/fft_float32.h`: single-precision complex butterflies, factorization, and scheduling.
- `math/include/audio_math/fft_float32.h`: common floating FFT plans/interfaces.
- `lhdc/common/src/dsp/fft_fixed32.c`, `fft_fixed32.h`: fixed butterflies, stage scaling, and integer rounding helpers.
- `lhdc/common/include/dsp/fft_fixed32.h`: common fixed FFT types/interfaces.

## Included license files

- [LICENSES/KISS-FFT-BSD-3-Clause.txt](LICENSES/KISS-FFT-BSD-3-Clause.txt): KISS FFT copyright/full three-clause BSD terms.
