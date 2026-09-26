#include "doppler/dp_complex.h"
#include "doppler/uq15_to_f32/uq15_to_f32_core.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  dp_uq15_to_f32_state_t *obj = dp_uq15_to_f32_create (32768.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)dp_uq15_to_f32_step (obj, 0U);

  /* reset */
  dp_uq15_to_f32_reset (obj);

  dp_uq15_to_f32_destroy (obj);
  DP_TEST_END ("test_uq15_to_f32_core");
}
