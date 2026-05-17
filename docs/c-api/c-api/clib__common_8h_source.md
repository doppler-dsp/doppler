

# File clib\_common.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the documentation of this file](clib__common_8h.md)


```C++

#ifndef CLIB_COMMON_H
#define CLIB_COMMON_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* CMPLXF/CMPLX are C11.  MinGW/ucrt64 does not expose them even with
 * <complex.h>, so provide __builtin_complex fallbacks for GCC builds. */
#ifndef CMPLXF
#define CMPLXF(x, y) __builtin_complex ((float)(x), (float)(y))
#endif
#ifndef CMPLX
#define CMPLX(x, y) __builtin_complex ((double)(x), (double)(y))
#endif

#endif /* CLIB_COMMON_H */
```
