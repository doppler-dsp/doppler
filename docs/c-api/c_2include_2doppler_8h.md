

# File doppler.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**doppler.h**](c_2include_2doppler_8h.md)

[Go to the source code of this file](c_2include_2doppler_8h_source.md)

_Doppler - Unified DSP and Streaming Library._ [More...](#detailed-description)

* `#include "dp/accumulator.h"`
* `#include "dp/buffer.h"`
* `#include "dp/core.h"`
* `#include "dp/ddc.h"`
* `#include "dp/delay.h"`
* `#include "dp/fft.h"`
* `#include "dp/fir.h"`
* `#include "dp/hbdecim.h"`
* `#include "dp/nco.h"`
* `#include "dp/resamp.h"`
* `#include "dp/resamp_dpmfs.h"`
* `#include "dp/stream.h"`
* `#include "dp/util.h"`
* `#include "dp/window.h"`

































































## Detailed Description


Convenience header that includes all Doppler modules:
* Core: Initialization and version (dp\_init, dp\_cleanup, dp\_version)
* Buffer: Lock-free circular buffers (dp\_f32\_buffer\_\*, etc.)
* FFT: Fast Fourier Transform (dp\_fft\_\*)
* FIR: Finite Impulse Response filters (dp\_fir\_\*)
* Util: SIMD-accelerated operations (dp\_c16\_mul)
* Stream: PUB/SUB, PUSH/PULL, REQ/REP streaming (dp\_pub\_\*, dp\_sub\_\*, etc.)




For selective inclusion, use individual headers:
```C++
#include <dp/core.h>
#include <dp/buffer.h>
#include <dp/fft.h>
#include <dp/fir.h>
#include <dp/util.h>
#include <dp/stream.h>
```



Or simply include everything:
```C++
#include <doppler.h>
```





------------------------------
The documentation for this class was generated from the following file `c/include/doppler.h`
