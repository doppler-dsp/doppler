

# File async\_dsss\_receiver\_private.h

[**File List**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_private.h**](async__dsss__receiver__private_8h.md)

[Go to the documentation of this file](async__dsss__receiver__private_8h.md)


```C++

#ifndef ASYNC_DSSS_RECEIVER_PRIVATE_H
#define ASYNC_DSSS_RECEIVER_PRIVATE_H

#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"

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

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_PRIVATE_H */
```


