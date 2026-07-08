

# File HalfbandDecimator\_core.h

[**File List**](files.md) **>** [**HalfbandDecimator**](dir_6ac3f68ee82e011454c15c865a37e192.md) **>** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md)

[Go to the documentation of this file](HalfbandDecimator__core_8h.md)


```C++

#ifndef HALFBANDDECIMATOR_CORE_H
#define HALFBANDDECIMATOR_CORE_H

#include "hbdecim/hbdecim_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef hbdecim_state_t HalfbandDecimator_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define HBDECIM_MAX_OUT 32768

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  HalfbandDecimator_state_t *HalfbandDecimator_create (size_t num_taps,
                                                       const float *h);

  void HalfbandDecimator_destroy (HalfbandDecimator_state_t *state);

  void HalfbandDecimator_reset (HalfbandDecimator_state_t *state);

  size_t HalfbandDecimator_state_bytes (const HalfbandDecimator_state_t *state);
  void HalfbandDecimator_get_state (const HalfbandDecimator_state_t *state,
                                    void *blob);
  int HalfbandDecimator_set_state (HalfbandDecimator_state_t *state,
                                   const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  size_t HalfbandDecimator_execute_max_out (HalfbandDecimator_state_t *state);

  size_t HalfbandDecimator_execute (HalfbandDecimator_state_t *state,
                                    const float complex *x, size_t x_len,
                                    float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double HalfbandDecimator_get_rate (const HalfbandDecimator_state_t *state);

  size_t
  HalfbandDecimator_get_num_taps (const HalfbandDecimator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* HALFBANDDECIMATOR_CORE_H */
```


