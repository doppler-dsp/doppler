

# File corr\_core.h

[**File List**](files.md) **>** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md) **>** [**corr\_core.h**](corr__core_8h.md)

[Go to the documentation of this file](corr__core_8h.md)


```C++

#ifndef CORR_CORE_H
#define CORR_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "fft/fft_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  fft_state_t *fwd;         
  fft_state_t *inv;         
  float complex *ref_spec;  
  float complex *work_fft;  
  float complex *accum;     
  float complex *work_pad;  
  size_t n;                 
  size_t n_out;             
  size_t dwell;             
  size_t count;             
} corr_state_t;

corr_state_t *corr_create(const float complex *ref, size_t n, size_t dwell,
                          int nthreads, size_t n_out);

void corr_destroy(corr_state_t *state);

void corr_reset(corr_state_t *state);

void corr_set_ref(corr_state_t *state, const float complex *ref);

size_t corr_execute_max_out(corr_state_t *state);

size_t corr_execute(corr_state_t *state, const float complex *in, size_t n_in,
                    float complex *out);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * running product-spectrum accumulator + frame count;
 * FFT plans + ref_spec are config, rebuilt by create. */
#define CORR_STATE_MAGIC DP_FOURCC ('C','O','R','R')
#define CORR_STATE_VERSION 1u
size_t corr_state_bytes (const corr_state_t *state);
void corr_get_state (const corr_state_t *state, void *blob);
int corr_set_state (corr_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CORR_CORE_H */
```


