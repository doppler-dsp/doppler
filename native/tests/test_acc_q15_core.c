#include "acc_q15/acc_q15_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return _almost_eq (crealf (a), crealf (b), tol)
         && _almost_eq (cimagf (a), cimagf (b), tol);
}
#define ALMOST_EQ(a, b, tol) _almost_eq ((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol)                                                \
  _almost_eq_c ((float complex) (a), (float complex) (b), tol)

int
main (void)
{
  acc_q15_state_t *obj = acc_q15_create (0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  DP_CHECK (acc_q15_get_acc (obj) == 0);
  acc_q15_set_acc (obj, 2);
  DP_CHECK (acc_q15_get_acc (obj) == 2);

  /* step: verify it runs without crashing */
  (void)acc_q15_step (obj, 0);

  /* reset restores defaults */
  acc_q15_set_acc (obj, 2);
  acc_q15_reset (obj);
  DP_CHECK (acc_q15_get_acc (obj) == 0);

  acc_q15_destroy (obj);
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    acc_q15_state_t *a = acc_q15_create (0);
    acc_q15_state_t *b = acc_q15_create (0);
    DP_CHECK (a != NULL && b != NULL);
    acc_q15_step (a, (int16_t)1234);
    acc_q15_step (a, (int16_t)-567);
    DP_STATE_ROUNDTRIP_TEST (acc_q15, a, b);
    DP_CHECK (acc_q15_get_acc (b) == acc_q15_get_acc (a));
    acc_q15_destroy (a);
    acc_q15_destroy (b);
  }

  DP_TEST_END ("test_acc_q15_core");
}
