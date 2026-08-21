#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  /* Minimal 3-tap halfband prototype: [0.25, 0.5, 0.25] */
  static const float         h[] = { 0.25f, 0.5f, 0.25f };
  HalfbandDecimator_state_t *obj = HalfbandDecimator_create (h, 3);
  DP_CHECK (obj != NULL);
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

    HalfbandDecimator_state_t *ra = HalfbandDecimator_create (h, 3);
    size_t nA = HalfbandDecimator_execute (ra, in, L, outA, L);
    HalfbandDecimator_destroy (ra);

    HalfbandDecimator_state_t *r1 = HalfbandDecimator_create (h, 3);
    size_t nB   = HalfbandDecimator_execute (r1, in, cut, outB, cut);
    size_t sb   = HalfbandDecimator_state_bytes (r1);
    void  *blob = malloc (sb);
    HalfbandDecimator_get_state (r1, blob);
    HalfbandDecimator_destroy (r1);

    HalfbandDecimator_state_t *r2 = HalfbandDecimator_create (h, 3);
    DP_CHECK (HalfbandDecimator_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    DP_CHECK (HalfbandDecimator_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += HalfbandDecimator_execute (r2, in + cut, L - cut, outB + nB,
                                     L - cut);
    HalfbandDecimator_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }
  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* 2:1 decimation: 64 inputs would emit 32, but the caller only has
     * room for 5. The wrapper used to pass a fixed HBDECIM_MAX_OUT. */
    float                      h[3] = { 0.25f, 0.5f, 0.25f };
    HalfbandDecimator_state_t *d    = HalfbandDecimator_create (h, 3);
    float complex              in[64], out[64];
    DP_CHECK (d != NULL);
    for (int i = 0; i < 64; i++)
      {
        in[i]  = (float)i + 0.0f * I;
        out[i] = 42.0f + 42.0f * I;
      }
    size_t n = HalfbandDecimator_execute (d, in, 64, out, 5);
    DP_CHECK (n <= 5);
    for (size_t i = n; i < 64; i++)
      DP_CHECK (out[i] == 42.0f + 42.0f * I); /* tail untouched */

    /* Zero capacity emits nothing at all. */
    DP_CHECK (HalfbandDecimator_execute (d, in, 64, out, 0) == 0);
    HalfbandDecimator_destroy (d);
  }

  DP_TEST_END ("test_HalfbandDecimator_core");
}
