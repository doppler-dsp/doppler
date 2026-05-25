

# File Resampler\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**Resampler**](dir_6dca75203c5d2d5de468e6acc97392e7.md) **>** [**Resampler\_core.h**](Resampler__core_8h.md)

[Go to the documentation of this file](Resampler__core_8h.md)


```C++

#ifndef RESAMPLER_CORE_H
#define RESAMPLER_CORE_H

#include "resamp/resamp_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef resamp_state_t Resampler_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define RESAMPLER_MAX_OUT 65536

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  Resampler_state_t *Resampler_create (double rate);

  Resampler_state_t *Resampler_create_custom (size_t num_phases,
                                              size_t num_taps,
                                              const float *bank,
                                              double rate);

  void Resampler_destroy (Resampler_state_t *state);

  void Resampler_reset (Resampler_state_t *state);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  size_t Resampler_execute_max_out (Resampler_state_t *state);

  size_t Resampler_execute (Resampler_state_t *state, const float complex *x,
                            size_t x_len, float complex *out);

  size_t Resampler_execute_ctrl_max_out (Resampler_state_t *state);

  size_t Resampler_execute_ctrl (Resampler_state_t *state,
                                 const float complex *x, size_t x_len,
                                 const float complex *ctrl, size_t ctrl_len,
                                 float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double Resampler_get_rate (const Resampler_state_t *state);
  void Resampler_set_rate (Resampler_state_t *state, double rate);
  size_t Resampler_get_num_phases (const Resampler_state_t *state);
  size_t Resampler_get_num_taps (const Resampler_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLER_CORE_H */
```


