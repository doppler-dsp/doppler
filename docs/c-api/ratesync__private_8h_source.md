

# File ratesync\_private.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ratesync**](dir_6debd7d9ad90888bb5b8a40a122d95e8.md) **>** [**ratesync\_private.h**](ratesync__private_8h.md)

[Go to the documentation of this file](ratesync__private_8h.md)


```C++

#ifndef DP_RATESYNC_PRIVATE_H
#define DP_RATESYNC_PRIVATE_H

#include "doppler/ratesync/ratesync_core.h"

/* The instant, on the input's timeline, that the terminal stage's next
   output reads. The inputs the stage has been handed and its accumulator's
   lead say where on its own timeline, the decimation ahead of it scales
   that to inputs, and the cascade's group delay is what an output lags the
   input by. Unsteered this is `outputs / rate - delay`; steered, the count
   of outputs no longer says and the accumulator still does. The cascade
   must be bound (s->loop.term). */
static inline double
dp_ratesync_next_read (const dp_ratesync_state_t *s)
{
  return s->decim
             * ((double)s->term_in + dp_resamp_get_ctrl_lead (s->loop.term))
         - s->delay;
}

#endif /* DP_RATESYNC_PRIVATE_H */
```


