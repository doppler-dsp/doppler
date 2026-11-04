

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

#define WFM_MIX_BLOCK 32

static WFM_NOINLINE void
wfm_mix_block (float _Complex *restrict out,
               const float _Complex *restrict c)
{
  for (int i = 0; i < WFM_MIX_BLOCK; i++)
    out[i] = out[i] * c[i];
}

static inline void
wfm_mix_carrier (float _Complex *restrict out,
                 const float _Complex *restrict carrier, size_t n)
{
  size_t i = 0;
  for (; i + WFM_MIX_BLOCK <= n; i += WFM_MIX_BLOCK)
    wfm_mix_block (out + i, carrier + i);
  if (i < n)
    {
      float _Complex o[WFM_MIX_BLOCK] = { 0 }, c[WFM_MIX_BLOCK] = { 0 };
      const size_t   r = n - i;
      for (size_t k = 0; k < r; k++)
        o[k] = out[i + k], c[k] = carrier[i + k];
      wfm_mix_block (o, c);
      for (size_t k = 0; k < r; k++)
        out[i + k] = o[k];
    }
}

#endif /* WFM_PRIVATE_H */
```


