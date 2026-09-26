

# File detector\_core.h

[**File List**](files.md) **>** [**detector**](dir_4cdf6fdfdd426ef1a31e056182554d6b.md) **>** [**detector\_core.h**](detector__core_8h.md)

[Go to the documentation of this file](detector__core_8h.md)


```C++

#ifndef DP_DETECTOR_CORE_H
#define DP_DETECTOR_CORE_H

#include "doppler/buffer/buffer.h"
#include "doppler/corr/corr_core.h"
#include "doppler/dp_state.h"

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
  dp_corr_state_t *corr;       
  dp_f32_t *ring;             
  float _Complex *out_buf;   
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
} dp_detector_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

dp_detector_state_t *dp_detector_create (const float _Complex *ref,
                                   size_t ref_len,
                                   size_t dwell, size_t noise_lo,
                                   size_t noise_hi,
                                   det_noise_mode_t noise_mode,
                                   float threshold, int nthreads);

void dp_detector_destroy (dp_detector_state_t *state);

void dp_detector_reset (dp_detector_state_t *state);

void detector_set_ref (dp_detector_state_t *state, const float _Complex *ref);

void detector_set_threshold (dp_detector_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t dp_detector_push (dp_detector_state_t *state, const float _Complex *in,
                      size_t n_in, det_result_t *result, size_t max_results);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * corr child + the input ring's unconsumed samples (zero-padded to ring_cap)
 * + the last-dump result fields; scratch is config (rebuilt by create). */
#define DETECTOR_STATE_MAGIC DP_FOURCC ('D','E','T','1')
#define DETECTOR_STATE_VERSION 1u
size_t dp_detector_state_bytes (const dp_detector_state_t *state);
void dp_detector_get_state (const dp_detector_state_t *state, void *blob);
int dp_detector_set_state (dp_detector_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR_CORE_H */
```


