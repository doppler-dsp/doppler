

# File fir\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**fir**](dir_057b0102e9f7b23965a3d08d49a38aec.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the documentation of this file](fir__core_8h.md)


```C++

#ifndef DP_FIR_CORE_H
#define DP_FIR_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"

#include "doppler/dp_complex.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float _Complex *taps;    /* complex taps  (NULL for real-tap filter)   */
    float *rtaps;           /* real taps     (NULL for complex-tap filter) */
    float _Complex *delay;   /* delay line, length num_taps - 1            */
    float _Complex *scratch; /* [delay | input] workspace, grown on demand  */
    size_t scratch_cap;
    size_t num_taps;
  } dp_fir_state_t;

  JM_FORCEINLINE JM_HOT float _Complex
  fir_step (dp_fir_state_t *s, float _Complex x)
  {
    size_t               M = s->num_taps;
    const float _Complex *d = s->delay;  /* length M-1 (NULL when M == 1) */
    const float         *h = s->rtaps;  /* real taps (fir_create_real)   */
    float                re = 0.0f, im = 0.0f;
    for (size_t k = 0; k < M; k++)
      {
        float _Complex cf = (k == 0) ? x : d[M - 1 - k];
        re += h[k] * crealf (cf);
        im += h[k] * cimagf (cf);
      }
    if (M > 1)
      {
        float _Complex *dl = s->delay; /* shift left, append x as newest */
        for (size_t i = 0; i + 2 < M; i++)
          dl[i] = dl[i + 1];
        dl[M - 2] = x;
      }
    return CMPLXF (re, im);
  }

  dp_fir_state_t *dp_fir_create (const float _Complex *taps, size_t taps_len);

  dp_fir_state_t *fir_create_real (const float *taps, size_t num_taps);

  void dp_fir_reset (dp_fir_state_t *state);

  /* Serializable state (standard bytes interface; see dp_state.h): the delay
   * line (num_taps-1 samples) after the envelope; taps/scratch are config. */
#define FIR_STATE_MAGIC DP_FOURCC ('F', 'I', 'R', '_')
#define FIR_STATE_VERSION 1u

  size_t dp_fir_state_bytes (const dp_fir_state_t *state);
  void dp_fir_get_state (const dp_fir_state_t *state, void *blob);
  int dp_fir_set_state (dp_fir_state_t *state, const void *blob);

  void dp_fir_destroy (dp_fir_state_t *state);

  size_t fir_get_num_taps (const dp_fir_state_t *state);

  int dp_fir_get_is_real (const dp_fir_state_t *state);

  double fir_dc_gain (const dp_fir_state_t *state);

  size_t dp_fir_execute_max_out (dp_fir_state_t *state);

  size_t dp_fir_execute (dp_fir_state_t *state, const float _Complex *in, size_t n_in,
                      float _Complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FIR_CORE_H */
```


