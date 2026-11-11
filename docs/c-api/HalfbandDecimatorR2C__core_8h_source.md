

# File HalfbandDecimatorR2C\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**HalfbandDecimatorR2C**](dir_de85e40507cd80906a5f603b94b719b5.md) **>** [**HalfbandDecimatorR2C\_core.h**](HalfbandDecimatorR2C__core_8h.md)

[Go to the documentation of this file](HalfbandDecimatorR2C__core_8h.md)


```C++

#ifndef DP_HALFBANDDECIMATORR2C_CORE_H
#define DP_HALFBANDDECIMATORR2C_CORE_H

#include "doppler/hbdecim/hbdecim_r2c_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef hbdecim_r2c_state_t dp_HalfbandDecimatorR2C_state_t;

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  dp_HalfbandDecimatorR2C_state_t *
  dp_HalfbandDecimatorR2C_create (const float *h, size_t h_len);

  void dp_HalfbandDecimatorR2C_destroy (dp_HalfbandDecimatorR2C_state_t *state);

  void dp_HalfbandDecimatorR2C_reset (dp_HalfbandDecimatorR2C_state_t *state);

  size_t
  dp_HalfbandDecimatorR2C_state_bytes (const dp_HalfbandDecimatorR2C_state_t *state);

  void
  dp_HalfbandDecimatorR2C_get_state (const dp_HalfbandDecimatorR2C_state_t *state,
                                     void *blob);

  int dp_HalfbandDecimatorR2C_set_state (dp_HalfbandDecimatorR2C_state_t *state,
                                         const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  size_t
  dp_HalfbandDecimatorR2C_execute_max_out (dp_HalfbandDecimatorR2C_state_t *state,
                                           size_t x_len);

  size_t dp_HalfbandDecimatorR2C_execute (dp_HalfbandDecimatorR2C_state_t *state,
                                          const float *x, size_t x_len,
                                          float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double
  dp_HalfbandDecimatorR2C_get_rate (const dp_HalfbandDecimatorR2C_state_t *state);

  size_t
  dp_HalfbandDecimatorR2C_get_num_taps (const dp_HalfbandDecimatorR2C_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DP_HALFBANDDECIMATORR2C_CORE_H */
```


