#include "doppler/HalfbandDecimatorR2C/HalfbandDecimatorR2C_core.h"
#include "doppler/dp_complex.h"
#include "dp_chunk_inv.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* dp_chunk_invariance plumbing: a fresh 3-tap decimator per call, and one
   execute per chunk. */
static const float CI_TAPS[3] = { 0.25f, 0.5f, 0.25f };

static void *
ci_create (void *arg)
{
  (void)arg;
  return dp_HalfbandDecimatorR2C_create (CI_TAPS, 3);
}

static void
ci_destroy (void *obj)
{
  dp_HalfbandDecimatorR2C_destroy (obj);
}

static size_t
ci_process (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  return dp_HalfbandDecimatorR2C_execute (obj, in, n, out, out_cap);
}

int
main (void)
{
  /* Minimal 3-tap halfband branch: [0.25, 0.5, 0.25] */
  static const float               h[] = { 0.25f, 0.5f, 0.25f };
  dp_HalfbandDecimatorR2C_state_t *obj = dp_HalfbandDecimatorR2C_create (h, 3);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;
  DP_CHECK (dp_HalfbandDecimatorR2C_get_rate (obj) == 0.5);
  DP_CHECK (dp_HalfbandDecimatorR2C_get_num_taps (obj) == 3);
  dp_HalfbandDecimatorR2C_reset (obj);
  dp_HalfbandDecimatorR2C_destroy (obj);

  /* create refuses what the leaf refuses */
  DP_CHECK (dp_HalfbandDecimatorR2C_create (NULL, 3) == NULL);
  DP_CHECK (dp_HalfbandDecimatorR2C_create (h, 0) == NULL);

  /* serializable state -- forwarded to the hbdecim_r2c leaf: split a real
   * stream at an odd sample (a held sample crosses the hand-off), resume in
   * a fresh decimator, and match one whole call bit for bit. */
  {
    const size_t    L = 512, cut = 157;
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (L * sizeof (float _Complex));
    float _Complex *outB = malloc (L * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.7 * (double)i);

    dp_HalfbandDecimatorR2C_state_t *ra
        = dp_HalfbandDecimatorR2C_create (h, 3);
    size_t nA = dp_HalfbandDecimatorR2C_execute (ra, in, L, outA, L);
    dp_HalfbandDecimatorR2C_destroy (ra);

    dp_HalfbandDecimatorR2C_state_t *r1
        = dp_HalfbandDecimatorR2C_create (h, 3);
    size_t nB   = dp_HalfbandDecimatorR2C_execute (r1, in, cut, outB, cut);
    size_t sb   = dp_HalfbandDecimatorR2C_state_bytes (r1);
    void  *blob = malloc (sb);
    dp_HalfbandDecimatorR2C_get_state (r1, blob);
    dp_HalfbandDecimatorR2C_destroy (r1);

    dp_HalfbandDecimatorR2C_state_t *r2
        = dp_HalfbandDecimatorR2C_create (h, 3);
    DP_CHECK (dp_HalfbandDecimatorR2C_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    DP_CHECK (dp_HalfbandDecimatorR2C_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_HalfbandDecimatorR2C_execute (r2, in + cut, L - cut, outB + nB,
                                           L - cut);
    dp_HalfbandDecimatorR2C_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }

  /* -- pass_capacity: emission stops at max_out (jm gh-138) -------------- */
  {
    dp_HalfbandDecimatorR2C_state_t *d = dp_HalfbandDecimatorR2C_create (h, 3);
    float                            in[64];
    float _Complex out[64];
    DP_CHECK (d != NULL);
    for (int i = 0; i < 64; i++)
      {
        in[i]  = (float)i;
        out[i] = 42.0f + 42.0f * I;
      }
    size_t n = dp_HalfbandDecimatorR2C_execute (d, in, 64, out, 5);
    DP_CHECK (n <= 5);
    for (size_t i = n; i < 64; i++)
      DP_CHECK (out[i] == 42.0f + 42.0f * I); /* tail untouched */
    DP_CHECK (dp_HalfbandDecimatorR2C_execute (d, in, 64, out, 0) == 0);
    dp_HalfbandDecimatorR2C_destroy (d);
  }

  /* -- execute_max_out is the real per-call bound, held sample included -- */
  {
    dp_HalfbandDecimatorR2C_state_t *d = dp_HalfbandDecimatorR2C_create (h, 3);
    float                            in[64];
    float _Complex out[64];
    DP_CHECK (d != NULL);
    for (int i = 0; i < 64; i++)
      in[i] = (float)i;
    DP_CHECK (dp_HalfbandDecimatorR2C_execute_max_out (d, 0) == 0);
    DP_CHECK (dp_HalfbandDecimatorR2C_execute_max_out (d, 1) == 1);
    DP_CHECK (dp_HalfbandDecimatorR2C_execute_max_out (d, 64) == 32);
    DP_CHECK (dp_HalfbandDecimatorR2C_execute_max_out (d, 65) == 33);
    /* 7 inputs: 3 out, 1 held. Then 7 more: (7 + 1) / 2 = 4 out, exactly
       the bound for 7 -- the held sample is inside it. */
    DP_CHECK (dp_HalfbandDecimatorR2C_execute (d, in, 7, out, 64) == 3);
    size_t n = dp_HalfbandDecimatorR2C_execute (d, in, 7, out, 64);
    DP_CHECK (n == 4);
    DP_CHECK (n <= dp_HalfbandDecimatorR2C_execute_max_out (d, 7));
    dp_HalfbandDecimatorR2C_destroy (d);
  }

  /* -- the shared state round trip: determinism, fidelity, reject -------- */
  {
    dp_HalfbandDecimatorR2C_state_t *a = dp_HalfbandDecimatorR2C_create (h, 3);
    dp_HalfbandDecimatorR2C_state_t *b = dp_HalfbandDecimatorR2C_create (h, 3);
    float                            in[33];
    float _Complex out[33];
    DP_CHECK (a != NULL && b != NULL);
    for (int i = 0; i < 33; i++) /* odd count: a sample is held */
      in[i] = (float)sin (0.3 * (double)i);
    (void)dp_HalfbandDecimatorR2C_execute (a, in, 33, out, 33);
    DP_STATE_ROUNDTRIP_TEST (dp_HalfbandDecimatorR2C, a, b);
    dp_HalfbandDecimatorR2C_destroy (a);
    dp_HalfbandDecimatorR2C_destroy (b);
  }

  /* -- output does not depend on how the input is chunked ---------------- */
  {
    enum
    {
      N = 1001
    };
    static float x[N];
    for (int i = 0; i < N; i++)
      x[i] = (float)cos (0.11 * (double)i)
             + 0.25f * (float)sin (1.9 * (double)i);
    dp_ci_spec_t spec = {
      .name     = "HalfbandDecimatorR2C",
      .create   = ci_create,
      .destroy  = ci_destroy,
      .process  = ci_process,
      .in_size  = sizeof (float),
      .out_size = sizeof (float _Complex),
      /* Exactly the one-shot count: N is odd, so the last sample is held
         and makes no output. A bound of (N + 1) / 2 would let a core that
         wrongly emitted for it through; both runs would agree on that. */
      .out_cap = N / 2,
    };
    DP_CHECK (dp_chunk_invariance (&spec, x, N) == 0);
  }

  DP_TEST_END ("test_HalfbandDecimatorR2C_core");
}
