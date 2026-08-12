#include "dp_test.h"
#include "fir/fir_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

int
main (void)
{

  /* ── create / destroy ─────────────────────────────────────────────── */
  float        rtaps[3] = { 0.5f, 0.25f, 0.125f };
  fir_state_t *f        = fir_create_real (rtaps, 3);
  DP_CHECK (f != NULL);
  if (!f)
    return 1;
  DP_CHECK (fir_get_num_taps (f) == 3);
  DP_CHECK (fir_get_is_real (f) == 1);

  /* ── impulse response ─────────────────────────────────────────────── */
  float complex in[6] = { 1.0f + 0.0f * I, 0, 0, 0, 0, 0 };
  float complex out[6];
  size_t        n = fir_execute (f, in, 6, out);
  DP_CHECK (n == 6);
  DP_CHECK (dp_nearf (crealf (out[0]), 0.5f, 1e-6f));
  DP_CHECK (dp_nearf (crealf (out[1]), 0.25f, 1e-6f));
  DP_CHECK (dp_nearf (crealf (out[2]), 0.125f, 1e-6f));
  DP_CHECK (dp_nearf (crealf (out[3]), 0.0f, 1e-6f));

  /* ── reset clears delay ───────────────────────────────────────────── */
  fir_execute (f, in, 6, out);
  fir_reset (f);
  fir_execute (f, in, 6, out);
  DP_CHECK (dp_nearf (crealf (out[0]), 0.5f, 1e-6f));

  /* ── complex taps ─────────────────────────────────────────────────── */
  float complex ctaps[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
  fir_state_t  *cf       = fir_create (ctaps, 2);
  DP_CHECK (cf != NULL);
  DP_CHECK (fir_get_is_real (cf) == 0);
  float complex cin[4] = { 1.0f + 0.0f * I, 0, 0, 0 };
  float complex cout[4];
  fir_execute (cf, cin, 4, cout);
  DP_CHECK (dp_cnearf (cout[0], 1.0f + 0.0f * I, 1e-6f));
  DP_CHECK (dp_cnearf (cout[1], 0.0f + 1.0f * I, 1e-6f));
  fir_destroy (cf);

  fir_destroy (f);

  /* ── Serializable state round-trip — the elastic-resume guarantee ─────────
   * Split a stream, hand the FIR's delay line to a fresh FIR (same taps), and
   * continue: the concatenated output equals an uninterrupted run exactly. */
  {
    const float taps[7] = { 0.05f, -0.12f, 0.30f, 0.6f, 0.30f, -0.12f, 0.05f };
    const size_t  L = 128, cut = 53;
    float complex in[128], outA[128], outB[128];
    for (size_t i = 0; i < L; i++)
      in[i] = (float)(i % 9) - 4.0f + I * ((float)(i % 5) - 2.0f);

    fir_state_t *ra = fir_create_real (taps, 7);
    fir_execute (ra, in, L, outA);
    fir_destroy (ra);

    fir_state_t *r1 = fir_create_real (taps, 7);
    fir_execute (r1, in, cut, outB);
    size_t        sb = fir_state_bytes (r1);
    unsigned char blob[64];
    DP_CHECK (sb <= sizeof blob);
    fir_get_state (r1, blob);
    fir_destroy (r1);

    fir_state_t *r2 = fir_create_real (taps, 7);
    DP_CHECK (fir_set_state (r2, blob) == DP_OK);
    /* standard envelope: a magic-clobbered blob is rejected, r2 untouched */
    blob[0] ^= (unsigned char)0xFF;
    DP_CHECK (fir_set_state (r2, blob) == DP_ERR_INVALID);
    blob[0] ^= (unsigned char)0xFF;
    fir_execute (r2, in + cut, L - cut, outB + cut);
    fir_destroy (r2);

    int ok = 1;
    for (size_t i = 0; i < L; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        ok = 0;
    DP_CHECK (ok);
  }

  DP_TEST_END ("test_fir_core");
}
