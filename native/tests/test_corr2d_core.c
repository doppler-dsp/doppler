#include "corr2d/corr2d_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
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

static uint32_t
_xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

static float
_rand_uniform (uint32_t *s)
{
  return ((float)(_xorshift32 (s) % 20001u) - 10000.0f) / 10000.0f;
}

/* Hand-written O(n^2) circular cross-correlation, matching corr2d's own
 * definition R[i,j] = IFFT2(FFT2(x)*conj(FFT2(h)))/(ny*nx) directly from
 * its time-domain form (the DFT correlation theorem):
 *   R[i,j] = sum_{a,b} x[a,b] * conj(h[(a-i) mod ny, (b-j) mod nx])
 * No further division: corr2d's own 1/(ny*nx) exactly cancels the 1/(ny*nx)
 * an unnormalised-forward+unnormalised-inverse FFT2 round trip introduces,
 * leaving this direct sum (verified against numpy: an ifft2/fft2 round
 * trip is already fully normalised, so it equals this sum with no extra
 * scaling — the earlier draft of this helper divided by ny*nx a second
 * time and consistently mismatched by exactly that factor).  Used to
 * validate both corr2d paths against dense (non-impulse) signals — the
 * impulse/shift tests above pass through almost any correlator trivially
 * and don't exercise general content. */
static void
_brute_corr2d (const float complex *x, const float complex *h, size_t ny,
               size_t nx, float complex *out)
{
  for (size_t i = 0; i < ny; i++)
    for (size_t j = 0; j < nx; j++)
      {
        float complex acc = 0.0f;
        for (size_t a = 0; a < ny; a++)
          for (size_t b = 0; b < nx; b++)
            {
              size_t ra = (a + ny - i) % ny;
              size_t rb = (b + nx - j) % nx;
              acc += x[a * nx + b] * conjf (h[ra * nx + rb]);
            }
        out[i * nx + j] = acc;
      }
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

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    CHECK (obj != NULL);
    CHECK (obj->ny == NY);
    CHECK (obj->nx == NX);
    CHECK (obj->dwell == 1);
    CHECK (obj->count == 0);
    /* A single-row ref with ny_out==ny takes the fast path (see corr2d.h): */
    CHECK (obj->fast_path == 1);
    CHECK (obj->fwd == NULL && obj->inv == NULL);
    CHECK (obj->fwd1d != NULL && obj->inv1d != NULL);
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

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
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

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 2, 1, 0, 0);
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

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
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

  /* ── max_out returns n_out (native = ny*nx) ──────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    corr2d_state_t *obj   = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    CHECK (corr2d_execute_max_out (obj) == N);
    corr2d_destroy (obj);
  }

  /* ── decoupled inverse: interpolated output size + peak location ──── *
   * impulse ref, input shifted to (row 1, col 0) → native peak at (1,0); *
   * inverted on a 4→8 grid, the peak lands at (1·8/4, 0) = (2, 0).       */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    float complex in[16]  = { 0 };
    in[NX]                = 1.0f; /* row 1, col 0 */
    corr2d_state_t *obj   = corr2d_create (ref, NY, NX, 1, 1, 8, 8);
    CHECK (obj->ny_out == 8 && obj->nx_out == 8);
    /* ny_out (8) != ny (4) -- Doppler-axis interpolation is requested, so
     * the fast path's identity doesn't apply (see corr2d.h); must fall
     * back to the general 2-D path even though ref is single-row. */
    CHECK (obj->fast_path == 0);
    CHECK (obj->fwd != NULL && obj->inv != NULL);
    CHECK (corr2d_execute_max_out (obj) == 64);

    float complex out[64];
    size_t        no = corr2d_execute (obj, in, N, out);
    CHECK (no == 64);
    size_t pk = 0;
    for (size_t k = 1; k < 64; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    CHECK (pk / 8 == 2 && pk % 8 == 0);
    corr2d_destroy (obj);
  }

  /* ── dense-signal correctness vs. a brute-force reference, both paths ── */
  {
    const size_t  ny = 5, nx = 7, n = ny * nx;
    uint32_t      seed          = 12345u;
    float complex dense_ref[35] = { 0 }, dense_in[35], expect[35], out[35];
    for (size_t j = 0; j < nx; j++)
      dense_ref[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
    for (size_t k = 0; k < n; k++)
      dense_in[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    /* single-row ref -> fast path */
    corr2d_state_t *fast = corr2d_create (dense_ref, ny, nx, 1, 1, 0, 0);
    CHECK (fast != NULL && fast->fast_path == 1);
    corr2d_execute (fast, dense_in, n, out);
    _brute_corr2d (dense_in, dense_ref, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      CHECK (ceq (out[k], expect[k]));
    corr2d_destroy (fast);

    /* genuinely multi-row ref -> general path; the pre-existing suite had
     * no non-impulse correctness check for this path either. */
    float complex dense_ref2[35];
    for (size_t k = 0; k < n; k++)
      dense_ref2[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
    corr2d_state_t *slow = corr2d_create (dense_ref2, ny, nx, 1, 1, 0, 0);
    CHECK (slow != NULL && slow->fast_path == 0);
    corr2d_execute (slow, dense_in, n, out);
    _brute_corr2d (dense_in, dense_ref2, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      CHECK (ceq (out[k], expect[k]));
    corr2d_destroy (slow);
  }

  /* ── fast path + nx_out interpolation (code-axis only, ny_out==ny) ────── *
   * single-row ref, nx doubled via decoupled inverse; peak should scale by
   * nx_out/nx exactly like the general-path decoupled test above scales by
   * ny_out/ny. */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    float complex in[16]  = { 0 };
    in[1]                 = 1.0f; /* row 0, col 1 */
    corr2d_state_t *obj   = corr2d_create (ref, NY, NX, 1, 1, 0, 8);
    CHECK (obj->fast_path == 1); /* ny_out==ny native; nx_out interpolated */
    CHECK (obj->ny_out == NY && obj->nx_out == 8);
    CHECK (corr2d_execute_max_out (obj) == NY * 8);

    float complex out[32];
    size_t        no = corr2d_execute (obj, in, N, out);
    CHECK (no == NY * 8);
    size_t pk = 0;
    for (size_t k = 1; k < NY * 8; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    /* native peak at (row=0, col=1); interpolated col = 1 * (8/4) = 2 */
    CHECK (pk / 8 == 0 && pk % 8 == 2);
    corr2d_destroy (obj);
  }

  /* ── corr2d_set_ref: fast-path accept vs. reject ──────────────────────── */
  {
    float complex ref1[16] = { 0 }, ref2[16] = { 0 }, bad_ref[16] = { 0 };
    ref1[0]     = 1.0f;
    ref2[1]     = 1.0f; /* still single-row -- row 0, col 1 */
    bad_ref[NX] = 1.0f; /* row 1 nonzero -- no longer single-row */

    corr2d_state_t *obj = corr2d_create (ref1, NY, NX, 1, 1, 0, 0);
    CHECK (obj->fast_path == 1);

    /* accept: still single-row */
    CHECK (corr2d_set_ref (obj, ref2) == 0);
    float complex in[16] = { 0 };
    in[1]                = 1.0f; /* row 0, col 1 -- matches ref2's replica */
    float complex out[16];
    corr2d_execute (obj, in, N, out);
    CHECK (ceq (out[0], 1.0f + 0.0f * I));

    /* reject: no longer single-row -- object's ref/spectrum must be left
     * completely untouched (execute() still reflects ref2, not bad_ref). */
    CHECK (corr2d_set_ref (obj, bad_ref) == -1);
    corr2d_execute (obj, in, N, out);
    CHECK (ceq (out[0], 1.0f + 0.0f * I));

    corr2d_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_corr2d_core FAILED (%d)\n", _fails);
      return 1;
    }
  /* serializable state — 2-D accumulator + count resume; plans + ref rebuilt.
   */
  {
    float complex ref[16], in[16], out[16];
    for (int i = 0; i < 16; i++)
      {
        ref[i] = (float)(i % 4) + 0.5f * I;
        in[i]  = (float)(i % 3) - 1.0f + 0.2f * I;
      }
    corr2d_state_t *a = corr2d_create (ref, 4, 4, 3, 1, 0, 0);
    corr2d_state_t *b = corr2d_create (ref, 4, 4, 3, 1, 0, 0);
    CHECK (a != NULL && b != NULL);
    (void)corr2d_execute (a, in, 16, out);
    DP_STATE_ROUNDTRIP_TEST (corr2d, a, b);
    CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
    corr2d_destroy (a);
    corr2d_destroy (b);
  }

  /* serializable state, fast path: accum's CONTENT differs (per-row nx
   * spectra rather than one flat 2-D spectrum) but the byte LAYOUT is
   * identical (n complex floats + count either way), so the triplet needs
   * no fast-path-specific code at all -- this proves that in practice. */
  {
    float complex ref[16] = { 0 }, in[16], out[16];
    ref[0]                = 1.0f;
    for (int i = 0; i < 16; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    corr2d_state_t *a = corr2d_create (ref, 4, 4, 3, 1, 0, 0);
    corr2d_state_t *b = corr2d_create (ref, 4, 4, 3, 1, 0, 0);
    CHECK (a != NULL && b != NULL);
    CHECK (a->fast_path == 1 && b->fast_path == 1);
    (void)corr2d_execute (a, in, 16, out);
    DP_STATE_ROUNDTRIP_TEST (corr2d, a, b);
    CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
    corr2d_destroy (a);
    corr2d_destroy (b);
  }

  printf ("test_corr2d_core PASSED\n");
  return 0;
}
