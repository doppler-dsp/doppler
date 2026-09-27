

# File ppe\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ppe**](dir_2835f10376dc04900139beed5b4d2457.md) **>** [**ppe\_core.h**](ppe__core_8h.md)

[Go to the documentation of this file](ppe__core_8h.md)


```C++

#ifndef DP_PPE_CORE_H
#define DP_PPE_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/fft/fft_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/dp_complex.h"
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
    dp_fft_state_t   *fft;    
    float _Complex *buf;    
    float _Complex *spec;   
    float         *mag;    
    float         *win;    
    double        *rowpk;  
    double        *rowfrq; 
  } dp_ppe_state_t;

  dp_ppe_state_t *dp_ppe_create (size_t max_len, double max_rate);

  void dp_ppe_destroy (dp_ppe_state_t *state);

  void dp_ppe_reset (dp_ppe_state_t *state);

  ppe_result_t dp_ppe_estimate (dp_ppe_state_t *state, const float _Complex *x,
                             size_t n_in);

#ifdef __cplusplus
}
#endif

#endif /* PPE_CORE_H */
```


