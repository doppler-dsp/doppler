/**
 * @file corr2d_hypothesis_order.c
 * @brief What each ordering of a hypothesis bank costs, timed in C.
 *
 * docs/design/corr2d-hypotheses.md settles, by a prototype, that a bank of
 * carrier x rate hypotheses on one DelayDoppler frame can run its
 * hypothesis-independent prefix once, its carrier hypotheses outside (each
 * paying the row inverses) and its rate hypotheses inside (each paying only
 * the slow-time FFT), and that a carrier offset between whole bins is
 * reached exactly by either of two methods:
 *
 *   padded  one L*nx-point FFT per row, each offset a stride-L gather;
 *   mix     one nx-point FFT per row per fraction, of the mixed row.
 *
 * Op counts cannot choose between them -- real code lengths are 2^n - 1,
 * L*nx changes the length's factors, the gathers and mixes are
 * memory-bound, and the pool splits each stage its own way -- so this
 * harness times them, against the FULL pipeline (every hypothesis mixed,
 * weighted and run through the shipped DelayDoppler).
 *
 * Every pipeline produces all carriers x rates maps of one frame and folds
 * each into one accumulator, acc += (h + 1) |map|^2, h the hypothesis
 * index. The weight makes the fold order-sensitive, so a permuted or
 * mis-signed hypothesis changes the result, which is what --check needs; all
 * three pipelines pay the same fold.
 *
 * The carriers are the L fractions of one whole bin, offsets j / (L nx) for
 * j = 0 .. L-1: whole-bin offsets cost both methods the same (a roll), so
 * they are left out of the comparison.
 *
 * Usage:
 *   validate_corr2d_hypothesis_order           the timings, readable
 *   validate_corr2d_hypothesis_order --emit    every row as one JSON document
 *   validate_corr2d_hypothesis_order --check   the three pipelines agree, at
 *                                              a small geometry (CTest)
 *
 * Times are the minimum over REPEATS runs of one whole frame, after one
 * warm-up run: the minimum is the run least disturbed by the rest of the
 * machine, which is the quantity a comparison of two orderings needs.
 */
#include "doppler/corr2d/corr2d_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_thread.h"
#include "doppler/fft/fft_core.h"
#include "doppler/timing/timing_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The slow-time stage gathers this many delay columns at a time, as
   corr2d_core.c's CORR2D_COL_TILE does, so the ordered pipelines are timed
   with the same cache behaviour as the shipped column stage they race. */
#define TILE 16
#define REPEATS 7
#define MAXW 64

typedef float _Complex cf;

/* One geometry, and every buffer and plan its pipelines need. */
typedef struct
{
  size_t ny, nx, ny_out, lf, m, carriers, rates;
  int    nw;   /* workers: chunks per stage */
  cf    *x;    /* the frame, ny x nx */
  cf    *code; /* nx */
  cf    *cref; /* conj(FFT(code)), nx */
  cf    *w;    /* rates x ny slow-time weights */
  cf    *ph;   /* lf x nx mix phasors, e^{-j 2 pi j n / M} */
  cf    *xm;   /* padded row spectra, ny x M */
  cf    *xj;   /* mixed row spectra, ny x nx (one fraction at a time) */
  cf    *rows; /* correlated rows, ny x nx (one carrier at a time) */
  cf    *pad;  /* nw x M zero-padded row scratch */
  cf    *spec; /* nw x nx product scratch */
  cf    *raw;  /* nw x TILE x ny gathered tile of correlated rows */
  cf    *col;  /* nw x TILE x ny_out column scratch, zero tails */
  cf    *colo; /* nw x TILE x ny_out transformed columns */
  float *acc;  /* ny_out x nx fold */
  float *accw; /* nw x ny_out x nx per-worker folds, summed per frame */
  dp_fft_state_t *fwd_m[MAXW], *fwd_n[MAXW], *inv_n[MAXW], *slow[MAXW];
  dp_pool_t      *pool;
  /* the full pipeline: one shipped DelayDoppler and its scratch */
  dp_corr2d_state_t *dd;
  cf                *hin, *hout;
} geo_t;

typedef struct
{
  double fwd, inv, col, other; /* seconds, per frame */
} stages_t;

static double
now_s (void)
{
  return 1e-9 * (double)dp_mono_ns ();
}

/* Row chunk t of nw: rows r0 .. r1-1, as contiguous and as even as can be. */
static void
chunk (size_t n, size_t t, int nw, size_t *r0, size_t *r1)
{
  *r0 = n * t / (size_t)nw;
  *r1 = n * (t + 1) / (size_t)nw;
}

/* ---- the ordered pipelines' stages, each run over nw chunks ---------- */

typedef struct
{
  geo_t *g;
  size_t j;   /* fraction (mix) or gather offset (padded) */
  size_t h0;  /* the first hypothesis index of this carrier */
  size_t nwr; /* workers the slow-time stage splits the rates over */
} ctx_t;

/* padded: every row zero-padded to M and transformed, once per frame. */
static void
st_fwd_padded (size_t t, void *v)
{
  geo_t *g = ((ctx_t *)v)->g;
  size_t r0, r1;
  chunk (g->ny, t, g->nw, &r0, &r1);
  cf *p = g->pad + t * g->m;
  for (size_t r = r0; r < r1; r++)
    {
      memcpy (p, g->x + r * g->nx, g->nx * sizeof *p);
      memset (p + g->nx, 0, (g->m - g->nx) * sizeof *p);
      dp_fft_execute_cf32 (g->fwd_m[t], p, g->m, g->xm + r * g->m, g->m);
    }
}

/* mix: every row mixed by fraction j and transformed at nx. */
static void
st_fwd_mix (size_t t, void *v)
{
  ctx_t *c = (ctx_t *)v;
  geo_t *g = c->g;
  size_t r0, r1;
  chunk (g->ny, t, g->nw, &r0, &r1);
  cf       *p  = g->spec + t * g->nx;
  const cf *ph = g->ph + c->j * g->nx;
  for (size_t r = r0; r < r1; r++)
    {
      const cf *xr = g->x + r * g->nx;
      for (size_t n = 0; n < g->nx; n++)
        p[n] = xr[n] * ph[n];
      dp_fft_execute_cf32 (g->fwd_n[t], p, g->nx, g->xj + r * g->nx, g->nx);
    }
}

/* Per carrier: the row spectrum for offset j (a stride-L gather of the
   padded spectrum, or the mixed spectrum as is) times the reference, then
   the row inverse, scaled 1/nx as DelayDoppler's is. */
static void
st_inv (size_t t, void *v, int padded)
{
  ctx_t *c = (ctx_t *)v;
  geo_t *g = c->g;
  size_t r0, r1;
  chunk (g->ny, t, g->nw, &r0, &r1);
  cf         *p   = g->spec + t * g->nx;
  const float inv = 1.0f / (float)g->nx;
  for (size_t r = r0; r < r1; r++)
    {
      if (padded)
        {
          /* L q + j < M for every q when j < L: the gather never wraps,
             so it is a plain stride-L read from offset j. */
          const cf *xr = g->xm + r * g->m + c->j;
          for (size_t q = 0; q < g->nx; q++)
            p[q] = xr[g->lf * q] * g->cref[q];
        }
      else
        {
          const cf *xr = g->xj + r * g->nx;
          for (size_t q = 0; q < g->nx; q++)
            p[q] = xr[q] * g->cref[q];
        }
      cf *o = g->rows + r * g->nx;
      dp_fft_execute_cf32 (g->inv_n[t], p, g->nx, o, g->nx);
      for (size_t q = 0; q < g->nx; q++)
        o[q] *= inv;
    }
}

static void
st_inv_padded (size_t t, void *v)
{
  st_inv (t, v, 1);
}

static void
st_inv_mix (size_t t, void *v)
{
  st_inv (t, v, 0);
}

/* The slow-time stage for one carrier: worker t takes rates i0 .. i1-1 and
   every delay column. A tile of TILE columns is gathered from the
   correlated rows once, then for each of its rates weighted, transformed
   down each column and folded, centered, into this worker's accumulator.
   Rates inside the tile loop is the ordering that reads each correlated
   tile once per carrier, not once per hypothesis; and one dispatch per
   carrier, not one per hypothesis, is what keeps the pool's wake-up cost
   off every map. */
static void
st_col (size_t t, void *v)
{
  ctx_t *c = (ctx_t *)v;
  geo_t *g = c->g;
  size_t i0, i1;
  chunk (g->rates, t, (int)c->nwr, &i0, &i1);
  cf    *raw  = g->raw + t * TILE * g->ny;
  cf    *col  = g->col + t * TILE * g->ny_out;
  cf    *colo = g->colo + t * TILE * g->ny_out;
  float *acc  = g->accw + t * g->ny_out * g->nx;
  for (size_t b = 0; b < g->nx; b += TILE)
    {
      size_t nb = (g->nx - b < TILE) ? g->nx - b : TILE;
      for (size_t r = 0; r < g->ny; r++)
        {
          const cf *row = g->rows + r * g->nx + b;
          for (size_t k = 0; k < nb; k++)
            raw[k * g->ny + r] = row[k];
        }
      for (size_t i = i0; i < i1; i++)
        {
          const cf   *w  = g->w + i * g->ny;
          const float wt = (float)(c->h0 + i + 1);
          for (size_t k = 0; k < nb; k++)
            for (size_t r = 0; r < g->ny; r++)
              col[k * g->ny_out + r] = raw[k * g->ny + r] * w[r];
          for (size_t k = 0; k < nb; k++)
            dp_fft_execute_cf32 (g->slow[t], col + k * g->ny_out, g->ny_out,
                                 colo + k * g->ny_out, g->ny_out);
          for (size_t k = 0; k < nb; k++)
            for (size_t u = 0; u < g->ny_out; u++)
              {
                cf     y   = colo[k * g->ny_out + u];
                size_t row = (u + g->ny_out / 2) % g->ny_out; /* fftshift */
                acc[row * g->nx + b + k]
                    += wt
                       * (crealf (y) * crealf (y) + cimagf (y) * cimagf (y));
              }
        }
    }
}

/* Sum the per-worker folds into acc, in worker order. */
static void
gather_acc (geo_t *g)
{
  size_t n = g->ny_out * g->nx;
  memset (g->acc, 0, n * sizeof *g->acc);
  for (int t = 0; t < g->nw; t++)
    for (size_t k = 0; k < n; k++)
      g->acc[k] += g->accw[(size_t)t * n + k];
  memset (g->accw, 0, (size_t)g->nw * n * sizeof *g->accw);
}

/* One frame, ordered: padded (method 1) or mix (method 0). */
static void
run_ordered (geo_t *g, int padded, stages_t *st)
{
  ctx_t c = { g, 0, 0, 0 };
  c.nwr   = (size_t)g->nw < g->rates ? (size_t)g->nw : g->rates;
  double t0;
  if (padded)
    {
      t0 = now_s ();
      dp_pool_run (g->pool, (size_t)g->nw, st_fwd_padded, &c);
      st->fwd += now_s () - t0;
    }
  for (size_t j = 0; j < g->carriers; j++)
    {
      c.j  = j;
      c.h0 = j * g->rates;
      if (!padded)
        {
          t0 = now_s ();
          dp_pool_run (g->pool, (size_t)g->nw, st_fwd_mix, &c);
          st->fwd += now_s () - t0;
        }
      t0 = now_s ();
      dp_pool_run (g->pool, (size_t)g->nw, padded ? st_inv_padded : st_inv_mix,
                   &c);
      st->inv += now_s () - t0;
      t0 = now_s ();
      dp_pool_run (g->pool, c.nwr, st_col, &c);
      st->col += now_s () - t0;
    }
  t0 = now_s ();
  gather_acc (g);
  st->other += now_s () - t0;
}

/* One frame, full pipeline: every hypothesis mixed and weighted here, then
   run through the shipped DelayDoppler, and folded. */
static void
run_full (geo_t *g, stages_t *st)
{
  size_t n = g->ny_out * g->nx;
  memset (g->acc, 0, n * sizeof *g->acc);
  for (size_t j = 0; j < g->carriers; j++)
    for (size_t i = 0; i < g->rates; i++)
      {
        double    t0 = now_s ();
        const cf *ph = g->ph + j * g->nx;
        const cf *w  = g->w + i * g->ny;
        for (size_t r = 0; r < g->ny; r++)
          for (size_t k = 0; k < g->nx; k++)
            g->hin[r * g->nx + k] = g->x[r * g->nx + k] * ph[k] * w[r];
        double t1 = now_s ();
        (void)dp_corr2d_execute (g->dd, g->hin, g->ny * g->nx, g->hout, n);
        double      t2 = now_s ();
        const float wt = (float)(j * g->rates + i + 1);
        for (size_t k = 0; k < n; k++)
          g->acc[k] += wt
                       * (crealf (g->hout[k]) * crealf (g->hout[k])
                          + cimagf (g->hout[k]) * cimagf (g->hout[k]));
        st->other += (t1 - t0) + (now_s () - t2);
        st->fwd += t2 - t1; /* the whole shipped pipeline, one call */
      }
}

/* ---- setup ----------------------------------------------------------- */

static void *
xcalloc (size_t n, size_t sz)
{
  void *p = calloc (n ? n : 1, sz);
  if (!p)
    {
      (void)fprintf (stderr, "validate_corr2d_hypothesis_order: no memory\n");
      exit (2);
    }
  return p;
}

/* A small deterministic generator, so the frame is the same every run. */
static double
urand (unsigned long long *s)
{
  *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
  return (double)(*s >> 11) / 9007199254740992.0;
}

static cf
cis (double a)
{
  return (float)cos (a) + I * (float)sin (a);
}

static geo_t *
geo_create (size_t ny, size_t nx, size_t ny_out, size_t lf, size_t rates,
            int nw)
{
  geo_t *g             = (geo_t *)xcalloc (1, sizeof *g);
  g->ny                = ny;
  g->nx                = nx;
  g->ny_out            = ny_out;
  g->lf                = lf;
  g->m                 = lf * nx;
  g->carriers          = lf;
  g->rates             = rates;
  g->nw                = nw;
  unsigned long long s = 2683;
  g->x                 = (cf *)xcalloc (ny * nx, sizeof (cf));
  g->code              = (cf *)xcalloc (nx, sizeof (cf));
  g->cref              = (cf *)xcalloc (nx, sizeof (cf));
  for (size_t k = 0; k < ny * nx; k++)
    g->x[k] = (float)(urand (&s) - 0.5) + I * (float)(urand (&s) - 0.5);
  for (size_t k = 0; k < nx; k++)
    g->code[k] = cis (2 * M_PI * urand (&s));
  dp_fft_state_t *f = dp_fft_create (nx, -1, 1);
  dp_fft_execute_cf32 (f, g->code, nx, g->cref, nx);
  dp_fft_destroy (f);
  for (size_t k = 0; k < nx; k++)
    g->cref[k] = conjf (g->cref[k]);
  g->w = (cf *)xcalloc (rates * ny, sizeof (cf));
  for (size_t i = 0; i < rates; i++)
    {
      double a
          = rates > 1 ? -1.0 + 2.0 * (double)i / (double)(rates - 1) : 0.0;
      for (size_t r = 0; r < ny; r++)
        {
          double rr        = (double)r - (double)(ny - 1) / 2;
          g->w[i * ny + r] = cis (-M_PI * a * rr * rr / (double)ny);
        }
    }
  g->ph = (cf *)xcalloc (lf * nx, sizeof (cf));
  for (size_t j = 0; j < lf; j++)
    for (size_t n = 0; n < nx; n++)
      g->ph[j * nx + n]
          = cis (-2 * M_PI * (double)j * (double)n / (double)g->m);
  g->xm   = (cf *)xcalloc (ny * g->m, sizeof (cf));
  g->xj   = (cf *)xcalloc (ny * nx, sizeof (cf));
  g->rows = (cf *)xcalloc (ny * nx, sizeof (cf));
  g->pad  = (cf *)xcalloc ((size_t)nw * g->m, sizeof (cf));
  g->spec = (cf *)xcalloc ((size_t)nw * nx, sizeof (cf));
  g->raw  = (cf *)xcalloc ((size_t)nw * TILE * ny, sizeof (cf));
  g->col  = (cf *)xcalloc ((size_t)nw * TILE * ny_out, sizeof (cf));
  g->colo = (cf *)xcalloc ((size_t)nw * TILE * ny_out, sizeof (cf));
  g->acc  = (float *)xcalloc (ny_out * nx, sizeof (float));
  g->accw = (float *)xcalloc ((size_t)nw * ny_out * nx, sizeof (float));
  for (int t = 0; t < nw; t++)
    {
      g->fwd_m[t] = dp_fft_create (g->m, -1, 1);
      g->fwd_n[t] = dp_fft_create (nx, -1, 1);
      g->inv_n[t] = dp_fft_create (nx, 1, 1);
      g->slow[t]  = dp_fft_create (ny_out, -1, 1);
    }
  g->pool = nw > 1 ? dp_pool_create (nw) : NULL;
  g->dd   = dp_corr2d_create_delay_doppler (g->code, nx, ny, 1, nw, ny_out);
  g->hin  = (cf *)xcalloc (ny * nx, sizeof (cf));
  g->hout = (cf *)xcalloc (ny_out * nx, sizeof (cf));
  if (!g->dd)
    {
      (void)fprintf (stderr, "validate_corr2d_hypothesis_order: "
                             "DelayDoppler refused the geometry\n");
      exit (2);
    }
  return g;
}

static void
geo_destroy (geo_t *g)
{
  for (int t = 0; t < g->nw; t++)
    {
      dp_fft_destroy (g->fwd_m[t]);
      dp_fft_destroy (g->fwd_n[t]);
      dp_fft_destroy (g->inv_n[t]);
      dp_fft_destroy (g->slow[t]);
    }
  dp_pool_destroy (g->pool);
  dp_corr2d_destroy (g->dd);
  free (g->x);
  free (g->code);
  free (g->cref);
  free (g->w);
  free (g->ph);
  free (g->xm);
  free (g->xj);
  free (g->rows);
  free (g->pad);
  free (g->spec);
  free (g->raw);
  free (g->col);
  free (g->colo);
  free (g->acc);
  free (g->accw);
  free (g->hin);
  free (g->hout);
  free (g);
}

/* ---- the checks and the timings -------------------------------------- */

/* Largest |a - b| relative to the largest |b|. */
static double
rel (const float *a, const float *b, size_t n)
{
  double d = 0, m = 0;
  for (size_t k = 0; k < n; k++)
    {
      double e = fabs ((double)a[k] - (double)b[k]);
      d        = e > d ? e : d;
      m        = fabs ((double)b[k]) > m ? fabs ((double)b[k]) : m;
    }
  return d / m;
}

/* The three pipelines agree on one frame, serial and on a pool. */
static int
check (void)
{
  int fails = 0;
  for (int nw = 1; nw <= 3; nw += 2)
    {
      geo_t   *g  = geo_create (8, 64, 16, 4, 3, nw);
      size_t   n  = g->ny_out * g->nx;
      float   *fu = (float *)xcalloc (n, sizeof (float));
      float   *pd = (float *)xcalloc (n, sizeof (float));
      stages_t st = { 0, 0, 0, 0 };
      run_full (g, &st);
      memcpy (fu, g->acc, n * sizeof *fu);
      run_ordered (g, 1, &st);
      memcpy (pd, g->acc, n * sizeof *pd);
      run_ordered (g, 0, &st);
      double ep = rel (pd, fu, n), em = rel (g->acc, fu, n);
      int    ok = ep < 1e-4 && em < 1e-4;
      (void)printf ("%s workers %d: padded vs full %.1e, mix vs full %.1e "
                    "(held to 1e-4)\n",
                    ok ? "ok  " : "FAIL", nw, ep, em);
      fails += !ok;
      free (fu);
      free (pd);
      geo_destroy (g);
    }
  return fails ? 1 : 0;
}

/* The minimum over REPEATS frames of one pipeline (0 full, 1 padded,
   2 mix), its stages taken from that same fastest frame. */
static stages_t
best (geo_t *g, int which)
{
  stages_t keep = { 1e30, 0, 0, 0 };
  for (int k = -1; k < REPEATS; k++) /* k = -1 warms the plans and caches */
    {
      stages_t st = { 0, 0, 0, 0 };
      if (which == 0)
        run_full (g, &st);
      else
        run_ordered (g, which == 1, &st);
      double tot = st.fwd + st.inv + st.col + st.other;
      if (k >= 0 && tot < keep.fwd + keep.inv + keep.col + keep.other)
        keep = st;
    }
  return keep;
}

int
main (int argc, char **argv)
{
  if (argc > 1 && strcmp (argv[1], "--check") == 0)
    return check ();
  const int    emit = argc > 1 && strcmp (argv[1], "--emit") == 0;
  const size_t ny = 64, ny_out = 64, rates = 12;
  const size_t nxs[] = { 512, 1023 };
  const size_t lfs[] = { 2, 4, 8 };
  int          pool  = dp_cpu_count ();
  if (pool > (int)ny)
    pool = (int)ny;
  if (pool > MAXW)
    pool = MAXW;
  /* Serial, a small pool and every usable core: a run of dp_pool_run wakes
     and waits for every helper, so its cost grows with the pool, and the
     middle row separates that from the work the stage does. */
  const int   nws[]  = { 1, 4, pool };
  const char *name[] = { "full", "padded", "mix" };
  if (emit)
    (void)printf ("{\n \"geometry\": {\"ny\": %zu, \"ny_out\": %zu, "
                  "\"rates\": %zu, \"repeats\": %d, \"pool\": %d},\n"
                  " \"rows\": [",
                  ny, ny_out, rates, REPEATS, pool);
  else
    (void)printf ("%-6s %-3s %-7s %-7s %10s %9s %9s %9s %9s\n", "nx", "L",
                  "workers", "method", "frame ms", "fwd", "inv", "col",
                  "other");
  int first = 1;
  for (size_t a = 0; a < 2; a++)
    for (size_t b = 0; b < 3; b++)
      for (size_t c = 0; c < 3; c++)
        {
          if (c > 0 && nws[c] <= nws[c - 1])
            continue;
          geo_t *g = geo_create (ny, nxs[a], ny_out, lfs[b], rates, nws[c]);
          for (int which = 0; which < 3; which++)
            {
              stages_t s   = best (g, which);
              double   tot = s.fwd + s.inv + s.col + s.other;
              if (emit)
                (void)printf ("%s\n  {\"nx\": %zu, \"L\": %zu, \"carriers\": "
                              "%zu, \"workers\": %d, \"method\": \"%s\", "
                              "\"frame_ms\": %.4f, \"fwd_ms\": %.4f, "
                              "\"inv_ms\": %.4f, \"col_ms\": %.4f, "
                              "\"other_ms\": %.4f}",
                              first ? "" : ",", nxs[a], lfs[b], g->carriers,
                              nws[c], name[which], 1e3 * tot, 1e3 * s.fwd,
                              1e3 * s.inv, 1e3 * s.col, 1e3 * s.other);
              else
                (void)printf ("%-6zu %-3zu %-7d %-7s %10.3f %9.3f %9.3f "
                              "%9.3f %9.3f\n",
                              nxs[a], lfs[b], nws[c], name[which], 1e3 * tot,
                              1e3 * s.fwd, 1e3 * s.inv, 1e3 * s.col,
                              1e3 * s.other);
              first = 0;
            }
          geo_destroy (g);
        }
  if (emit)
    (void)printf ("\n ]\n}\n");
  return 0;
}
