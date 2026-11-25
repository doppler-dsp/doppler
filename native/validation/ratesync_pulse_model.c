/**
 * @file ratesync_pulse_model.c
 * @brief The rectangle as the matched bank sees it, and the best arm for it.
 *
 * RateSync's matched bank sits behind the planned cascade's integer stages
 * (halfbands, a CIC). Those stages band-limit the rectangle, so what reaches
 * the bank is not a rectangle, and the best arm for it is not one either.
 * This is the model that says what it is, for a plan as built
 * (docs/design/ratesync-timing.md, "The pulse the bank sees").
 *
 * The cascade ahead of the bank is linear and decimates by `D`. Write `g[n]`
 * for its response at the INPUT rate, so a bank-grid sample is
 * `s[j] = sum_i x[i] g[j*D - i]`. One unit symbol is `S` input samples of
 * one, starting at input `a`; `a mod D` is where the symbol's edge falls
 * inside a bank-grid interval. Then at the bank's input
 *
 *     q_a[j] = sum_{i=a}^{a+S-1} g[j*D - i]        the pulse
 *     r[k]   = sum_n g[n] g[n + k*D]               white input noise, there
 *
 * and an arm `h` (tap `t` on `s[j - t]`) returns, for that symbol and for
 * white noise of variance `N0` an input sample,
 *
 *     signal  h'q        noise variance  N0 * h'Rh,    R[t,u] = r[t - u]
 *
 * An ideal integrate-and-dump at the input rate has signal `S` and noise
 * variance `N0 * S`, so the arm's loss against it is
 *
 *     L(h) = (h'q)^2 / (S * h'Rh)        <= 1
 *
 * which `h = R^-1 q` maximizes: `L_opt = q'R^-1 q / S`. That is the matched
 * filter for the rectangle given the cascade, one arm a position of the
 * edge. The ISI of an arm is its response to the neighboring symbols,
 * `h'q_{a +- S} / h'q_a`.
 *
 * `g`, `q` and `r` are MEASURED from the built converter through its own
 * pre-terminal tap (dp_RateConverter_execute_ctrl_push_tap): an impulse at
 * each of the D input positions gives `g`. Nothing about a stage is assumed.
 * The arms compared are the ones the converter built
 * (the terminal Resampler's bank, droop compensation folded in).
 *
 * For each plan and each edge position this prints the best built arm's
 * loss and ISI beside the optimum's. `--check` holds what must be true of
 * the model itself: no arm beats the optimum, the optimum never exceeds the
 * ideal integrate-and-dump, and with no stage ahead of the bank (D = 1) and
 * the edge on the grid the built arm IS the optimum.
 *
 * Usage:  ratesync_pulse_model [--check]
 */

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/dp_complex.h"
#include "doppler/resamp/resamp_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "ratesync_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double db10 (double x);

typedef struct
{
  double sps; /* input samples a symbol                */
  size_t m;   /* outputs a symbol                      */
} plan_t;

/* q_a at bank sample j (input time j*d + first), for a symbol of s inputs
   starting at input a. */
static double
qat (const double *g, size_t d, size_t first, long j, long a, size_t s)
{
  double acc = 0.0;
  for (size_t i = 0; i < s; i++)
    acc += gat (g, (long)first + j * (long)d - (a + (long)i));
  return acc;
}

typedef struct
{
  double loss_built, loss_opt; /* linear, <= 1                       */
  double isi_built, isi_opt;   /* worst neighbor, as a ratio         */
  size_t arm;                  /* the built arm that did best        */
} cell_t;

/* One edge position: the best built arm and the optimum, each at its own
   best bank sample. */
static int
evaluate (const double *g, size_t d, size_t first, const resamp_state_t *rs,
          size_t s, size_t edge, cell_t *c)
{
  size_t nt = rs->num_taps;
  if (nt > MAXT)
    return -1;
  long   a = (long)I0 + (long)edge;
  double r[MAXT];
  for (size_t k = 0; k < nt; k++)
    {
      r[k] = 0.0;
      for (long n = -(long)GOFF; n < (long)(GLEN - GOFF); n++)
        r[k] += gat (g, n) * gat (g, n + (long)(k * d));
    }
  memset (c, 0, sizeof *c);
  /* From before the symbol to after the cascade has let go of it. */
  long reach = 0;
  for (long n = 0; n < (long)(GLEN - GOFF); n++)
    if (fabs (gat (g, n)) > 1e-12)
      reach = n;
  long j_lo = ((long)I0 - (long)first) / (long)d - 4;
  long j_hi = j_lo + ((long)s + reach) / (long)d + (long)nt + 8;
  for (long j = j_lo; j <= j_hi; j++)
    {
      double q[MAXT], qm[MAXT], qp[MAXT];
      for (size_t t = 0; t < nt; t++)
        {
          q[t]  = qat (g, d, first, j - (long)t, a, s);
          qm[t] = qat (g, d, first, j - (long)t, a - (long)s, s);
          qp[t] = qat (g, d, first, j - (long)t, a + (long)s, s);
        }
      /* the optimum: h = R^-1 q */
      static double mat[MAXT * MAXT];
      double        h[MAXT];
      for (size_t t = 0; t < nt; t++)
        {
          h[t] = q[t];
          for (size_t u = 0; u < nt; u++)
            mat[t * nt + u] = r[t > u ? t - u : u - t];
        }
      if (solve (mat, h, nt) == 0)
        {
          double sig = 0.0, im = 0.0, ip = 0.0;
          for (size_t t = 0; t < nt; t++)
            {
              sig += h[t] * q[t];
              im += h[t] * qm[t];
              ip += h[t] * qp[t];
            }
          double loss = sig / (double)s; /* q'R^-1 q / S */
          if (loss > c->loss_opt)
            {
              c->loss_opt = loss;
              c->isi_opt  = fmax (fabs (im), fabs (ip)) / fabs (sig);
            }
        }
      /* every built arm */
      for (size_t p = 0; p < rs->num_phases; p++)
        {
          const float *row = rs->bank + p * nt;
          double       sig = 0.0, im = 0.0, ip = 0.0, nv = 0.0;
          for (size_t t = 0; t < nt; t++)
            {
              sig += (double)row[t] * q[t];
              im += (double)row[t] * qm[t];
              ip += (double)row[t] * qp[t];
              for (size_t u = 0; u < nt; u++)
                nv += (double)row[t] * (double)row[u]
                      * r[t > u ? t - u : u - t];
            }
          double loss = nv > 0.0 ? sig * sig / ((double)s * nv) : 0.0;
          if (loss > c->loss_built)
            {
              c->loss_built = loss;
              c->isi_built  = fmax (fabs (im), fabs (ip)) / fabs (sig);
              c->arm        = p;
            }
        }
    }
  return 0;
}

/* ── The detectors on that pulse ──────────────────────────────────────────
 *
 * Two timing detectors, each fed the same symbols through the measured
 * cascade, open loop, at the eye center:
 *
 *   - Gardner, as RateSync runs it: e = Re{ conj(mid) * (on - prev) }, the
 *     on-time strobes and the one half a symbol between them, all from one
 *     arm. Run on the best BUILT arm and again on the optimum arm.
 *   - The derivative detector: e = Re{ conj(y) * dy }, y from the optimum
 *     arm h(tau) and dy from its derivative in the timing hypothesis,
 *     dh/dtau, a second bank. dh/dtau is the central difference of the
 *     optimum arms for an edge one input sample either side.
 *
 * For each, the slope Kd at lock (from the mean output with the symbols
 * moved one input sample early and late, no noise) and the output's
 * spectral density at zero, S_e(0) (BLK times the variance of BLK-symbol
 * block means). A loop of noise bandwidth bn then has a timing variance
 * of 2 * bn * S_e(0) / Kd^2 symbols^2, so S_e(0) / Kd^2 is what the two
 * detectors are compared on.
 */

#define NSYM 40000 /* symbols a run                         */
#define BLK 256    /* symbols a block mean                  */
#define NDET 3     /* gardner built, gardner optimum, derivative */

typedef struct
{
  double mean[NDET], s0[NDET];
} det_run_t;

typedef struct
{
  size_t nt, m4;       /* taps an arm, bank samples a symbol  */
  long   j_opt, j_blt; /* each arm's best bank sample         */
  double h[MAXT];      /* optimum arm, unit symbol out        */
  double hd[MAXT];     /* its derivative, per SYMBOL of timing */
  double b[MAXT];      /* best built arm, unit symbol out     */
} arms_t;

static void
autocorr (const double *g, size_t d, size_t nt, double *r)
{
  for (size_t k = 0; k < nt; k++)
    {
      r[k] = 0.0;
      for (long n = -(long)GOFF; n < (long)(GLEN - GOFF); n++)
        r[k] += gat (g, n) * gat (g, n + (long)(k * d));
    }
}

/* h = R^-1 q for a symbol starting at input a, on bank sample j. Returns
   h'q, or a negative number when the solve fails. */
static double
opt_arm (const double *g, size_t d, size_t first, size_t nt, const double *r,
         long j, long a, size_t s, double *h)
{
  static double mat[MAXT * MAXT];
  double        q[MAXT];
  for (size_t t = 0; t < nt; t++)
    {
      q[t] = h[t] = qat (g, d, first, j - (long)t, a, s);
      for (size_t u = 0; u < nt; u++)
        mat[t * nt + u] = r[t > u ? t - u : u - t];
    }
  if (solve (mat, h, nt) != 0)
    return -1.0;
  double sig = 0.0;
  for (size_t t = 0; t < nt; t++)
    sig += h[t] * q[t];
  return sig;
}

/* The arms for a symbol whose edge is on the bank grid. */
static int
build_arms (const double *g, size_t d, size_t first, const resamp_state_t *rs,
            size_t s, arms_t *A)
{
  size_t nt = rs->num_taps;
  double r[MAXT], tmp[MAXT];
  autocorr (g, d, nt, r);
  long reach = 0;
  for (long n = 0; n < (long)(GLEN - GOFF); n++)
    if (fabs (gat (g, n)) > 1e-12)
      reach = n;
  long   a    = (long)I0;
  long   j_lo = ((long)I0 - (long)first) / (long)d - 4;
  long   j_hi = j_lo + ((long)s + reach) / (long)d + (long)nt + 8;
  double best = -1.0, bestb = -1.0;
  size_t arm = 0;
  for (long j = j_lo; j <= j_hi; j++)
    {
      double sig = opt_arm (g, d, first, nt, r, j, a, s, tmp);
      if (sig > best)
        {
          best     = sig;
          A->j_opt = j;
        }
      for (size_t p = 0; p < rs->num_phases; p++)
        {
          const float *row = rs->bank + p * nt;
          double       sg = 0.0, nv = 0.0;
          for (size_t t = 0; t < nt; t++)
            {
              sg += (double)row[t] * qat (g, d, first, j - (long)t, a, s);
              for (size_t u = 0; u < nt; u++)
                nv += (double)row[t] * (double)row[u]
                      * r[t > u ? t - u : u - t];
            }
          double loss = nv > 0.0 ? sg * sg / nv : 0.0;
          if (loss > bestb)
            {
              bestb    = loss;
              A->j_blt = j;
              arm      = p;
            }
        }
    }
  if (best <= 0.0 || bestb <= 0.0)
    return -1;
  A->nt = nt;
  A->m4 = s / d;
  double hp[MAXT], hm[MAXT];
  double sig = opt_arm (g, d, first, nt, r, A->j_opt, a, s, A->h);
  if (sig <= 0.0 || opt_arm (g, d, first, nt, r, A->j_opt, a + 1, s, hp) <= 0.0
      || opt_arm (g, d, first, nt, r, A->j_opt, a - 1, s, hm) <= 0.0)
    return -1;
  double sgb = 0.0;
  for (size_t t = 0; t < nt; t++)
    sgb += (double)rs->bank[arm * nt + t]
           * qat (g, d, first, A->j_blt - (long)t, a, s);
  for (size_t t = 0; t < nt; t++)
    {
      /* one input sample is 1/s of a symbol, so the central difference
         over two of them is scaled by s/2 */
      A->hd[t] = (hp[t] - hm[t]) / sig * (double)s / 2.0;
      A->h[t] /= sig;
      A->b[t] = (double)rs->bank[arm * nt + t] / sgb;
    }
  return 0;
}

/* One open-loop run: symbols `shift` input samples late, complex noise of
   variance n0 an input sample. */
static int
det_run (const double *g, size_t d, size_t first, const arms_t *A, size_t s,
         long shift, double n0, uint32_t seed, det_run_t *o)
{
  size_t nx    = (size_t)I0 + (NSYM + 8) * s;
  long   reach = 0;
  for (long n = 0; n < (long)(GLEN - GOFF); n++)
    if (fabs (gat (g, n)) > 1e-12)
      reach = n;
  float _Complex  *x  = malloc (nx * sizeof *x);
  size_t           nj = (nx - first) / d;
  double _Complex *sj = malloc (nj * sizeof *sj);
  if (!x || !sj)
    {
      free (x);
      free (sj);
      return -1;
    }
  uint32_t rb = seed, rn = seed ^ 0x9E3779B9u;
  double   sg  = sqrt (n0);
  float    bit = 1.0f;
  for (size_t i = 0; i < nx; i++)
    {
      long rel = (long)i - ((long)I0 + shift);
      if (rel >= 0 && rel % (long)s == 0)
        bit = (float)dp_bit (&rb);
      x[i] = (rel >= 0 ? bit : 0.0f) + (float)sg * dp_cgauss (&rn);
    }
  for (size_t j = 0; j < nj; j++)
    {
      double _Complex acc = 0.0;
      long top            = (long)first + (long)j * (long)d;
      for (long n = 0; n <= reach && n <= top; n++)
        acc += gat (g, n) * (double _Complex)x[top - n];
      sj[j] = acc;
    }
  double sum[NDET] = { 0 }, bsum[NDET] = { 0 }, bm[NDET] = { 0 },
         bm2[NDET] = { 0 };
  size_t nb = 0, cnt = 0, inb = 0;
  double _Complex prev_o = 0.0, prev_b = 0.0;
  for (size_t k = 16; k + 16 < NSYM; k++)
    {
      long jo           = A->j_opt + (long)(k * A->m4);
      long jb           = A->j_blt + (long)(k * A->m4);
      long hf           = (long)(A->m4 / 2);
      double _Complex y = 0.0, dy = 0.0, mo = 0.0, yb = 0.0, mb = 0.0;
      for (size_t t = 0; t < A->nt; t++)
        {
          y += A->h[t] * sj[jo - (long)t];
          dy += A->hd[t] * sj[jo - (long)t];
          mo += A->h[t] * sj[jo - hf - (long)t];
          yb += A->b[t] * sj[jb - (long)t];
          mb += A->b[t] * sj[jb - hf - (long)t];
        }
      double e[NDET];
      e[0]   = creal (conj (mb) * (yb - prev_b));
      e[1]   = creal (conj (mo) * (y - prev_o));
      e[2]   = creal (conj (y) * dy);
      prev_o = y;
      prev_b = yb;
      if (k == 16)
        continue; /* no previous strobe yet */
      cnt++;
      for (int i = 0; i < NDET; i++)
        {
          sum[i] += e[i];
          bsum[i] += e[i];
        }
      if (++inb == BLK)
        {
          for (int i = 0; i < NDET; i++)
            {
              double v = bsum[i] / BLK;
              bm[i] += v;
              bm2[i] += v * v;
              bsum[i] = 0.0;
            }
          nb++;
          inb = 0;
        }
    }
  for (int i = 0; i < NDET; i++)
    {
      o->mean[i] = sum[i] / (double)cnt;
      double mu  = bm[i] / (double)nb;
      o->s0[i]   = BLK * (bm2[i] / (double)nb - mu * mu);
    }
  free (x);
  free (sj);
  return 0;
}

static int
detectors (const double *g, size_t d, size_t first, const resamp_state_t *rs,
           size_t s)
{
  static const char *const name[NDET]
      = { "gardner, built arm", "gardner, best arm", "derivative, best arm" };
  arms_t A;
  DP_REQUIRE (build_arms (g, d, first, rs, s, &A) == 0);
  det_run_t lo, hi;
  DP_REQUIRE (det_run (g, d, first, &A, s, -1, 0.0, 7u, &lo) == 0);
  DP_REQUIRE (det_run (g, d, first, &A, s, +1, 0.0, 7u, &hi) == 0);
  double kd[NDET];
  for (int i = 0; i < NDET; i++)
    kd[i] = (hi.mean[i] - lo.mean[i]) / (2.0 / (double)s);
  printf ("  detectors, open loop at the eye center (%d symbols a run, "
          "slope from +-1/%zu symbol):\n",
          NSYM, s);
  printf ("  %-6s  %-21s  %9s  %10s  %12s  %s\n", "Es/N0", "detector", "Kd",
          "S_e(0)", "S_e(0)/Kd^2", "against gardner built, dB");
  static const double esn0_db[4] = { 0.0, 4.0, 7.0, 10.0 };
  for (int gi = 0; gi < 4; gi++)
    {
      double n0       = gi ? (double)s / pow (10.0, esn0_db[gi] / 10.0) : 0.0;
      double s0[NDET] = { 0 };
      enum
      {
        NSEED = 4
      };
      for (uint32_t sd = 0; sd < NSEED; sd++)
        {
          det_run_t r;
          DP_REQUIRE (det_run (g, d, first, &A, s, 0, n0, 11u + sd, &r) == 0);
          for (int i = 0; i < NDET; i++)
            s0[i] += r.s0[i] / NSEED;
        }
      for (int i = 0; i < NDET; i++)
        {
          double fig = s0[i] / (kd[i] * kd[i]);
          double ref = s0[0] / (kd[0] * kd[0]);
          char   lab[16];
          if (gi)
            snprintf (lab, sizeof lab, "%.0f dB", esn0_db[gi]);
          else
            snprintf (lab, sizeof lab, "none");
          printf ("  %-6s  %-21s  %9.4f  %10.3e  %12.3e  %+.2f\n", lab,
                  name[i], kd[i], s0[i], fig, db10 (fig / ref));
        }
    }
  return 0;
}

static double
db10 (double x)
{
  return x > 0.0 ? 10.0 * log10 (x) : -999.0;
}

static int detectors (const double *g, size_t d, size_t first,
                      const resamp_state_t *rs, size_t s);

static int
run_plan (plan_t pl, int check)
{
  dp_RateConverter_state_t *rc = dp_RateConverter_create_matched (
      (double)pl.m / pl.sps, 1, RC_PULSE_IANDD, 0.0, 1, (double)pl.m, 1024);
  DP_REQUIRE (rc != NULL);
  const resamp_state_t *rs
      = (const resamp_state_t *)rc->stage_ptrs[rc->n_stages - 1];
  static double g[GLEN];
  size_t        first = 0;
  size_t        d     = measure_g (rc, g, &first);
  DP_REQUIRE (d > 0);

  char label[256] = "";
  for (int i = 0; i < rc->n_stages; i++)
    {
      char one[64];
      (void)dp_RateConverter_stage_label (rc, i, one, sizeof one);
      strncat (label, i ? " + " : "", sizeof label - strlen (label) - 1);
      strncat (label, one, sizeof label - strlen (label) - 1);
    }
  size_t s = (size_t)llround (pl.sps);
  printf ("\nsps %.0f, m %zu: %s; bank grid %.3f samples a symbol, D %zu, "
          "%zu taps an arm\n",
          pl.sps, pl.m, label, dp_RateConverter_get_bank_sps (rc), d,
          rs->num_taps);
  printf ("  %-22s  %-20s  %-20s  %s\n", "edge, of a bank sample",
          "loss dB built / best", "ISI dB built / best", "built arm");
  double worst_built = 1.0, worst_opt = 1.0;
  size_t steps = d < 8 ? d : 8;
  for (size_t e = 0; e < steps; e++)
    {
      size_t edge = e * d / steps;
      cell_t c;
      DP_REQUIRE (evaluate (g, d, first, rs, s, edge, &c) == 0);
      printf ("  %-22.3f  %8.3f / %-8.3f  %8.1f / %-8.1f  %zu\n",
              (double)edge / (double)d, db10 (c.loss_built), db10 (c.loss_opt),
              2.0 * db10 (c.isi_built + 1e-30), 2.0 * db10 (c.isi_opt + 1e-30),
              c.arm);
      worst_built = fmin (worst_built, c.loss_built);
      worst_opt   = fmin (worst_opt, c.loss_opt);
      /* The model's own invariants. The optimum is the maximum of L over
         every arm, so a built arm above it means q, r or the solve is
         wrong; and no filter of the cascade's output beats the ideal
         integrate-and-dump at its input. */
      DP_CHECK (c.loss_built <= c.loss_opt * (1.0 + 1e-6));
      DP_CHECK (c.loss_opt <= 1.0 + 1e-6);
      /* With nothing ahead of the bank the pulse IS the sampled rectangle
         and the noise is white, so the built arm at the grid is exact. */
      if (d == 1)
        DP_CHECK (c.loss_built > 1.0 - 1e-6);
    }
  printf ("  worst edge: built %.3f dB, best %.3f dB; the built bank gives "
          "up %.3f dB\n",
          db10 (worst_built), db10 (worst_opt),
          db10 (worst_opt) - db10 (worst_built));
  /* The detectors, where the bank's grid is 4 samples a symbol and the
     input is fine enough to move an edge by a small step. */
  if (!check && rs->num_taps <= MAXT && s / d == 4 && d >= 4)
    DP_CHECK (detectors (g, d, first, rs, s) == 0);
  dp_RateConverter_destroy (rc);
  return 0;
}

int
main (int argc, char **argv)
{
  int                 check = argc > 1 && strcmp (argv[1], "--check") == 0;
  static const plan_t all[] = {
    { 8.0, 8 },  { 8.0, 4 },  { 8.0, 2 },  { 16.0, 8 }, { 16.0, 4 },
    { 16.0, 2 }, { 64.0, 8 }, { 64.0, 4 }, { 64.0, 2 },
  };
  static const plan_t few[] = { { 8.0, 8 }, { 8.0, 2 }, { 64.0, 2 } };
  const plan_t       *pl    = check ? few : all;
  size_t n = check ? sizeof few / sizeof *few : sizeof all / sizeof *all;
  printf ("The rectangle through each planned cascade: an arm's loss "
          "against an ideal\nintegrate-and-dump at the input, and its "
          "worst neighbor's ISI. 'best' is\nthe matched filter for the "
          "pulse and noise the cascade delivers.\n");
  for (size_t i = 0; i < n; i++)
    DP_CHECK (run_plan (pl[i], check) == 0);
  DP_TEST_END ("validate_ratesync_pulse_model");
}
