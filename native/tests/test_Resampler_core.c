#include "doppler/Resampler/Resampler_core.h"
#include "doppler/dp_complex.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
int
main (void)
{
  dp_Resampler_state_t *obj = dp_Resampler_create (0.0);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* no step() generated (--no-step) */

  /* reset */
  dp_Resampler_reset (obj);

  dp_Resampler_destroy (obj);

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

    dp_Resampler_state_t *ra = dp_Resampler_create (0.5);
    size_t                nA = dp_Resampler_execute (ra, in, L, outA, L);
    dp_Resampler_destroy (ra);

    dp_Resampler_state_t *r1   = dp_Resampler_create (0.5);
    size_t                nB   = dp_Resampler_execute (r1, in, cut, outB, cut);
    size_t                sb   = dp_Resampler_state_bytes (r1);
    void                 *blob = malloc (sb);
    dp_Resampler_get_state (r1, blob);
    dp_Resampler_destroy (r1);

    dp_Resampler_state_t *r2 = dp_Resampler_create (0.5);
    DP_CHECK (dp_Resampler_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    DP_CHECK (dp_Resampler_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_Resampler_execute (r2, in + cut, L - cut, outB + nB, L - cut);
    dp_Resampler_destroy (r2);
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
    /* The wrapper used to hand the leaf a fixed RESAMPLER_MAX_OUT no
     * matter what the caller had allocated; it now forwards the real
     * capacity, so an under-sized buffer truncates instead of overruns. */
    dp_Resampler_state_t *r = dp_Resampler_create (1.0);
    float _Complex in[64], out[64];
    DP_CHECK (r != NULL);
    for (int i = 0; i < 64; i++)
      {
        in[i]  = (float)i + 0.0f * I;
        out[i] = 42.0f + 42.0f * I;
      }
    size_t n = dp_Resampler_execute (r, in, 64, out, 5);
    DP_CHECK (n <= 5);
    for (size_t i = n; i < 64; i++)
      DP_CHECK (out[i] == 42.0f + 42.0f * I); /* tail untouched */

    /* Zero capacity emits nothing at all. */
    DP_CHECK (dp_Resampler_execute (r, in, 64, out, 0) == 0);

    double ctrl[64];
    for (int i = 0; i < 64; i++)
      ctrl[i] = 0.0;
    DP_CHECK (dp_Resampler_execute_ctrl (r, in, 64, ctrl, 64, out, 3) <= 3);
    dp_Resampler_destroy (r);
  }

  DP_TEST_END ("test_Resampler_core");
}
