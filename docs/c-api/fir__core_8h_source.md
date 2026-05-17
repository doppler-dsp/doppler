

# File fir\_core.h

[**File List**](files.md) **>** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the documentation of this file](fir__core_8h.md)


```C++

#ifndef FIR_CORE_H
#define FIR_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float complex *taps;    /* complex taps  (NULL for real-tap filter)   */
    float *rtaps;           /* real taps     (NULL for complex-tap filter) */
    float complex *delay;   /* delay line, length num_taps - 1            */
    float complex *scratch; /* [delay | input] workspace, grown on demand  */
    size_t scratch_cap;
    size_t num_taps;
  } fir_state_t;

  fir_state_t *fir_create (const float complex *taps, size_t num_taps);

  fir_state_t *fir_create_real (const float *taps, size_t num_taps);

  void fir_reset (fir_state_t *state);

  void fir_destroy (fir_state_t *state);

  size_t fir_get_num_taps (const fir_state_t *state);

  int fir_get_is_real (const fir_state_t *state);

  size_t fir_execute_max_out (fir_state_t *state);

  size_t fir_execute (fir_state_t *state, const float complex *in, size_t n_in,
                      float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FIR_CORE_H */
```


