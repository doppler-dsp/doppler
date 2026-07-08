

# File fir\_core.h

[**File List**](files.md) **>** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the documentation of this file](fir__core_8h.md)


```C++

#ifndef FIR_CORE_H
#define FIR_CORE_H

#include "clib_common.h"
#include "dp_state.h"
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

  JM_FORCEINLINE JM_HOT float complex
  fir_step (fir_state_t *s, float complex x)
  {
    size_t               M = s->num_taps;
    const float complex *d = s->delay;  /* length M-1 (NULL when M == 1) */
    const float         *h = s->rtaps;  /* real taps (fir_create_real)   */
    float                re = 0.0f, im = 0.0f;
    for (size_t k = 0; k < M; k++)
      {
        float complex cf = (k == 0) ? x : d[M - 1 - k];
        re += h[k] * crealf (cf);
        im += h[k] * cimagf (cf);
      }
    if (M > 1)
      {
        float complex *dl = s->delay; /* shift left, append x as newest */
        for (size_t i = 0; i + 2 < M; i++)
          dl[i] = dl[i + 1];
        dl[M - 2] = x;
      }
    return CMPLXF (re, im);
  }

  fir_state_t *fir_create (const float complex *taps, size_t num_taps);

  fir_state_t *fir_create_real (const float *taps, size_t num_taps);

  void fir_reset (fir_state_t *state);

  /* Serializable state (standard bytes interface; see dp_state.h): the delay
   * line (num_taps-1 samples) after the envelope; taps/scratch are config. */
#define FIR_STATE_MAGIC DP_FOURCC ('F', 'I', 'R', '_')
#define FIR_STATE_VERSION 1u

  size_t fir_state_bytes (const fir_state_t *state);
  void fir_get_state (const fir_state_t *state, void *blob);
  int fir_set_state (fir_state_t *state, const void *blob);

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


