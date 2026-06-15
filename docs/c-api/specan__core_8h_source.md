

# File specan\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**specan**](dir_6d702d949620e4073485867cfd9038e4.md) **>** [**specan\_core.h**](specan__core_8h.md)

[Go to the documentation of this file](specan__core_8h.md)


```C++

#ifndef SPECAN_CORE_H
#define SPECAN_CORE_H

#include "ddc/ddc_core.h"
#include "welch/welch_core.h"
#include <complex.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "acc_trace/acc_trace_core.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    ddc_state_t   *ddc;      
    welch_state_t *psd;      
    float complex *scratch;  
    size_t scratch_cap;      
    float complex *pend;     
    size_t         pend_len; 
    size_t         pend_cap; 
    float         *pwr;   
    double         fs_in; 
    double src_center;    
    double center;        
    double span;          
    double rbw;           
    double ref_db;        
    double fs_out;        
    double beta;          
    size_t n;             
    size_t nfft;          
    size_t navg;          
    size_t disp_n;        
    size_t disp_lo;       
  } specan_state_t;

  specan_state_t *specan_create (double fs, double span, double rbw,
                                 double src_center, double center,
                                 double ref_db, int window, size_t navg);

  void specan_destroy (specan_state_t *state);

  void specan_reset (specan_state_t *state);

  size_t specan_execute_max_out (specan_state_t *state);

  size_t specan_execute (specan_state_t *state, const float complex *x,
                         size_t x_len, float *out, size_t max_out);

  void specan_retune (specan_state_t *state, double center);

#ifdef __cplusplus
}
#endif

#endif /* SPECAN_CORE_H */
```


