#include "doppler/acc_q15/acc_q15_core.h"
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
  dp_acc_q15_state_t *obj = dp_acc_q15_create (0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (dp_acc_q15_get_acc (obj) == 0);
  dp_acc_q15_set_acc (obj, 2);
  DP_CHECK (dp_acc_q15_get_acc (obj) == 2);

  /* step: verify it runs without crashing */
  (void)dp_acc_q15_step (obj, 0);

  /* reset restores defaults */
  dp_acc_q15_set_acc (obj, 2);
  dp_acc_q15_reset (obj);
  DP_CHECK (dp_acc_q15_get_acc (obj) == 0);

  dp_acc_q15_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_acc_q15_state_t *a = dp_acc_q15_create (0);
    dp_acc_q15_state_t *b = dp_acc_q15_create (0);
    DP_CHECK (a != NULL && b != NULL);
    dp_acc_q15_step (a, (int16_t)1234);
    dp_acc_q15_step (a, (int16_t)-567);
    DP_STATE_ROUNDTRIP_TEST (dp_acc_q15, a, b);
    DP_CHECK (dp_acc_q15_get_acc (b) == dp_acc_q15_get_acc (a));
    dp_acc_q15_destroy (a);
    dp_acc_q15_destroy (b);
  }

  DP_TEST_END ("test_acc_q15_core");
}
