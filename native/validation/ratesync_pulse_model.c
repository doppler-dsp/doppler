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
#include "dp_test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEN 8192  /* input samples a probe run feeds               */
#define I0 2048   /* where a probe's impulse or symbol starts      */
#define MAXT 96   /* most taps an arm may have here                */
#define GOFF 4096 /* g[] index of n = 0                            */
#define GLEN 12288

typedef struct
{
  double sps; /* input samples a symbol                */
  size_t m;   /* outputs a symbol                      */
} plan_t;

/* Solve A x = b for a symmetric positive definite A (n x n, row-major),
   by Gaussian elimination with partial pivoting. A and b are destroyed. */
static int
solve (double *a, double *b, size_t n)
{
  for (size_t c = 0; c < n; c++)
    {
      size_t piv = c;
      for (size_t r = c + 1; r < n; r++)
        if (fabs (a[r * n + c]) > fabs (a[piv * n + c]))
          piv = r;
      if (fabs (a[piv * n + c]) < 1e-14)
        return -1;
      if (piv != c)
        {
          for (size_t k = 0; k < n; k++)
            {
              double t       = a[c * n + k];
              a[c * n + k]   = a[piv * n + k];
              a[piv * n + k] = t;
            }
          double t = b[c];
          b[c]     = b[piv];
          b[piv]   = t;
        }
      for (size_t r = c + 1; r < n; r++)
        {
          double f = a[r * n + c] / a[c * n + c];
          for (size_t k = c; k < n; k++)
            a[r * n + k] -= f * a[c * n + k];
          b[r] -= f * b[c];
        }
    }
  for (size_t c = n; c-- > 0;)
    {
      for (size_t k = c + 1; k < n; k++)
        b[c] -= a[c * n + k] * b[k];
      b[c] /= a[c * n + c];
    }
  return 0;
}

/* The cascade's response at the input rate, g[GOFF + n], from an impulse
   at each of the D input positions, read at the pre-terminal tap. Returns
   D, or 0 on a failure. `first` receives the input index of the first
   pre-terminal sample, which fixes the decimation phase. */
static size_t
measure_g (dp_RateConverter_state_t *rc, double *g, size_t *first)
{
  size_t        d = 0, f0 = 0;
  static double base[LEN];
  memset (g, 0, GLEN * sizeof *g);
  /* The first pass feeds zeros alone. It gives the decimation and its
     phase, from where the first two bank samples fall, and the cascade's
     own output for no input, which a CIC plan does not start at zero
     (doppler#3365) and which every later pass has subtracted. */
  for (int pass = 0, k = 0; pass == 0 || (size_t)k < d; pass++)
    {
      dp_RateConverter_reset (rc);
      size_t j = 0;
      for (size_t i = 0; i < LEN; i++)
        {
          float _Complex out[8], pre = 0.0f;
          int np = 0;
          (void)dp_RateConverter_execute_ctrl_push_tap (
              rc, pass > 0 && i == I0 + (size_t)k ? 1.0f : 0.0f, 0.0, out, 8,
              &pre, &np);
          if (!np)
            continue;
          if (pass == 0)
            {
              if (j == 0)
                f0 = i;
              else if (j == 1)
                d = i - f0;
              base[j] = (double)crealf (pre);
            }
          else
            {
              /* bank sample j sits at input f0 + j*d; the impulse at
                 I0 + k */
              long n = (long)(f0 + j * d) - (long)(I0 + (size_t)k);
              if (n + GOFF >= 0 && n + GOFF < GLEN)
                g[n + GOFF] = (double)crealf (pre) - base[j];
            }
          j++;
        }
      if (!d)
        return 0;
      if (pass > 0)
        k++;
    }
  *first = f0;
  return d;
}

static double
gat (const double *g, long n)
{
  return (n + GOFF >= 0 && n + GOFF < GLEN) ? g[n + GOFF] : 0.0;
}

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

static double
db10 (double x)
{
  return x > 0.0 ? 10.0 * log10 (x) : -999.0;
}

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
  (void)check;
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
