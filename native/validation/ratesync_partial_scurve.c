/**
 * @file ratesync_partial_scurve.c
 * @brief Validation: the open-loop Gardner S-curve the async DSSS receiver's
 *        timing loop sees, for the rectangle bank as built and as it was.
 *
 * The async DSSS receiver (adr_build_track_chain) hands its demodulator a
 * stream of code-period PARTIALS -- `segments` integrate-and-dumps per code
 * period -- interpolated by a plain RateConverter to 8 samples a symbol, and
 * an MpskReceiver with the rectangle (IANDD) matched filter recovers timing
 * from that with RateSync's Gardner loop. The loop is normalised by
 * `ted_scale = 1 / dp_symsync_ted_slope(...)`, the slope of the IDEAL
 * rectangle's composite (a triangle). Nothing says the partial stream has
 * that composite: a partial that straddles a symbol edge averages the two
 * symbols, and the interpolator smooths again.
 *
 * This harness measures the composite directly, with the loop OPEN
 * (`bn = 0`, the same device ratesync_scurve.c uses), and reports the
 * detector's NORMALISED slope at the stable zero -- `rs->loop.last_error`,
 * which already carries `ted_scale` -- so 1.0 means the loop gets the gain
 * `bn` was designed for.
 *
 *   - **Stimulus.** BPSK symbols from the library's PRBS (dp_tx_symbols),
 *     unit amplitude, a rectangle each, symbol n occupying
 *     [n + tau, n + 1 + tau). Partial k is the MEAN of that waveform over
 *     [k/P, (k+1)/P) symbols: exactly an integrate-and-dump over 1/P of a
 *     symbol. P is non-integer (4.19 at 2.2 GHz, 5.33 at 2.8 GHz) so the
 *     edge phase walks continuously, as it does in the receiver.
 *   - **Interpolation.** The same plain `dp_RateConverter_create(8/P, 0)`
 *     the receiver builds.
 *   - **Reference.** P = 8 with no interpolator: the ideal rectangle,
 *     area-sampled at 8 samples a symbol.
 *   - **Banks.** "corrected" is the bank RateSync builds today (coverage-
 *     weighted edge taps, rc_tap_w). "sampled" is the bank as it was (each
 *     tap in the symbol or out), rebuilt here from the same layout and the
 *     same 1/e0 scale and swapped into the cascade's terminal stage through
 *     dp_resamp_create_custom(). It is a reconstruction of code that no
 *     longer exists, kept here only as the A side of an A/B.
 *
 * Columns: `slope` is the normalised slope at the stable zero (paired
 * central difference, same symbols both sides); `Pin` is the mean power of
 * the 8-sample stream into the cascade; `slope/Pin` is the slope a converged
 * pre-terminal AGC would give (dp_RateConverter_enable_agc regulates input
 * power to e0/bank_sps = 1 for this bank, and Gardner goes as amplitude
 * squared); `|y|` is the mean on-time magnitude at the stable zero; `pk+` and
 * `pk-` the S-curve's extremes and where they sit relative to the stable
 * zero; `zc` the number of zero crossings over one symbol (2 is the healthy
 * count: the stable one and the unstable one at T/2).
 *
 * Usage:  validate_ratesync_partial_scurve [--check]
 *         (--check gates the reference row only: the ideal rectangle must
 *         read a normalised slope of 1 on the corrected bank.)
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/ccsds/ccsds_415.h"
#include "doppler/nco/nco_core.h"
#include "doppler/ratesync/ratesync_core.h"
#include "doppler/resamp/resamp_core.h"
#include "dp_tx_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPS 8.0      /* the demodulator's samples a symbol */
#define M_OUT 8u     /* mpsk_rx_derive_m_out(8, 0) */
#define NPHASE 1024u /* MpskReceiver's MPSK_RX_NUM_PHASES */
#define NSYM 20000
#define SKIP 300  /* symbols of cascade fill discarded */
#define NTAU 41   /* offsets over one symbol */
#define DTAU 0.01 /* slope half-step, symbols */
#define SEED 7u

enum
{
  BANK_CORRECTED = 0,
  BANK_SAMPLED   = 1
};

/* The bank as it was before coverage weighting: the half-open rectangle
   SAMPLED at each tap, same layout (arm p lags p/NPHASE of an input) and the
   same single 1/e0 scale as rc_build_bank. Swapped in for the terminal
   stage, keeping its tap count and rate. Returns 0 on success. */
static int
swap_in_sampled_bank (dp_RateConverter_state_t *rc)
{
  int last = rc->n_stages - 1;
  if (last < 0 || rc->stage_types[last] != RC_STAGE_RESAMP)
    return -1;
  resamp_state_t *old  = (resamp_state_t *)rc->stage_ptrs[last];
  size_t          nt   = dp_resamp_get_num_taps (old);
  double          rate = dp_resamp_get_rate (old);
  double          sps  = rc->bank_sps;
  float          *bank = (float *)calloc (NPHASE * nt, sizeof (float));
  if (!bank)
    return -1;
  double e0 = 0.0;
  for (size_t t = 0; t < nt; t++)
    {
      double ts = -(double)t / sps + 0.5;
      e0 += (ts >= -0.5 && ts < 0.5) ? 1.0 : 0.0;
    }
  for (size_t p = 0; p < NPHASE; p++)
    for (size_t t = 0; t < nt; t++)
      {
        double ts        = -(double)t / sps + 0.5 - ((double)p / NPHASE) / sps;
        bank[p * nt + t] = (ts >= -0.5 && ts < 0.5) ? (float)(1.0 / e0) : 0.f;
      }
  resamp_state_t *nw = dp_resamp_create_custom (NPHASE, nt, bank, rate);
  free (bank);
  if (!nw)
    return -1;
  dp_resamp_destroy (old);
  rc->stage_ptrs[last] = nw;
  return 0;
}

/* The partial stream at P partials a symbol, offset tau, then (P != SPS)
   the receiver's plain RateConverter to SPS. Returns the 8-sps stream. */
static float complex *
make_stream (const int8_t *a, double P, double tau, size_t *n_out)
{
  size_t         npart = (size_t)((double)(NSYM - 2) * P);
  float complex *part  = malloc (npart * sizeof *part);
  if (!part)
    return NULL;
  for (size_t k = 0; k < npart; k++)
    {
      double t0 = (double)k / P, t1 = (double)(k + 1) / P, acc = 0.0;
      long   n0 = (long)floor (t0 - tau), n1 = (long)floor (t1 - tau);
      for (long n = n0; n <= n1; n++)
        {
          double lo = fmax (t0, (double)n + tau);
          double hi = fmin (t1, (double)n + 1.0 + tau);
          if (hi > lo && n >= 0 && n < NSYM)
            acc += (double)a[n] * (hi - lo);
        }
      part[k] = (float)(acc * P);
    }
  if (P == SPS)
    {
      *n_out = npart;
      return part;
    }
  dp_RateConverter_state_t *rc  = dp_RateConverter_create (SPS / P, 0);
  size_t                    cap = (size_t)((double)npart * SPS / P) + 64;
  float complex            *out = malloc (cap * sizeof *out);
  size_t                    n
      = (rc && out) ? dp_RateConverter_execute (rc, part, npart, out, cap) : 0;
  dp_RateConverter_destroy (rc);
  free (part);
  *n_out = n;
  return out;
}

typedef struct
{
  double err, eye, pin;
} point_t;

/* Where the open loop parks the bank: the terminal stage's arm, as a
   fraction of one input interval in [0, 1). At rate m/sps = 1 with ctrl 0
   the accumulator never moves, so the arm stays where it is put. */
static void
park_arm (dp_RateConverter_state_t *rc, double f)
{
  resamp_state_t *term = (resamp_state_t *)rc->stage_ptrs[rc->n_stages - 1];
  term->ctrl_phase     = nco_norm_phase_to_word (f);
}

static point_t
measure_split (const int8_t *a, double P, double tau, double arm, int bank)
{
  point_t              r = { 0, 0, 0 };
  size_t               n = 0;
  float complex       *x = make_stream (a, P, tau, &n);
  dp_ratesync_state_t *rs
      = dp_ratesync_create (SPS, RATESYNC_PULSE_IANDD, 0.0, 8, M_OUT, NPHASE,
                            0.0, 0.707, RATESYNC_TED_GARDNER);
  if (!x || !rs || (bank == BANK_SAMPLED && swap_in_sampled_bank (rs->mf)))
    {
      (void)fprintf (stderr, "setup failed\n");
      exit (2);
    }
  park_arm (rs->mf, arm);
  long          cnt = 0, used = 0;
  float complex sym = 0;
  for (size_t i = 0; i < n; i++)
    {
      r.pin += (double)(crealf (x[i]) * crealf (x[i]));
      if (ratesync_step (rs, x[i], &sym) && ++cnt > SKIP)
        {
          r.err += rs->loop.last_error; /* already x ted_scale */
          r.eye += cabsf (sym);
          used++;
        }
    }
  r.pin /= (double)(n ? n : 1);
  r.err /= (double)(used ? used : 1);
  r.eye /= (double)(used ? used : 1);
  dp_ratesync_destroy (rs);
  free (x);
  return r;
}

/* One timing offset, in symbols, split the way the closed loop carries it:
   whole input samples by the stimulus, the fraction by the bank's arm.
   arm_sign is the arm's direction relative to the stimulus's tau: the arm
   is a LAG (rc_build_bank's derivation), so a later stimulus is met by a
   smaller arm. main() checks the sign before trusting any row. */
static const int arm_sign = -1;

static point_t
measure (const int8_t *a, double P, double tau, int bank)
{
  double u = (double)arm_sign * tau * SPS;
  double f = u - floor (u);
  return measure_split (a, P, (u - f) / ((double)arm_sign * SPS), f, bank);
}

/* Secant half-widths, symbols. A smooth S-curve reads the same slope at
   every one of them inside its linear region. A staircase does not: its
   slope is zero on a tread and a step's height over the half-width across
   one, so the column at each width is the gain a loop whose error is that
   large actually gets -- the describing function, read off the curve. */
static const double HW[] = { 0.01, 1.0 / 32.0, 1.0 / 16.0, 1.0 / 8.0, 0.25 };
#define NHW (sizeof HW / sizeof *HW)

typedef struct
{
  double slope[NHW], pin, eye, pkp, pkp_at, pkm, pkm_at, tau0;
  int    zc;
  double curve[NTAU];
} row_t;

static double
wrap (double t)
{
  return t - floor (t + 0.5);
}

static row_t
run_case (const int8_t *a, double P, int bank)
{
  row_t   o = { 0 };
  point_t pt[NTAU];
  double  tau[NTAU];
  for (int i = 0; i < NTAU; i++)
    {
      tau[i]     = -0.5 + (double)i / NTAU;
      pt[i]      = measure (a, P, tau[i], bank);
      o.curve[i] = pt[i].err;
    }
  /* Zero crossings over the (periodic) symbol; the stable one is the one
     with the open eye (ratesync_scurve.c's cascade_eye, for the reason). */
  double best_eye = -1.0, lo = 0.0;
  for (int i = 0; i < NTAU; i++)
    {
      int j = (i + 1) % NTAU;
      if ((pt[i].err < 0.0) != (pt[j].err < 0.0))
        {
          o.zc++;
          double f  = pt[i].err / (pt[i].err - pt[j].err);
          double ey = pt[i].eye + f * (pt[j].eye - pt[i].eye);
          if (ey > best_eye)
            {
              best_eye = ey;
              lo       = tau[i];
            }
        }
    }
  /* Bisect the bracket: a staircase's zero is a riser, and an interpolated
     crossing would put it anywhere on the tread. The stimulus is fixed, so
     the error is a deterministic function of tau and bisection converges. */
  double hi  = lo + 1.0 / NTAU;
  double elo = measure (a, P, lo, bank).err;
  for (int it = 0; it < 30; it++)
    {
      double mid = 0.5 * (lo + hi);
      double em  = measure (a, P, mid, bank).err;
      if ((em < 0.0) == (elo < 0.0))
        {
          lo  = mid;
          elo = em;
        }
      else
        hi = mid;
    }
  o.tau0 = wrap (0.5 * (lo + hi));
  for (size_t h = 0; h < NHW; h++)
    {
      double sp  = measure (a, P, o.tau0 + HW[h], bank).err;
      double sm  = measure (a, P, o.tau0 - HW[h], bank).err;
      o.slope[h] = fabs ((sp - sm) / (2.0 * HW[h]));
    }
  point_t s0 = measure (a, P, o.tau0, bank);
  o.pin      = s0.pin;
  o.eye      = s0.eye;
  o.pkp      = -1e9;
  o.pkm      = 1e9;
  for (int i = 0; i < NTAU; i++)
    {
      if (pt[i].err > o.pkp)
        {
          o.pkp    = pt[i].err;
          o.pkp_at = wrap (tau[i] - o.tau0);
        }
      if (pt[i].err < o.pkm)
        {
          o.pkm    = pt[i].err;
          o.pkm_at = wrap (tau[i] - o.tau0);
        }
    }
  return o;
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int     curve = (argc > 1 && strcmp (argv[1], "--curve") == 0);
  int8_t *a     = malloc (NSYM);
  if (!a || dp_tx_symbols (a, NSYM, SEED))
    return 2;

  double cr_lo = dp_ccsds_415_chip_rate (DP_CCSDS_415_FC_LO);
  double cr_hi = dp_ccsds_415_chip_rate (DP_CCSDS_415_FC_HI);
  /* segments * chip_rate / (code_len * symbol_rate): adr_partials_per_symbol
     with the receiver's segments = 4. */
  struct
  {
    const char *name;
    double      P;
  } cases[] = {
    { "ideal8sps", SPS },
    { "2.2GHz", 4.0 * cr_lo / (DP_CCSDS_415_SF * DP_CCSDS_415_SYM) },
    { "2.8GHz", 4.0 * cr_hi / (DP_CCSDS_415_SF * DP_CCSDS_415_SYM) },
  };
  /* The split is only the loop's view if the arm moves the instant the
     way the stimulus does. On the ideal area-sampled rectangle the
     coverage bank is exactly shift-invariant, so carrying a fraction of a
     sample on the arm must give the error a stimulus shift gives. The
     wrong sign folds the curve back on itself between whole samples
     (measured: +0.086 against -0.086 at tau 0.36). */
  double worst = 0.0;
  for (int i = 1; i < 16; i++)
    {
      double t = 0.25 + (double)i / 64.0;
      double d = measure (a, SPS, t, BANK_CORRECTED).err
                 - measure_split (a, SPS, t, 0.0, BANK_CORRECTED).err;
      worst    = fmax (worst, fabs (d));
    }
  printf ("arm-vs-stimulus check, ideal rectangle, corrected bank: "
          "max |diff| %.2e (gate 1e-3)\n\n",
          worst);
  int         fail    = !(worst < 1e-3);
  const int   ncase   = check ? 1 : 3;
  const char *bname[] = { "corrected", "sampled" };

  printf ("Open-loop Gardner S-curve, RateSync IANDD at %g sps, m %u, "
          "%d symbols/point, %d offsets\n"
          "slope@h: normalised secant slope (x ted_scale) over +-h symbols "
          "about the stable zero;\n1.0 = the gain bn assumes. s/Pin: the "
          "h=1/32 slope under a converged AGC.\n\n",
          SPS, M_OUT, NSYM, NTAU);
  printf ("%-9s %-9s %5s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %2s "
          "%6s\n",
          "case", "bank", "P", "@.01", "@1/32", "@1/16", "@1/8", "@1/4", "Pin",
          "s/Pin", "|y|", "pk+", "at", "pk-", "at", "zc", "tau0");
  row_t rows[6];
  for (int c = 0; c < ncase; c++)
    for (int b = 0; b < 2; b++)
      {
        row_t o         = run_case (a, cases[c].P, b);
        rows[c * 2 + b] = o;
        printf ("%-9s %-9s %5.2f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f "
                "%6.3f %6.3f %6.3f %6.3f %6.3f %2d %6.3f\n",
                cases[c].name, bname[b], cases[c].P, o.slope[0], o.slope[1],
                o.slope[2], o.slope[3], o.slope[4], o.pin, o.slope[1] / o.pin,
                o.eye, o.pkp, o.pkp_at, o.pkm, o.pkm_at, o.zc, o.tau0);
        /* The one claim this file gates: on the ideal rectangle the
           shipped normaliser gives the loop its designed gain. */
        if (c == 0 && b == BANK_CORRECTED && fabs (o.slope[1] - 1.0) > 0.1)
          fail = 1;
      }
  if (curve)
    {
      printf ("\nS(tau), tau in symbols (stimulus axis)\n%7s", "tau");
      for (int r = 0; r < ncase * 2; r++)
        printf (" %6.6s-%c", cases[r / 2].name, bname[r % 2][0]);
      printf ("\n");
      for (int i = 0; i < NTAU; i++)
        {
          printf ("%7.4f", -0.5 + (double)i / NTAU);
          for (int r = 0; r < ncase * 2; r++)
            printf (" %8.4f", rows[r].curve[i]);
          printf ("\n");
        }
    }
  free (a);
  if (check)
    printf ("\n%s\n", fail ? "FAIL" : "PASS");
  return fail;
}
