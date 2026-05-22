

# File detector\_core.h

[**File List**](files.md) **>** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md) **>** [**detector\_core.h**](detector__core_8h.md)

[Go to the documentation of this file](detector__core_8h.md)


```C++

#ifndef DETECTOR_CORE_H
#define DETECTOR_CORE_H

#include "buffer/buffer.h"
#include "corr/corr_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Noise aggregation mode ─────────────────────────────────────────────── */

#ifndef DET_NOISE_MODE_T_DEFINED
#define DET_NOISE_MODE_T_DEFINED
typedef enum
{
  DET_NOISE_MEAN = 0,   
  DET_NOISE_MEDIAN = 1, 
  DET_NOISE_MIN = 2,    
  DET_NOISE_MAX = 3,    
} det_noise_mode_t;
#endif /* DET_NOISE_MODE_T_DEFINED */

/* ── Per-detection result ───────────────────────────────────────────────── */

typedef struct
{
  size_t lag;       
  float peak_mag;   
  float noise_est;  
  float test_stat;  
} det_result_t;

/* ── Detector state ─────────────────────────────────────────────────────── */

typedef struct
{
  corr_state_t *corr;       
  dp_f32_t *ring;             
  float complex *out_buf;   
  float *mag_buf;           
  float *noise_scratch;     
  size_t n;                 
  size_t ring_cap;          
  size_t noise_lo;          
  size_t noise_hi;          
  det_noise_mode_t noise_mode;
  float threshold;          
  /* Last dump results — updated on every dump regardless of threshold. */
  size_t peak_lag;
  float peak_mag;
  float noise_est;
  float test_stat;
  int _last_corr_valid;     
} detector_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

detector_state_t *detector_create (const float complex *ref, size_t n,
                                   size_t dwell, size_t noise_lo,
                                   size_t noise_hi,
                                   det_noise_mode_t noise_mode,
                                   float threshold, int nthreads);

void detector_destroy (detector_state_t *state);

void detector_reset (detector_state_t *state);

void detector_set_ref (detector_state_t *state, const float complex *ref);

void detector_set_threshold (detector_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t detector_push (detector_state_t *state, const float complex *in,
                      size_t n_in, det_result_t *result, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR_CORE_H */
```


