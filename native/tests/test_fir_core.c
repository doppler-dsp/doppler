#include "fir/fir_core.h"
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

static inline int
_feq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_ceq (float complex a, float complex b, float tol)
{
  return _feq (crealf (a), crealf (b), tol)
         && _feq (cimagf (a), cimagf (b), tol);
}

int
main (void)
{
  int _fails = 0;

  /* ── create / destroy ─────────────────────────────────────────────── */
  float        rtaps[3] = { 0.5f, 0.25f, 0.125f };
  fir_state_t *f        = fir_create_real (rtaps, 3);
  CHECK (f != NULL);
  if (!f)
    return 1;
  CHECK (fir_get_num_taps (f) == 3);
  CHECK (fir_get_is_real (f) == 1);

  /* ── impulse response ─────────────────────────────────────────────── */
  float complex in[6] = { 1.0f + 0.0f * I, 0, 0, 0, 0, 0 };
  float complex out[6];
  size_t        n = fir_execute (f, in, 6, out);
  CHECK (n == 6);
  CHECK (_feq (crealf (out[0]), 0.5f, 1e-6f));
  CHECK (_feq (crealf (out[1]), 0.25f, 1e-6f));
  CHECK (_feq (crealf (out[2]), 0.125f, 1e-6f));
  CHECK (_feq (crealf (out[3]), 0.0f, 1e-6f));

  /* ── reset clears delay ───────────────────────────────────────────── */
  fir_execute (f, in, 6, out);
  fir_reset (f);
  fir_execute (f, in, 6, out);
  CHECK (_feq (crealf (out[0]), 0.5f, 1e-6f));

  /* ── complex taps ─────────────────────────────────────────────────── */
  float complex ctaps[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
  fir_state_t  *cf       = fir_create (ctaps, 2);
  CHECK (cf != NULL);
  CHECK (fir_get_is_real (cf) == 0);
  float complex cin[4] = { 1.0f + 0.0f * I, 0, 0, 0 };
  float complex cout[4];
  fir_execute (cf, cin, 4, cout);
  CHECK (_ceq (cout[0], 1.0f + 0.0f * I, 1e-6f));
  CHECK (_ceq (cout[1], 0.0f + 1.0f * I, 1e-6f));
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
    CHECK (sb <= sizeof blob);
    fir_get_state (r1, blob);
    fir_destroy (r1);

    fir_state_t *r2 = fir_create_real (taps, 7);
    CHECK (fir_set_state (r2, blob) == DP_OK);
    /* standard envelope: a magic-clobbered blob is rejected, r2 untouched */
    blob[0] ^= (unsigned char)0xFF;
    CHECK (fir_set_state (r2, blob) == DP_ERR_INVALID);
    blob[0] ^= (unsigned char)0xFF;
    fir_execute (r2, in + cut, L - cut, outB + cut);
    fir_destroy (r2);

    int ok = 1;
    for (size_t i = 0; i < L; i++)
      if (crealf (outA[i]) != crealf (outB[i])
          || cimagf (outA[i]) != cimagf (outB[i]))
        ok = 0;
    CHECK (ok);
  }

  if (_fails)
    {
      fprintf (stderr, "test_fir_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_fir_core PASSED\n");
  return 0;
}
