#include "ber_meter/ber_meter_core.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  ber_meter_state_t *obj = ber_meter_create (4, 200, 0.99);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  ber_meter_reset (obj);

  ber_meter_destroy (obj);
  DP_TEST_END ("test_ber_meter_core");
}
