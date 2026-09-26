

# File Resampler\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**Resampler**](dir_9b0990bb8296ade48d8f038050fb64f1.md) **>** [**Resampler\_core.h**](Resampler__core_8h.md)

[Go to the documentation of this file](Resampler__core_8h.md)


```C++

#ifndef DP_RESAMPLER_CORE_H
#define DP_RESAMPLER_CORE_H

#include "doppler/resamp/resamp_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef resamp_state_t dp_Resampler_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define RESAMPLER_MAX_OUT 65536

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  dp_Resampler_state_t *dp_Resampler_create (double rate);

  dp_Resampler_state_t *Resampler_create_custom (size_t num_phases,
                                              size_t num_taps,
                                              const float *bank,
                                              double rate);

  void dp_Resampler_destroy (dp_Resampler_state_t *state);

  void dp_Resampler_reset (dp_Resampler_state_t *state);

  size_t dp_Resampler_state_bytes (const dp_Resampler_state_t *state);
  void dp_Resampler_get_state (const dp_Resampler_state_t *state, void *blob);
  int dp_Resampler_set_state (dp_Resampler_state_t *state, const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  size_t dp_Resampler_execute_max_out (dp_Resampler_state_t *state);

  size_t dp_Resampler_execute (dp_Resampler_state_t *state, const float _Complex *x,
                            size_t x_len, float _Complex *out,
                            size_t max_out);

  size_t dp_Resampler_execute_ctrl_max_out (dp_Resampler_state_t *state);

  size_t dp_Resampler_execute_ctrl (dp_Resampler_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 const double *ctrl, size_t ctrl_len,
                                 float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double dp_Resampler_get_rate (const dp_Resampler_state_t *state);
  void dp_Resampler_set_rate (dp_Resampler_state_t *state, double rate);

  double dp_Resampler_get_ctrl_acc (const dp_Resampler_state_t *state);

  size_t dp_Resampler_get_num_phases (const dp_Resampler_state_t *state);

  size_t dp_Resampler_get_num_taps (const dp_Resampler_state_t *state);

  double dp_Resampler_get_delay (const dp_Resampler_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLER_CORE_H */
```


