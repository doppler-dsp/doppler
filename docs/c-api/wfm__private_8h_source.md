

# File wfm\_private.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_private.h**](wfm__private_8h.md)

[Go to the documentation of this file](wfm__private_8h.md)


```C++

#ifndef WFM_PRIVATE_H
#define WFM_PRIVATE_H

#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define WFM_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define WFM_NOINLINE __declspec(noinline)
#else
#define WFM_NOINLINE
#endif

static WFM_NOINLINE void
wfm_add_scaled (float _Complex *restrict out, const float _Complex *restrict in,
                size_t n, float gain)
{
  for (size_t i = 0; i < n; i++)
    out[i] += gain * in[i];
}

#endif /* WFM_PRIVATE_H */
```


