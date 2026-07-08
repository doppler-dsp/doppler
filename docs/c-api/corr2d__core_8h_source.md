

# File corr2d\_core.h

[**File List**](files.md) **>** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md) **>** [**corr2d\_core.h**](corr2d__core_8h.md)

[Go to the documentation of this file](corr2d__core_8h.md)


```C++

#ifndef CORR2D_CORE_H
#define CORR2D_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  fft2d_state_t *fwd;       
  fft2d_state_t *inv;       
  float complex *ref_spec;  
  float complex *work_fft;  
  float complex *accum;     
  /* Decoupled-inverse scratch — allocated only when (ny_out,nx_out) differ
   * from (ny,nx); NULL on the native path.  zeropad goes accum → ztmp (rows)
   * → work_pad (cols), then inverse(work_pad). */
  float complex *work_pad;  
  float complex *ztmp;      
  float complex *zcol;      
  float complex *zcolout;   
  size_t ny;                
  size_t nx;                
  size_t n;                 
  size_t ny_out;            
  size_t nx_out;            
  size_t n_out;             
  size_t dwell;             
  size_t count;             
} corr2d_state_t;

corr2d_state_t *corr2d_create(const float complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out);

void corr2d_destroy(corr2d_state_t *state);

void corr2d_reset(corr2d_state_t *state);

void corr2d_set_ref(corr2d_state_t *state, const float complex *ref);

size_t corr2d_execute_max_out(corr2d_state_t *state);

size_t corr2d_execute(corr2d_state_t *state, const float complex *in,
                      size_t n_in, float complex *out);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * running 2-D product-spectrum accumulator + frame count;
 * FFT plans + ref_spec are config, rebuilt by create. */
#define CORR2D_STATE_MAGIC DP_FOURCC ('C','R','2','D')
#define CORR2D_STATE_VERSION 1u
size_t corr2d_state_bytes (const corr2d_state_t *state);
void corr2d_get_state (const corr2d_state_t *state, void *blob);
int corr2d_set_state (corr2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
```


