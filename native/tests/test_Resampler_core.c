#include "Resampler/Resampler_core.h"
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
  int                _fails = 0;
  Resampler_state_t *obj    = Resampler_create (0.0);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  Resampler_reset (obj);

  Resampler_destroy (obj);

  /* serializable state — forwarded to the resamp leaf; split a stream, hand
   * the state to a fresh Resampler, and resume bit-for-bit. */
  {
    const size_t    L = 512, cut = 157, CAP = 1024;
    float _Complex *in   = malloc (L * sizeof (float _Complex));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i]
          = (float)cos (0.03 * (double)i) + I * (float)sin (0.03 * (double)i);

    Resampler_state_t *ra = Resampler_create (0.5);
    size_t             nA = Resampler_execute (ra, in, L, outA);
    Resampler_destroy (ra);

    Resampler_state_t *r1   = Resampler_create (0.5);
    size_t             nB   = Resampler_execute (r1, in, cut, outB);
    size_t             sb   = Resampler_state_bytes (r1);
    void              *blob = malloc (sb);
    Resampler_get_state (r1, blob);
    Resampler_destroy (r1);

    Resampler_state_t *r2 = Resampler_create (0.5);
    CHECK (Resampler_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    CHECK (Resampler_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += Resampler_execute (r2, in + cut, L - cut, outB + nB);
    Resampler_destroy (r2);
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
      fprintf (stderr, "test_Resampler_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_Resampler_core PASSED\n");
  return 0;
}
