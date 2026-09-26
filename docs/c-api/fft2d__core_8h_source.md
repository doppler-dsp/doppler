

# File fft2d\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**fft2d**](dir_384db21119e775e355fd287e1a7652d5.md) **>** [**fft2d\_core.h**](fft2d__core_8h.md)

[Go to the documentation of this file](fft2d__core_8h.md)


```C++

#ifndef DP_FFT2D_CORE_H
#define DP_FFT2D_CORE_H

#include "doppler/clib_common.h"
#include "doppler/pocketfft/pocketfft.h"

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
    double _Complex *work_trunc;
  } dp_fft2d_state_t;

  dp_fft2d_state_t *dp_fft2d_create (size_t ny, size_t nx, int sign, int nthreads);

  void dp_fft2d_destroy (dp_fft2d_state_t *state);

  void dp_fft2d_reset (dp_fft2d_state_t *state);

  size_t dp_fft2d_execute_cf64_max_out (dp_fft2d_state_t *state);

  size_t dp_fft2d_execute_cf64 (dp_fft2d_state_t *state, const double _Complex *in,
                             size_t n_in, double _Complex *out,
                             size_t max_out);

  size_t dp_fft2d_execute_cf32_max_out (dp_fft2d_state_t *state);

  size_t dp_fft2d_execute_cf32 (dp_fft2d_state_t *state, const float _Complex *in,
                             size_t n_in, float _Complex *out,
                             size_t max_out);

  size_t dp_fft2d_execute_inplace_cf64_max_out (dp_fft2d_state_t *state);

  size_t dp_fft2d_execute_inplace_cf64 (dp_fft2d_state_t *state,
                                     const double _Complex *in, size_t n_in,
                                     double _Complex *out, size_t max_out);

  size_t dp_fft2d_execute_inplace_cf32_max_out (dp_fft2d_state_t *state);

  size_t dp_fft2d_execute_inplace_cf32 (dp_fft2d_state_t *state,
                                     const float _Complex *in, size_t n_in,
                                     float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* FFT2D_CORE_H */
```


