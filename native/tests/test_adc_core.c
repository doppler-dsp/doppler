#include "doppler/adc/adc_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  dp_adc_state_t *obj = dp_adc_create (16, -10.0f, 0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)dp_adc_step (obj, 0.0f);

  /* reset */
  dp_adc_reset (obj);

  dp_adc_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_adc_state_t *a = dp_adc_create (8, 0.0f, 1);
    dp_adc_state_t *b = dp_adc_create (8, 0.0f, 1);
    DP_CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 20; i++)
      (void)dp_adc_step (a, 2.0f); /* clip + advance dither RNG */
    DP_STATE_ROUNDTRIP_TEST (dp_adc, a, b);
    DP_CHECK (b->rng == a->rng && b->clipped == a->clipped);
    DP_CHECK (dp_adc_step (b, 0.3f) == dp_adc_step (a, 0.3f));
    dp_adc_destroy (a);
    dp_adc_destroy (b);
  }

  /* #2651: steps() latches the clip flag on the value step() latches on --
     the scaled sample BEFORE rounding -- whichever path the sample takes.
     127.3 codes rounds to 127, inside the range, but it was outside it
     before rounding; the SIMD body used to latch only after llroundf. */
  {
    float x[64];
    for (int i = 0; i < 64; i++)
      x[i] = 127.3f / 128.0f;
    int64_t         y[64];
    dp_adc_state_t *one = dp_adc_create (8, 0.0f, 0);
    dp_adc_state_t *blk = dp_adc_create (8, 0.0f, 0);
    DP_REQUIRE (one && blk);
    (void)dp_adc_step (one, x[0]);
    dp_adc_steps (blk, x, y, 64);
    DP_CHECK (one->clipped == 1);
    DP_CHECK (blk->clipped == 1);
    DP_CHECK (y[0] == 127 && y[63] == 127);
    dp_adc_destroy (one);
    dp_adc_destroy (blk);
  }

  /* #2651: the TPDF dither is two INDEPENDENT uniforms -- two successive
     xorshift32 draws -- so one dithered sample advances the generator by
     exactly two steps. The second "uniform" used to be r1 * 2654435761, a
     fixed function of the first, and the generator moved one step. */
  {
    dp_adc_state_t *d = dp_adc_create (8, 0.0f, 1);
    DP_REQUIRE (d != NULL);
    uint32_t r = d->rng; /* the shared xorshift32, the ADC's own generator */
    (void)dp_xs32 (&r);
    (void)dp_xs32 (&r);
    (void)dp_adc_step (d, 0.0f);
    DP_CHECK (d->rng == r);
    dp_adc_destroy (d);
  }

  DP_TEST_END ("test_adc_core");
}
