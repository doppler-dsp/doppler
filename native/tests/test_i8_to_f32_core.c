#include "dp_test.h"
#include "i8_to_f32/i8_to_f32_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  DP_CHECK (i8_to_f32_create (0.0f) == NULL);
  DP_CHECK (i8_to_f32_create (-1.0f) == NULL);
  i8_to_f32_state_t *obj = i8_to_f32_create (128.0f);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* full-scale boundaries: +max -> ~+1, -min -> -1, 0 -> 0 */
  DP_CHECK (dp_nearf (i8_to_f32_step (obj, INT8_MAX), 127.0f / 128.0f, 1e-6f));
  DP_CHECK (dp_nearf (i8_to_f32_step (obj, INT8_MIN), -1.0f, 1e-6f));
  DP_CHECK (dp_nearf (i8_to_f32_step (obj, 0), 0.0f, 1e-7f));

  /* reset */
  i8_to_f32_reset (obj);

  i8_to_f32_destroy (obj);
  DP_TEST_END ("test_i8_to_f32_core");
}
