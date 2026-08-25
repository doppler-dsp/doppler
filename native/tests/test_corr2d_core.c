#include "corr2d/corr2d_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TOL 1e-4f

static float
_rand_uniform (uint32_t *s)
{
  return ((float)(dp_xs32 (s) % 20001u) - 10000.0f) / 10000.0f;
}

/* One complex sample, drawn in a DEFINED order.
 *
 * `_rand_uniform (&s) + _rand_uniform (&s) * I` reads fine and is not: the
 * two calls are indeterminately sequenced (C11 6.5.2.2p10), and gcc and
 * clang disagree about which runs first — gcc gives the real part the SECOND
 * draw, clang the first. doppler builds this file with both (`make test` is
 * gcc, `make coverage` is clang), so that spelling produced two different
 * fixtures. gcc's order is preserved here. See dp_rng_test.h. */
static float complex
_rand_cplx (uint32_t *s)
{
  float im = _rand_uniform (s);
  float re = _rand_uniform (s);
  return re + im * I;
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
  const size_t NY = 4;
  const size_t NX = 4;
  const size_t N  = NY * NX; /* 16 */

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->ny == NY);
    DP_CHECK (obj->nx == NX);
    DP_CHECK (obj->dwell == 1);
    DP_CHECK (obj->count == 0);
    /* A single-row ref with ny_out==ny takes the fast path (see corr2d.h): */
    DP_CHECK (obj->fast_path == 1);
    DP_CHECK (obj->fwd == NULL && obj->inv == NULL);
    DP_CHECK (obj->fwd1d != NULL && obj->inv1d != NULL);
    corr2d_reset (obj);
    DP_CHECK (obj->count == 0);
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
    size_t          n_out = corr2d_execute (obj, ref, N, out, N);

    DP_CHECK (n_out == N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));
    for (size_t k = 1; k < N; k++)
      DP_CHECK (dp_cnearf (out[k], 0.0f + 0.0f * I, TOL));

    corr2d_destroy (obj);
  }

  /* ── integrate-and-dump: dwell=2 ─────────────────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;

    corr2d_state_t *obj = corr2d_create (ref, NY, NX, 2, 1, 0, 0);
    float complex   out[16];

    size_t n1 = corr2d_execute (obj, ref, N, out, N);
    DP_CHECK (n1 == 0);
    DP_CHECK (obj->count == 1);

    size_t n2 = corr2d_execute (obj, ref, N, out, N);
    DP_CHECK (n2 == N); /* dump on second call */
    DP_CHECK (obj->count == 0);

    /* Two frames of impulse × impulse = 2.0 at lag (0,0). */
    DP_CHECK (crealf (out[0]) > 1.9f && crealf (out[0]) < 2.1f);

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
    corr2d_execute (obj, in, N, out, N);

    /* Peak should be at row=1, col=0 → flat index = 1*NX + 0 = NX */
    size_t peak_idx = NX;
    DP_CHECK (dp_cnearf (out[peak_idx], 1.0f + 0.0f * I, TOL));

    /* All other bins zero */
    for (size_t k = 0; k < N; k++)
      if (k != peak_idx)
        DP_CHECK (dp_cnearf (out[k], 0.0f + 0.0f * I, TOL));

    corr2d_destroy (obj);
  }

  /* ── max_out returns n_out (native = ny*nx) ──────────────────────── */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    corr2d_state_t *obj   = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    DP_CHECK (corr2d_execute_max_out (obj) == N);
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
    DP_CHECK (obj->ny_out == 8 && obj->nx_out == 8);
    /* ny_out (8) != ny (4) -- Doppler-axis interpolation is requested, so
     * the fast path's identity doesn't apply (see corr2d.h); must fall
     * back to the general 2-D path even though ref is single-row. */
    DP_CHECK (obj->fast_path == 0);
    DP_CHECK (obj->fwd != NULL && obj->inv != NULL);
    DP_CHECK (corr2d_execute_max_out (obj) == 64);

    float complex out[64];
    size_t        no = corr2d_execute (obj, in, N, out, 64);
    DP_CHECK (no == 64);
    size_t pk = 0;
    for (size_t k = 1; k < 64; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    DP_CHECK (pk / 8 == 2 && pk % 8 == 0);
    corr2d_destroy (obj);
  }

  /* ── dense-signal correctness vs. a brute-force reference, both paths ── */
  {
    const size_t  ny = 5, nx = 7, n = ny * nx;
    uint32_t      seed          = 12345u;
    float complex dense_ref[35] = { 0 }, dense_in[35], expect[35], out[35];
    for (size_t j = 0; j < nx; j++)
      dense_ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      dense_in[k] = _rand_cplx (&seed);

    /* single-row ref -> fast path */
    corr2d_state_t *fast = corr2d_create (dense_ref, ny, nx, 1, 1, 0, 0);
    DP_CHECK (fast != NULL && fast->fast_path == 1);
    corr2d_execute (fast, dense_in, n, out, n);
    _brute_corr2d (dense_in, dense_ref, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (dp_cnearf (out[k], expect[k], TOL));
    corr2d_destroy (fast);

    /* genuinely multi-row ref -> general path; the pre-existing suite had
     * no non-impulse correctness check for this path either. */
    float complex dense_ref2[35];
    for (size_t k = 0; k < n; k++)
      dense_ref2[k] = _rand_cplx (&seed);
    corr2d_state_t *slow = corr2d_create (dense_ref2, ny, nx, 1, 1, 0, 0);
    DP_CHECK (slow != NULL && slow->fast_path == 0);
    corr2d_execute (slow, dense_in, n, out, n);
    _brute_corr2d (dense_in, dense_ref2, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (dp_cnearf (out[k], expect[k], TOL));
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
    DP_CHECK (obj->fast_path
              == 1); /* ny_out==ny native; nx_out interpolated */
    DP_CHECK (obj->ny_out == NY && obj->nx_out == 8);
    DP_CHECK (corr2d_execute_max_out (obj) == NY * 8);

    float complex out[32];
    size_t        no = corr2d_execute (obj, in, N, out, 32);
    DP_CHECK (no == NY * 8);
    size_t pk = 0;
    for (size_t k = 1; k < NY * 8; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    /* native peak at (row=0, col=1); interpolated col = 1 * (8/4) = 2 */
    DP_CHECK (pk / 8 == 0 && pk % 8 == 2);
    corr2d_destroy (obj);
  }

  /* ── corr2d_set_ref: fast-path accept vs. reject ──────────────────────── */
  {
    float complex ref1[16] = { 0 }, ref2[16] = { 0 }, bad_ref[16] = { 0 };
    ref1[0]     = 1.0f;
    ref2[1]     = 1.0f; /* still single-row -- row 0, col 1 */
    bad_ref[NX] = 1.0f; /* row 1 nonzero -- no longer single-row */

    corr2d_state_t *obj = corr2d_create (ref1, NY, NX, 1, 1, 0, 0);
    DP_CHECK (obj->fast_path == 1);

    /* accept: still single-row */
    DP_CHECK (corr2d_set_ref (obj, ref2) == 0);
    float complex in[16] = { 0 };
    in[1]                = 1.0f; /* row 0, col 1 -- matches ref2's replica */
    float complex out[16];
    corr2d_execute (obj, in, N, out, N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));

    /* reject: no longer single-row -- object's ref/spectrum must be left
     * completely untouched (execute() still reflects ref2, not bad_ref). */
    DP_CHECK (corr2d_set_ref (obj, bad_ref) == -1);
    corr2d_execute (obj, in, N, out, N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));

    corr2d_destroy (obj);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* corr2d has TWO write paths -- the 2-D inverse and the per-row fast
     * path -- and the fast one bypasses the slow path entirely, so both are
     * exercised here. A clamp applied to only one would leave the other
     * overrunning, which is the shape of the bug this guards. */
    const size_t  ny = 5, nx = 7, n = ny * nx;
    uint32_t      seed        = 999u;
    float complex row_ref[35] = { 0 }, full_ref[35], input[35];
    float complex full[35], part[35];
    for (size_t j = 0; j < nx; j++)
      row_ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      {
        full_ref[k] = _rand_cplx (&seed);
        input[k]    = _rand_cplx (&seed);
      }

    for (int which = 0; which < 2; which++)
      {
        const float complex *rf = which ? full_ref : row_ref;
        corr2d_state_t      *a  = corr2d_create (rf, ny, nx, 1, 1, 0, 0);
        corr2d_state_t      *b  = corr2d_create (rf, ny, nx, 1, 1, 0, 0);
        DP_CHECK (a != NULL && b != NULL);
        DP_CHECK (a->fast_path == (which ? 0 : 1)); /* both paths covered */
        for (size_t k = 0; k < n; k++)
          part[k] = 42.0f + 42.0f * I;

        DP_CHECK (corr2d_execute (a, input, n, full, n) == n);
        DP_CHECK (corr2d_execute (b, input, n, part, 6) == 6);
        for (size_t k = 0; k < 6; k++)
          DP_CHECK (dp_cnearf (part[k], full[k],
                               TOL)); /* prefix is the same surface */
        for (size_t k = 6; k < n; k++)
          DP_CHECK (dp_cnearf (part[k], 42.0f + 42.0f * I,
                               TOL)); /* tail untouched */

        for (size_t k = 0; k < n; k++)
          part[k] = 42.0f + 42.0f * I;
        DP_CHECK (corr2d_execute (b, input, n, part, 0) == 0);
        for (size_t k = 0; k < n; k++)
          DP_CHECK (dp_cnearf (part[k], 42.0f + 42.0f * I, TOL));
        corr2d_destroy (a);
        corr2d_destroy (b);
      }
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
    DP_CHECK (a != NULL && b != NULL);
    (void)corr2d_execute (a, in, 16, out, 16);
    DP_STATE_ROUNDTRIP_TEST (corr2d, a, b);
    DP_CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
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
    DP_CHECK (a != NULL && b != NULL);
    DP_CHECK (a->fast_path == 1 && b->fast_path == 1);
    (void)corr2d_execute (a, in, 16, out, 16);
    DP_STATE_ROUNDTRIP_TEST (corr2d, a, b);
    DP_CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
    corr2d_destroy (a);
    corr2d_destroy (b);
  }

  /* ── reset really zeroes the ACCUMULATOR, not just the counter ────────
   *
   * The pre-existing lifecycle check called corr2d_reset() on a freshly
   * created object and asserted `count == 0` -- which was already true
   * before the call, so a reset() with an empty body passed it. That is
   * the vacuous-reject shape docs/dev/contributing/validation.md warns
   * about by name.
   *
   * The header claims reset() is "equivalent to starting a fresh dwell
   * cycle", so that is what is measured: run a PARTIAL dwell into one
   * object, reset it, then drive both it and an untouched object through
   * the same full dwell and require the dumps to agree bit-for-bit. A
   * reset that zeroed the counter but left the accumulator would carry
   * the stale frame into the dump and fail. */
  {
    const size_t  ny = 4, nx = 4, n = ny * nx;
    uint32_t      seed    = 777u;
    float complex ref[16] = { 0 }, stale[16], fresh[16];
    float complex out_a[16], out_b[16];
    for (size_t j = 0; j < nx; j++)
      ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      stale[k] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      fresh[k] = _rand_cplx (&seed);

    corr2d_state_t *a = corr2d_create (ref, ny, nx, 2, 1, 0, 0);
    corr2d_state_t *b = corr2d_create (ref, ny, nx, 2, 1, 0, 0);
    DP_CHECK (a != NULL && b != NULL);

    /* `a` takes one frame of a dwell-2 cycle, so its accumulator is
       non-zero and its counter is 1 -- the precondition the old test
       lacked. Assert it, or this test can go vacuous the same way. */
    DP_CHECK (corr2d_execute (a, stale, n, out_a, n) == 0);
    DP_CHECK (a->count == 1);
    int accum_nonzero = 0;
    for (size_t k = 0; k < n; k++)
      if (cabsf (a->accum[k]) > 0.0f)
        accum_nonzero = 1;
    DP_CHECK (accum_nonzero);

    corr2d_reset (a);
    DP_CHECK (a->count == 0);

    /* Both now drive a full dwell-2 cycle over identical input. */
    DP_CHECK (corr2d_execute (a, fresh, n, out_a, n) == 0);
    DP_CHECK (corr2d_execute (b, fresh, n, out_b, n) == 0);
    size_t na = corr2d_execute (a, fresh, n, out_a, n);
    size_t nb = corr2d_execute (b, fresh, n, out_b, n);
    DP_CHECK (na == n && nb == n);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (out_a[k] == out_b[k]); /* bit-for-bit: same arithmetic */

    corr2d_destroy (a);
    corr2d_destroy (b);
  }

  /* ── nthreads is accepted and IGNORED, as the header says ─────────────
   *
   * A documented no-op is still a claim. Be precise about what this can
   * catch, because it CANNOT be sabotaged today: corr2d_state_t has no
   * nthreads member at all -- create() forwards the argument to
   * fft_create/fft2d_create, and both of those open with
   * `(void)nthreads;`. So the parameter is discarded at the bottom of the
   * stack and there is no state to corrupt.
   *
   * That makes this a forward guard rather than a proof: it fires on the
   * day someone wires nthreads up to a threaded reduction whose summation
   * order differs, which is precisely when "ignored" stops being true and
   * a caller's output starts depending on a parameter documented not to
   * matter. Bit-for-bit across four values, on both paths. */
  {
    const size_t  ny = 5, nx = 7, n = ny * nx;
    uint32_t      seed     = 4242u;
    float complex ref1[35] = { 0 }, refN[35], in[35];
    float complex base[35], other[35];
    for (size_t j = 0; j < nx; j++)
      ref1[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      refN[k] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      in[k] = _rand_cplx (&seed);

    for (int path = 0; path < 2; path++)
      {
        const float complex *r  = path ? refN : ref1;
        corr2d_state_t      *c0 = corr2d_create (r, ny, nx, 1, 1, 0, 0);
        DP_CHECK (c0 != NULL && c0->fast_path == (path ? 0 : 1));
        DP_CHECK (corr2d_execute (c0, in, n, base, n) == n);
        corr2d_destroy (c0);

        for (size_t nt = 2; nt <= 8; nt *= 2)
          {
            corr2d_state_t *c = corr2d_create (r, ny, nx, 1, nt, 0, 0);
            DP_CHECK (c != NULL);
            DP_CHECK (corr2d_execute (c, in, n, other, n) == n);
            for (size_t k = 0; k < n; k++)
              DP_CHECK (other[k] == base[k]);
            corr2d_destroy (c);
          }
      }
  }

  /* ── the native path allocates NO interpolation scratch ───────────────
   *
   * "Native is bit-exact and allocates no extra buffers" is a header
   * claim about memory, and the only way it shows up from outside is the
   * pointers being NULL. A regression that allocated them unconditionally
   * would cost every acquisition instance the padded grid it never uses. */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    corr2d_state_t *nat   = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    DP_CHECK (nat != NULL);
    DP_CHECK (nat->ny_out == NY && nat->nx_out == NX);
    DP_CHECK (nat->work_pad == NULL && nat->ztmp == NULL);
    DP_CHECK (nat->zcol == NULL && nat->zcolout == NULL);
    corr2d_destroy (nat);

    /* ... and the padded path DOES allocate them, so the check above is
       not passing because the fields are always NULL. */
    corr2d_state_t *pad = corr2d_create (ref, NY, NX, 1, 1, 8, 8);
    DP_CHECK (pad != NULL && pad->work_pad != NULL);
    corr2d_destroy (pad);
  }

  /* ── sub-bin interpolation recovers a FRACTIONAL peak ─────────────────
   *
   * The interpolated-inverse claim is "the band-limited (Dirichlet)
   * interpolation of the correlation on a finer grid -- same peak, sub-bin
   * resolution", and the suite pinned it only at INTEGER shifts, where the
   * native grid already lands on the answer and interpolation proves
   * nothing.
   *
   * The external truth needs no correlator: build the input as a
   * band-limited impulse at a KNOWN fractional position -- the inverse DFT
   * of a pure linear phase -- and require the interpolated peak to land on
   * it. Correlating that against a unit impulse leaves the input itself,
   * so the answer is known in closed form and the correlator is the only
   * thing under test.
   *
   * BUILD THE PHASE RAMP OVER SIGNED FREQUENCIES. Summing u = 0..nx-1
   * treats bins nx/2..nx-1 as high POSITIVE frequencies rather than the
   * negative ones they are, which is not a spectrum any correlation
   * surface has. Written that way the fine grid shows two equal peaks
   * straddling a null at the true position -- which reads exactly like a
   * broken interpolator, and is not: with the frequencies signed, the peak
   * lands on the fractional offset to within a hundredth of a bin at every
   * offset below, on both parities of nx. */
  {
    const size_t up       = 8; /* interpolate nx -> nx*up */
    const double fracs[5] = { 3.0, 3.25, 3.5, 3.75, 4.0 };

    for (int parity = 0; parity < 2; parity++)
      {
        const size_t  nx      = parity ? 15 : 16; /* odd has no Nyquist bin */
        float complex ref[16] = { 0 }, in[16];
        float complex out[16 * 8];
        ref[0] = 1.0f;

        for (int fi = 0; fi < 5; fi++)
          {
            const double frac = fracs[fi];
            for (size_t j = 0; j < nx; j++)
              {
                double ar = 0.0, ai = 0.0;
                for (size_t k = 0; k < nx; k++)
                  {
                    /* 0, +1, -1, +2, -2, ... -- signed, never a bare
                       0..nx-1 sweep. */
                    long   u  = (k == 0) ? 0
                                         : ((k % 2) ? (long)((k + 1) / 2)
                                                    : -(long)(k / 2));
                    double ph = 2.0 * M_PI * (double)u * ((double)j - frac)
                                / (double)nx;
                    ar += cos (ph);
                    ai += sin (ph);
                  }
                in[j] = (float complex) (ar / (double)nx)
                        + (float complex) (ai / (double)nx) * I;
              }

            corr2d_state_t *fine
                = corr2d_create (ref, 1, nx, 1, 1, 0, nx * up);
            DP_CHECK (fine != NULL);
            size_t no = corr2d_execute (fine, in, nx, out, nx * up);
            DP_CHECK (no == nx * up);

            size_t pk = 0;
            for (size_t k = 1; k < no; k++)
              if (cabsf (out[k]) > cabsf (out[pk]))
                pk = k;
            double got = (double)pk / (double)up;
            DP_CHECK (fabs (got - frac) < 0.02);
            corr2d_destroy (fine);
          }

        /* The native grid genuinely cannot do this: at a half-bin offset
           its best is 0.5 away, which is what the interpolation buys. */
        {
          const double frac = 3.5;
          for (size_t j = 0; j < nx; j++)
            {
              double ar = 0.0, ai = 0.0;
              for (size_t k = 0; k < nx; k++)
                {
                  long   u  = (k == 0) ? 0
                                       : ((k % 2) ? (long)((k + 1) / 2)
                                                  : -(long)(k / 2));
                  double ph = 2.0 * M_PI * (double)u * ((double)j - frac)
                              / (double)nx;
                  ar += cos (ph);
                  ai += sin (ph);
                }
              in[j] = (float complex) (ar / (double)nx)
                      + (float complex) (ai / (double)nx) * I;
            }
          corr2d_state_t *nat = corr2d_create (ref, 1, nx, 1, 1, 0, 0);
          DP_CHECK (nat != NULL);
          float complex onat[16];
          DP_CHECK (corr2d_execute (nat, in, nx, onat, nx) == nx);
          size_t pk = 0;
          for (size_t k = 1; k < nx; k++)
            if (cabsf (onat[k]) > cabsf (onat[pk]))
              pk = k;
          DP_CHECK (fabs ((double)pk - frac) >= 0.5 - 1e-9);
          corr2d_destroy (nat);
        }
      }
  }

  /* ── the interpolation keeps a REAL correlation real ──────────────────
   *
   * corr2d_zeropad_1d splits the Nyquist bin for even n and its comment
   * claims the result "matches scipy.signal.resample to machine
   * precision". The peak-location test above does NOT cover that split --
   * measured, by deleting it: the peaks stay put and the section stays
   * green, because the split changes the interpolated SHAPE and barely
   * moves its argmax.
   *
   * This invariant does see it, and needs no reference implementation. A
   * real input against a real reference gives a real correlation, whose
   * spectrum is conjugate-symmetric; band-limited interpolation of a real
   * sequence is real. Leaving the Nyquist bin unsplit turns cos(pi t) into
   * exp(i pi t), which is identical at the original sample points and
   * complex everywhere between them -- so the imaginary part is exactly
   * the residue of the bug. Measured: 5.5e-08 with the split, 6.3e-03
   * without it, five orders of magnitude apart. */
  {
    const size_t  nx = 16, up = 4;
    float complex ref[16] = { 0 }, in[16];
    float complex out[64];
    ref[0] = 1.0f;
    for (size_t j = 0; j < nx; j++)
      in[j] = (j == 3) ? 1.0f : (j == 4) ? 0.7f : (j == 11) ? -0.4f : 0.0f;

    corr2d_state_t *fine = corr2d_create (ref, 1, nx, 1, 1, 0, nx * up);
    DP_CHECK (fine != NULL);
    size_t no = corr2d_execute (fine, in, nx, out, nx * up);
    DP_CHECK (no == nx * up);

    float worst = 0.0f;
    for (size_t k = 0; k < no; k++)
      if (fabsf (cimagf (out[k])) > worst)
        worst = fabsf (cimagf (out[k]));
    DP_CHECK (worst < 1e-5f);

    /* Not vacuous: the real part is genuinely large, so `worst` is small
       because the interpolation is right and not because the output is. */
    float peak = 0.0f;
    for (size_t k = 0; k < no; k++)
      if (fabsf (crealf (out[k])) > peak)
        peak = fabsf (crealf (out[k]));
    DP_CHECK (peak > 0.5f);
    corr2d_destroy (fine);
  }

  /* ── dwell = 0 is refused, not silently accepted ──────────────────────
   *
   * The header says dwell "must be >= 1" and create() enforced only the
   * output-grid rule, so a zero dwell built an object that was not merely
   * degenerate but silent: the dump test is `++count == dwell`, which zero
   * never satisfies, so it accumulated every frame it was given and
   * emitted nothing until count wrapped. A caller whose dwell came from a
   * computed value that underflowed got silence and unbounded growth.
   *
   * Checked here rather than only in detector2d because detector2d_create
   * forwards its own dwell straight into this call -- one validation at
   * the primitive covers both objects, and a copy in each would be the
   * thing that drifts.
   *
   * NULL is the whole error report by design: the repo's convention
   * (docs/dev/contributing/error-convention.md) is that create() returns
   * NULL for an allocation failure OR an invalid argument, and the binding
   * turns that into MemoryError. */
  {
    float complex ref[16] = { 0 };
    ref[0]                = 1.0f;
    DP_CHECK (corr2d_create (ref, NY, NX, 0, 1, 0, 0) == NULL);
    /* Not vacuous: the neighbouring value builds. */
    corr2d_state_t *ok = corr2d_create (ref, NY, NX, 1, 1, 0, 0);
    DP_CHECK (ok != NULL);
    corr2d_destroy (ok);
    /* The pre-existing output-grid rule still holds. */
    DP_CHECK (corr2d_create (ref, NY, NX, 1, 1, 0, NX - 1) == NULL);
  }

  DP_TEST_END ("test_corr2d_core");
}
