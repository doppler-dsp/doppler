

# File async\_dsss\_receiver\_private.h

[**File List**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_private.h**](async__dsss__receiver__private_8h.md)

[Go to the documentation of this file](async__dsss__receiver__private_8h.md)


```C++

#ifndef ASYNC_DSSS_RECEIVER_PRIVATE_H
#define ASYNC_DSSS_RECEIVER_PRIVATE_H

#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/correlator/correlator_core.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

int dp_async_dsss_receiver_set_pooled (dp_async_dsss_receiver_state_t *state,
                                       bool on);

int dp_async_dsss_receiver_set_search_only (
    dp_async_dsss_receiver_state_t *state, size_t n_picks);

size_t dp_async_dsss_receiver_take_picks (
    dp_async_dsss_receiver_state_t *state, double *chip_phase,
    double *doppler_hz, double *cn0_dbhz, size_t max);

static inline int
dp__adsss_same_code_phase (double a, double b, double code_len)
{
  double d = fabs (a - b);
  if (d > code_len / 2.0)
    d = code_len - d;
  return d <= DP_CORRELATOR_SAME_PHASE_CHIPS;
}

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_PRIVATE_H */
```


