#include "doppler/acc_f32/acc_f32_core.h"
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
  dp_acc_f32_state_t *obj = dp_acc_f32_create (0.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (dp_acc_f32_get_acc (obj) == 0.0f);
  dp_acc_f32_set_acc (obj, 2.0f);
  DP_CHECK (dp_acc_f32_get_acc (obj) == 2.0f);

  /* step: verify it runs without crashing */
  (void)dp_acc_f32_step (obj, 0.0f);

  /* reset restores defaults */
  dp_acc_f32_set_acc (obj, 2.0f);
  dp_acc_f32_reset (obj);
  DP_CHECK (dp_acc_f32_get_acc (obj) == 0.0f);

  dp_acc_f32_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_acc_f32_state_t *a = dp_acc_f32_create (0.0f);
    dp_acc_f32_state_t *b = dp_acc_f32_create (0.0f);
    DP_CHECK (a != NULL && b != NULL);
    dp_acc_f32_step (a, 1.5f);
    dp_acc_f32_step (a, -0.25f);
    DP_STATE_ROUNDTRIP_TEST (dp_acc_f32, a, b);
    DP_CHECK (dp_acc_f32_get_acc (b) == dp_acc_f32_get_acc (a));
    dp_acc_f32_destroy (a);
    dp_acc_f32_destroy (b);
  }

  DP_TEST_END ("test_acc_f32_core");
}
