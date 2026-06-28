#include "adc/adc_core.h"
#include "dp_state_test.h"
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
  int          _fails = 0;
  adc_state_t *obj    = adc_create (16, -10.0f, 0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* step: verify it runs without crashing */
  (void)adc_step (obj, 0.0f);

  /* reset */
  adc_reset (obj);

  adc_destroy (obj);
  if (_fails)
    {
      fprintf (stderr, "test_adc_core FAILED (%d)\n", _fails);
      return 1;
    }
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    adc_state_t *a = adc_create (8, 0.0f, 1);
    adc_state_t *b = adc_create (8, 0.0f, 1);
    CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 20; i++)
      (void)adc_step (a, 2.0f); /* clip + advance dither RNG */
    DP_STATE_ROUNDTRIP_TEST (adc, a, b);
    CHECK (b->rng == a->rng && b->clipped == a->clipped);
    CHECK (adc_step (b, 0.3f) == adc_step (a, 0.3f));
    adc_destroy (a);
    adc_destroy (b);
  }

  printf ("test_adc_core PASSED\n");
  return 0;
}
