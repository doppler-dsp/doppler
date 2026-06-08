#include "acc_q8/acc_q8_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

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
  int             _fails = 0;
  acc_q8_state_t *obj    = acc_q8_create (0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* acc: getter / setter */
  CHECK (acc_q8_get_acc (obj) == 0);
  acc_q8_set_acc (obj, 2);
  CHECK (acc_q8_get_acc (obj) == 2);

  /* step: verify it runs without crashing */
  (void)acc_q8_step (obj, 0);

  /* reset restores defaults */
  acc_q8_set_acc (obj, 2);
  acc_q8_reset (obj);
  CHECK (acc_q8_get_acc (obj) == 0);

  acc_q8_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_acc_q8_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acc_q8_core PASSED\n");
  return 0;
}
