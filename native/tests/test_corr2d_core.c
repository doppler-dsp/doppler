#include "corr2d/corr2d_core.h"
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

#define TOL 1e-4f

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
  const size_t NY     = 4;
  const size_t NX     = 4;
  const size_t N      = NY * NX; /* 16 */

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1);
    CHECK (obj != NULL);
    CHECK (obj->ny == NY);
    CHECK (obj->nx == NX);
    CHECK (obj->dwell == 1);
    CHECK (obj->count == 0);
    CHECK (obj->fwd != NULL);
    CHECK (obj->inv != NULL);
    corr2d_reset (obj);
    CHECK (obj->count == 0);
    corr2d_destroy (obj);
    corr2d_destroy (NULL);
  }

  /* ── self-correlation of 2-D unit impulse ─────────────────────────── *
   * The 2-D circular cross-correlation of δ with itself is δ.           */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f + 0.0f * I;

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1);
    float complex   out[16];
    size_t          n_out = corr2d_execute (obj, ref, N, out);

    CHECK (n_out == N);
    CHECK (ceq (out[0], 1.0f + 0.0f * I));
    for (size_t k = 1; k < N; k++)
      CHECK (ceq (out[k], 0.0f + 0.0f * I));

    corr2d_destroy (obj);
  }

  /* ── integrate-and-dump: dwell=2 ─────────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 2, 1);
    float complex   out[16];

    size_t n1 = corr2d_execute (obj, ref, N, out);
    CHECK (n1 == 0);
    CHECK (obj->count == 1);

    size_t n2 = corr2d_execute (obj, ref, N, out);
    CHECK (n2 == N); /* dump on second call */
    CHECK (obj->count == 0);

    /* Two frames of impulse × impulse = 2.0 at lag (0,0). */
    CHECK (crealf (out[0]) > 1.9f && crealf (out[0]) < 2.1f);

    corr2d_destroy (obj);
  }

  /* ── 2-D shift: shifted input produces shifted peak ──────────────── *
   * ref = δ[0,0], in = δ[1,0].                                          *
   * R[i,j] = IFFT2(FFT2(in) · conj(FFT2(ref))) / (ny*nx)               *
   *        = δ[i-1, j] → peak at (row=1, col=0).                        */
  {
    float complex ref[16] = { 0 };
    float complex in[16]  = { 0 };
    ref[0]                = 1.0f;
    in[NX]                = 1.0f; /* row 1, col 0 */

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1);
    float complex   out[16];
    corr2d_execute (obj, in, N, out);

    /* Peak should be at row=1, col=0 → flat index = 1*NX + 0 = NX */
    size_t peak_idx = NX;
    CHECK (ceq (out[peak_idx], 1.0f + 0.0f * I));

    /* All other bins zero */
    for (size_t k = 0; k < N; k++)
      if (k != peak_idx)
        CHECK (ceq (out[k], 0.0f + 0.0f * I));

    corr2d_destroy (obj);
  }

  /* ── max_out always returns ny*nx ────────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    corr2d_state_t *obj   = corr2d_create (ref, NY, NX, 1, 1);
    CHECK (corr2d_execute_max_out (obj) == N);
    corr2d_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_corr2d_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_corr2d_core PASSED\n");
  return 0;
}
