#include "doppler/corr2d/corr2d_core.h"
#include "doppler/dp_complex.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
static float _Complex _rand_cplx (uint32_t *s)
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
_brute_corr2d (const float _Complex *x, const float _Complex *h, size_t ny,
               size_t nx, float _Complex *out)
{
  for (size_t i = 0; i < ny; i++)
    for (size_t j = 0; j < nx; j++)
      {
        float _Complex acc = 0.0f;
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

/* ── DelayDoppler (#2666) helpers ───────────────────────────────────────
 *
 * The direct reference, in double, straight from the definition with no
 * FFT anywhere: per row, the circular correlation against the code,
 *   R[i][j] = sum_p conj(code[p]) * x[i*nx + (p + j) mod nx]
 * (what IFFT(FFT(row) * conj(FFT(code))) / nx expands to), summed over the
 * dwell's frames, then the ny_out-point DFT down each column of the ny
 * rows -- rows ny..ny_out-1 being the zero-pad, they contribute nothing --
 *   D[k][j] = sum_{i < ny} R[i][j] * exp(-2*pi*i*k*i / ny_out)
 * written FFT-shifted: bin k to row (k + ny_out/2) mod ny_out. */
static void
_dd_direct (const float _Complex *const *frames, size_t dwell,
            const float _Complex *code, size_t ny, size_t nx, size_t nyo,
            double _Complex *out)
{
  double _Complex *r = calloc (ny * nx, sizeof *r);
  for (size_t f = 0; f < dwell; f++)
    for (size_t i = 0; i < ny; i++)
      for (size_t j = 0; j < nx; j++)
        for (size_t p = 0; p < nx; p++)
          r[i * nx + j] += conj ((double _Complex)code[p])
                           * (double _Complex)frames[f][i * nx + (p + j) % nx];
  for (size_t k = 0; k < nyo; k++)
    for (size_t j = 0; j < nx; j++)
      {
        double _Complex acc = 0.0;
        for (size_t i = 0; i < ny; i++)
          acc += r[i * nx + j]
                 * cexp (-2.0 * M_PI * I * (double)(k * i % nyo)
                         / (double)nyo);
        out[((k + nyo / 2) % nyo) * nx + j] = acc;
      }
  free (r);
}

/* max |got - want| / max |want| */
static double
_dd_relerr (const float _Complex *got, const double _Complex *want, size_t n)
{
  double emax = 0.0, wmax = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      double e = cabs ((double _Complex)got[k] - want[k]);
      double w = cabs (want[k]);
      emax     = e > emax ? e : emax;
      wmax     = w > wmax ? w : wmax;
    }
  return emax / wmax;
}

/* A unit-modulus code of nx random phases, deterministic per seed. */
static void
_dd_code (float _Complex *code, size_t nx, uint32_t seed)
{
  for (size_t p = 0; p < nx; p++)
    code[p] = cexpf (2.0f * (float)M_PI * I * (float)(dp_xs32 (&seed) % 1000u)
                     / 1000.0f);
}

/* ny code periods delayed by d samples and rotated by f cycles/sample. */
static void
_dd_signal (float _Complex *x, const float _Complex *code, size_t ny,
            size_t nx, size_t d, double f)
{
  for (size_t m = 0; m < ny * nx; m++)
    x[m] = code[(m % nx + nx - d) % nx]
           * (float _Complex)cexp (2.0 * M_PI * I * f * (double)m);
}

/* Index of the largest-magnitude element. */
static size_t
_dd_argmax (const float _Complex *x, size_t n)
{
  size_t best = 0;
  for (size_t k = 1; k < n; k++)
    if (cabsf (x[k]) > cabsf (x[best]))
      best = k;
  return best;
}

static void
_test_delay_doppler (void)
{
  /* ── against the direct reference: shapes, padding, dwell, threads ──── */
  {
    const struct
    {
      size_t ny, nx, ny_out, dwell;
      int    nthreads;
    } cases[] = {
      { 4, 8, 0, 1, 1 }, { 5, 7, 12, 3, 1 },  { 6, 10, 6, 2, 3 },
      { 1, 5, 4, 1, 2 }, { 9, 16, 32, 2, 4 }, { 3, 33, 7, 1, 0 },
    };
    for (size_t c = 0; c < sizeof cases / sizeof *cases; c++)
      {
        const size_t    ny = cases[c].ny, nx = cases[c].nx;
        const size_t    nyo  = cases[c].ny_out ? cases[c].ny_out : ny;
        uint32_t        seed = 7u + (uint32_t)c;
        float _Complex *code = malloc (nx * sizeof *code);
        _dd_code (code, nx, seed);
        float _Complex *frames[3];
        for (size_t f = 0; f < cases[c].dwell; f++)
          {
            frames[f] = malloc (ny * nx * sizeof **frames);
            for (size_t m = 0; m < ny * nx; m++)
              frames[f][m] = _rand_cplx (&seed);
          }
        dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (
            code, nx, ny, cases[c].dwell, cases[c].nthreads, cases[c].ny_out);
        DP_CHECK (dd != NULL);
        DP_CHECK (dp_corr2d_execute_max_out (dd) == nyo * nx);
        float _Complex  *out  = malloc (nyo * nx * sizeof *out);
        double _Complex *want = malloc (nyo * nx * sizeof *want);
        /* Every frame but the last is accumulated silently. */
        for (size_t f = 0; f + 1 < cases[c].dwell; f++)
          {
            DP_CHECK (dp_corr2d_execute (dd, frames[f], ny * nx, out, nyo * nx)
                      == 0);
            DP_CHECK (dd->count == f + 1);
          }
        DP_CHECK (dp_corr2d_execute (dd, frames[cases[c].dwell - 1], ny * nx,
                                     out, nyo * nx)
                  == nyo * nx);
        DP_CHECK (dd->count == 0);
        _dd_direct ((const float _Complex *const *)frames, cases[c].dwell,
                    code, ny, nx, nyo, want);
        double err = _dd_relerr (out, want, nyo * nx);
        if (err > 1e-5)
          printf ("  delay-doppler case %zu: relerr %.3g\n", c, err);
        DP_CHECK (err < 1e-5);
        /* The accumulator is clear: a fresh dwell of the same frames gives
           the same map, to the byte. */
        float _Complex *again = malloc (nyo * nx * sizeof *again);
        for (size_t f = 0; f < cases[c].dwell; f++)
          (void)dp_corr2d_execute (dd, frames[f], ny * nx, again, nyo * nx);
        DP_CHECK (memcmp (out, again, nyo * nx * sizeof *out) == 0);
        free (again);
        for (size_t f = 0; f < cases[c].dwell; f++)
          free (frames[f]);
        free (code);
        free (out);
        free (want);
        dp_corr2d_destroy (dd);
      }
  }

  /* ── a known (delay, Doppler) lands on its (row, col) ─────────────────
   * Delay 7, Doppler -3 native bins (f = -3 / (ny*nx)): column 7, row
   * ny/2 - 3.  A unit-amplitude signal against a unit-modulus code peaks at
   * ny * nx less the intra-period rotation's loss, here 3 bins over the
   * ny*nx span, i.e. 3/ny of a cycle per period: sinc(3/16) = 0.94. */
  {
    const size_t ny = 16, nx = 32, d = 7;
    float _Complex code[32], x[16 * 32], out[16 * 32];
    _dd_code (code, nx, 11u);
    _dd_signal (x, code, ny, nx, d, -3.0 / (double)(ny * nx));
    dp_corr2d_state_t *dd
        = dp_corr2d_create_delay_doppler (code, nx, ny, 1, 1, 0);
    DP_CHECK (dd != NULL);
    DP_CHECK (fabs (dd->doppler_step - 1.0 / (double)(ny * nx)) < 1e-15);
    DP_CHECK (dp_corr2d_execute (dd, x, ny * nx, out, ny * nx) == ny * nx);
    size_t pk = _dd_argmax (out, ny * nx);
    DP_CHECK (pk / nx == ny / 2 - 3 && pk % nx == d);
    DP_CHECK (cabsf (out[pk]) > 0.9f * (float)(ny * nx));
    DP_CHECK (cabsf (out[pk]) <= (float)(ny * nx) * 1.0001f);
    dp_corr2d_destroy (dd);
  }

  /* ── ny_out interpolates: a Doppler between native bins ──────────────
   * f = 2.5 native bins sits exactly between rows 2 and 3 at ny_out = ny;
   * at ny_out = 4*ny the grid is 4x finer and the half bin is a whole row:
   * row ny_out/2 + 10.  doppler_step follows the grid. */
  {
    const size_t ny = 16, nx = 32, nyo = 64, d = 3;
    float _Complex code[32], x[16 * 32];
    float _Complex *out = malloc (nyo * nx * sizeof *out);
    _dd_code (code, nx, 13u);
    _dd_signal (x, code, ny, nx, d, 2.5 / (double)(ny * nx));
    dp_corr2d_state_t *dd
        = dp_corr2d_create_delay_doppler (code, nx, ny, 1, 1, nyo);
    DP_CHECK (dd != NULL);
    DP_CHECK (fabs (dd->doppler_step - 1.0 / (double)(nyo * nx)) < 1e-15);
    DP_CHECK (dp_corr2d_execute (dd, x, ny * nx, out, nyo * nx) == nyo * nx);
    size_t pk = _dd_argmax (out, nyo * nx);
    DP_CHECK (pk / nx == nyo / 2 + 10 && pk % nx == d);
    dp_corr2d_destroy (dd);
    free (out);
  }

  /* ── bit-identical across thread counts (bytes, not tolerance) ───────
   * Odd sizes, so the chunk boundaries fall differently for every count,
   * the last column tile is partial, and the slow-time length is padded. */
  {
    const size_t ny = 37, nx = 100, nyo = 50, n = 37 * 100;
    uint32_t     seed = 99u;
    float _Complex code[100];
    _dd_code (code, nx, 17u);
    float _Complex *x = malloc (2 * n * sizeof *x);
    for (size_t m = 0; m < 2 * n; m++)
      x[m] = _rand_cplx (&seed);
    const int       threads[] = { 1, 2, 7 };
    float _Complex *outs[3];
    for (size_t t = 0; t < 3; t++)
      {
        dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (
            code, nx, ny, 2, threads[t], nyo);
        DP_CHECK (dd != NULL);
        DP_CHECK (dd->nworkers == threads[t]);
        outs[t] = malloc (nyo * nx * sizeof **outs);
        (void)dp_corr2d_execute (dd, x, n, outs[t], nyo * nx);
        DP_CHECK (dp_corr2d_execute (dd, x + n, n, outs[t], nyo * nx)
                  == nyo * nx);
        dp_corr2d_destroy (dd);
      }
    DP_CHECK (memcmp (outs[0], outs[1], nyo * nx * sizeof **outs) == 0);
    DP_CHECK (memcmp (outs[0], outs[2], nyo * nx * sizeof **outs) == 0);
    for (size_t t = 0; t < 3; t++)
      free (outs[t]);

    /* Corr2D's fast path honors nthreads the same way. */
    float _Complex *ref = calloc (n, sizeof *ref);
    memcpy (ref, code, nx * sizeof *code);
    float _Complex *c_out[2];
    for (size_t t = 0; t < 2; t++)
      {
        dp_corr2d_state_t *c
            = dp_corr2d_create (ref, ny, nx, 1, t ? 7 : 1, 0, 0, -1);
        DP_CHECK (c != NULL && c->fast_path == 1);
        DP_CHECK (c->nworkers == (t ? 7 : 1));
        c_out[t] = malloc (n * sizeof **c_out);
        DP_CHECK (dp_corr2d_execute (c, x, n, c_out[t], n) == n);
        dp_corr2d_destroy (c);
      }
    DP_CHECK (memcmp (c_out[0], c_out[1], n * sizeof **c_out) == 0);

    /* A huge request is capped at the most chunks a stage can use --
       Corr2D: ny rows; delay-Doppler: max(ny, ceil(nx / 16)) = 37 here --
       and the output is still the same bytes. */
    dp_corr2d_state_t *big = dp_corr2d_create (ref, ny, nx, 1, 2000, 0, 0, -1);
    DP_CHECK (big != NULL && big->nworkers == (int)ny);
    DP_CHECK (dp_corr2d_execute (big, x, n, c_out[1], n) == n);
    DP_CHECK (memcmp (c_out[0], c_out[1], n * sizeof **c_out) == 0);
    dp_corr2d_destroy (big);
    float _Complex *dd_out[2];
    for (size_t t = 0; t < 2; t++)
      {
        dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (
            code, nx, ny, 1, t ? 2000 : 1, nyo);
        DP_CHECK (dd != NULL);
        DP_CHECK (dd->nworkers == (t ? 37 : 1));
        dd_out[t] = malloc (nyo * nx * sizeof **dd_out);
        DP_CHECK (dp_corr2d_execute (dd, x, n, dd_out[t], nyo * nx)
                  == nyo * nx);
        dp_corr2d_destroy (dd);
      }
    DP_CHECK (memcmp (dd_out[0], dd_out[1], nyo * nx * sizeof **dd_out) == 0);
    /* Wide and short: the column stage has more chunks than the rows. */
    {
      float _Complex wcode[640];
      _dd_code (wcode, 640, 31u);
      dp_corr2d_state_t *w
          = dp_corr2d_create_delay_doppler (wcode, 640, 4, 1, 2000, 0);
      DP_CHECK (w != NULL && w->nworkers == 40);
      dp_corr2d_destroy (w);
    }
    /* The known-lag path never runs the row stage: no pool, one worker. */
    dp_corr2d_state_t *lag = dp_corr2d_create (ref, ny, nx, 1, 8, 0, 0, 3);
    DP_CHECK (lag != NULL && lag->pool == NULL && lag->nworkers == 1);
    dp_corr2d_destroy (lag);
    free (dd_out[0]);
    free (dd_out[1]);
    free (c_out[0]);
    free (c_out[1]);
    free (ref);
    free (x);
  }

  /* ── refusals ─────────────────────────────────────────────────────── */
  {
    float _Complex code[8] = { 1.0f };
    DP_CHECK (dp_corr2d_create_delay_doppler (NULL, 8, 4, 1, 1, 0) == NULL);
    DP_CHECK (dp_corr2d_create_delay_doppler (code, 0, 4, 1, 1, 0) == NULL);
    DP_CHECK (dp_corr2d_create_delay_doppler (code, 8, 0, 1, 1, 0) == NULL);
    DP_CHECK (dp_corr2d_create_delay_doppler (code, 8, 4, 0, 1, 0) == NULL);
    DP_CHECK (dp_corr2d_create_delay_doppler (code, 8, 4, 1, 1, 3) == NULL);
    /* ny_out == ny is the native grid, not a refusal. */
    dp_corr2d_state_t *ok
        = dp_corr2d_create_delay_doppler (code, 8, 4, 1, 1, 4);
    DP_CHECK (ok != NULL);
    dp_corr2d_destroy (ok);
  }

  /* ── serializable state: resume, flavor and key rejects ──────────── */
  {
    const size_t ny = 4, nx = 8, n = 32;
    float _Complex code[8], x[32];
    uint32_t seed = 5u;
    _dd_code (code, nx, 23u);
    for (size_t m = 0; m < n; m++)
      x[m] = _rand_cplx (&seed);
    dp_corr2d_state_t *a
        = dp_corr2d_create_delay_doppler (code, nx, ny, 3, 1, 8);
    dp_corr2d_state_t *b
        = dp_corr2d_create_delay_doppler (code, nx, ny, 3, 2, 8);
    DP_CHECK (a != NULL && b != NULL);
    float _Complex out_a[64], out_b[64];
    (void)dp_corr2d_execute (a, x, n, out_a, 64);
    DP_STATE_ROUNDTRIP_TEST (dp_corr2d, a, b);
    /* Mid-dwell split: b resumes a's dwell and dumps the same map. */
    {
      void *blob = malloc (dp_corr2d_state_bytes (a));
      dp_corr2d_get_state (a, blob);
      DP_CHECK (dp_corr2d_set_state (b, blob) == DP_OK);
      free (blob);
    }
    for (int f = 0; f < 2; f++)
      {
        (void)dp_corr2d_execute (a, x, n, out_a, 64);
        (void)dp_corr2d_execute (b, x, n, out_b, 64);
      }
    DP_CHECK (memcmp (out_a, out_b, sizeof out_a) == 0);

    /* Cross-flavor: a Corr2D blob over the same (ny, nx) and code, and the
       delay-Doppler blob back into that Corr2D, are both refused. */
    float _Complex ref[32] = { 0 };
    memcpy (ref, code, sizeof code);
    dp_corr2d_state_t *c = dp_corr2d_create (ref, ny, nx, 3, 1, 0, 0, -1);
    DP_CHECK (c != NULL);
    {
      void *cb = malloc (dp_corr2d_state_bytes (c));
      void *db = malloc (dp_corr2d_state_bytes (a));
      dp_corr2d_get_state (c, cb);
      dp_corr2d_get_state (a, db);
      DP_CHECK (dp_corr2d_set_state (a, cb) == DP_ERR_INVALID);
      DP_CHECK (dp_corr2d_set_state (c, db) == DP_ERR_INVALID);
      free (cb);
      free (db);
    }
    dp_corr2d_destroy (c);

    /* Reject keys at an equal blob size: ny_out 8 vs 6 (the map), ny 4 vs
       8 over the same ny*nx (a (4, 8) accumulator read as (8, 4)), and a
       count the target's shorter dwell would never dump. */
    {
      (void)dp_corr2d_execute (a, x, n, out_a, 64);
      (void)dp_corr2d_execute (a, x, n, out_a, 64); /* count 2 of 3 */
      DP_CHECK (a->count == 2);
      void *blob = malloc (dp_corr2d_state_bytes (a));
      dp_corr2d_get_state (a, blob);
      dp_corr2d_state_t *o6
          = dp_corr2d_create_delay_doppler (code, nx, ny, 3, 1, 6);
      dp_corr2d_state_t *t8
          = dp_corr2d_create_delay_doppler (code, 4, 8, 3, 1, 8);
      dp_corr2d_state_t *d2
          = dp_corr2d_create_delay_doppler (code, nx, ny, 2, 1, 8);
      DP_CHECK (o6 && t8 && d2);
      DP_CHECK (dp_corr2d_state_bytes (o6) == dp_corr2d_state_bytes (a));
      DP_CHECK (dp_corr2d_state_bytes (t8) == dp_corr2d_state_bytes (a));
      DP_CHECK (dp_corr2d_set_state (o6, blob) == DP_ERR_INVALID);
      DP_CHECK (dp_corr2d_set_state (t8, blob) == DP_ERR_INVALID);
      DP_CHECK (dp_corr2d_set_state (d2, blob) == DP_ERR_INVALID);
      DP_CHECK (o6->count == 0 && t8->count == 0 && d2->count == 0);
      dp_corr2d_destroy (o6);
      dp_corr2d_destroy (t8);
      dp_corr2d_destroy (d2);
      free (blob);
    }
    dp_corr2d_destroy (a);
    dp_corr2d_destroy (b);
  }
}

/* ── Batches (#2683) ──────────────────────────────────────────────────────
 *
 * One object of each path: general 2-D (native and interpolated), the
 * single-row fast path (native and nx_out > nx), the known column, and the
 * delay-Doppler flavor with ny_out > ny.  Every reference below comes from
 * the object's own public execute(), so a batch is held to what the object
 * already promises, bit for bit. */
enum
{
  B_GEN,
  B_GEN_PAD,
  B_FAST,
  B_FAST_PAD,
  B_COL,
  B_DD,
  B_KINDS
};

static dp_corr2d_state_t *
_batch_make (int kind, size_t dwell, int nthreads)
{
  uint32_t seed = 0x2683u + (uint32_t)kind;
  float _Complex ref[42];
  const size_t ny = (kind <= B_GEN_PAD) ? 3 : 6;
  const size_t nx = (kind <= B_GEN_PAD) ? 5 : 7;
  for (size_t k = 0; k < ny * nx; k++)
    {
      const float _Complex z = _rand_cplx (&seed);
      /* The general path needs energy outside row 0; the others refuse it. */
      ref[k] = (kind <= B_GEN_PAD || k < nx) ? z : 0.0f;
    }
  switch (kind)
    {
    case B_GEN:
      return dp_corr2d_create (ref, ny, nx, dwell, nthreads, 0, 0, -1);
    case B_GEN_PAD:
      return dp_corr2d_create (ref, ny, nx, dwell, nthreads, 4, 8, -1);
    case B_FAST:
      return dp_corr2d_create (ref, ny, nx, dwell, nthreads, 0, 0, -1);
    case B_FAST_PAD:
      return dp_corr2d_create (ref, ny, nx, dwell, nthreads, 0, 10, -1);
    case B_COL:
      return dp_corr2d_create (ref, ny, nx, dwell, nthreads, 0, 0, 2);
    default:
      return dp_corr2d_create_delay_doppler (ref, nx, 5, dwell, nthreads, 8);
    }
}

/* H frames of the object's frame length, deterministic per seed. */
static float _Complex *
_batch_frames (const dp_corr2d_state_t *c, size_t H, uint32_t seed)
{
  float _Complex *x = malloc (H * c->n * sizeof *x);
  for (size_t k = 0; k < H * c->n; k++)
    x[k] = _rand_cplx (&seed);
  return x;
}

/* The maps H fresh dwell-1, one-thread objects return for the frames. */
static float _Complex *
_batch_reference (int kind, const float _Complex *x, size_t H)
{
  dp_corr2d_state_t *f = _batch_make (kind, 1, 1);
  const size_t       n = f->n, no = f->n_out;
  float _Complex    *ref = malloc (H * no * sizeof *ref);
  for (size_t h = 0; h < H; h++)
    {
      dp_corr2d_destroy (f);
      f = _batch_make (kind, 1, 1);
      DP_CHECK (dp_corr2d_execute (f, x + h * n, n, ref + h * no, no) == no);
    }
  dp_corr2d_destroy (f);
  return ref;
}

static float
_batch_pow (float _Complex z)
{
  return crealf (z) * crealf (z) + cimagf (z) * cimagf (z);
}

static int
_close (float got, float want)
{
  return fabsf (got - want) <= 1e-5f * fmaxf (1.0f, fabsf (want));
}

/* ── the best map by its own statistic is not the max map's: #2683 ─────
 * The request's requirement, built so the two answers disagree. Map 0 is
 * a clean code at delay 5: a sharp peak over a low mean. Map 1 is the
 * code 1.5x stronger at delay 40 under noise: the strongest single cell,
 * but a high mean. Ranked by peak-to-mean, PEAKS must pick map 0, while
 * POWER_MAX's strongest cell comes from map 1 -- and the peak-to-mean of
 * the pointwise-max map is not map 0's. A PEAKS read off the pointwise
 * max would give both records the same statistic, which the strict
 * comparison below refuses. */
static int
_test_batch_ranking (void)
{
  enum
  {
    DNX = 64,
    DNY = 8,
    DN  = DNX * DNY,
    D0  = 5,
    D1  = 40
  };
  uint32_t seed = 0x2683bu, rng = 0x5eedu;
  float _Complex code[DNX];
  for (size_t j = 0; j < DNX; j++)
    code[j] = crealf (_rand_cplx (&seed)) >= 0.0f ? 1.0f : -1.0f;
  static float _Complex x[2 * DN];
  for (size_t m = 0; m < DN; m++)
    {
      x[m] = code[(m % DNX + DNX - D0) % DNX];
      x[DN + m]
          = 1.5f * code[(m % DNX + DNX - D1) % DNX] + 3.0f * dp_cgauss (&rng);
    }
  dp_corr2d_state_t *dd
      = dp_corr2d_create_delay_doppler (code, DNX, DNY, 1, 2, 0);
  DP_REQUIRE (dd != NULL && dd->n == DN && dd->n_out == DN);
  float    pk[2], mn[2], pm[DN];
  uint32_t pr[2], pc[2], pi[DN];
  DP_REQUIRE (dp_corr2d_execute_peaks (dd, x, 2 * DN, pk, pr, pc, mn, 2) == 2);
  DP_REQUIRE (dp_corr2d_execute_power_max (dd, x, 2 * DN, pm, pi, DN) == DN);
  /* each map's own peak sits at its own code delay */
  DP_CHECK (pc[0] == D0 && pc[1] == D1);
  /* ranked by its own statistic, map 0 wins, strictly */
  DP_CHECK_MSG (pk[0] / mn[0] > pk[1] / mn[1],
                "peak-to-mean ranks the clean map first");
  /* the strongest cell over every map is map 1's, at its delay */
  size_t best = 0;
  double sum  = 0.0;
  for (size_t k = 0; k < DN; k++)
    {
      sum += pm[k];
      if (pm[k] > pm[best])
        best = k;
    }
  DP_CHECK (pi[best] == 1 && best % DNX == D1);
  /* and the pointwise-max map's own peak-to-mean is not map 0's */
  DP_CHECK_MSG (fabs ((double)pm[best] / (sum / DN) - (double)(pk[0] / mn[0]))
                    > 0.1 * (double)(pk[0] / mn[0]),
                "the max map's statistic differs from the best map's");
  dp_corr2d_destroy (dd);
  return 0;
}

static void
_test_batch (void)
{
  const int    threads[] = { 1, 2, 7 };
  const size_t Hs[]      = { 1, 3, 9 }; /* 9 >= workers: whole maps */

  for (int kind = 0; kind < B_KINDS; kind++)
    for (size_t hi = 0; hi < sizeof Hs / sizeof *Hs; hi++)
      for (size_t ti = 0; ti < sizeof threads / sizeof *threads; ti++)
        {
          const size_t       H = Hs[hi];
          dp_corr2d_state_t *c = _batch_make (kind, 3, threads[ti]);
          DP_CHECK (c != NULL);
          const size_t    n = c->n, no = c->n_out;
          float _Complex *x    = _batch_frames (c, H, 0x51u + (uint32_t)H);
          float _Complex *want = _batch_reference (kind, x, H);

          /* A dwell in progress: one frame in, its blob kept. */
          float _Complex *dump = malloc (no * sizeof *dump);
          DP_CHECK (dp_corr2d_execute (c, x, n, dump, no) == 0);
          const size_t   nb     = dp_corr2d_state_bytes (c);
          unsigned char *before = malloc (nb), *after = malloc (nb);
          dp_corr2d_get_state (c, before);

          /* ALL: every map is the fresh object's, bit for bit. */
          float _Complex *all = malloc (H * no * sizeof *all);
          DP_CHECK (dp_corr2d_execute_all_max_out (c, H * n) == H * no);
          DP_CHECK (dp_corr2d_execute_all (c, x, H * n, all, H * no)
                    == H * no);
          DP_CHECK_MSG (memcmp (all, want, H * no * sizeof *all) == 0,
                        "batch map h == a fresh dwell-1 object's map");

          /* COHERENT: bit for bit a fresh object with dwell = H; and the
             sum of the maps up to rounding. */
          float _Complex *coh = malloc (no * sizeof *coh);
          float _Complex *cw  = malloc (no * sizeof *cw);
          {
            dp_corr2d_state_t *f = _batch_make (kind, H, 1);
            for (size_t h = 0; h < H; h++)
              (void)dp_corr2d_execute (f, x + h * n, n, cw, no);
            dp_corr2d_destroy (f);
          }
          DP_CHECK (dp_corr2d_execute_coherent (c, x, H * n, coh, no) == no);
          DP_CHECK_MSG (memcmp (coh, cw, no * sizeof *coh) == 0,
                        "coherent == a fresh object with dwell = H");
          {
            double emax = 0.0, wmax = 0.0;
            for (size_t k = 0; k < no; k++)
              {
                float _Complex sum = 0.0f;
                for (size_t h = 0; h < H; h++)
                  sum += all[h * no + k];
                emax = fmax (emax, cabsf (coh[k] - sum));
                wmax = fmax (wmax, cabsf (sum));
              }
            DP_CHECK (emax <= 1e-5 * wmax);
          }

          /* POWER_SUM, POWER_MAX and PEAKS against folds over ALL. */
          float *ps = malloc (no * sizeof *ps), *pm = malloc (no * sizeof *pm);
          uint32_t *pi = malloc (no * sizeof *pi);
          float *pk = malloc (H * sizeof *pk), *mn = malloc (H * sizeof *mn);
          uint32_t *pr = malloc (H * sizeof *pr),
                   *pc = malloc (H * sizeof *pc);
          DP_CHECK (dp_corr2d_execute_power_sum (c, x, H * n, ps, no) == no);
          DP_CHECK (dp_corr2d_execute_power_max (c, x, H * n, pm, pi, no)
                    == no);
          DP_CHECK (dp_corr2d_execute_peaks_max_out (c, H * n) == H);
          DP_CHECK (dp_corr2d_execute_peaks (c, x, H * n, pk, pr, pc, mn, H)
                    == H);
          int sum_ok = 1, max_ok = 1, peak_ok = 1;
          for (size_t k = 0; k < no; k++)
            {
              float    s = 0.0f, m = -1.0f;
              uint32_t at = 0;
              for (size_t h = 0; h < H; h++)
                {
                  const float p = _batch_pow (all[h * no + k]);
                  s += p;
                  if (p > m)
                    {
                      m  = p;
                      at = (uint32_t)h;
                    }
                }
              sum_ok &= _close (ps[k], s);
              max_ok &= _close (pm[k], m) && pi[k] == at;
            }
          const size_t width = (kind == B_COL) ? 1 : c->nx_out;
          for (size_t h = 0; h < H; h++)
            {
              const float _Complex *m    = all + h * no;
              size_t                best = 0;
              double                s    = 0.0;
              for (size_t k = 0; k < no; k++)
                {
                  s += _batch_pow (m[k]);
                  if (_batch_pow (m[k]) > _batch_pow (m[best]))
                    best = k;
                }
              const uint32_t col
                  = (kind == B_COL) ? 2u : (uint32_t)(best % width);
              peak_ok &= _close (pk[h], _batch_pow (m[best]))
                         && pr[h] == best / width && pc[h] == col
                         && _close (mn[h], (float)(s / (double)no));
            }
          /* The folds run in h order whatever the thread count: the same
             reductions from a one-thread object, bit for bit. */
          {
            dp_corr2d_state_t *one = _batch_make (kind, 1, 1);
            float             *ps1 = malloc (no * sizeof *ps1);
            float             *pm1 = malloc (no * sizeof *pm1);
            uint32_t          *pi1 = malloc (no * sizeof *pi1);
            float             *pk1 = malloc (H * sizeof *pk1);
            float             *mn1 = malloc (H * sizeof *mn1);
            uint32_t          *pr1 = malloc (H * sizeof *pr1);
            uint32_t          *pc1 = malloc (H * sizeof *pc1);
            (void)dp_corr2d_execute_power_sum (one, x, H * n, ps1, no);
            (void)dp_corr2d_execute_power_max (one, x, H * n, pm1, pi1, no);
            (void)dp_corr2d_execute_peaks (one, x, H * n, pk1, pr1, pc1, mn1,
                                           H);
            DP_CHECK_MSG (memcmp (ps, ps1, no * sizeof *ps) == 0
                              && memcmp (pm, pm1, no * sizeof *pm) == 0
                              && memcmp (pi, pi1, no * sizeof *pi) == 0
                              && memcmp (pk, pk1, H * sizeof *pk) == 0
                              && memcmp (mn, mn1, H * sizeof *mn) == 0
                              && memcmp (pr, pr1, H * sizeof *pr) == 0
                              && memcmp (pc, pc1, H * sizeof *pc) == 0,
                          "reductions are bit-identical at any thread count");
            free (ps1), free (pm1), free (pi1), free (pk1), free (mn1),
                free (pr1), free (pc1);
            dp_corr2d_destroy (one);
          }
          DP_CHECK_MSG (sum_ok, "power_sum == sum over h of |map|^2");
          DP_CHECK_MSG (max_ok, "power_max == max over h, lowest h on a tie");
          DP_CHECK_MSG (peak_ok, "peaks == each map's own peak and mean");

          /* None of it touched the dwell: same blob, and the dwell's next
             dumps equal a twin's that never saw a batch. */
          dp_corr2d_get_state (c, after);
          DP_CHECK_MSG (memcmp (before, after, nb) == 0,
                        "a batch leaves accum and count alone");
          {
            dp_corr2d_state_t *twin = _batch_make (kind, 3, 1);
            float _Complex    *td   = malloc (no * sizeof *td);
            (void)dp_corr2d_execute (twin, x, n, td, no);
            for (size_t f = 1; f < 3; f++)
              {
                const float _Complex *xf = x + (f % H) * n;
                DP_CHECK (dp_corr2d_execute (c, xf, n, dump, no)
                          == dp_corr2d_execute (twin, xf, n, td, no));
              }
            DP_CHECK (memcmp (dump, td, no * sizeof *td) == 0);
            dp_corr2d_destroy (twin);
            free (td);
          }

          free (ps), free (pm), free (pi), free (pk), free (mn), free (pr),
              free (pc);
          free (coh), free (cw), free (all), free (before), free (after);
          free (dump), free (want), free (x);
          dp_corr2d_destroy (c);
        }

  /* ── coherent cancels, power does not: frames x and -x ──────────────── */
  for (int kind = 0; kind < B_KINDS; kind++)
    {
      dp_corr2d_state_t *c = _batch_make (kind, 1, 2);
      const size_t       n = c->n, no = c->n_out;
      float _Complex    *x  = _batch_frames (c, 2, 7u);
      float _Complex    *m  = malloc (no * sizeof *m);
      float _Complex    *co = malloc (no * sizeof *co);
      float             *ps = malloc (no * sizeof *ps);
      for (size_t k = 0; k < n; k++)
        x[n + k] = -x[k];
      DP_CHECK (dp_corr2d_execute (c, x, n, m, no) == no);
      DP_CHECK (dp_corr2d_execute_coherent (c, x, 2 * n, co, no) == no);
      DP_CHECK (dp_corr2d_execute_power_sum (c, x, 2 * n, ps, no) == no);
      int cancel = 1, doubled = 1;
      for (size_t k = 0; k < no; k++)
        {
          cancel &= cabsf (co[k]) <= 1e-5f * (1.0f + cabsf (m[k]));
          doubled &= _close (ps[k], 2.0f * _batch_pow (m[k]));
        }
      DP_CHECK_MSG (cancel, "coherent sum of x and -x is zero");
      DP_CHECK_MSG (doubled, "power sum of x and -x is twice one map");
      free (x), free (m), free (co), free (ps);
      dp_corr2d_destroy (c);
    }

  /* ── ties go to the lowest h: identical frames ──────────────────────── */
  for (int kind = 0; kind < B_KINDS; kind++)
    {
      dp_corr2d_state_t *c = _batch_make (kind, 1, 7);
      const size_t       n = c->n, no = c->n_out, H = 9;
      float _Complex    *x  = _batch_frames (c, H, 9u);
      float             *pm = malloc (no * sizeof *pm);
      uint32_t          *pi = malloc (no * sizeof *pi);
      for (size_t h = 1; h < H; h++)
        memcpy (x + h * n, x, n * sizeof *x);
      DP_CHECK (dp_corr2d_execute_power_max (c, x, H * n, pm, pi, no) == no);
      int zero = 1;
      for (size_t k = 0; k < no; k++)
        zero &= pi[k] == 0;
      DP_CHECK_MSG (zero, "equal maps: power_max index is 0 everywhere");
      free (x), free (pm), free (pi);
      dp_corr2d_destroy (c);
    }

  /* ── refusals: nothing written, nothing folded ──────────────────────── */
  for (int kind = 0; kind < B_KINDS; kind++)
    {
      dp_corr2d_state_t *c = _batch_make (kind, 1, 2);
      const size_t       n = c->n, no = c->n_out, H = 2;
      float _Complex    *x = _batch_frames (c, H + 1, 3u);
      enum
      {
        CAN = 1 << 12
      };
      static float _Complex cf[CAN];
      static float    ff[CAN], f3[CAN];
      static uint32_t u1[CAN], u2[CAN];
      DP_CHECK (H * no <= CAN);
      for (size_t k = 0; k < CAN; k++)
        {
          cf[k] = CMPLXF (-7.0f, -7.0f);
          ff[k] = f3[k] = -7.0f;
          u1[k] = u2[k] = 77u;
        }
      const size_t bad[] = { 0, 1, n - 1, n + 1, 2 * n + 1 };
      for (size_t b = 0; b < sizeof bad / sizeof *bad; b++)
        {
          const size_t m = bad[b];
          DP_CHECK (dp_corr2d_execute_all (c, x, m, cf, CAN) == SIZE_MAX);
          DP_CHECK (dp_corr2d_execute_coherent (c, x, m, cf, CAN) == SIZE_MAX);
          DP_CHECK (dp_corr2d_execute_power_sum (c, x, m, ff, CAN)
                    == SIZE_MAX);
          DP_CHECK (dp_corr2d_execute_power_max (c, x, m, ff, u1, CAN)
                    == SIZE_MAX);
          DP_CHECK (dp_corr2d_execute_peaks (c, x, m, ff, u1, u2, f3, CAN)
                    == SIZE_MAX);
          size_t           calls = 0;
          dp_corr2d_fold_t f     = { NULL, &calls };
          /* A NULL fold would crash if called: refusal must come first. */
          DP_CHECK (dp_corr2d_execute_fold (c, x, m, &f) == SIZE_MAX);
        }
      /* Room for one value less than the whole result. */
      DP_CHECK (dp_corr2d_execute_all (c, x, H * n, cf, H * no - 1)
                == SIZE_MAX);
      DP_CHECK (dp_corr2d_execute_coherent (c, x, H * n, cf, no - 1)
                == SIZE_MAX);
      DP_CHECK (dp_corr2d_execute_power_sum (c, x, H * n, ff, no - 1)
                == SIZE_MAX);
      DP_CHECK (dp_corr2d_execute_power_max (c, x, H * n, ff, u1, no - 1)
                == SIZE_MAX);
      DP_CHECK (dp_corr2d_execute_peaks (c, x, H * n, ff, u1, u2, f3, H - 1)
                == SIZE_MAX);
      int untouched = 1;
      for (size_t k = 0; k < CAN; k++)
        untouched &= crealf (cf[k]) == -7.0f && ff[k] == -7.0f
                     && f3[k] == -7.0f && u1[k] == 77u && u2[k] == 77u;
      DP_CHECK_MSG (untouched, "a refused batch writes nothing");
      free (x);
      dp_corr2d_destroy (c);
    }
}

/* ── The hypothesis bank (#2683 phase 3) ─────────────────────────────────
 * Every claim of dp_corr2d_bank_fold, _power_max and _detect against an
 * oracle built from the object's own execute(): map h of a bank is a fresh
 * dwell-1 object's map of the frame corrected by hand. */

/* Every map of a bank, copied out in fold order. */
typedef struct
{
  float _Complex *maps;
  size_t          n_out, calls, order_ok;
} _bank_copy_t;

static void
_bank_copy (void *ctx, size_t h, const float _Complex *map, size_t n_out)
{
  _bank_copy_t *c = (_bank_copy_t *)ctx;
  c->order_ok &= (h == c->calls); /* maps arrive in h order */
  memcpy (c->maps + h * n_out, map, n_out * sizeof *map);
  c->calls++;
}

/* The frame corrected by hand: period i times rate w[i] (when given) times
   exp(-j 2 pi g n / (L nx)), n counted within the period, in double. */
static void
_bank_correct (const float _Complex *x, size_t ny, size_t nx, long g, size_t L,
               const float _Complex *w, float _Complex *y)
{
  for (size_t i = 0; i < ny; i++)
    for (size_t n = 0; n < nx; n++)
      {
        const double a
            = -2.0 * M_PI * (double)g * (double)n / ((double)L * (double)nx);
        double _Complex v
            = (double _Complex)x[i * nx + n] * (cos (a) + I * sin (a));
        if (w)
          v *= (double _Complex)w[i];
        y[i * nx + n] = (float _Complex)v;
      }
}

/* Every object a bank runs on: Corr2D's fast path (native, and with an
   interpolated delay axis), its general 2-D path and a known column, and
   DelayDoppler with an interpolated Doppler axis.  Same code, so the oracle
   is one execute() away. */
enum
{
  K_FAST,
  K_FAST_PAD,
  K_GEN,
  K_COL,
  K_DD,
  K_BANK_KINDS
};

static dp_corr2d_state_t *
_bank_make (int kind, const float _Complex *code, size_t ny, size_t nx,
            int nthreads)
{
  float _Complex *ref = calloc (ny * nx, sizeof *ref);
  memcpy (ref, code, nx * sizeof *ref);
  /* the general path needs energy outside row 0: the code, scaled per row */
  if (kind == K_GEN)
    for (size_t i = 1; i < ny; i++)
      for (size_t p = 0; p < nx; p++)
        ref[i * nx + p] = code[p] * (float)(i + 1) * 0.5f;
  dp_corr2d_state_t *c
      = kind == K_FAST ? dp_corr2d_create (ref, ny, nx, 1, nthreads, 0, 0, -1)
        : kind == K_FAST_PAD
            ? dp_corr2d_create (ref, ny, nx, 1, nthreads, 0, 2 * nx, -1)
        : kind == K_GEN ? dp_corr2d_create (ref, ny, nx, 1, nthreads, 0, 0, -1)
        : kind == K_COL ? dp_corr2d_create (ref, ny, nx, 1, nthreads, 0, 0, 3)
                        : dp_corr2d_create_delay_doppler (code, nx, ny, 1,
                                                          nthreads, 2 * ny);
  free (ref);
  return c;
}

static int
_test_bank (void)
{
  enum
  {
    BNY = 6,
    BNX = 20, /* not a power of two: the general FFT, and a 2-element roll */
    BN  = BNY * BNX
  };
  const size_t L    = 3;            /* not a power of two either */
  const int    gmin = -7, gmax = 5; /* negative and positive, both ends
                                      off the whole-bin grid */
  const size_t C    = (size_t)(gmax - gmin + 1);
  uint32_t     seed = 0xBA4Cu;
  float _Complex code[BNX], x[BN], rates[2 * BNY];
  for (size_t p = 0; p < BNX; p++)
    code[p] = crealf (_rand_cplx (&seed)) >= 0.0f ? 1.0f : -1.0f;
  for (size_t k = 0; k < BN; k++)
    x[k] = _rand_cplx (&seed);
  for (size_t k = 0; k < 2 * BNY; k++)
    {
      const double a = 2.0 * M_PI * (double)(_rand_cplx (&seed) + 1.0f);
      rates[k]       = (float)cos (a) + (float)sin (a) * I;
    }

  for (int kind = 0; kind < K_BANK_KINDS; kind++)
    {
      /* rates on every kind: per-period weights, as defined */
      const size_t          R    = 2;
      const float _Complex *rv   = rates;
      const size_t          rl   = 2 * BNY;
      float _Complex       *ref1 = NULL;
      size_t                no   = 0;
      for (int ti = 0; ti < 3; ti++)
        {
          static const int   th[3] = { 1, 2, 7 };
          dp_corr2d_state_t *c     = _bank_make (kind, code, BNY, BNX, th[ti]);
          DP_REQUIRE (c != NULL);
          no                    = c->n_out;
          float _Complex  *maps = malloc (C * R * no * sizeof *maps);
          _bank_copy_t     cp   = { maps, no, 0, 1 };
          dp_corr2d_fold_t f    = { _bank_copy, &cp };

          /* mid-dwell, with its blob kept, to show the bank leaves it */
          const size_t   nb     = dp_corr2d_state_bytes (c);
          unsigned char *before = malloc (nb), *after = malloc (nb);
          dp_corr2d_get_state (c, before);

          DP_CHECK (dp_corr2d_bank_fold (c, x, BN, L, gmin, gmax, rv, rl, &f)
                    == C * R);
          DP_CHECK_MSG (cp.calls == C * R && cp.order_ok,
                        "bank: one map per hypothesis, in h order");
          dp_corr2d_get_state (c, after);
          DP_CHECK_MSG (memcmp (before, after, nb) == 0,
                        "bank: the dwell's blob is untouched");

          /* the oracle: a fresh dwell-1 object per corrected frame */
          dp_corr2d_state_t *o    = _bank_make (kind, code, BNY, BNX, 1);
          float _Complex    *y    = malloc (BN * sizeof *y);
          float _Complex    *want = malloc (no * sizeof *want);
          double             err = 0.0, peak = 0.0;
          for (size_t ci = 0; ci < C; ci++)
            for (size_t r = 0; r < R; r++)
              {
                _bank_correct (x, BNY, BNX, gmin + (long)ci, L,
                               rv ? rv + r * BNY : NULL, y);
                DP_CHECK (dp_corr2d_execute (o, y, BN, want, no) == no);
                const float _Complex *got = maps + (ci * R + r) * no;
                for (size_t k = 0; k < no; k++)
                  {
                    err  = fmax (err, cabs (got[k] - want[k]));
                    peak = fmax (peak, cabs (want[k]));
                  }
              }
          DP_CHECK_MSG (err <= 1e-5 * peak,
                        "bank map h == the hand-corrected frame's map");

          /* every worker count gives the same maps, bit for bit */
          if (ti == 0)
            {
              ref1 = malloc (C * R * no * sizeof *ref1);
              memcpy (ref1, maps, C * R * no * sizeof *maps);
            }
          else
            DP_CHECK_MSG (memcmp (ref1, maps, C * R * no * sizeof *maps) == 0,
                          "bank maps are bit-identical for any nthreads");

          /* power_max is the bank's POWER_MAX: pointwise max, lowest h */
          float    *pw = malloc (no * sizeof *pw);
          uint32_t *pi = malloc (no * sizeof *pi);
          DP_CHECK (dp_corr2d_bank_power_max_max_out (c, BN) == no);
          DP_CHECK (dp_corr2d_bank_power_max (c, x, BN, L, gmin, gmax, rv, rl,
                                              pw, pi, no)
                    == no);
          int pm_ok = 1;
          for (size_t k = 0; k < no; k++)
            {
              size_t bh = 0;
              float  bp = -1.0f;
              for (size_t h = 0; h < C * R; h++)
                {
                  const float _Complex z = maps[h * no + k];
                  const double re = crealf (z), im = cimagf (z);
                  const float  p = (float)(re * re + im * im);
                  if (p > bp)
                    bp = p, bh = h;
                }
              pm_ok &= pw[k] == bp && pi[k] == bh;
            }
          DP_CHECK_MSG (pm_ok, "bank_power_max == max over the bank's maps");
          free (pw), free (pi);
          free (y), free (want), free (maps), free (before), free (after);
          dp_corr2d_destroy (o);
          dp_corr2d_destroy (c);
        }
      free (ref1);
    }

  /* ── an offset between grid points loses at most sinc(1/(2L)) ────────────
   * A noiseless code at a continuous carrier phi (in whole bins), on
   * DelayDoppler: the bank's strongest cell against the exact correction's,
   * at several phi including the worst, half a grid step from both
   * neighbors, where the loss is the bound itself. */
  {
    enum
    {
      ONY = 8,
      ONX = 64,
      ON  = ONY * ONX
    };
    float _Complex oc[ONX], ox[ON], oy[ON], map[ON];
    float    pw[ON];
    uint32_t pi[ON];
    for (size_t p = 0; p < ONX; p++)
      oc[p] = crealf (_rand_cplx (&seed)) >= 0.0f ? 1.0f : -1.0f;
    dp_corr2d_state_t *d
        = dp_corr2d_create_delay_doppler (oc, ONX, ONY, 1, 1, 0);
    DP_REQUIRE (d != NULL);
    /* the midpoints of L = 1, 2, 4 (0.5, 0.25, 0.125 and 0.375) and points
       off them */
    static const double phis[]
        = { 0.0, 0.1, 0.125, 0.25, 0.3, 0.375, 0.5, -0.625 };
    for (size_t L2 = 1; L2 <= 4; L2 *= 2)
      {
        const double bound
            = sin (M_PI / (2.0 * (double)L2)) / (M_PI / (2.0 * (double)L2));
        int ok = 1, tight = 0;
        for (size_t q = 0; q < sizeof phis / sizeof *phis; q++)
          {
            const double phi = phis[q];
            for (size_t m = 0; m < ON; m++)
              {
                const double a = 2.0 * M_PI * phi * (double)m / ONX;
                ox[m]          = oc[(m + ONX - 13) % ONX]
                                 * ((float)cos (a) + (float)sin (a) * I);
              }
            /* the exact correction: the same within-period mix at phi */
            for (size_t i = 0; i < ONY; i++)
              for (size_t n = 0; n < ONX; n++)
                {
                  const double a  = -2.0 * M_PI * phi * (double)n / ONX;
                  oy[i * ONX + n] = ox[i * ONX + n]
                                    * ((float)cos (a) + (float)sin (a) * I);
                }
            DP_REQUIRE (dp_corr2d_execute (d, oy, ON, map, ON) == ON);
            double exact = 0.0;
            for (size_t k = 0; k < ON; k++)
              exact = fmax (exact,
                            (double)(crealf (map[k]) * crealf (map[k])
                                     + cimagf (map[k]) * cimagf (map[k])));
            DP_REQUIRE (dp_corr2d_bank_power_max (d, ox, ON, L2, -2 * (int)L2,
                                                  2 * (int)L2, NULL, 0, pw, pi,
                                                  ON)
                        == ON);
            double best = 0.0;
            for (size_t k = 0; k < ON; k++)
              best = fmax (best, (double)pw[k]);
            const double ratio = sqrt (best / exact);
            ok &= ratio >= bound - 1e-3;
            /* half a step from both grid points: the loss IS the bound */
            const double frac = phi * (double)L2 - floor (phi * (double)L2);
            if (fabs (frac - 0.5) < 1e-12)
              tight |= fabs (ratio - bound) < 2e-3;
          }
        DP_CHECK_MSG (ok, "off-grid loss is at most sinc(1/(2L))");
        DP_CHECK_MSG (tight, "half a step from both points loses the bound");
      }
    dp_corr2d_destroy (d);
  }

  /* ── refusals: nothing folded, nothing written ──────────────────────── */
  {
    enum
    {
      RNY = 4,
      RNX = 8,
      RN  = RNY * RNX
    };
    float _Complex rc[RNX] = { 1, -1, 1, 1, -1, 1, -1, -1 }, rx[RN] = { 0 };
    float _Complex rw[2 * RNY];
    for (size_t k = 0; k < 2 * RNY; k++)
      rw[k] = 1.0f;
    float _Complex gref[RN];
    for (size_t k = 0; k < RN; k++)
      gref[k] = _rand_cplx (&seed); /* energy off row 0: the general path */
    dp_corr2d_state_t *dd
        = dp_corr2d_create_delay_doppler (rc, RNX, RNY, 1, 2, 0);
    dp_corr2d_state_t *gen = dp_corr2d_create (gref, RNY, RNX, 1, 1, 0, 0, -1);
    float _Complex r0[RN];
    memset (r0, 0, sizeof r0);
    memcpy (r0, rc, sizeof rc);
    dp_corr2d_state_t *col  = dp_corr2d_create (r0, RNY, RNX, 1, 1, 0, 0, 3);
    dp_corr2d_state_t *fast = dp_corr2d_create (r0, RNY, RNX, 1, 1, 0, 0, -1);
    dp_corr2d_state_t *pad
        = dp_corr2d_create (r0, RNY, RNX, 1, 1, 0, 2 * RNX, -1);
    DP_REQUIRE (dd && gen && col && fast && pad && !gen->fast_path);
    _bank_copy_t     cp = { NULL, RN, 0, 1 };
    dp_corr2d_fold_t f  = { _bank_copy, &cp };
    DP_CHECK (dp_corr2d_bank_fold (dd, rx, RN - 1, 2, 0, 1, NULL, 0, &f)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_fold (dd, rx, 2 * RN, 2, 0, 1, NULL, 0, &f)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_fold (dd, rx, RN, 0, 0, 1, NULL, 0, &f)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_fold (dd, rx, RN, 2, 1, 0, NULL, 0, &f)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_fold (dd, rx, RN, 2, 0, 1, rw, RNY + 1, &f)
              == SIZE_MAX);
    DP_CHECK_MSG (cp.calls == 0, "a refused bank folds nothing");

    enum
    {
      CAN = 64
    };
    float    po[CAN], o0[CAN];
    uint32_t pi[CAN], u2[CAN], u3[CAN], u4[CAN];
    int32_t  g1[CAN];
    for (size_t k = 0; k < CAN; k++)
      po[k] = o0[k] = -7.0f, pi[k] = u2[k] = u3[k] = u4[k] = 77u, g1[k] = 77;
    DP_CHECK (
        dp_corr2d_bank_power_max (dd, rx, RN, 2, 0, 1, NULL, 0, po, pi, RN - 1)
        == SIZE_MAX);
    /* detect: a zero search grid, a grid that does not divide, a resolution
       below the search, a short output, and a refined carrier past int32 */
    DP_CHECK (dp_corr2d_bank_detect (dd, rx, RN, 0, 4, 0, 1, 0.f, 0.f, NULL, 0,
                                     o0, g1, u2, u3, u4, CAN)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_detect (dd, rx, RN, 3, 4, 0, 1, 0.f, 0.f, NULL, 0,
                                     o0, g1, u2, u3, u4, CAN)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_detect (dd, rx, RN, 4, 2, 0, 1, 0.f, 0.f, NULL, 0,
                                     o0, g1, u2, u3, u4, CAN)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_detect (dd, rx, RN, 2, 4, 0, 1, 0.f, 0.f, NULL, 0,
                                     o0, g1, u2, u3, u4, RN - 1)
              == SIZE_MAX);
    DP_CHECK (dp_corr2d_bank_detect (dd, rx, RN, 1, 4, INT32_MAX / 2,
                                     INT32_MAX / 2, 0.f, 0.f, NULL, 0, o0, g1,
                                     u2, u3, u4, CAN)
              == SIZE_MAX);
    int clean = 1;
    for (size_t k = 0; k < CAN; k++)
      clean &= po[k] == -7.0f && o0[k] == -7.0f && pi[k] == 77u && u2[k] == 77u
               && u3[k] == 77u && u4[k] == 77u && g1[k] == 77;
    DP_CHECK_MSG (clean, "a refused bank call writes nothing");
    dp_corr2d_destroy (dd), dp_corr2d_destroy (gen), dp_corr2d_destroy (col);
    dp_corr2d_destroy (fast), dp_corr2d_destroy (pad);
  }
  return 0;
}

/* ── detect coarse, refine the candidate, confirm (section 6) ────────────
 * A noiseless code on a carrier between two coarse points: one record, at
 * the signal's lag and Doppler row, on the fine carrier nearest it, whose
 * power is the fine bank's own map at that cell and beats the coarse one by
 * the straddle loss the refine recovers.  And the rate that matches is the
 * rate reported. */
static int
_test_bank_detect (void)
{
  enum
  {
    DNY  = 16,
    DNX  = 128,
    DN   = DNY * DNX,
    DLAG = 41
  };
  uint32_t seed = 0xDE7Cu;
  float _Complex code[DNX], x[DN];
  for (size_t p = 0; p < DNX; p++)
    code[p] = crealf (_rand_cplx (&seed)) >= 0.0f ? 1.0f : -1.0f;
  dp_corr2d_state_t *d
      = dp_corr2d_create_delay_doppler (code, DNX, DNY, 1, 2, 0);
  DP_REQUIRE (d != NULL);
  static float        pw[DN], fp[DN], po[DN];
  static uint32_t     pi[DN], fi[DN], ur[DN], urow[DN], ucol[DN];
  static int32_t      gr[DN];
  static const double phis[]
      = { 0.25, -0.75, 1.25 }; /* between L_d = 2 points */
  for (size_t q = 0; q < sizeof phis / sizeof *phis; q++)
    {
      const double phi = phis[q];
      for (size_t m = 0; m < DN; m++)
        {
          const double a = 2.0 * M_PI * phi * (double)m / DNX;
          x[m]           = code[(m + DNX - DLAG) % DNX]
                           * ((float)cos (a) + (float)sin (a) * I);
        }
      DP_REQUIRE (
          dp_corr2d_bank_power_max (d, x, DN, 2, -4, 4, NULL, 0, pw, pi, DN)
          == DN);
      float top = 0.0f;
      for (size_t k = 0; k < DN; k++)
        top = fmaxf (top, pw[k]);
      const size_t nrec = dp_corr2d_bank_detect (
          d, x, DN, 2, 4, -4, 4, 0.5f * top, 0.5f * top, NULL, 0, po, gr, ur,
          urow, ucol, DN);
      DP_CHECK_MSG (nrec == 1, "one signal, one record");
      const int32_t want_g = (int32_t)lround (4.0 * phi);
      DP_CHECK_MSG (nrec >= 1 && gr[0] == want_g && ucol[0] == DLAG
                        && ur[0] == 0,
                    "the record is the signal's fine carrier and lag");
      /* the refined power is the fine (L = 4) bank's map at that cell */
      DP_REQUIRE (dp_corr2d_bank_power_max (d, x, DN, 4, want_g, want_g, NULL,
                                            0, fp, fi, DN)
                  == DN);
      const size_t cell = (size_t)urow[0] * DNX + ucol[0];
      DP_CHECK_MSG (nrec >= 1 && fabsf (po[0] - fp[cell]) <= 1e-5f * fp[cell],
                    "the refine equals the fine bank's map at the cell");
      /* and recovers the straddle: 1 / sinc(1/4)^2 over the coarse peak */
      const double gain = sin (M_PI / 4.0) / (M_PI / 4.0);
      DP_CHECK_MSG (
          nrec >= 1
              && fabs ((double)po[0] / (double)top - 1.0 / (gain * gain))
                     < 1e-3,
          "the refine recovers the coarse grid's straddle loss");
      /* a confirm threshold above the refined power keeps nothing, and so
         does a candidate threshold above the coarse peak */
      DP_CHECK (dp_corr2d_bank_detect (d, x, DN, 2, 4, -4, 4, 0.5f * top,
                                       1.01f * po[0], NULL, 0, po, gr, ur,
                                       urow, ucol, DN)
                == 0);
      DP_CHECK (dp_corr2d_bank_detect (d, x, DN, 2, 4, -4, 4, 1.01f * top,
                                       0.0f, NULL, 0, po, gr, ur, urow, ucol,
                                       DN)
                == 0);
    }

  /* a candidate is a local maximum: a Doppler 0.3 bins off a row puts the
     next row above both thresholds too, and it is the same signal, so it
     must not be a second record.  The carrier stays nearest fine point 1. */
  {
    const double phi = 0.25 + 0.3 / DNY;
    for (size_t m = 0; m < DN; m++)
      {
        const double a = 2.0 * M_PI * phi * (double)m / DNX;
        x[m]           = code[(m + DNX - DLAG) % DNX]
                         * ((float)cos (a) + (float)sin (a) * I);
      }
    DP_REQUIRE (
        dp_corr2d_bank_power_max (d, x, DN, 2, -4, 4, NULL, 0, pw, pi, DN)
        == DN);
    size_t at = 0;
    for (size_t k = 0; k < DN; k++)
      at = pw[k] > pw[at] ? k : at;
    const float  top = pw[at];
    const size_t nb  = ((at / DNX + 1) % DNY) * DNX + at % DNX;
    /* not vacuous: the neighbor row clears the thresholds below */
    DP_REQUIRE (pw[nb] > 0.1f * top);
    const size_t nrec
        = dp_corr2d_bank_detect (d, x, DN, 2, 4, -4, 4, 0.1f * top, 0.1f * top,
                                 NULL, 0, po, gr, ur, urow, ucol, DN);
    DP_CHECK_MSG (nrec == 1 && gr[0] == 1 && ucol[0] == DLAG,
                  "a candidate is a local maximum: one signal, one record");
  }

  /* the matching rate is the one reported: a quadratic slow-time phase
     (a Doppler rate) on the signal, and two rate hypotheses, the second its
     de-chirp */
  {
    float _Complex w[2 * DNY];
    for (size_t i = 0; i < DNY; i++)
      {
        const double a = M_PI * 0.9 * (double)(i * i) / DNY;
        w[i]           = 1.0f;
        w[DNY + i]     = (float)cos (-a) + (float)sin (-a) * I;
        for (size_t n = 0; n < DNX; n++)
          x[i * DNX + n] = code[(n + DNX - DLAG) % DNX]
                           * ((float)cos (a) + (float)sin (a) * I);
      }
    DP_REQUIRE (
        dp_corr2d_bank_power_max (d, x, DN, 2, -2, 2, w, 2 * DNY, pw, pi, DN)
        == DN);
    float top = 0.0f;
    for (size_t k = 0; k < DN; k++)
      top = fmaxf (top, pw[k]);
    const size_t nrec
        = dp_corr2d_bank_detect (d, x, DN, 2, 4, -2, 2, 0.5f * top, 0.5f * top,
                                 w, 2 * DNY, po, gr, ur, urow, ucol, DN);
    DP_CHECK_MSG (nrec == 1 && ur[0] == 1 && gr[0] == 0 && ucol[0] == DLAG,
                  "the de-chirping rate is the rate reported");
  }
  dp_corr2d_destroy (d);

  /* the refine is the fine bank's own value at the cell, on every path: the
     time-domain refine (fast path and DelayDoppler, native delay) and the
     one that makes the fine map (an interpolated delay axis, the general
     path, a known column) are held to the same reference */
  {
    enum
    {
      PNY = 6,
      PNX = 20,
      PN  = PNY * PNX
    };
    float _Complex pc[PNX], px[PN];
    for (size_t p = 0; p < PNX; p++)
      pc[p] = crealf (_rand_cplx (&seed)) >= 0.0f ? 1.0f : -1.0f;
    for (size_t m = 0; m < PN; m++)
      {
        const double a = 2.0 * M_PI * 0.6 * (double)m / PNX;
        px[m] = pc[(m + PNX - 7) % PNX] * ((float)cos (a) + (float)sin (a) * I)
                + 0.05f * _rand_cplx (&seed);
      }
    int every = 1, kinds = 0;
    for (int kind = 0; kind < K_BANK_KINDS; kind++)
      {
        dp_corr2d_state_t *k  = _bank_make (kind, pc, PNY, PNX, 2);
        const size_t       no = k->n_out;
        float *cp = malloc (no * sizeof *cp), *fpw = malloc (no * sizeof *fpw);
        float *rp     = malloc (no * sizeof *rp);
        uint32_t *ci  = malloc (no * sizeof *ci),
                 *fi2 = malloc (no * sizeof *fi2);
        uint32_t *rr  = malloc (no * sizeof *rr),
                 *rw2 = malloc (no * sizeof *rw2);
        uint32_t *rc  = malloc (no * sizeof *rc);
        int32_t  *rg  = malloc (no * sizeof *rg);
        DP_REQUIRE (
            dp_corr2d_bank_power_max (k, px, PN, 2, -3, 3, NULL, 0, cp, ci, no)
            == no);
        float top = 0.0f;
        for (size_t q = 0; q < no; q++)
          top = fmaxf (top, cp[q]);
        const size_t nrec
            = dp_corr2d_bank_detect (k, px, PN, 2, 4, -3, 3, 0.3f * top, 0.0f,
                                     NULL, 0, rp, rg, rr, rw2, rc, no);
        DP_REQUIRE (nrec != SIZE_MAX);
        kinds += nrec > 0;
        const size_t width = k->col_out >= 0 ? 1 : k->nx_out;
        for (size_t q = 0; q < nrec; q++)
          {
            DP_REQUIRE (dp_corr2d_bank_power_max (k, px, PN, 4, rg[q], rg[q],
                                                  NULL, 0, fpw, fi2, no)
                        == no);
            const size_t col  = k->col_out >= 0 ? 0 : rc[q];
            const float  want = fpw[(size_t)rw2[q] * width + col];
            every &= fabsf (rp[q] - want) <= 1e-5f * fmaxf (want, 1e-12f);
          }
        free (cp), free (fpw), free (rp), free (ci), free (fi2), free (rr);
        free (rw2), free (rc), free (rg);
        dp_corr2d_destroy (k);
      }
    DP_CHECK_MSG (kinds == K_BANK_KINDS,
                  "every path makes a record (none is vacuous)");
    DP_CHECK_MSG (every,
                  "the refine equals the fine bank's map, on every path");
  }
  return 0;
}

int
main (void)
{
  const size_t NY = 4;
  const size_t NX = 4;
  const size_t N  = NY * NX; /* 16 */

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->ny == NY);
    DP_CHECK (obj->nx == NX);
    DP_CHECK (obj->dwell == 1);
    DP_CHECK (obj->count == 0);
    /* A single-row ref with ny_out==ny takes the fast path (see corr2d.h): */
    DP_CHECK (obj->fast_path == 1);
    DP_CHECK (obj->fwd == NULL && obj->inv == NULL);
    DP_CHECK (obj->fwd1d != NULL && obj->inv1d != NULL);
    dp_corr2d_reset (obj);
    DP_CHECK (obj->count == 0);
    dp_corr2d_destroy (obj);
    dp_corr2d_destroy (NULL);
  }

  /* ── self-correlation of 2-D unit impulse ─────────────────────────── *
   * The 2-D circular cross-correlation of δ with itself is δ.           */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f + 0.0f * I;

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    float _Complex out[16];
    size_t n_out = dp_corr2d_execute (obj, ref, N, out, N);

    DP_CHECK (n_out == N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));
    for (size_t k = 1; k < N; k++)
      DP_CHECK (dp_cnearf (out[k], 0.0f + 0.0f * I, TOL));

    dp_corr2d_destroy (obj);
  }

  /* ── integrate-and-dump: dwell=2 ─────────────────────────────────── */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 2, 1, 0, 0, -1);
    float _Complex out[16];

    size_t n1 = dp_corr2d_execute (obj, ref, N, out, N);
    DP_CHECK (n1 == 0);
    DP_CHECK (obj->count == 1);

    size_t n2 = dp_corr2d_execute (obj, ref, N, out, N);
    DP_CHECK (n2 == N); /* dump on second call */
    DP_CHECK (obj->count == 0);

    /* Two frames of impulse × impulse = 2.0 at lag (0,0). */
    DP_CHECK (crealf (out[0]) > 1.9f && crealf (out[0]) < 2.1f);

    dp_corr2d_destroy (obj);
  }

  /* ── 2-D shift: shifted input produces shifted peak ──────────────── *
   * ref = δ[0,0], in = δ[1,0].                                          *
   * R[i,j] = IFFT2(FFT2(in) · conj(FFT2(ref))) / (ny*nx)               *
   *        = δ[i-1, j] → peak at (row=1, col=0).                        */
  {
    float _Complex ref[16] = { 0 };
    float _Complex in[16]  = { 0 };
    ref[0]                 = 1.0f;
    in[NX]                 = 1.0f; /* row 1, col 0 */

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    float _Complex out[16];
    dp_corr2d_execute (obj, in, N, out, N);

    /* Peak should be at row=1, col=0 → flat index = 1*NX + 0 = NX */
    size_t peak_idx = NX;
    DP_CHECK (dp_cnearf (out[peak_idx], 1.0f + 0.0f * I, TOL));

    /* All other bins zero */
    for (size_t k = 0; k < N; k++)
      if (k != peak_idx)
        DP_CHECK (dp_cnearf (out[k], 0.0f + 0.0f * I, TOL));

    dp_corr2d_destroy (obj);
  }

  /* ── max_out returns n_out (native = ny*nx) ──────────────────────── */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (dp_corr2d_execute_max_out (obj) == N);
    dp_corr2d_destroy (obj);
  }

  /* ── decoupled inverse: interpolated output size + peak location ──── *
   * impulse ref, input shifted to (row 1, col 0) → native peak at (1,0); *
   * inverted on a 4→8 grid, the peak lands at (1·8/4, 0) = (2, 0).       */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    float _Complex in[16]  = { 0 };
    in[NX]                 = 1.0f; /* row 1, col 0 */
    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 8, 8, -1);
    DP_CHECK (obj->ny_out == 8 && obj->nx_out == 8);
    /* ny_out (8) != ny (4) -- Doppler-axis interpolation is requested, so
     * the fast path's identity doesn't apply (see corr2d.h); must fall
     * back to the general 2-D path even though ref is single-row. */
    DP_CHECK (obj->fast_path == 0);
    DP_CHECK (obj->fwd != NULL && obj->inv != NULL);
    DP_CHECK (dp_corr2d_execute_max_out (obj) == 64);

    float _Complex out[64];
    size_t no = dp_corr2d_execute (obj, in, N, out, 64);
    DP_CHECK (no == 64);
    size_t pk = 0;
    for (size_t k = 1; k < 64; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    DP_CHECK (pk / 8 == 2 && pk % 8 == 0);
    dp_corr2d_destroy (obj);
  }

  /* ── dense-signal correctness vs. a brute-force reference, both paths ── */
  {
    const size_t ny = 5, nx = 7, n = ny * nx;
    uint32_t     seed            = 12345u;
    float _Complex dense_ref[35] = { 0 }, dense_in[35], expect[35], out[35];
    for (size_t j = 0; j < nx; j++)
      dense_ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      dense_in[k] = _rand_cplx (&seed);

    /* single-row ref -> fast path */
    dp_corr2d_state_t *fast
        = dp_corr2d_create (dense_ref, ny, nx, 1, 1, 0, 0, -1);
    DP_CHECK (fast != NULL && fast->fast_path == 1);
    dp_corr2d_execute (fast, dense_in, n, out, n);
    _brute_corr2d (dense_in, dense_ref, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (dp_cnearf (out[k], expect[k], TOL));
    dp_corr2d_destroy (fast);

    /* genuinely multi-row ref -> general path; the pre-existing suite had
     * no non-impulse correctness check for this path either. */
    float _Complex dense_ref2[35];
    for (size_t k = 0; k < n; k++)
      dense_ref2[k] = _rand_cplx (&seed);
    dp_corr2d_state_t *slow
        = dp_corr2d_create (dense_ref2, ny, nx, 1, 1, 0, 0, -1);
    DP_CHECK (slow != NULL && slow->fast_path == 0);
    dp_corr2d_execute (slow, dense_in, n, out, n);
    _brute_corr2d (dense_in, dense_ref2, ny, nx, expect);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (dp_cnearf (out[k], expect[k], TOL));
    dp_corr2d_destroy (slow);
  }

  /* ── fast path + nx_out interpolation (code-axis only, ny_out==ny) ────── *
   * single-row ref, nx doubled via decoupled inverse; peak should scale by
   * nx_out/nx exactly like the general-path decoupled test above scales by
   * ny_out/ny. */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    float _Complex in[16]  = { 0 };
    in[1]                  = 1.0f; /* row 0, col 1 */
    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 8, -1);
    DP_CHECK (obj->fast_path
              == 1); /* ny_out==ny native; nx_out interpolated */
    DP_CHECK (obj->ny_out == NY && obj->nx_out == 8);
    DP_CHECK (dp_corr2d_execute_max_out (obj) == NY * 8);

    float _Complex out[32];
    size_t no = dp_corr2d_execute (obj, in, N, out, 32);
    DP_CHECK (no == NY * 8);
    size_t pk = 0;
    for (size_t k = 1; k < NY * 8; k++)
      if (cabsf (out[k]) > cabsf (out[pk]))
        pk = k;
    /* native peak at (row=0, col=1); interpolated col = 1 * (8/4) = 2 */
    DP_CHECK (pk / 8 == 0 && pk % 8 == 2);
    dp_corr2d_destroy (obj);
  }

  /* ── dp_corr2d_set_ref: fast-path accept vs. reject ────────────────────────
   */
  {
    float _Complex ref1[16] = { 0 }, ref2[16] = { 0 }, bad_ref[16] = { 0 };
    ref1[0]     = 1.0f;
    ref2[1]     = 1.0f; /* still single-row -- row 0, col 1 */
    bad_ref[NX] = 1.0f; /* row 1 nonzero -- no longer single-row */

    dp_corr2d_state_t *obj = dp_corr2d_create (ref1, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (obj->fast_path == 1);

    /* accept: still single-row */
    DP_CHECK (dp_corr2d_set_ref (obj, ref2) == 0);
    float _Complex in[16] = { 0 };
    in[1]                 = 1.0f; /* row 0, col 1 -- matches ref2's replica */
    float _Complex out[16];
    dp_corr2d_execute (obj, in, N, out, N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));

    /* reject: no longer single-row -- object's ref/spectrum must be left
     * completely untouched (execute() still reflects ref2, not bad_ref). */
    DP_CHECK (dp_corr2d_set_ref (obj, bad_ref) == -1);
    dp_corr2d_execute (obj, in, N, out, N);
    DP_CHECK (dp_cnearf (out[0], 1.0f + 0.0f * I, TOL));

    dp_corr2d_destroy (obj);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* corr2d has TWO write paths -- the 2-D inverse and the per-row fast
     * path -- and the fast one bypasses the slow path entirely, so both are
     * exercised here. A clamp applied to only one would leave the other
     * overrunning, which is the shape of the bug this guards. */
    const size_t ny = 5, nx = 7, n = ny * nx;
    uint32_t     seed          = 999u;
    float _Complex row_ref[35] = { 0 }, full_ref[35], input[35];
    float _Complex full[35], part[35];
    for (size_t j = 0; j < nx; j++)
      row_ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      {
        full_ref[k] = _rand_cplx (&seed);
        input[k]    = _rand_cplx (&seed);
      }

    for (int which = 0; which < 2; which++)
      {
        const float _Complex *rf = which ? full_ref : row_ref;
        dp_corr2d_state_t *a = dp_corr2d_create (rf, ny, nx, 1, 1, 0, 0, -1);
        dp_corr2d_state_t *b = dp_corr2d_create (rf, ny, nx, 1, 1, 0, 0, -1);
        DP_CHECK (a != NULL && b != NULL);
        DP_CHECK (a->fast_path == (which ? 0 : 1)); /* both paths covered */
        for (size_t k = 0; k < n; k++)
          part[k] = 42.0f + 42.0f * I;

        DP_CHECK (dp_corr2d_execute (a, input, n, full, n) == n);
        DP_CHECK (dp_corr2d_execute (b, input, n, part, 6) == 6);
        for (size_t k = 0; k < 6; k++)
          DP_CHECK (dp_cnearf (part[k], full[k],
                               TOL)); /* prefix is the same surface */
        for (size_t k = 6; k < n; k++)
          DP_CHECK (dp_cnearf (part[k], 42.0f + 42.0f * I,
                               TOL)); /* tail untouched */

        for (size_t k = 0; k < n; k++)
          part[k] = 42.0f + 42.0f * I;
        DP_CHECK (dp_corr2d_execute (b, input, n, part, 0) == 0);
        for (size_t k = 0; k < n; k++)
          DP_CHECK (dp_cnearf (part[k], 42.0f + 42.0f * I, TOL));
        dp_corr2d_destroy (a);
        dp_corr2d_destroy (b);
      }
  }

  /* serializable state — 2-D accumulator + count resume; plans + ref rebuilt.
   */
  {
    float _Complex ref[16], in[16], out[16];
    for (int i = 0; i < 16; i++)
      {
        ref[i] = (float)(i % 4) + 0.5f * I;
        in[i]  = (float)(i % 3) - 1.0f + 0.2f * I;
      }
    dp_corr2d_state_t *a = dp_corr2d_create (ref, 4, 4, 3, 1, 0, 0, -1);
    dp_corr2d_state_t *b = dp_corr2d_create (ref, 4, 4, 3, 1, 0, 0, -1);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_corr2d_execute (a, in, 16, out, 16);
    DP_STATE_ROUNDTRIP_TEST (dp_corr2d, a, b);
    DP_CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
    dp_corr2d_destroy (a);
    dp_corr2d_destroy (b);
  }

  /* serializable state, fast path: accum's CONTENT differs (per-row nx
   * spectra rather than one flat 2-D spectrum) but the byte LAYOUT is
   * identical (n complex floats + count either way), so the triplet needs
   * no fast-path-specific code at all -- this proves that in practice. */
  {
    float _Complex ref[16] = { 0 }, in[16], out[16];
    ref[0]                 = 1.0f;
    for (int i = 0; i < 16; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    dp_corr2d_state_t *a = dp_corr2d_create (ref, 4, 4, 3, 1, 0, 0, -1);
    dp_corr2d_state_t *b = dp_corr2d_create (ref, 4, 4, 3, 1, 0, 0, -1);
    DP_CHECK (a != NULL && b != NULL);
    DP_CHECK (a->fast_path == 1 && b->fast_path == 1);
    (void)dp_corr2d_execute (a, in, 16, out, 16);
    DP_STATE_ROUNDTRIP_TEST (dp_corr2d, a, b);
    DP_CHECK (b->count == a->count && b->accum[0] == a->accum[0]);
    dp_corr2d_destroy (a);
    dp_corr2d_destroy (b);
  }

  /* ── reset really zeroes the ACCUMULATOR, not just the counter ────────
   *
   * The pre-existing lifecycle check called dp_corr2d_reset() on a freshly
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
    const size_t ny = 4, nx = 4, n = ny * nx;
    uint32_t     seed      = 777u;
    float _Complex ref[16] = { 0 }, stale[16], fresh[16];
    float _Complex out_a[16], out_b[16];
    for (size_t j = 0; j < nx; j++)
      ref[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      stale[k] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      fresh[k] = _rand_cplx (&seed);

    dp_corr2d_state_t *a = dp_corr2d_create (ref, ny, nx, 2, 1, 0, 0, -1);
    dp_corr2d_state_t *b = dp_corr2d_create (ref, ny, nx, 2, 1, 0, 0, -1);
    DP_CHECK (a != NULL && b != NULL);

    /* `a` takes one frame of a dwell-2 cycle, so its accumulator is
       non-zero and its counter is 1 -- the precondition the old test
       lacked. Assert it, or this test can go vacuous the same way. */
    DP_CHECK (dp_corr2d_execute (a, stale, n, out_a, n) == 0);
    DP_CHECK (a->count == 1);
    int accum_nonzero = 0;
    for (size_t k = 0; k < n; k++)
      if (cabsf (a->accum[k]) > 0.0f)
        accum_nonzero = 1;
    DP_CHECK (accum_nonzero);

    dp_corr2d_reset (a);
    DP_CHECK (a->count == 0);

    /* Both now drive a full dwell-2 cycle over identical input. */
    DP_CHECK (dp_corr2d_execute (a, fresh, n, out_a, n) == 0);
    DP_CHECK (dp_corr2d_execute (b, fresh, n, out_b, n) == 0);
    size_t na = dp_corr2d_execute (a, fresh, n, out_a, n);
    size_t nb = dp_corr2d_execute (b, fresh, n, out_b, n);
    DP_CHECK (na == n && nb == n);
    for (size_t k = 0; k < n; k++)
      DP_CHECK (out_a[k] == out_b[k]); /* bit-for-bit: same arithmetic */

    dp_corr2d_destroy (a);
    dp_corr2d_destroy (b);
  }

  /* ── nthreads never changes the output ───────────────────────────────
   *
   * nthreads is honored on the single-row fast path (#2666: the rows fan
   * out over a pool, each worker on its own plans) and ignored by the
   * general 2-D path. Either way it is documented not to change the result,
   * so this pins the output bit-for-bit across four values on both paths.
   * On the fast path that is a real check -- sharing one plan between the
   * workers, which race on its scratch, turns it red (the delay-Doppler
   * thread test below holds the same property at sizes where the chunks
   * differ more). With ny = 5, counts above 5 are capped to 5 workers. */
  {
    const size_t ny = 5, nx = 7, n = ny * nx;
    uint32_t     seed       = 4242u;
    float _Complex ref1[35] = { 0 }, refN[35], in[35];
    float _Complex base[35], other[35];
    for (size_t j = 0; j < nx; j++)
      ref1[j] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      refN[k] = _rand_cplx (&seed);
    for (size_t k = 0; k < n; k++)
      in[k] = _rand_cplx (&seed);

    for (int path = 0; path < 2; path++)
      {
        const float _Complex *r = path ? refN : ref1;
        dp_corr2d_state_t *c0   = dp_corr2d_create (r, ny, nx, 1, 1, 0, 0, -1);
        DP_CHECK (c0 != NULL && c0->fast_path == (path ? 0 : 1));
        DP_CHECK (dp_corr2d_execute (c0, in, n, base, n) == n);
        dp_corr2d_destroy (c0);

        for (size_t nt = 2; nt <= 8; nt *= 2)
          {
            dp_corr2d_state_t *c
                = dp_corr2d_create (r, ny, nx, 1, nt, 0, 0, -1);
            DP_CHECK (c != NULL);
            DP_CHECK (dp_corr2d_execute (c, in, n, other, n) == n);
            for (size_t k = 0; k < n; k++)
              DP_CHECK (other[k] == base[k]);
            dp_corr2d_destroy (c);
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
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    dp_corr2d_state_t *nat = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (nat != NULL);
    DP_CHECK (nat->ny_out == NY && nat->nx_out == NX);
    DP_CHECK (nat->work_pad == NULL && nat->ztmp == NULL);
    DP_CHECK (nat->zcol == NULL && nat->zcolout == NULL);
    dp_corr2d_destroy (nat);

    /* ... and the padded path DOES allocate them, so the check above is
       not passing because the fields are always NULL. */
    dp_corr2d_state_t *pad = dp_corr2d_create (ref, NY, NX, 1, 1, 8, 8, -1);
    DP_CHECK (pad != NULL && pad->work_pad != NULL);
    dp_corr2d_destroy (pad);
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
        const size_t nx        = parity ? 15 : 16; /* odd has no Nyquist bin */
        float _Complex ref[16] = { 0 }, in[16];
        float _Complex out[16 * 8];
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
                in[j] = (float _Complex) (ar / (double)nx)
                        + (float _Complex) (ai / (double)nx) * I;
              }

            dp_corr2d_state_t *fine
                = dp_corr2d_create (ref, 1, nx, 1, 1, 0, nx * up, -1);
            DP_CHECK (fine != NULL);
            size_t no = dp_corr2d_execute (fine, in, nx, out, nx * up);
            DP_CHECK (no == nx * up);

            size_t pk = 0;
            for (size_t k = 1; k < no; k++)
              if (cabsf (out[k]) > cabsf (out[pk]))
                pk = k;
            double got = (double)pk / (double)up;
            DP_CHECK (fabs (got - frac) < 0.02);
            dp_corr2d_destroy (fine);
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
              in[j] = (float _Complex) (ar / (double)nx)
                      + (float _Complex) (ai / (double)nx) * I;
            }
          dp_corr2d_state_t *nat
              = dp_corr2d_create (ref, 1, nx, 1, 1, 0, 0, -1);
          DP_CHECK (nat != NULL);
          float _Complex onat[16];
          DP_CHECK (dp_corr2d_execute (nat, in, nx, onat, nx) == nx);
          size_t pk = 0;
          for (size_t k = 1; k < nx; k++)
            if (cabsf (onat[k]) > cabsf (onat[pk]))
              pk = k;
          DP_CHECK (fabs ((double)pk - frac) >= 0.5 - 1e-9);
          dp_corr2d_destroy (nat);
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
    const size_t nx = 16, up = 4;
    float _Complex ref[16] = { 0 }, in[16];
    float _Complex out[64];
    ref[0] = 1.0f;
    for (size_t j = 0; j < nx; j++)
      in[j] = (j == 3) ? 1.0f : (j == 4) ? 0.7f : (j == 11) ? -0.4f : 0.0f;

    dp_corr2d_state_t *fine
        = dp_corr2d_create (ref, 1, nx, 1, 1, 0, nx * up, -1);
    DP_CHECK (fine != NULL);
    size_t no = dp_corr2d_execute (fine, in, nx, out, nx * up);
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
    dp_corr2d_destroy (fine);
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
   * Checked here rather than only in detector2d because dp_detector2d_create
   * forwards its own dwell straight into this call -- one validation at
   * the primitive covers both objects, and a copy in each would be the
   * thing that drifts.
   *
   * NULL is the whole error report by design: the repo's convention
   * (docs/dev/contributing/error-convention.md) is that create() returns
   * NULL for an allocation failure OR an invalid argument, and the binding
   * turns that into MemoryError. */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    DP_CHECK (dp_corr2d_create (ref, NY, NX, 0, 1, 0, 0, -1) == NULL);
    /* Not vacuous: the neighbouring value builds. */
    dp_corr2d_state_t *ok = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (ok != NULL);
    dp_corr2d_destroy (ok);
    /* The pre-existing output-grid rule still holds. */
    DP_CHECK (dp_corr2d_create (ref, NY, NX, 1, 1, 0, NX - 1, -1) == NULL);
  }

  /* ── a known output column equals that column of the full map ─────────
   *
   * The whole point of col_out is that a caller who already knows the lag
   * skips the inverse transform -- so the value it returns must be the one
   * the inverse would have produced, not merely something correlated with
   * it. Compared against the full map at EVERY lag, not just lag 0 -- a
   * sum that dropped the (p + j0) rotation would agree at j0 == 0 and
   * nowhere else, and lag 0 is the only one refine itself uses.
   *
   * The reference is single-row and the input is random, so a peak does
   * not sit conveniently at lag 0 -- a table that dropped the phase would
   * agree at j0 == 0 and nowhere else. */
  {
    enum
    {
      ny = 6,
      nx = 8
    };
    /* COMPLEX reference, not a real +-1 code: the sum conjugates it, and a
       real replica would let a dropped conjugate pass unnoticed. */
    float _Complex ref[ny * nx] = { 0 };
    for (size_t j = 0; j < nx; j++)
      ref[j] = ((j % 3 == 0) ? 1.0f : -1.0f) + ((float)(j % 5) - 2.0f) * I;

    float _Complex in[ny * nx];
    uint32_t st = 12345u;
    for (size_t k = 0; k < ny * nx; k++)
      {
        const float re = (float)(2.0 * dp_uni (&st) - 1.0);
        const float im = (float)(2.0 * dp_uni (&st) - 1.0);
        in[k]          = re + im * I;
      }

    const size_t nxo_cases[1] = { 0 }; /* native only -- see col_out's doc */
    for (size_t c = 0; c < 1; c++)
      {
        const size_t nxo = nxo_cases[c] ? nxo_cases[c] : nx;

        dp_corr2d_state_t *full
            = dp_corr2d_create (ref, ny, nx, 1, 1, 0, nxo_cases[c], -1);
        DP_CHECK (full != NULL);
        float _Complex *map = malloc (ny * nxo * sizeof (*map));
        DP_CHECK (map != NULL);
        DP_CHECK (dp_corr2d_execute (full, in, ny * nx, map, ny * nxo)
                  == ny * nxo);

        for (size_t j0 = 0; j0 < nxo; j0++)
          {
            dp_corr2d_state_t *one = dp_corr2d_create (ref, ny, nx, 1, 1, 0,
                                                       nxo_cases[c], (int)j0);
            DP_CHECK (one != NULL);
            /* One value per ROW, not a map. */
            DP_CHECK (one->n_out == ny);
            DP_CHECK (dp_corr2d_execute_max_out (one) == ny);

            float _Complex col[ny];
            DP_CHECK (dp_corr2d_execute (one, in, ny * nx, col, ny) == ny);
            for (size_t i = 0; i < ny; i++)
              {
                const float _Complex want = map[i * nxo + j0];
                DP_CHECK (cabsf (col[i] - want)
                          <= 1e-4f * (cabsf (want) + 1.0f));
              }
            dp_corr2d_destroy (one);
          }
        free (map);
        dp_corr2d_destroy (full);
      }
  }

  /* ── set_ref refreshes the known-lag replica too ──────────────────────
   *
   * col_out reads a time-domain copy of the reference, which is a SECOND
   * place the replica lives; refreshing only the spectrum would leave this
   * path correlating against the retired code with no symptom but a wrong
   * answer. */
  {
    enum
    {
      ny = 3,
      nx = 4
    };
    float _Complex r0[ny * nx] = { 0 }, r1[ny * nx] = { 0 };
    for (size_t j = 0; j < nx; j++)
      {
        r0[j] = (j == 0) ? 1.0f : 0.0f;
        r1[j] = (j == 1) ? 1.0f : 0.0f; /* a DIFFERENT code */
      }
    float _Complex in[ny * nx];
    for (size_t k = 0; k < ny * nx; k++)
      in[k] = (float)(k + 1);

    dp_corr2d_state_t *c = dp_corr2d_create (r0, ny, nx, 1, 1, 0, 0, 0);
    DP_CHECK (c != NULL);
    float _Complex a[ny], b[ny];
    DP_CHECK (dp_corr2d_execute (c, in, ny * nx, a, ny) == ny);
    DP_CHECK (dp_corr2d_set_ref (c, r1) == 0);
    DP_CHECK (dp_corr2d_execute (c, in, ny * nx, b, ny) == ny);
    /* A stale replica would return the same answer for both codes. */
    DP_CHECK (cabsf (a[0] - b[0]) > 1e-3f);
    /* ... and the new answer is the one r1 actually selects. */
    dp_corr2d_state_t *fresh = dp_corr2d_create (r1, ny, nx, 1, 1, 0, 0, 0);
    DP_CHECK (fresh != NULL);
    float _Complex f[ny];
    DP_CHECK (dp_corr2d_execute (fresh, in, ny * nx, f, ny) == ny);
    for (size_t i = 0; i < ny; i++)
      DP_CHECK (cabsf (b[i] - f[i]) <= 1e-5f);
    dp_corr2d_destroy (fresh);
    dp_corr2d_destroy (c);
  }

  /* ── col_out is refused where it has no meaning ───────────────────────
   *
   * Silently falling back to the full map would hand the caller n_out it
   * did not ask for, which is how a buffer overrun gets written. */
  {
    float _Complex ref[16] = { 0 };
    ref[0]                 = 1.0f;
    /* Past the output grid. */
    DP_CHECK (dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, NX) == NULL);
    /* A decoupled grid: an interpolated column is a fractional lag. */
    DP_CHECK (dp_corr2d_create (ref, NY, NX, 1, 1, 0, 2 * NX, 0) == NULL);
    /* Not the single-row shape the fast path needs: energy outside row 0. */
    float _Complex dense[16];
    for (size_t k = 0; k < 16; k++)
      dense[k] = 1.0f;
    DP_CHECK (dp_corr2d_create (dense, NY, NX, 1, 1, 0, 0, 0) == NULL);
    /* Not vacuous: the same shapes build with col_out disabled, and the
       single-row reference builds WITH it. */
    dp_corr2d_state_t *a = dp_corr2d_create (dense, NY, NX, 1, 1, 0, 0, -1);
    DP_CHECK (a != NULL);
    dp_corr2d_destroy (a);
    dp_corr2d_state_t *b = dp_corr2d_create (ref, NY, NX, 1, 1, 0, 0, 0);
    DP_CHECK (b != NULL);
    dp_corr2d_destroy (b);
  }

  /* ── n_in must equal ny*nx: any other length is REFUSED (#1925) ──────
   * As test_corr_core.c: exact-size heap arrays (ASan), a canary-filled out,
   * and a refusal that does not count toward the dwell. */
  {
    float _Complex ref[16];
    for (size_t i = 0; i < N; i++)
      ref[i] = (i == 0) ? 1.0f : 0.0f;
    dp_corr2d_state_t *obj = dp_corr2d_create (ref, NY, NX, 2, 1, 0, 0, -1);
    DP_CHECK (obj != NULL);
    float _Complex good[16], out[16];
    for (size_t i = 0; i < N; i++)
      {
        good[i] = 1.0f;
        out[i]  = CMPLXF (-7.0f, -7.0f);
      }
    DP_CHECK (dp_corr2d_execute (obj, good, N, out, N) == 0); /* frame 1 */
    const size_t bad[] = { 0, 1, N - 1, N + 1, 2 * N };
    for (size_t k = 0; k < sizeof bad / sizeof *bad; k++)
      {
        float _Complex *x
            = (float _Complex *)calloc (bad[k] ? bad[k] : 1, sizeof *x);
        DP_CHECK (dp_corr2d_execute (obj, x, bad[k], out, N) == SIZE_MAX);
        DP_CHECK (obj->count == 1); /* not counted */
        free (x);
      }
    int untouched = 1;
    for (size_t i = 0; i < N; i++)
      untouched &= crealf (out[i]) == -7.0f;
    DP_CHECK (untouched);
    DP_CHECK (dp_corr2d_execute (obj, good, N, out, N) == N); /* dumps */
    dp_corr2d_destroy (obj);
  }

  _test_delay_doppler ();
  _test_batch ();
  (void)_test_batch_ranking (); /* a REQUIRE failure is counted */
  (void)_test_bank ();
  (void)_test_bank_detect ();

  DP_TEST_END ("test_corr2d_core");
}
