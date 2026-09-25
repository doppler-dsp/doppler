

# File specan\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**specan**](dir_6ce576ad24803d600633e2545d7ab991.md) **>** [**specan\_core.h**](specan__core_8h.md)

[Go to the documentation of this file](specan__core_8h.md)


```C++

#ifndef SPECAN_CORE_H
#define SPECAN_CORE_H

#include "doppler/ddc/ddc_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/dp_state.h"
#include "doppler/dp_complex.h"
#include <stddef.h>
#include "doppler/lo/lo_core.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/agc/agc_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    ddc_state_t   *ddc;      
    psd_state_t *psd;      
    float _Complex *scratch;  
    size_t scratch_cap;      
    float _Complex *pend;     
    size_t         pend_len; 
    size_t         pend_cap; 
    float         *pwr;   
    double         fs_in; 
    double src_center;    
    double center;        
    double span;          
    double rbw;           
    double offset_db;     
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
                                 double offset_db, double full_scale,
                                 size_t bits, int window, size_t navg);

  void specan_destroy (specan_state_t *state);

  void specan_reset (specan_state_t *state);

  size_t specan_execute_max_out (specan_state_t *state);

  size_t specan_execute (specan_state_t *state, const float _Complex *x,
                         size_t x_len, float *out, size_t max_out);

  void specan_retune (specan_state_t *state, double center);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * ddc + psd children + the pending decimated samples (sized to n*navg);
 * display/rate config restored by create. */
#define SPECAN_STATE_MAGIC DP_FOURCC ('S','P','A','N')
#define SPECAN_STATE_VERSION 1u
size_t specan_state_bytes (const specan_state_t *state);
void specan_get_state (const specan_state_t *state, void *blob);
int specan_set_state (specan_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* SPECAN_CORE_H */
```


