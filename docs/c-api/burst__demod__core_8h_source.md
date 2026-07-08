

# File burst\_demod\_core.h

[**File List**](files.md) **>** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the documentation of this file](burst__demod__core_8h.md)


```C++

#ifndef BURST_DEMOD_CORE_H
#define BURST_DEMOD_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "ppe/ppe_core.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    /* ── configuration ── */
    uint8_t *data_code; 
    size_t   data_sf;   
    uint8_t *acq_code;  
    size_t   acq_sf;    
    size_t   acq_reps;  
    int8_t  *sync;      
    size_t   sync_len;  
    size_t   spc;       
    double   chip_rate; 
    double   carrier_hz; 
    double   max_rate;  
    size_t   payload_len;  
    size_t   est_segments; 
    double   f0_prior;     
    size_t   start;        

    /* ── engine ── */
    ppe_state_t   *ppe;  
    float complex *part; 
    size_t         n_part;

    /* ── read-backs (after demod) ── */
    int    frame_valid;  
    size_t frame_offset; 
    size_t n_symbols;    
    double est_freq_hz;  
    double est_rate_hz;  
    double est_snr_db;   
  } burst_demod_state_t;

  burst_demod_state_t *burst_demod_create (const uint8_t *data_code,
                                           size_t data_code_len, size_t spc,
                                           double chip_rate, double carrier_hz,
                                           double max_rate, size_t payload_len,
                                           size_t est_segments);

  void burst_demod_destroy (burst_demod_state_t *state);

  void burst_demod_reset (burst_demod_state_t *state);

  void burst_demod_set_preamble (burst_demod_state_t *state,
                                 const uint8_t *acq_code, size_t acq_code_len,
                                 size_t reps);

  void burst_demod_set_sync (burst_demod_state_t *state, const uint8_t *sync,
                             size_t sync_len);

  void burst_demod_set_prior (burst_demod_state_t *state, double f0_coarse,
                              size_t start);

  size_t burst_demod_demod_max_out (burst_demod_state_t *state);

  size_t burst_demod_demod (burst_demod_state_t *state, const float complex *x,
                            size_t x_len, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DEMOD_CORE_H */
```


