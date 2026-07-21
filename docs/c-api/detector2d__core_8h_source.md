

# File detector2d\_core.h

[**File List**](files.md) **>** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md) **>** [**detector2d\_core.h**](detector2d__core_8h.md)

[Go to the documentation of this file](detector2d__core_8h.md)


```C++

#ifndef DETECTOR2D_CORE_H
#define DETECTOR2D_CORE_H

#include "buffer/buffer.h"
#include "corr2d/corr2d_core.h"
#include "dp_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Noise aggregation mode (shared definition with detector_core.h) ────── */

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
  size_t row;       
  size_t col;       
  float peak_mag;   
  float noise_est;  
  float test_stat;  
} det_result2d_t;

/* ── Detector2D state ───────────────────────────────────────────────────── */

typedef struct
{
  corr2d_state_t *corr;     
  dp_f32_t *ring;             
  float complex *out_buf;   
  float *mag_buf;           
  float *noise_scratch;     
  size_t ny;                
  size_t nx;                
  size_t n;                 
  size_t ring_cap;          
  size_t noise_lo;          
  size_t noise_hi;          
  det_noise_mode_t noise_mode;
  float threshold;          
  /* Last dump results — updated on every dump regardless of threshold. */
  size_t peak_row;
  size_t peak_col;
  float peak_mag;
  float noise_est;
  float test_stat;
  int _last_corr_valid;     
} detector2d_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

detector2d_state_t *detector2d_create (const float complex *ref, size_t ny,
                                       size_t nx, size_t dwell,
                                       size_t noise_lo, size_t noise_hi,
                                       det_noise_mode_t noise_mode,
                                       float threshold, int nthreads);

void detector2d_destroy (detector2d_state_t *state);

void detector2d_reset (detector2d_state_t *state);

int detector2d_set_ref (detector2d_state_t *state, const float complex *ref);

void detector2d_set_threshold (detector2d_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t detector2d_push (detector2d_state_t *state, const float complex *in,
                        size_t n_in, det_result2d_t *result,
                        size_t max_results);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * corr2d child + the input ring's unconsumed samples (zero-padded to ring_cap)
 * + the last-dump result fields; scratch is config (rebuilt by create). */
#define DETECTOR2D_STATE_MAGIC DP_FOURCC ('D','E','T','2')
#define DETECTOR2D_STATE_VERSION 1u
size_t detector2d_state_bytes (const detector2d_state_t *state);
void detector2d_get_state (const detector2d_state_t *state, void *blob);
int detector2d_set_state (detector2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR2D_CORE_H */
```


