#include "doppler/adc/adc_core.h"
#include "doppler/dp_complex.h"
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

  DP_TEST_END ("test_adc_core");
}
