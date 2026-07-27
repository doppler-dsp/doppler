#include "ber_meter/ber_meter_core.h"
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
  int                _fails = 0;
  ber_meter_state_t *obj    = ber_meter_create (4, 200, 0.99);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  ber_meter_reset (obj);

  ber_meter_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_ber_meter_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ber_meter_core PASSED\n");
  return 0;
}
