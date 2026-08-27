

# File burst\_demod\_core.h

[**File List**](files.md) **>** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the documentation of this file](burst__demod__core_8h.md)


```C++

#ifndef BURST_DEMOD_CORE_H
#define BURST_DEMOD_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "ccsds_tm/ccsds_tm_frame.h" /* CCSDS_TM_ASM_BITS + the ops */
#include "wfm/wfm_frame.h"           /* the frame DESCRIPTION          */
#include "ppe/ppe_core.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#include "conv/conv_core.h"
#include "rs/rs_core.h"
#include "pn/pn_core.h"
#include "gold/gold_core.h"
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
    uint8_t *sync_bits; 
    uint8_t  marker[CCSDS_TM_ASM_BITS]; 
    wfm_frame_desc_t d;   
    wfm_frame_desc_layout_t lay; 
    unsigned payload_field;      
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
    int    frame_checked; 
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

  int burst_demod_set_frame (burst_demod_state_t *state, const uint8_t *sync,
                             size_t sync_len, int crc, unsigned rs_depth,
                             int randomise, int attach_asm);

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


