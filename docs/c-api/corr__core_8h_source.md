

# File corr\_core.h

[**File List**](files.md) **>** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md) **>** [**corr\_core.h**](corr__core_8h.md)

[Go to the documentation of this file](corr__core_8h.md)


```C++

#ifndef CORR_CORE_H
#define CORR_CORE_H

#include "clib_common.h"
#include "fft/fft_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  fft_state_t *fwd;         
  fft_state_t *inv;         
  float complex *ref_spec;  
  float complex *work_fft;  
  float complex *work_ifft; 
  float complex *accum;     
  size_t n;                 
  size_t dwell;             
  size_t count;             
} corr_state_t;

corr_state_t *corr_create(const float complex *ref, size_t n, size_t dwell,
                          int nthreads);

void corr_destroy(corr_state_t *state);

void corr_reset(corr_state_t *state);

void corr_set_ref(corr_state_t *state, const float complex *ref);

size_t corr_execute_max_out(corr_state_t *state);

size_t corr_execute(corr_state_t *state, const float complex *in, size_t n_in,
                    float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* CORR_CORE_H */
```


