#include "doppler/acc_q8/acc_q8_core.h"
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
  dp_acc_q8_state_t *obj = dp_acc_q8_create (0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (dp_acc_q8_get_acc (obj) == 0);
  dp_acc_q8_set_acc (obj, 2);
  DP_CHECK (dp_acc_q8_get_acc (obj) == 2);

  /* step: verify it runs without crashing */
  (void)dp_acc_q8_step (obj, 0);

  /* reset restores defaults */
  dp_acc_q8_set_acc (obj, 2);
  dp_acc_q8_reset (obj);
  DP_CHECK (dp_acc_q8_get_acc (obj) == 0);

  dp_acc_q8_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_acc_q8_state_t *a = dp_acc_q8_create (0);
    dp_acc_q8_state_t *b = dp_acc_q8_create (0);
    DP_CHECK (a != NULL && b != NULL);
    dp_acc_q8_step (a, (int8_t)42);
    dp_acc_q8_step (a, (int8_t)-13);
    DP_STATE_ROUNDTRIP_TEST (dp_acc_q8, a, b);
    DP_CHECK (dp_acc_q8_get_acc (b) == dp_acc_q8_get_acc (a));
    dp_acc_q8_destroy (a);
    dp_acc_q8_destroy (b);
  }

  DP_TEST_END ("test_acc_q8_core");
}
