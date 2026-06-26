#include "corr/corr_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

#define TOL 1e-4f /* CF32 round-trip tolerance */

static inline int
ceq (float complex a, float complex b)
{
  return fabsf (crealf (a) - crealf (b)) < TOL
         && fabsf (cimagf (a) - cimagf (b)) < TOL;
}

int
main (void)
{
  int          _fails = 0;
  const size_t N      = 16;

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[16];
    for (size_t i = 0; i < N; i++)
      ref[i] = 1.0f + 0.0f * I;

    corr_state_t *obj = corr_create (ref, N, 1, 1, 0);
    CHECK (obj != NULL);
    CHECK (obj->n == N);
    CHECK (obj->dwell == 1);
    CHECK (obj->count == 0);
    CHECK (obj->fwd != NULL);
    CHECK (obj->inv != NULL);
    corr_reset (obj); /* must not crash */
    CHECK (obj->count == 0);
    corr_destroy (obj);
    corr_destroy (NULL); /* must not crash */
  }

  /* ── self-correlation of a unit impulse (dwell=1) ─────────────────── *
   * corr(δ, δ)[τ] = δ[τ]  — peak at lag 0, zeros elsewhere.
   * With the FFT correlator the output is circular; the impulse at index 0
   * maps to lag 0 = index 0 in the output.                               */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f + 0.0f * I; /* unit impulse */

    corr_state_t *obj = corr_create (ref, N, 1, 1, 0);
    CHECK (obj != NULL);

    float complex out[16];
    size_t        n_out = corr_execute (obj, ref, N, out);

    CHECK (n_out == N);                    /* dwell=1 → always dumps */
    CHECK (ceq (out[0], 1.0f + 0.0f * I)); /* peak at lag 0           */
    for (size_t k = 1; k < N; k++)
      CHECK (ceq (out[k], 0.0f + 0.0f * I));

    corr_destroy (obj);
  }

  /* ── known tone: self-correlation equals tone scaled by N ─────────── *
   * For x[n] = exp(j·2π·k₀·n/N), the circular auto-correlation is       *
   * N · exp(j·2π·k₀·τ/N) — the same tone scaled by N; every lag has     *
   * equal magnitude N.  Verify a few lag values.                          */
  {
    float complex ref[16];
    for (size_t n = 0; n < N; n++)
      {
        float ph = 2.0f * 3.14159265358979f * 2.0f * (float)n / (float)N;
        ref[n]   = cosf (ph) + sinf (ph) * I;
      }

    corr_state_t *obj = corr_create (ref, N, 1, 1, 0);
    float complex out[16];
    corr_execute (obj, ref, N, out);

    /* out[τ] = N · ref[τ]; magnitude at every lag should be N. */
    for (size_t k = 0; k < N; k++)
      {
        float expected_re = (float)N * crealf (ref[k]);
        float expected_im = (float)N * cimagf (ref[k]);
        CHECK (fabsf (crealf (out[k]) - expected_re) < TOL * (float)N);
        CHECK (fabsf (cimagf (out[k]) - expected_im) < TOL * (float)N);
      }

    corr_destroy (obj);
  }

  /* ── integrate-and-dump: dwell=3 ─────────────────────────────────── *
   * First two calls return 0; third call returns n and resets counter.  */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr_state_t *obj = corr_create (ref, N, 3, 1, 0);
    float complex out[16];

    size_t n1 = corr_execute (obj, ref, N, out);
    CHECK (n1 == 0);
    CHECK (obj->count == 1);

    size_t n2 = corr_execute (obj, ref, N, out);
    CHECK (n2 == 0);
    CHECK (obj->count == 2);

    size_t n3 = corr_execute (obj, ref, N, out);
    CHECK (n3 == N);         /* dump on third call */
    CHECK (obj->count == 0); /* counter reset */

    /* After 3 frames of impulse-against-impulse, out[0] ≈ 3.0 */
    CHECK (crealf (out[0]) > 2.9f && crealf (out[0]) < 3.1f);

    /* Immediate fourth call starts fresh — returns 0 */
    size_t n4 = corr_execute (obj, ref, N, out);
    CHECK (n4 == 0);
    CHECK (obj->count == 1);

    corr_destroy (obj);
  }

  /* ── corr_reset clears mid-dwell accumulation ─────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr_state_t *obj = corr_create (ref, N, 4, 1, 0);
    float complex out[16];

    corr_execute (obj, ref, N, out); /* count = 1 */
    corr_execute (obj, ref, N, out); /* count = 2 */
    corr_reset (obj);                /* back to 0 */
    CHECK (obj->count == 0);
    CHECK (ceq (obj->accum[0], 0.0f + 0.0f * I));

    corr_destroy (obj);
  }

  /* ── corr_set_ref recomputes and resets ───────────────────────────── */
  {
    float complex ref_a[16] = { 0 };
    float complex ref_b[16] = { 0 };
    ref_a[0]                = 1.0f;
    ref_b[1]                = 1.0f; /* impulse at lag 1 */

    corr_state_t *obj = corr_create (ref_a, N, 1, 1, 0);
    float complex out[16];

    /* Correlate ref_b (impulse at 1) against ref_a (impulse at 0).
     * R[τ] = IFFT(FFT(δ[n-1]) · conj(FFT(δ[n]))) / N
     *       = δ[τ-1]  → peak at index 1.                         */
    corr_execute (obj, ref_b, N, out);
    CHECK (ceq (out[1], 1.0f + 0.0f * I));

    /* Switch to ref_b; correlate ref_b against itself → peak at lag 0. */
    corr_set_ref (obj, ref_b);
    CHECK (obj->count == 0);
    corr_execute (obj, ref_b, N, out);
    CHECK (ceq (out[0], 1.0f + 0.0f * I));

    corr_destroy (obj);
  }

  /* ── max_out returns n_out (native = n) ────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    corr_state_t *obj     = corr_create (ref, N, 1, 1, 0);
    CHECK (corr_execute_max_out (obj) == N);
    corr_destroy (obj);
  }

  /* ── decoupled inverse: interpolated output length + peak location ── *
   * impulse ref, input shifted to lag 1 → native peak at 1; inverted on  *
   * a 16→32 grid, the peak lands at 1·32/16 = 2.                         */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    float complex in[16]  = { 0 };
    in[1]                 = 1.0f;
    corr_state_t *obj     = corr_create (ref, N, 1, 1, 32);
    CHECK (obj->n_out == 32);
    CHECK (corr_execute_max_out (obj) == 32);

    float complex out[32];
    size_t        no = corr_execute (obj, in, N, out);
    CHECK (no == 32);
    size_t pk = 0;
    for (size_t k = 1; k < 32; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    CHECK (pk == 2);
    corr_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_corr_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_corr_core PASSED\n");
  return 0;
}
