

# File lo\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the documentation of this file](lo__core_8h.md)


```C++

#ifndef LO_CORE_H
#define LO_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } lo_state_t;

  lo_state_t *lo_create (double norm_freq);

  void lo_destroy (lo_state_t *state);

  void lo_reset (lo_state_t *state);

  /* ---- Properties ---- */

  double lo_get_norm_freq (const lo_state_t *state);
  void lo_set_norm_freq (lo_state_t *state, double norm_freq);

  uint32_t lo_get_phase (const lo_state_t *state);
  void lo_set_phase (lo_state_t *state, uint32_t phase);

  uint32_t lo_get_phase_inc (const lo_state_t *state);

  /* ---- Block generators ---- */

  size_t lo_steps_max_out (lo_state_t *state);

  size_t lo_steps (lo_state_t *state, size_t n, float complex *out);

  size_t lo_steps_ctrl_max_out (lo_state_t *state);

  size_t lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
                        float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
```


