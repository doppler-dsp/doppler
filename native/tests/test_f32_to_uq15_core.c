#include "doppler/dp_complex.h"
#include "doppler/f32_to_uq15/f32_to_uq15_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  dp_f32_to_uq15_state_t *obj = dp_f32_to_uq15_create (32768.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)dp_f32_to_uq15_step (obj, 0.0f);

  /* reset */
  dp_f32_to_uq15_reset (obj);

  dp_f32_to_uq15_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_f32_to_uq15_state_t *a = dp_f32_to_uq15_create (32768.0f);
    dp_f32_to_uq15_state_t *b = dp_f32_to_uq15_create (32768.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_f32_to_uq15_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (dp_f32_to_uq15, a, b);
    DP_CHECK (b->clipped == a->clipped);
    dp_f32_to_uq15_destroy (a);
    dp_f32_to_uq15_destroy (b);
  }

  /* #2645: a NaN or infinite scale is refused like zero and a negative
     one; `scale <= 0` alone let both through. */
  DP_CHECK (dp_f32_to_uq15_create (NAN) == NULL);
  DP_CHECK (dp_f32_to_uq15_create (INFINITY) == NULL);
  DP_CHECK (dp_f32_to_uq15_create (-INFINITY) == NULL);
  DP_CHECK (dp_f32_to_uq15_create (-1.0f) == NULL);

  /* #2645: a NaN sample fails both clip comparisons; it must still latch
     the flag, and it leaves as the negative full-scale code. */
  {
    dp_f32_to_uq15_state_t *nc = dp_f32_to_uq15_create (32768.0f);
    DP_REQUIRE (nc != NULL);
    uint16_t ny = dp_f32_to_uq15_step (nc, NAN);
    DP_CHECK (nc->clipped == 1);
    DP_CHECK (ny == (uint16_t)(0));
    dp_f32_to_uq15_destroy (nc);
  }

  DP_TEST_END ("test_f32_to_uq15_core");
}
