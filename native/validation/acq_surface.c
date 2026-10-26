/**
 * @file acq_surface.c
 * @brief What one emitter puts on the continuous engine's surface, by the
 *        code's ambiguity function, and what the engine's peak lists hold:
 *        the numbers of docs/design/acq-surface.md §8.
 *
 * The geometry is the receiver validators' operating point, read off a
 * continuous engine built there rather than restated: the 1023-chip Gold
 * code of dp_dsss_gold_1023(), 3.069 Mcps at two samples a chip, an
 * uncertainty of +/-5 kHz, and the specification window (code_only_epochs 449,
 * (docs/design/async-dsss-receiver.md) doppler_rate 500 Hz/s). From the engine
 * come the epoch N (`code_bins`), the tiles W (`window_bins`) at the epoch
 * rate f_E = 2 `doppler_span_hz`, the depth D (`coherent_bins`), the
 * interpolation I (`interp`) and the zone (`shape.zone`, samples). The code's
 * ambiguity is computed through the library's FFT,
 *
 *     |chi(Delta, nu)| = |IFFT{ FFT{c e^{j 2 pi nu t}} conj(FFT{c}) }[Delta]|,
 *
 * the circular correlation §3 of the page performs, with c the +-1
 * template held S samples a chip.
 *
 * Sections, each a block of the page (--emit prints them all as one JSON
 * document, which docs/design/acq-surface-measurements/record.py renders):
 *
 *   ambiguity  for each emitter Doppler f of the page, the largest
 *              |chi(Delta, f - q f_E)| with Delta more than a chip from 0,
 *              over every tile q, relative to the emitter's own peak (the
 *              largest |chi(0, f - q f_E)|), with every tile that reaches
 *              it (two at a tile edge, where the emitter is symmetric about
 *              the edge) and its circular distance from the peak in chips;
 *   sweep      the same maximum with the emitter swept over the whole
 *              uncertainty every SWEEP_HZ: the band's worst, and where;
 *   fast       a window on the epoch before the correlation. Its SNR kept
 *              at residual nu is |sum w[n] e^{j 2 pi nu t_n}|^2 / (N sum
 *              w^2), so the noise bandwidth is in it: at the tile edge,
 *              averaged over the tile (the mean power, in dB), the next
 *              tile against the window's own centre, and the ambiguity
 *              row above with the window on;
 *   slow       a window on the block's D epochs before FFT_DI. With the
 *              emitter rho native bins from a fine row, the row keeps
 *              |sum w[i] e^{j 2 pi rho i / D}|^2 / (D sum w^2): the worst
 *              over the half fine bin an emitter can sit from its nearest
 *              row, the average over it, and the largest row past the zone
 *              (the window's main lobe, in native bins) relative to the
 *              picked row, over every emitter offset;
 *   lists      the shipped engine's peak lists over the windowed stream:
 *              per depth and C/N0, how many lists hold a peak more than a
 *              chip from the list's first, and the largest such peak
 *              relative to the first, in power;
 *   data       the deep engine on a stream that is data throughout.
 *
 * A window is the symmetric one whose zeros fall outside the block, so no
 * epoch or sample is weighted zero: Hann (the library's dp_hann_window())
 * and Blackman, which reach zero, are L+2 points with their two zeros
 * dropped; Hamming, which does not, is L points. Each window's zone is its
 * main lobe's half-width: 1 native bin for none, 2 for Hann and Hamming,
 * 3 for Blackman.
 *
 * Usage:
 *   validate_acq_surface            the tables, readable (about 90 s)
 *   validate_acq_surface --emit     every section as one JSON document
 *   validate_acq_surface --check    the spot checks CTest runs (seconds):
 *                                   chi at zero Doppler is the engine's
 *                                   own psl; the tile-edge row ties two
 *                                   tiles; the band's worst exceeds every
 *                                   row's; no window's worst loss is
 *                                   smaller than its average; a window
 *                                   raises the off-lag sidelobes; a deep
 *                                   list's off-lag peaks sit near the
 *                                   ambiguity, and a deep engine on data
 *                                   lists none
 */
#include "doppler/acq/acq_core.h"
#include "doppler/clib_common.h"
#include "doppler/fft/fft_core.h"
#include "doppler/spectral/spectral_core.h"
#include "dp_dsss_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The operating point. */
enum
{
  SF  = 1023,
  SPC = 2
};
static const double CHIP_RATE = 3.069e6;
static const double FS        = 3.069e6 * SPC;
static const double SYM_RATE  = 2700.0;
static const double U_HZ      = 5000.0; /* one-sided uncertainty */
static const double CN0_SIZED = 47.0;   /* the engine's design C/N0 */
static const double PFA = 1e-3, PD = 0.9;
/* The specification window, in whole code-only epochs, and its Doppler rate.
 */
enum
{
  CODE_ONLY_EPOCHS = 449
};
static const double DOPPLER_RATE = 500.0;

/* The page's emitters, and the band sweep's step. */
static const double ROW_HZ[] = { 200.0, 1500.0, -2600.0 };
enum
{
  N_ROWS = sizeof ROW_HZ / sizeof ROW_HZ[0]
};
static const double SWEEP_HZ = 25.0;

/* The list probe: 600-symbol streams framed 450 code-only symbols in 500,
   PRE samples of noise per seed before the signal, max_peaks 16. */
static const double LIST_HZ[]  = { 200.0, 1500.0, -2600.0, 750.0 };
static const double LIST_CN0[] = { 47.0, 50.0, 53.0, 56.0, 60.0, 70.0 };
static const double DATA_CN0[] = { 60.0, 70.0 };
enum
{
  N_LIST_HZ  = sizeof LIST_HZ / sizeof LIST_HZ[0],
  N_LIST_CN0 = sizeof LIST_CN0 / sizeof LIST_CN0[0],
  N_DATA_CN0 = sizeof DATA_CN0 / sizeof DATA_CN0[0],
  N_SEEDS    = 6,
  N_SYM      = 600,
  W_SYM      = 450,
  F_SYM      = 500,
  PRE        = 300,
  MAX_PEAKS  = 16
};

/* Two cells within this many dB are a tie. */
static const double TIE_DB = 0.005;

typedef enum
{
  WIN_NONE,
  WIN_HANN,
  WIN_HAMMING,
  WIN_BLACKMAN,
  N_WIN
} win_t;
static const char *const WIN_NAME[N_WIN]
    = { "none", "hann", "hamming", "blackman" };
/* The main lobe's half-width, native bins: the zone a window needs. */
static const int WIN_ZONE[N_WIN] = { 1, 2, 2, 3 };

/* Fill w[0..n) with window k (the file doc's rule). */
static void
window_fill (win_t k, double *w, size_t n)
{
  if (k == WIN_HANN)
    {
      float *h = malloc ((n + 2) * sizeof *h);
      dp_hann_window (h, n + 2);
      for (size_t i = 0; i < n; i++)
        w[i] = h[i + 1];
      free (h);
      return;
    }
  for (size_t i = 0; i < n; i++)
    {
      const double x  = 2.0 * M_PI * (double)i / (double)(n - 1);
      const double xb = 2.0 * M_PI * (double)(i + 1) / (double)(n + 1);
      w[i] = k == WIN_HAMMING    ? 0.54 - 0.46 * cos (x)
             : k == WIN_BLACKMAN ? 0.42 - 0.5 * cos (xb) + 0.08 * cos (2 * xb)
                                 : 1.0;
    }
}

static double
db10 (double p)
{
  return 10.0 * log10 (p);
}

/* ── the code's ambiguity ─────────────────────────────────────────────── */

typedef struct
{
  size_t           n, spc, w_tiles;
  double           f_e, fs;
  dp_fft_state_t  *fwd, *inv;
  double _Complex *tmpl_conj; /* conj FFT{c} */
  double          *c;         /* the +-1 template */
  double _Complex *s, *spec, *z;
} chi_t;

static void
chi_open (chi_t *x, const uint8_t *code, const dp_acq_state_t *a)
{
  x->n       = a->code_bins;
  x->spc     = a->spc;
  x->w_tiles = a->window_bins;
  if (x->w_tiles > 16)
    {
      (void)fprintf (stderr, "validate_acq_surface: %zu tiles, at most 16\n",
                     x->w_tiles);
      exit (1);
    }
  x->f_e       = 2.0 * a->doppler_span_hz;
  x->fs        = FS;
  x->fwd       = dp_fft_create (x->n, -1, 1);
  x->inv       = dp_fft_create (x->n, +1, 1);
  x->c         = malloc (x->n * sizeof *x->c);
  x->s         = malloc (x->n * sizeof *x->s);
  x->spec      = malloc (x->n * sizeof *x->spec);
  x->z         = malloc (x->n * sizeof *x->z);
  x->tmpl_conj = malloc (x->n * sizeof *x->tmpl_conj);
  for (size_t i = 0; i < x->n; i++)
    {
      x->c[i] = code[i / x->spc] & 1 ? -1.0 : 1.0;
      x->s[i] = x->c[i];
    }
  dp_fft_execute_cf64 (x->fwd, x->s, x->n, x->tmpl_conj, x->n);
  for (size_t i = 0; i < x->n; i++)
    x->tmpl_conj[i] = conj (x->tmpl_conj[i]);
}

static void
chi_close (chi_t *x)
{
  dp_fft_destroy (x->fwd);
  dp_fft_destroy (x->inv);
  free (x->c);
  free (x->s);
  free (x->spec);
  free (x->z);
  free (x->tmpl_conj);
}

/* |chi_w(lag, nu)| for every lag into x->z (as magnitudes in .real). */
static void
chi_row (chi_t *x, const double *w, double nu)
{
  for (size_t i = 0; i < x->n; i++)
    {
      const double ph = 2.0 * M_PI * nu * (double)i / x->fs;
      x->s[i]         = w[i] * x->c[i] * (cos (ph) + I * sin (ph));
    }
  dp_fft_execute_cf64 (x->fwd, x->s, x->n, x->spec, x->n);
  for (size_t i = 0; i < x->n; i++)
    x->spec[i] *= x->tmpl_conj[i];
  dp_fft_execute_cf64 (x->inv, x->spec, x->n, x->z, x->n);
  for (size_t i = 0; i < x->n; i++)
    x->z[i] = cabs (x->z[i]);
}

/* The largest off-lag |chi| over every tile, relative to the emitter's own
   peak, with each tile that reaches it and its distance in chips. */
typedef struct
{
  double db;
  int    tiles[8];
  int    n_tiles;
  double chips;
} worst_t;

static worst_t
chi_worst (chi_t *x, const double *w, double f)
{
  double own = 0.0, side[16] = { 0 };
  size_t lag[16] = { 0 };
  long   q[16];
  for (size_t r = 0; r < x->w_tiles; r++)
    {
      q[r] = dp_fftfreq_index (r, x->w_tiles);
      chi_row (x, w, f - (double)q[r] * x->f_e);
      if (creal (x->z[0]) > own)
        own = creal (x->z[0]);
      for (size_t m = x->spc + 1; m + x->spc < x->n; m++)
        if (creal (x->z[m]) > side[r])
          {
            side[r] = creal (x->z[m]);
            lag[r]  = m;
          }
    }
  double best = 0.0;
  for (size_t r = 0; r < x->w_tiles; r++)
    if (side[r] > best)
      best = side[r];
  worst_t o = { .db = 20.0 * log10 (best / own) };
  /* Every tile within TIE_DB of the worst, in ascending q. */
  for (long qq = -(long)x->w_tiles; qq <= (long)x->w_tiles; qq++)
    for (size_t r = 0; r < x->w_tiles; r++)
      if (q[r] == qq && fabs (20.0 * log10 (side[r] / best)) < TIE_DB
          && o.n_tiles < 8)
        {
          const size_t d = lag[r] < x->n - lag[r] ? lag[r] : x->n - lag[r];
          o.tiles[o.n_tiles++] = (int)qq;
          o.chips              = (double)d / (double)x->spc;
        }
  return o;
}

/* ── a window on either transform ─────────────────────────────────────── */

/* The SNR a window of n points keeps at a residual of `cyc` cycles over
   the n points, noise bandwidth included: |sum w e^{j 2 pi cyc i/n}|^2 /
   (n sum w^2). */
static double
kept (const double *w, size_t n, double cyc)
{
  double _Complex acc = 0.0;
  double e            = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      const double ph = 2.0 * M_PI * cyc * (double)i / (double)n;
      acc += w[i] * (cos (ph) + I * sin (ph));
      e += w[i] * w[i];
    }
  return creal (acc * conj (acc)) / ((double)n * e);
}

typedef struct
{
  double edge_db, avg_db, next_db; /* next_db -INFINITY: a null */
  double side_db[N_ROWS];
} fast_t;

static fast_t
fast_window (chi_t *x, win_t k)
{
  double *w = malloc (x->n * sizeof *w);
  window_fill (k, w, x->n);
  fast_t o;
  /* The residual nu turns nu T_E = nu / f_E cycles over the epoch. */
  o.edge_db     = db10 (kept (w, x->n, 0.5));
  const int K   = 1200;
  double    sum = 0.0;
  for (int i = 0; i < K; i++)
    sum += kept (w, x->n, -0.5 + (i + 0.5) / K);
  o.avg_db          = db10 (sum / K);
  const double next = kept (w, x->n, 1.0) / kept (w, x->n, 0.0);
  o.next_db         = next < 1e-12 ? -INFINITY : db10 (next);
  for (int r = 0; r < N_ROWS; r++)
    o.side_db[r] = chi_worst (x, w, ROW_HZ[r]).db;
  free (w);
  return o;
}

typedef struct
{
  double worst_db, avg_db, leak_db;
} slow_t;

static slow_t
slow_window (win_t k, size_t d, size_t interp)
{
  double *w = malloc (d * sizeof *w);
  window_fill (k, w, d);
  const double half = 0.5 / (double)interp; /* half a fine bin, native */
  slow_t       o    = { .worst_db = 0.0, .leak_db = -INFINITY };
  /* The worst: the furthest an emitter sits from its nearest fine row. */
  for (int i = 0; i <= 500; i++)
    {
      const double v = db10 (kept (w, d, half * i / 500.0));
      if (v < o.worst_db)
        o.worst_db = v;
    }
  const int K   = 1000;
  double    sum = 0.0;
  for (int i = 0; i < K; i++)
    sum += kept (w, d, -half + 2.0 * half * (i + 0.5) / K);
  o.avg_db = db10 (sum / K);
  /* The leakage: an emitter rho0 from row 0, the fine rows j / interp of
     one slow-time period, the strongest one picked, and the largest row
     past the window's zone relative to it. */
  const long rows = (long)(d * interp);
  double    *p    = malloc ((size_t)rows * sizeof *p);
  for (int s = 0; s <= 200; s++)
    {
      const double rho0 = -half + 2.0 * half * s / 200.0;
      long         pick = 0;
      for (long j = 0; j < rows; j++)
        {
          const long jj = j - rows / 2;
          p[j]          = kept (w, d, (double)jj / (double)interp - rho0);
          if (p[j] > p[pick])
            pick = j;
        }
      for (long j = 0; j < rows; j++)
        if (labs (j - pick) > (long)(WIN_ZONE[k] * (long)interp))
          {
            const double v = db10 (p[j] / p[pick]);
            if (v > o.leak_db)
              o.leak_db = v;
          }
    }
  free (p);
  free (w);
  return o;
}

/* ── the engine's peak lists ──────────────────────────────────────────── */

static dp_acq_state_t *
engine (const uint8_t *code, int deep)
{
  dp_acq_state_t *a = dp_acq_create_continuous (
      code, SF, SPC, CHIP_RATE, SYM_RATE, CN0_SIZED, U_HZ, PFA, PD, 0,
      deep ? CODE_ONLY_EPOCHS : 1, deep ? DOPPLER_RATE : 0.0);
  if (!a || dp_acq_set_max_peaks (a, MAX_PEAKS) != 0)
    {
      (void)fprintf (stderr, "validate_acq_surface: the engine refused the "
                             "operating point\n");
      exit (1);
    }
  return a;
}

/* Circular distance between two chip phases, chips. */
static double
chip_dist (double a, double b)
{
  const double d = fabs (a - b);
  return d > SF / 2.0 ? SF - d : d;
}

typedef struct
{
  size_t lists, off_lists, n_off;
  double worst_db; /* -INFINITY: no off-lag peak in any list */
} lists_t;

/* One stream through a fresh engine; accumulate into `o`. */
static void
list_stream (const uint8_t *code, int deep, double f_hz, double cn0,
             uint32_t seed, size_t f_sym, lists_t *o)
{
  float _Complex *x;
  double         *data;
  size_t          n;
  dp_dsss_windowed_capture (code, SF, SPC, FS, FS / SYM_RATE, f_hz, cn0, N_SYM,
                            PRE * seed, seed, W_SYM, f_sym, &x, &n, &data);
  dp_acq_state_t *a = engine (code, deep);
  acq_result_t    h[MAX_PEAKS];
  size_t          pos = 0;
  while (pos < n)
    {
      const size_t k    = dp_acq_push (a, x + pos, n - pos, h, MAX_PEAKS);
      const size_t took = dp_acq_consumed (a);
      pos += took;
      if (k)
        {
          o->lists++;
          acq_handoff_t h0;
          dp_acq_build_handoff (a, &h[0], SF, SPC, &h0);
          int any = 0;
          for (size_t i = 1; i < k; i++)
            {
              acq_handoff_t hi;
              dp_acq_build_handoff (a, &h[i], SF, SPC, &hi);
              if (chip_dist (hi.chip_phase, h0.chip_phase) > 1.0)
                {
                  const double v
                      = 20.0 * log10 (h[i].peak_mag / h[0].peak_mag);
                  if (v > o->worst_db)
                    o->worst_db = v;
                  o->n_off++;
                  any = 1;
                }
            }
          o->off_lists += (size_t)any;
        }
      if (!took)
        break;
    }
  dp_acq_destroy (a);
  free (x);
  free (data);
}

static lists_t
lists_at (const uint8_t *code, int deep, double cn0, size_t f_sym)
{
  lists_t o = { .worst_db = -INFINITY };
  for (int f = 0; f < N_LIST_HZ; f++)
    for (uint32_t seed = 1; seed <= N_SEEDS; seed++)
      list_stream (code, deep, LIST_HZ[f], cn0, seed, f_sym, &o);
  return o;
}

/* ── output ───────────────────────────────────────────────────────────── */

/* A dB value as JSON: a number, or null for a null (-inf). */
static void
jnum (double v)
{
  if (isinf (v))
    printf ("null");
  else
    printf ("%.4f", v);
}

int
main (int argc, char **argv)
{
  const int check = argc > 1 && strcmp (argv[1], "--check") == 0;
  const int emit  = argc > 1 && strcmp (argv[1], "--emit") == 0;
  uint8_t   code[SF];
  dp_dsss_gold_1023 (code);

  dp_acq_state_t *deep = engine (code, 1);
  chi_t           x;
  chi_open (&x, code, deep);
  const size_t d = deep->coherent_bins, interp = deep->interp;
  double      *flat = malloc (x.n * sizeof *flat);
  window_fill (WIN_NONE, flat, x.n);

  worst_t amb[N_ROWS];
  for (int r = 0; r < N_ROWS; r++)
    amb[r] = chi_worst (&x, flat, ROW_HZ[r]);
  /* The zero-residual row alone: the code's periodic autocorrelation,
     whose largest off-lag value the constructor already holds as psl. */
  chi_row (&x, flat, 0.0);
  double psl = 0.0;
  for (size_t m = x.spc + 1; m + x.spc < x.n; m++)
    if (creal (x.z[m]) > psl)
      psl = creal (x.z[m]);
  const double psl_db   = 20.0 * log10 (psl / creal (x.z[0]));
  double       sweep_db = -INFINITY, sweep_hz = 0.0;
  for (double f = -U_HZ; f <= U_HZ + 1e-9; f += SWEEP_HZ)
    {
      const double v = chi_worst (&x, flat, f).db;
      if (v > sweep_db)
        {
          sweep_db = v;
          sweep_hz = f;
        }
    }
  fast_t fast[N_WIN];
  slow_t slow[N_WIN];
  for (int k = 0; k < N_WIN; k++)
    {
      fast[k] = fast_window (&x, (win_t)k);
      slow[k] = slow_window ((win_t)k, d, interp);
    }

  if (check)
    {
      DP_CHECK_MSG (fabs (psl_db - dp_acq_psl_db (deep)) < 0.01,
                    "chi at zero residual is the engine's own psl");
      DP_CHECK_MSG (amb[1].n_tiles == 2, "the tile-edge row ties two tiles");
      for (int r = 0; r < N_ROWS; r++)
        DP_CHECK_MSG (sweep_db > amb[r].db,
                      "the band's worst exceeds every row's");
      DP_CHECK_MSG (fabs (fast[WIN_NONE].edge_db - 20.0 * log10 (2.0 / M_PI))
                        < 0.01,
                    "the unwindowed tile edge is the Dirichlet's 2/pi");
      for (int k = 0; k < N_WIN; k++)
        {
          DP_CHECK_MSG (slow[k].worst_db <= slow[k].avg_db,
                        "no window's worst loss is smaller than its average");
          DP_CHECK_MSG (fast[k].edge_db <= fast[k].avg_db,
                        "the tile edge is the fast-time worst");
        }
      for (int k = WIN_HANN; k < N_WIN; k++)
        {
          DP_CHECK_MSG (slow[k].avg_db < slow[WIN_NONE].worst_db,
                        "a slow-time window costs more than the scalloping "
                        "it removes, at every Doppler");
          for (int r = 0; r < N_ROWS; r++)
            DP_CHECK_MSG (fast[k].side_db[r] > fast[WIN_NONE].side_db[r],
                          "a fast-time window raises the off-lag sidelobes");
        }
      /* One stream each: a deep list's off-lag peaks at high C/N0 sit
         within a dB of the ambiguity's band worst, and a deep engine on
         data lists none. */
      lists_t l = { .worst_db = -INFINITY };
      list_stream (code, 1, 1500.0, 70.0, 1u, F_SYM, &l);
      DP_CHECK_MSG (l.lists > 0 && l.off_lists > 0
                        && fabs (l.worst_db - sweep_db) < 1.0,
                    "a deep list's off-lag peak is the ambiguity's");
      lists_t dl = { .worst_db = -INFINITY };
      list_stream (code, 1, 1500.0, 70.0, 1u, 0, &dl);
      DP_CHECK_MSG (dl.off_lists == 0,
                    "a deep engine on data lists no off-lag peak");
      free (flat);
      chi_close (&x);
      dp_acq_destroy (deep);
      DP_TEST_END ("validate_acq_surface");
    }

  lists_t lists[2][N_LIST_CN0], data[N_DATA_CN0];
  size_t  depth[2];
  for (int dp = 0; dp < 2; dp++)
    {
      dp_acq_state_t *a = engine (code, dp);
      depth[dp]         = a->coherent_bins;
      dp_acq_destroy (a);
      for (int c = 0; c < N_LIST_CN0; c++)
        lists[dp][c] = lists_at (code, dp, LIST_CN0[c], F_SYM);
    }
  for (int c = 0; c < N_DATA_CN0; c++)
    data[c] = lists_at (code, 1, DATA_CN0[c], 0);

  if (emit)
    {
      printf ("{\n \"geometry\": {\"sf\": %d, \"spc\": %zu, \"n\": %zu, "
              "\"fs_hz\": %.1f, \"f_e_hz\": %.4f, \"window_bins\": %zu, "
              "\"coherent_bins\": %zu, \"interp\": %zu, \"zone\": %zu, "
              "\"psl_db\": %.4f, \"uncertainty_hz\": %.1f, \"sweep_hz\": "
              "%.1f},\n",
              SF, x.spc, x.n, FS, x.f_e, x.w_tiles, d, interp,
              deep->shape.zone, dp_acq_psl_db (deep), U_HZ, SWEEP_HZ);
      printf (" \"ambiguity\": [");
      for (int r = 0; r < N_ROWS; r++)
        {
          printf ("%s\n  {\"f_hz\": %.1f, \"db\": %.4f, \"tiles\": [",
                  r ? "," : "", ROW_HZ[r], amb[r].db);
          for (int t = 0; t < amb[r].n_tiles; t++)
            printf ("%s%d", t ? ", " : "", amb[r].tiles[t]);
          printf ("], \"chips\": %.1f}", amb[r].chips);
        }
      printf ("],\n \"sweep\": {\"db\": %.4f, \"at_hz\": %.1f},\n", sweep_db,
              sweep_hz);
      printf (" \"fast\": [");
      for (int k = 0; k < N_WIN; k++)
        {
          printf ("%s\n  {\"window\": \"%s\", \"edge_db\": ", k ? "," : "",
                  WIN_NAME[k]);
          jnum (fast[k].edge_db);
          printf (", \"avg_db\": ");
          jnum (fast[k].avg_db);
          printf (", \"next_db\": ");
          jnum (fast[k].next_db);
          printf (", \"side_db\": [");
          for (int r = 0; r < N_ROWS; r++)
            {
              printf ("%s", r ? ", " : "");
              jnum (fast[k].side_db[r]);
            }
          printf ("]}");
        }
      printf ("],\n \"slow\": [");
      for (int k = 0; k < N_WIN; k++)
        printf ("%s\n  {\"window\": \"%s\", \"worst_db\": %.4f, \"avg_db\": "
                "%.4f, \"leak_db\": %.4f, \"zone_bins\": %d}",
                k ? "," : "", WIN_NAME[k], slow[k].worst_db, slow[k].avg_db,
                slow[k].leak_db, WIN_ZONE[k]);
      printf ("],\n \"lists\": [");
      int first = 1;
      for (int dp = 0; dp < 2; dp++)
        for (int c = 0; c < N_LIST_CN0; c++)
          {
            const lists_t *l = &lists[dp][c];
            printf ("%s\n  {\"depth\": %zu, \"cn0\": %.1f, \"lists\": %zu, "
                    "\"off_lists\": %zu, \"n_off\": %zu, \"worst_db\": ",
                    first ? "" : ",", depth[dp], LIST_CN0[c], l->lists,
                    l->off_lists, l->n_off);
            jnum (l->worst_db);
            printf ("}");
            first = 0;
          }
      printf ("],\n \"data\": [");
      for (int c = 0; c < N_DATA_CN0; c++)
        printf ("%s\n  {\"depth\": %zu, \"cn0\": %.1f, \"lists\": %zu, "
                "\"off_lists\": %zu}",
                c ? "," : "", depth[1], DATA_CN0[c], data[c].lists,
                data[c].off_lists);
      printf ("]\n}\n");
    }
  else
    {
      printf ("geometry: N %zu, W %zu tiles at f_E %.1f Hz, D %zu, I %zu, "
              "zone %zu samples, psl %.2f dB\n",
              x.n, x.w_tiles, x.f_e, d, interp, deep->shape.zone,
              dp_acq_psl_db (deep));
      for (int r = 0; r < N_ROWS; r++)
        {
          printf ("  %7.0f Hz: worst off-lag %.2f dB, %.1f chips off, tile",
                  ROW_HZ[r], amb[r].db, amb[r].chips);
          for (int t = 0; t < amb[r].n_tiles; t++)
            printf (" %+d", amb[r].tiles[t]);
          printf ("\n");
        }
      printf ("  band worst %.2f dB at %.0f Hz (every %.0f Hz)\n", sweep_db,
              sweep_hz, SWEEP_HZ);
      for (int k = 0; k < N_WIN; k++)
        printf ("  %-8s fast: edge %6.2f avg %6.2f next %7.2f side %6.2f "
                "%6.2f %6.2f | slow: worst %6.2f avg %6.2f leak %7.2f\n",
                WIN_NAME[k], fast[k].edge_db, fast[k].avg_db, fast[k].next_db,
                fast[k].side_db[0], fast[k].side_db[1], fast[k].side_db[2],
                slow[k].worst_db, slow[k].avg_db, slow[k].leak_db);
      for (int dp = 0; dp < 2; dp++)
        for (int c = 0; c < N_LIST_CN0; c++)
          printf ("  D %-3zu %2.0f dB-Hz: %zu lists, %zu with an off-lag "
                  "peak, worst %.1f dB\n",
                  depth[dp], LIST_CN0[c], lists[dp][c].lists,
                  lists[dp][c].off_lists, lists[dp][c].worst_db);
      for (int c = 0; c < N_DATA_CN0; c++)
        printf ("  data, D %zu, %2.0f dB-Hz: %zu lists, %zu with an off-lag "
                "peak\n",
                depth[1], DATA_CN0[c], data[c].lists, data[c].off_lists);
    }
  free (flat);
  chi_close (&x);
  dp_acq_destroy (deep);
  return 0;
}
