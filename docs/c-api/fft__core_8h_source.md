

# File fft\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**fft**](dir_1dec96a47631eec21a10469aab3e9a96.md) **>** [**fft\_core.h**](fft__core_8h.md)

[Go to the documentation of this file](fft__core_8h.md)


```C++

#ifndef DP_FFT_CORE_H
#define DP_FFT_CORE_H

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
    size_t n;                 
    int sign;                 
    double _Complex *work_trunc;
  } dp_fft_state_t;

  dp_fft_state_t *dp_fft_create (size_t n, int sign, int nthreads);

  void dp_fft_destroy (dp_fft_state_t *state);

  void dp_fft_reset (dp_fft_state_t *state);

  size_t dp_fft_execute_cf64_max_out (dp_fft_state_t *state);

  size_t dp_fft_execute_cf64 (dp_fft_state_t *state, const double _Complex *in,
                           size_t n_in, double _Complex *out, size_t max_out);

  size_t dp_fft_execute_cf32_max_out (dp_fft_state_t *state);

  size_t dp_fft_execute_cf32 (dp_fft_state_t *state, const float _Complex *in,
                           size_t n_in, float _Complex *out, size_t max_out);

  size_t dp_fft_execute_inplace_cf64_max_out (dp_fft_state_t *state);

  size_t dp_fft_execute_inplace_cf64 (dp_fft_state_t *state,
                                   const double _Complex *in, size_t n_in,
                                   double _Complex *out, size_t max_out);

  size_t dp_fft_execute_inplace_cf32_max_out (dp_fft_state_t *state);

  size_t dp_fft_execute_inplace_cf32 (dp_fft_state_t *state, const float _Complex *in,
                                   size_t n_in, float _Complex *out,
                                   size_t max_out);

  size_t fft_execute_ci16_max_out (dp_fft_state_t *state);

  size_t fft_execute_ci16 (dp_fft_state_t *state, const int16_t *in, size_t n_in,
                           float _Complex *out);

  size_t fft_execute_ci8_max_out (dp_fft_state_t *state);

  size_t fft_execute_ci8 (dp_fft_state_t *state, const int8_t *in, size_t n_in,
                          float _Complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT_CORE_H */
```


