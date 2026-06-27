#include "ddc/ddc_core.h"
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
  int          _fails = 0;
  ddc_state_t *obj    = ddc_create (0.0, 0.25);
  CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  ddc_reset (obj);

  ddc_destroy (obj);

  /* ── DDCR full-chain serializable-state round-trip ────────────────────────
   * The integration gate for the elastic ddc_fn: serialize the whole chain
   * (hbdecim_r2c -> LO -> RateConverter) mid-stream, rebuild a fresh DDCR from
   * the same (norm_freq, rate) descriptor, restore the blob, and continue —
   * the concatenated CF32 output must equal an uninterrupted run bit-for-bit.
   * This also covers hbdecim_r2c and RateConverter (no standalone targets). */
  {
    const double    norm_freq = -0.3, rate = 0.25;
    const size_t    L = 4096, cut = 1503, CAP = 2048;
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.17 * (double)i) + 0.5f * (float)sin (0.013 * i);

    ddcr_state_t *ra = ddcr_create (norm_freq, rate);
    size_t        nA = ddcr_execute (ra, in, L, outA, CAP);
    ddcr_destroy (ra);

    ddcr_state_t *r1   = ddcr_create (norm_freq, rate);
    size_t        nB   = ddcr_execute (r1, in, cut, outB, CAP);
    size_t        sb   = ddcr_state_bytes (r1);
    void         *blob = malloc (sb);
    ddcr_get_state (r1, blob);
    ddcr_destroy (r1);

    ddcr_state_t *r2 = ddcr_create (norm_freq, rate);
    CHECK (ddcr_set_state (r2, blob) == 0);
    /* a mismatched-rate engine must reject the blob */
    ddcr_state_t *rbad = ddcr_create (norm_freq, 0.2);
    CHECK (ddcr_set_state (rbad, blob) == -1);
    ddcr_destroy (rbad);

    nB += ddcr_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
    ddcr_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    int bad = 0;
    for (size_t i = 0; i < nA && i < nB; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        bad++;
    CHECK (bad == 0);
    free (in);
    free (outA);
    free (outB);
  }

  if (_fails)
    {
      fprintf (stderr, "test_ddc_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ddc_core PASSED\n");
  return 0;
}
