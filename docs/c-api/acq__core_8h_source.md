

# File acq\_core.h

[**File List**](files.md) **>** [**acq**](dir_25a1e6db36731e5901b5cfb158eaa462.md) **>** [**acq\_core.h**](acq__core_8h.md)

[Go to the documentation of this file](acq__core_8h.md)


```C++

#ifndef ACQ_CORE_H
#define ACQ_CORE_H

#include "buffer/buffer.h"
#include "clib_common.h"
#include "corr2d/corr2d_core.h"
#include "detection/detection_core.h"
#include "dp_state.h"
#include "fft/fft_core.h"
#include "jm_perf.h"
/* detector2d_core.h supplies det_noise_mode_t (guarded typedef). */
#include "detector2d/detector2d_core.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t doppler_bin; 
    size_t code_phase; 
    float  peak_mag;   
    float  noise_est;  
    float  test_stat;  
    float cn0_dbhz_est; 
  } acq_result_t;

  typedef struct
  {
    corr2d_state_t *corr; 
    fft_state_t *slow_fft; 
    dp_f32_t    *ring;  
    float complex *ref; 
    float complex *yframe;  
    float complex *colbuf;  
    float complex *colout;  
    float complex *out_buf; 
    float *mag_buf;       
    float *noise_scratch; 
    float *nc_surface;    

    size_t
        doppler_bins; 
    size_t code_bins; 
    size_t n;         
    size_t sf;        
    size_t spc;       
    size_t reps;      
    size_t
        searched_bins; 
    size_t n_noncoh;   
    size_t max_noncoh; 
    size_t nc_count; 
    size_t ring_cap; 
    size_t noise_lo; 
    size_t noise_hi; 
    det_noise_mode_t noise_mode; 

    double chip_rate; 
    double fs;        
    double cn0_dbhz;  
    double
        doppler_span_hz; 
    double
        doppler_res_hz; 
    double pfa; 
    double doppler_uncertainty; 
    double symbol_rate; 
    double epochs_per_symbol; 
    double doppler_resolution; 
    double doppler_rate; 

    float  threshold; 
    float  eta;       
    float  eta_nc;    
    double pfa_cell;  
    double pd;        
    double pd_predicted;  
    double straddle_loss; 
    uint8_t underpowered; 

    uint64_t
        samples_consumed; 

    /* Last-dump bookkeeping (for inspection). */
    size_t peak_row;
    size_t peak_col;
    float  peak_mag;
    float  noise_est;
    float  test_stat;
  } acq_state_t;

  typedef struct
  {
    uint16_t has_nc; 
    uint16_t _pad;
    uint32_t n_noncoh;         
    uint64_t n;                
    uint64_t samples_consumed; 
    uint32_t nc_count;     
    uint32_t n_unconsumed; 
  } acq_extra_t;

#define ACQ_STATE_MAGIC DP_FOURCC ('A', 'C', 'Q', 'R')
#define ACQ_STATE_VERSION 1u

  acq_state_t *acq_create (const uint8_t *code, size_t code_len, size_t reps,
                           size_t spc, double chip_rate, double cn0_dbhz,
                           double doppler_uncertainty, double pfa, double pd,
                           int noise_mode, size_t max_noncoh,
                           double symbol_rate, double doppler_resolution,
                           double doppler_rate);

  void acq_destroy (acq_state_t *state);

  void acq_reset (acq_state_t *state);

  int acq_configure_search_raw (acq_state_t *state, size_t doppler_bins,
                                size_t n_noncoh);

  size_t acq_push (acq_state_t *state, const float complex *in, size_t n_in,
                   acq_result_t *result, size_t max_results);

  /* ── Serializable state — the elastic / pure-transducer face
   * ─────────────────
   *
   * The OO engine above is convenient but stateful.  These match the rest of
   * the library's serializable objects (lo/cic/fir/ddcr): serialize a
   * channel's cross-call state to a flat POD, ship (descriptor, state, input)
   * to any thread/process/pod, rebuild the engine from the descriptor
   * (acq_create), inject the state, and continue — bit-identical to an
   * uninterrupted run.
   */

  size_t acq_state_bytes (const acq_state_t *state);

  void acq_get_state (const acq_state_t *state, void *blob);

  int acq_set_state (acq_state_t *state, const void *blob);

  size_t acq_run (acq_state_t *state, const void *state_in, void *state_out,
                  const float complex *in, size_t n_in, acq_result_t *result,
                  size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* ACQ_CORE_H */
```


