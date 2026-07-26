#include "mpsk_receiver_r/mpsk_receiver_r_core.h"
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
  int                      _fails = 0;
  mpsk_receiver_r_state_t *obj    = mpsk_receiver_r_create (
      4, 16.0, 4, 0, 0.35, 8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0, 1024,
      MPSK_RX_NDA_TAP_STROBE);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  mpsk_receiver_r_reset (obj);

  mpsk_receiver_r_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_mpsk_receiver_r_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_mpsk_receiver_r_core PASSED\n");
  return 0;
}
