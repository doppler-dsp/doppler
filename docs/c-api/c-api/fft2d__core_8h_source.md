

# File fft2d\_core.h

[**File List**](files.md) **>** [**fft2d**](dir_9009a3f6624dc57956402cd0407c056b.md) **>** [**fft2d\_core.h**](fft2d__core_8h.md)

[Go to the documentation of this file](fft2d__core_8h.md)


```C++

#ifndef FFT2D_CORE_H
#define FFT2D_CORE_H

#include "clib_common.h"
#include "pocketfft/pocketfft.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    pocketfft_plan *plan_f64;
    pocketfft_plan *plan_f32;
    size_t ny;
    size_t nx;
    int sign;
  } fft2d_state_t;

  fft2d_state_t *fft2d_create (size_t ny, size_t nx, int sign, int nthreads);

  void fft2d_destroy (fft2d_state_t *state);

  void fft2d_reset (fft2d_state_t *state);

  size_t fft2d_execute_cf64_max_out (fft2d_state_t *state);

  size_t fft2d_execute_cf64 (fft2d_state_t *state, const double complex *in,
                             size_t n_in, double complex *out);

  size_t fft2d_execute_cf32_max_out (fft2d_state_t *state);

  size_t fft2d_execute_cf32 (fft2d_state_t *state, const float complex *in,
                             size_t n_in, float complex *out);

  size_t fft2d_execute_inplace_cf64_max_out (fft2d_state_t *state);

  size_t fft2d_execute_inplace_cf64 (fft2d_state_t *state,
                                     const double complex *in, size_t n_in,
                                     double complex *out);

  size_t fft2d_execute_inplace_cf32_max_out (fft2d_state_t *state);

  size_t fft2d_execute_inplace_cf32 (fft2d_state_t *state,
                                     const float complex *in, size_t n_in,
                                     float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT2D_CORE_H */
```
