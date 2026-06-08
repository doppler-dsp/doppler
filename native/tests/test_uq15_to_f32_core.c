#include "uq15_to_f32/uq15_to_f32_core.h"
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
  int                  _fails = 0;
  uq15_to_f32_state_t *obj    = uq15_to_f32_create (32768.0f);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)uq15_to_f32_step (obj, 0U);

  /* reset */
  uq15_to_f32_reset (obj);

  uq15_to_f32_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_uq15_to_f32_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_uq15_to_f32_core PASSED\n");
  return 0;
}
