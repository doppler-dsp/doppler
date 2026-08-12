#include "acc_f32/acc_f32_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  acc_f32_state_t *obj = acc_f32_create (0.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (acc_f32_get_acc (obj) == 0.0f);
  acc_f32_set_acc (obj, 2.0f);
  DP_CHECK (acc_f32_get_acc (obj) == 2.0f);

  /* step: verify it runs without crashing */
  (void)acc_f32_step (obj, 0.0f);

  /* reset restores defaults */
  acc_f32_set_acc (obj, 2.0f);
  acc_f32_reset (obj);
  DP_CHECK (acc_f32_get_acc (obj) == 0.0f);

  acc_f32_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    acc_f32_state_t *a = acc_f32_create (0.0f);
    acc_f32_state_t *b = acc_f32_create (0.0f);
    DP_CHECK (a != NULL && b != NULL);
    acc_f32_step (a, 1.5f);
    acc_f32_step (a, -0.25f);
    DP_STATE_ROUNDTRIP_TEST (acc_f32, a, b);
    DP_CHECK (acc_f32_get_acc (b) == acc_f32_get_acc (a));
    acc_f32_destroy (a);
    acc_f32_destroy (b);
  }

  DP_TEST_END ("test_acc_f32_core");
}
