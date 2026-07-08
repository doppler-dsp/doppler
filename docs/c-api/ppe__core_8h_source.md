

# File ppe\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**ppe**](dir_d640b2c624b0e530b2e913b3aa05ce26.md) **>** [**ppe\_core.h**](ppe__core_8h.md)

[Go to the documentation of this file](ppe__core_8h.md)


```C++

#ifndef PPE_CORE_H
#define PPE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double freq_norm; 
    double rate_norm; 
    double snr_db;    
  } ppe_result_t;

  typedef struct
  {
    size_t max_len;  
    size_t nfft;     
    double max_rate; 
    size_t n_rate;   
    double drate;    
    fft_state_t   *fft;    
    float complex *buf;    
    float complex *spec;   
    float         *mag;    
    float         *win;    
    double        *rowpk;  
    double        *rowfrq; 
  } ppe_state_t;

  ppe_state_t *ppe_create (size_t max_len, double max_rate);

  void ppe_destroy (ppe_state_t *state);

  void ppe_reset (ppe_state_t *state);

  ppe_result_t ppe_estimate (ppe_state_t *state, const float complex *in,
                             size_t n_in);

#ifdef __cplusplus
}
#endif

#endif /* PPE_CORE_H */
```


