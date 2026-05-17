

# File doppler.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**doppler.h**](c_2include_2doppler_8h.md)

[Go to the documentation of this file](c_2include_2doppler_8h.md)


```C++


#ifndef DP_H
#define DP_H

/* Define _GNU_SOURCE before any standard headers (required for memfd_create on
 * Linux) */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "dp/accumulator.h"
#include "dp/buffer.h"
#include "dp/core.h"
#include "dp/ddc.h"
#include "dp/delay.h"
#include "dp/fft.h"
#include "dp/fir.h"
#include "dp/hbdecim.h"
#include "dp/nco.h"
#include "dp/resamp.h"
#include "dp/resamp_dpmfs.h"
#include "dp/stream.h"
#include "dp/util.h"
#include "dp/window.h"

#endif /* DP_H */
```
