#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
  int _fails = 0;
  /* Minimal 3-tap halfband prototype: [0.25, 0.5, 0.25] */
  static const float         h[] = { 0.25f, 0.5f, 0.25f };
  HalfbandDecimator_state_t *obj = HalfbandDecimator_create (3, h);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  HalfbandDecimator_reset (obj);

  HalfbandDecimator_destroy (obj);

  /* serializable state — forwarded to the hbdecim leaf; split a stream, hand
   * the delay lines to a fresh decimator, and resume bit-for-bit. */
  {
    const size_t    L = 512, cut = 157, CAP = 512;
    float _Complex *in   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i]
          = (float)cos (0.05 * (double)i) + I * (float)sin (0.05 * (double)i);

    HalfbandDecimator_state_t *ra = HalfbandDecimator_create (3, h);
    size_t nA = HalfbandDecimator_execute (ra, in, L, outA);
    HalfbandDecimator_destroy (ra);

    HalfbandDecimator_state_t *r1 = HalfbandDecimator_create (3, h);
    size_t nB   = HalfbandDecimator_execute (r1, in, cut, outB);
    size_t sb   = HalfbandDecimator_state_bytes (r1);
    void  *blob = malloc (sb);
    HalfbandDecimator_get_state (r1, blob);
    HalfbandDecimator_destroy (r1);

    HalfbandDecimator_state_t *r2 = HalfbandDecimator_create (3, h);
    CHECK (HalfbandDecimator_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    CHECK (HalfbandDecimator_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += HalfbandDecimator_execute (r2, in + cut, L - cut, outB + nB);
    HalfbandDecimator_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      CHECK (crealf (outA[i]) == crealf (outB[i])
             && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }
  if (_fails)
    {
      fprintf (stderr, "test_HalfbandDecimator_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_HalfbandDecimator_core PASSED\n");
  return 0;
}
