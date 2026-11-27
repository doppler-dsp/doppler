/**
 * @file ratesync_private.h
 * @brief Where the terminal stage's next output reads the input.
 *
 * Not part of the public API. dp_ratesync_seed() places an output by this
 * mapping, and the harnesses that measure an output's timing error read it
 * back (native/tests/ratesync/ratesync_settle_test.h), so there is one
 * statement of it and a change to it moves both.
 */
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
