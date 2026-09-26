#include "doppler/acc_cf64/acc_cf64_core.h"
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
  dp_acc_cf64_state_t *obj = dp_acc_cf64_create (0.0 + 0.0 * I);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (dp_acc_cf64_get_acc (obj) == 0.0 + 0.0 * I);
  dp_acc_cf64_set_acc (obj, 2.0 + 0.0 * I);
  DP_CHECK (dp_acc_cf64_get_acc (obj) == 2.0 + 0.0 * I);

  /* step: verify it runs without crashing */
  (void)dp_acc_cf64_step (obj, 0.0 + 0.0 * I);

  /* reset restores defaults */
  dp_acc_cf64_set_acc (obj, 2.0 + 0.0 * I);
  dp_acc_cf64_reset (obj);
  DP_CHECK (dp_acc_cf64_get_acc (obj) == 0.0 + 0.0 * I);

  dp_acc_cf64_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_acc_cf64_state_t *a = dp_acc_cf64_create (0.0 + 0.0 * I);
    dp_acc_cf64_state_t *b = dp_acc_cf64_create (0.0 + 0.0 * I);
    DP_CHECK (a != NULL && b != NULL);
    dp_acc_cf64_step (a, 3.0 + 1.0 * I);
    dp_acc_cf64_step (a, -1.0 + 2.0 * I);
    DP_STATE_ROUNDTRIP_TEST (dp_acc_cf64, a, b);
    DP_CHECK (dp_acc_cf64_get_acc (b) == dp_acc_cf64_get_acc (a));
    dp_acc_cf64_destroy (a);
    dp_acc_cf64_destroy (b);
  }

  DP_TEST_END ("test_acc_cf64_core");
}
