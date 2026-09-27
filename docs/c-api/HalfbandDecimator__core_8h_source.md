

# File HalfbandDecimator\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**HalfbandDecimator**](dir_7d0e752fb42c448bafa7d80e7fc4aaf0.md) **>** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md)

[Go to the documentation of this file](HalfbandDecimator__core_8h.md)


```C++

#ifndef DP_HALFBANDDECIMATOR_CORE_H
#define DP_HALFBANDDECIMATOR_CORE_H

#include "doppler/hbdecim/hbdecim_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef hbdecim_state_t dp_HalfbandDecimator_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define HBDECIM_MAX_OUT 32768

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  dp_HalfbandDecimator_state_t *dp_HalfbandDecimator_create (const float *h,
                                                       size_t h_len);

  void dp_HalfbandDecimator_destroy (dp_HalfbandDecimator_state_t *state);

  void dp_HalfbandDecimator_reset (dp_HalfbandDecimator_state_t *state);

  size_t dp_HalfbandDecimator_state_bytes (const dp_HalfbandDecimator_state_t *state);
  void dp_HalfbandDecimator_get_state (const dp_HalfbandDecimator_state_t *state,
                                    void *blob);
  int dp_HalfbandDecimator_set_state (dp_HalfbandDecimator_state_t *state,
                                   const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  size_t dp_HalfbandDecimator_execute_max_out (dp_HalfbandDecimator_state_t *state);

  size_t dp_HalfbandDecimator_execute (dp_HalfbandDecimator_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double dp_HalfbandDecimator_get_rate (const dp_HalfbandDecimator_state_t *state);

  size_t
  dp_HalfbandDecimator_get_num_taps (const dp_HalfbandDecimator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* HALFBANDDECIMATOR_CORE_H */
```


