

# File fft\_core.h

[**File List**](files.md) **>** [**fft**](dir_5dc24668fb1cbe963321608da9e9d4ca.md) **>** [**fft\_core.h**](fft__core_8h.md)

[Go to the documentation of this file](fft__core_8h.md)


```C++

#ifndef FFT_CORE_H
#define FFT_CORE_H

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
    size_t n;                 
    int sign;                 
  } fft_state_t;

  fft_state_t *fft_create (size_t n, int sign, int nthreads);

  void fft_destroy (fft_state_t *state);

  void fft_reset (fft_state_t *state);

  size_t fft_execute_cf64_max_out (fft_state_t *state);

  size_t fft_execute_cf64 (fft_state_t *state, const double complex *in,
                           size_t n_in, double complex *out);

  size_t fft_execute_cf32_max_out (fft_state_t *state);

  size_t fft_execute_cf32 (fft_state_t *state, const float complex *in,
                           size_t n_in, float complex *out);

  size_t fft_execute_inplace_cf64_max_out (fft_state_t *state);

  size_t fft_execute_inplace_cf64 (fft_state_t *state,
                                   const double complex *in, size_t n_in,
                                   double complex *out);

  size_t fft_execute_inplace_cf32_max_out (fft_state_t *state);

  size_t fft_execute_inplace_cf32 (fft_state_t *state, const float complex *in,
                                   size_t n_in, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT_CORE_H */
```


