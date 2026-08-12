#include "dp_state_test.h"
#include "dp_test.h"
#include "f32_to_uq15/f32_to_uq15_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  f32_to_uq15_state_t *obj = f32_to_uq15_create (32768.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)f32_to_uq15_step (obj, 0.0f);

  /* reset */
  f32_to_uq15_reset (obj);

  f32_to_uq15_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    f32_to_uq15_state_t *a = f32_to_uq15_create (32768.0f);
    f32_to_uq15_state_t *b = f32_to_uq15_create (32768.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)f32_to_uq15_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (f32_to_uq15, a, b);
    DP_CHECK (b->clipped == a->clipped);
    f32_to_uq15_destroy (a);
    f32_to_uq15_destroy (b);
  }

  DP_TEST_END ("test_f32_to_uq15_core");
}
