/**
 * @file costas_pullin.c
 * @brief Monte-Carlo validation: how far from truth the Costas carrier loop
 *        may be STARTED and still converge — the `bn / m` acquisition bound,
 *        measured on both signs.
 *
 * `costas_jitter.c` measures this loop's steady-state tracking jitter. That is
 * the wrong question on its own, and the MPSK subject that establishes the
 * same bound for `carrier_nda` says why:
 *
 *   > Tracking is not the question. Both loops track far beyond what they can
 *   > acquire; what is unpredictable is PULL-IN.
 *   > (src/doppler/track/tests/characterization/pull_in/characterize.py)
 *
 * Until this file existed `costas` had jitter and nothing else, while its
 * MPSK sibling `carrier_nda` had four validators including a pull-in one — so
 * the loop whose pull-in an entire receiver depends on was the one nobody
 * measured. `async_dsss_receiver_core.h` states its pull-in assumption
 * ("~tens of Hz") as prose, re-derived by nothing, and doppler#982 is what
 * that cost: the receiver hands off 2-6x outside the bound and the resulting
 * lock failures were read as an acquisition problem for a day.
 *
 * THE BOUND. A Costas loop's discriminator is an M-th power (squaring, so
 * m = 2 for BPSK), which divides the frequency error it can see. So the
 * offset it may be seeded with is `bn / m` — `dp_test_freq_offset_inside_bw`
 * is the one spelling of that, shared with the MPSK harness. `bn` here is
 * cycles per UPDATE and an update is `tsamps` samples, so in cycles per
 * SAMPLE the bound is `bn / (m * tsamps)`.
 *
 * WHAT IS ASSERTED, AND WHAT IS ONLY REPORTED. `dp_sym_test.h` sets the rule
 * this file follows:
 *
 *   > Pull-in BEYOND the bound is a real property and worth measuring — as a
 *   > characterization sweep with a reported success fraction, never as a
 *   > pass/fail assertion.
 *
 * So the gate is: acquire AT and inside the bound, on both signs, and the
 * bound scales with `bn`. The envelope past it is printed as a success
 * fraction and asserts nothing — it is a number that moves with the noise
 * realisation, and freezing it would be pinning the draw rather than the
 * loop.
 *
 * BOTH SIGNS, DELIBERATELY. doppler#982 measured an ASYMMETRY in the async
 * receiver — a positive residual never failed (out to +6x the bound) while a
 * negative one failed from about -2.5x. Nothing distinguishes +f0 from -f0 for
 * a loop that is working, so this file sweeps them separately and compares.
 * If costas alone is symmetric, the asymmetry lives in the chain around it and
 * #982 narrows; if it is not, #982 is localised to here. Either answer is
 * worth having, which is why the symmetry check is a first-class section
 * rather than a note.
 *
 * Usage:  costas_pullin [--check]
 *         costas_pullin --design             the half-bandwidth prediction
 *         costas_pullin --half-bw [--shard i/n]   its draws, CSV
 */
#include "dp_rng_test.h"
#include "dp_stats_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"

#include "dp_s415_test.h"

#include "doppler/awgn/awgn_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_complex.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

/** Samples per loop update. 2046 = a 1023-chip code period at 2 samples/chip,
 *  which is the async DSSS receiver's own cadence — the configuration this
 *  file exists to have measured. */
#define TSAMPS 2046u

/** Loop bandwidth, cycles per update. ASYNC_DSSS_RX_BN_CARRIER's value. */
#define BN_NOMINAL 0.04

/** The discriminator is a squaring (BPSK) one. */
#define M_BPSK 2

/** Updates a trial runs for. 5/bn is the loop-filter settling budget (125
 *  updates at the nominal bn), so this is 2x that — enough for the loop to
 *  settle AND for the COSTAS_LOCK_ALPHA=0.1 lock EMA to rise behind it.
 *  Halving it from 400 changed no result; measured, not guessed. */
#define N_UPDATES 250u

/** The loop order the trials run (docs/design/costas.md section 3): 2 is
 *  the loop every Costas starts on, 3 is selected by dp_costas_set_order()
 *  at the same bn before the first update. Both get the same N_UPDATES, so
 *  the two envelopes compare at one budget. Beyond the bound the rows at
 *  250 and at 1,000 updates are reported, never asserted (doppler#3123). */
static int g_order = 2;

/** The design point: the per-update SNR at which this loop meets the
 *  owner's loop-SNR rule for this receiver's loops:
 *  a loop SNR of 20 dB, `rho - 10 log10(2 bn)`, at the nominal bn, so
 *  `rho = 20 + 10 log10(2 bn)` dB, 9.0 dB at bn 0.04. Per sample it is that
 *  less 10 log10(TSAMPS). The pull-in claim is made at it: a loop sized to
 *  the rule, fed the SNR the rule assumes. */
static double
design_rho_db (void)
{
  return 20.0 + 10.0 * log10 (2.0 * BN_NOMINAL);
}

/** The lock threshold a converged trial must pass, derived from the lock
 *  metric's distribution with NO carrier: the metric is an EMA, at
 *  COSTAS_LOCK_ALPHA, of |Re P|/|P| = |cos theta| for a uniform theta, of
 *  mean 2/pi and variance 1/2 - 4/pi^2; the EMA's variance is that times
 *  alpha/(2 - alpha). The threshold sits LOCK_PFA above the mean, through the
 *  library's own Gaussian quantile (dp_det_q_inv): a loop with no carrier
 *  passes it on fewer than 1 in 1,000 trials, where the 0.5 this file used
 *  sat under the no-carrier mean and passed always. The EMA is taken as
 *  Gaussian, the approximation the header's own lock defaults rest on; that
 *  derivation is itself unmeasured (#3138), and this threshold depends on
 *  it. */
#define LOCK_PFA 1e-3
static double
lock_threshold (void)
{
  const double a   = COSTAS_LOCK_ALPHA;
  const double var = (0.5 - 4.0 / (M_PI * M_PI)) * a / (2.0 - a);
  return 2.0 / M_PI + dp_det_q_inv (LOCK_PFA) * sqrt (var);
}

/**
 * @brief The seed offset, in cycles per SAMPLE, at `frac` of the bound.
 *
 * `dp_test_freq_offset_inside_bw` returns cycles per UPDATE here (its `bn` is
 * per-update for this loop, where the MPSK harness's is per-symbol); dividing
 * by `tsamps` puts it in the cycles/sample the stimulus is built in. Signed:
 * `frac` may be negative, which is the whole point of the symmetry section.
 */
static double
seed_offset_cyc_per_sample (double bn, int m, double frac, unsigned tsamps)
{
  return dp_test_freq_offset_inside_bw (bn, m, frac) / (double)tsamps;
}

/**
 * @brief One trial: seed the loop at 0 and give it a carrier at `f0`; did it
 *        converge?
 *
 * Converged means BOTH the tracked frequency landed on `f0` and the loop's own
 * lock metric declared. Frequency alone would accept a loop sitting at the
 * right rate with a spinning constellation; the metric alone would accept a
 * mislock. #982's failures had a healthy code loop and a lock metric pinned at
 * zero-mean, so the pair is what separates them.
 *
 * @param modulated  Non-zero to put random BPSK data on the carrier — the
 *                   squaring discriminator's actual job. Zero for a bare tone.
 */
static int
acquires (double bn, double f0, int modulated, uint32_t seed, size_t n_updates)
{
  dp_costas_state_t *c = dp_costas_create (bn, 0.707, 0.0, TSAMPS, 0.0);
  if (!c)
    return 0;
  if (g_order == 3 && dp_costas_set_order (c, 3, bn) != DP_OK)
    {
      dp_costas_destroy (c);
      return 0;
    }
  uint32_t      ds = seed * 7919u + 1u;
  float complex in[TSAMPS], nz[TSAMPS];
  float complex out[4];
  double        sym = 1.0;
  /* The shipped noise, sized for a unit carrier at the per-sample SNR the
     design point gives (dp_awgn_amplitude_for_snr: both rails counted). */
  dp_awgn_state_t *g = dp_awgn_create (
      seed,
      dp_awgn_amplitude_for_snr (
          (float)(design_rho_db () - 10.0 * log10 ((double)TSAMPS)), 1.0f));
  if (!g)
    {
      dp_costas_destroy (c);
      return 0;
    }

  for (size_t u = 0; u < n_updates; u++)
    {
      if (modulated)
        sym = (dp_xs32 (&ds) & 1u) ? 1.0 : -1.0;
      (void)dp_awgn_generate (g, TSAMPS, nz, TSAMPS);
      for (size_t i = 0; i < TSAMPS; i++)
        {
          size_t k  = u * (size_t)TSAMPS + i;
          double ph = TWOPI * f0 * (double)k;
          in[i]     = (float complex) (sym * cos (ph))
                      + (float complex) (sym * sin (ph)) * I + nz[i];
        }
      (void)dp_costas_steps (c, in, TSAMPS, out, 4);
    }

  /* dp_costas_get_norm_freq is cycles per SAMPLE, the same units as f0. The
     tolerance is a tenth of the bound: tight enough that a loop parked at the
     wrong rate fails, loose enough that steady-state jitter does not. */
  double tol = fabs (seed_offset_cyc_per_sample (bn, M_BPSK, 0.1, TSAMPS));
  int    ok  = fabs (dp_costas_get_norm_freq (c) - f0) < tol
               && dp_costas_get_lock_metric (c) > lock_threshold ();
  dp_costas_destroy (c);
  dp_awgn_destroy (g);
  return ok;
}

/** Trials, of `n` independent draws at one offset, that acquire. */
static int
hits_at (double bn, double frac, int modulated, int n, size_t n_updates)
{
  int hits = 0;
  for (int t = 0; t < n; t++)
    hits
        += acquires (bn, seed_offset_cyc_per_sample (bn, M_BPSK, frac, TSAMPS),
                     modulated, (uint32_t)(101 + 17 * t), n_updates);
  return hits;
}

/** Prints a cell's count with its Wilson 95% interval; asserts all of them
    acquired when `claim` names the claim. */
static void
cell (const char *label, int hits, int n, const char *claim)
{
  double lo, hi;
  dp_wilson ((double)hits, (double)n, &lo, &hi);
  printf ("  %-34s %2d of %2d  [%.2f, %.2f]\n", label, hits, n, lo, hi);
  if (claim)
    DP_CHECK_MSG (hits == n, claim);
}

/* ------------------------------------------------------------------------
 * The half-bandwidth rule (docs/design/async-dsss-receiver.md, "The
 * half-bandwidth rule, tested on Costas alone"). The owner's rule: a
 * second-order loop is predictable for a frequency step under half its loop
 * bandwidth, B_L / 2 with B_L = bn / T, and where that holds depends on the
 * detector. This section measures it on this object alone, configured as the
 * async receiver builds its pre-despread loop (order 2, zeta 0.707, no FLL),
 * at the four 415.1 cells' update SNRs and at both bandwidths: the receiver's
 * rule (dp_async_dsss_receiver_carrier_bn() at the floor) and 0.04.
 *
 *   --design   the prediction, from the detector's S-curve: no draw, no
 *              noise. Printed before any --half-bw run is read.
 *   --half-bw  the draws, one CSV row each (--shard i/n splits them).
 *
 * The prediction is per draw, so a reading is set against the number the
 * mean loop gives for that draw's own start phase and step.
 * ------------------------------------------------------------------------ */

#define HB_DRAWS 60u
#define HB_SUSTAIN 200u  /* updates in a row: dp_ber_lock_symbol()'s rule */
#define HB_TAIL 1000u    /* updates read past the first sustained run      */
#define HB_CAP_S 20.0    /* the owner's lock allocation                    */
#define HB_TOL_SIGMA 3.0 /* the phase test: inside 3 sigma of a lock point */
#define HB_CONTROL 10.0  /* the control's step, B_L                        */
#define HB_NPH 361       /* S-curve points over (-90, 90] degrees, 0.5 deg  */
#define HB_NQ 481        /* quadrature points a noise axis, +/-6 sigma    */
#define HB_NSTART 144    /* start phases --design reads per step and sign  */

/* The detectors --design derives. TODAY is Costas's own, the one the draws
   run; the three I*Q forms on the window sum w = I + jQ differ only in
   what divides I*Q: the signal power A^2 (unit amplitude), the update's own
   |w|^2 (the limiter carrier_nda_disc() is), or the mean total power
   A^2 + E|n|^2. */
enum
{
  HB_DET_TODAY,
  HB_DET_IQ_UNIT,
  HB_DET_IQ_LIM,
  HB_DET_IQ_PWR,
  HB_N_DET
};
static const char *const HB_DET_NAME[HB_N_DET]
    = { "today", "iq_unit", "iq_limiter", "iq_total_power" };

static const double HB_GRID[]
    = { 0.25, 0.35, 0.40, 0.45, 0.50, 0.55, 0.65, 0.75, 1.0, 1.5, 2.0 };
#define HB_NGRID (sizeof HB_GRID / sizeof HB_GRID[0])

/** One configuration: a cell's update SNR and one of the two bandwidths. */
typedef struct
{
  double fc, esn0, bn, t_upd;   /* carrier, Es/N0 dB, bn, code period s    */
  int    det;                   /* HB_DET_*: the detector derived          */
  double gamma_db;              /* C/N0 T, the update's coherent SNR, dB    */
  double ph[HB_NPH], s[HB_NPH]; /* the mean S-curve E[e | phase error]    */
  double k, v;                  /* its slope at 0; var(e) at the lock point */
  double beff, sigma;           /* noise bandwidth at slope k, Hz; jitter   */
} hb_cfg_t;

static double
hb_fold (double a)
{
  return a - M_PI * floor (a / M_PI + 0.5);
}

/* The detector's statistics at update SNR gamma (linear): the S-curve
   E[sin(fold(arg(A e^{j phi} + n)))] over the phase error phi, its slope at
   0 and its variance at 0, by quadrature of the complex Gaussian n (unit
   variance a rail, A^2 = 2 gamma). This is Costas's own discriminator,
   sgn(Re P) Im P / |P| (costas_update()), written as the folded phase. */
static void
hb_scurve (hb_cfg_t *c)
{
  const double a    = sqrt (2.0 * pow (10.0, c->gamma_db / 10.0));
  const double h    = 12.0 / (HB_NQ - 1);
  double       wsum = 0.0;
  for (int i = 0; i < HB_NQ; i++)
    for (int j = 0; j < HB_NQ; j++)
      {
        const double x = -6.0 + i * h, y = -6.0 + j * h;
        wsum += exp (-0.5 * (x * x + y * y));
      }
  for (int p = 0; p < HB_NPH; p++)
    {
      c->ph[p]   = -0.5 * M_PI + M_PI * p / (HB_NPH - 1);
      double acc = 0.0, acc2 = 0.0;
      for (int i = 0; i < HB_NQ; i++)
        for (int j = 0; j < HB_NQ; j++)
          {
            const double x = -6.0 + i * h, y = -6.0 + j * h;
            const double w  = exp (-0.5 * (x * x + y * y)) / wsum;
            const double ii = a * cos (c->ph[p]) + x;
            const double qq = a * sin (c->ph[p]) + y;
            const double e
                = c->det == HB_DET_TODAY     ? sin (hb_fold (atan2 (qq, ii)))
                  : c->det == HB_DET_IQ_UNIT ? ii * qq / (a * a)
                  : c->det == HB_DET_IQ_LIM  ? ii * qq / (ii * ii + qq * qq)
                                             : ii * qq / (a * a + 2.0);
            acc += w * e;
            acc2 += w * e * e;
          }
      c->s[p] = acc;
      if (p == (HB_NPH - 1) / 2)
        c->v = acc2 - acc * acc;
    }
  const int m = (HB_NPH - 1) / 2;
  c->k        = (c->s[m + 1] - c->s[m - 1]) / (c->ph[m + 1] - c->ph[m - 1]);
  /* The loop at slope k: wn scales by sqrt(k), zeta by sqrt(k); its noise
     bandwidth, against the designed one, is sqrt(k) (zeta' + 1/(4 zeta'))
     / (zeta + 1/(4 zeta)). The jitter is var(e)/k^2 through 2 B_eff T. */
  const double z = 0.707, zk = z * sqrt (c->k);
  const double ratio = sqrt (c->k) * (zk + 0.25 / zk) / (z + 0.25 / z);
  c->beff            = ratio * c->bn / c->t_upd;
  c->sigma           = sqrt (2.0 * c->beff * c->t_upd * c->v / (c->k * c->k));
}

static double
hb_s (const hb_cfg_t *c, double phi)
{
  const double f = hb_fold (phi);
  double       x = (f - c->ph[0]) / (c->ph[1] - c->ph[0]);
  int          i = (int)x;
  if (i < 0)
    i = 0;
  if (i > HB_NPH - 2)
    i = HB_NPH - 2;
  x -= i;
  return c->s[i] * (1.0 - x) + c->s[i + 1] * x;
}

/** The mean loop's answer for one draw: the lock points it crosses on the
    way in (signed, half cycles) and the last update its phase error is
    more than sigma from where it settles. */
typedef struct
{
  long   n;
  size_t t_last;
  int    settled;
} hb_pred_t;

/* The loop's own recursion with the mean S-curve in place of the noisy
   discriminator: costas_update()'s order, integ += ki e, then the NCO
   advances by integ and is nudged by kp e. th is truth minus NCO, radians;
   dw the step in radians an update. */
static hb_pred_t
hb_mean_loop (const hb_cfg_t *c, double frac, double th0, size_t max_upd)
{
  dp_loop_filter_state_t lf;
  dp_loop_filter_init (&lf, c->bn, 0.707, 1.0);
  const double dw = TWOPI * frac * c->bn;
  double       th = th0, integ = 0.0;
  hb_pred_t    r  = { 0, 0, 0 };
  const long   n0 = lround (th0 / M_PI);
  for (size_t u = 0; u < max_upd; u++)
    {
      const double e = hb_s (c, th);
      integ += lf.ki * e;
      th += dw - integ - lf.kp * e;
      if (fabs (hb_fold (th)) > c->sigma)
        r.t_last = u + 1;
      if (u > 100 && fabs (dw - integ) < 1e-9 && fabs (hb_fold (th)) < 1e-6)
        {
          r.settled = 1;
          break;
        }
    }
  r.n = lround (th / M_PI) - n0;
  return r;
}

static void
hb_cfg_det (hb_cfg_t *c, double fc, double esn0, int rule, int det)
{
  c->det            = det;
  const double chip = dp_s415_chip_rate (fc);
  c->fc             = fc;
  c->esn0           = esn0;
  c->t_upd          = (double)DP_S415_SF / chip;
  c->bn             = rule ? dp_async_dsss_receiver_carrier_bn (
                                 dp_s415_cn0 (DP_S415_FLOOR_DB), chip, DP_S415_SF)
                           : ASYNC_DSSS_RX_BN_CARRIER;
  c->gamma_db       = dp_s415_cn0 (esn0) + 10.0 * log10 (c->t_upd);
  hb_scurve (c);
}

static void
hb_cfg (hb_cfg_t *c, double fc, double esn0, int rule)
{
  hb_cfg_det (c, fc, esn0, rule, HB_DET_TODAY);
}

/* The eight configurations, in the order --half-bw numbers its points. */
static void
hb_cfgs (hb_cfg_t *c)
{
  const double fcs[2] = { 2.2e9, 2.8e9 }, es[2] = { 4.0, 10.0 };
  int          i = 0;
  for (int e = 0; e < 2; e++)
    for (int f = 0; f < 2; f++)
      for (int rule = 1; rule >= 0; rule--)
        hb_cfg (&c[i++], fcs[f], es[e], rule);
}

/* The largest step, in B_L, from which no start phase (worst start) or the
   start at 0 crosses two or more lock points on the mean loop: bisection
   to 1/2^13 of the bracket. */
static int
hb_slips (const hb_cfg_t *c, double frac, int worst, size_t max_upd)
{
  if (!worst)
    return labs (hb_mean_loop (c, frac, 0.0, max_upd).n) >= 1;
  for (int sg = -1; sg <= 1; sg += 2)
    for (int k = 1; k <= HB_NSTART; k++)
      if (labs (hb_mean_loop (c, sg * frac, -0.5 * M_PI + M_PI * k / HB_NSTART,
                              max_upd)
                    .n)
          >= 2)
        return 1;
  return 0;
}

static double
hb_edge (const hb_cfg_t *c, int worst)
{
  double       lo = 0.05, hi = 2.0;
  const size_t mx = (size_t)(60.0 / c->bn);
  for (int i = 0; i < 13; i++)
    {
      const double m = 0.5 * (lo + hi);
      if (hb_slips (c, m, worst, mx))
        hi = m;
      else
        lo = m;
    }
  return lo;
}

/* --design: everything the run is held to, before it runs. */
static int
hb_design (void)
{
  hb_cfg_t *cs = dp_xmalloc (8 * sizeof *cs);
  hb_cfgs (cs);
  printf ("# design\ncell,bn,b_l_hz,gamma_db,k,s_peak,s_peak_deg,sigma_deg,"
          "b_eff_hz,five_over_b_eff_s\n");
  for (int i = 0; i < 8; i++)
    {
      const hb_cfg_t *c  = &cs[i];
      int             pk = 0;
      for (int p = 0; p < HB_NPH; p++)
        if (c->s[p] > c->s[pk])
          pk = p;
      printf ("%.1f/%.0f,%.5f,%.2f,%.2f,%.3f,%.3f,%.0f,%.2f,%.2f,%.3f\n",
              c->fc / 1e9, c->esn0, c->bn, c->bn / c->t_upd, c->gamma_db, c->k,
              c->s[pk], c->ph[pk] * 180.0 / M_PI, c->sigma * 180.0 / M_PI,
              c->beff, 5.0 / c->beff);
    }
  printf ("# design_step: per step, over %d start phases and both signs: "
          "the most lock points crossed, the share crossing one, the share "
          "crossing two or more, and the worst settle (s) of the starts "
          "3 sigma or more from the null\n",
          HB_NSTART);
  printf ("cell,bn,frac,n_max,share_one,share_two,t_settle_s\n");
  for (int i = 0; i < 8; i++)
    {
      const hb_cfg_t *c   = &cs[i];
      const size_t    cap = (size_t)(HB_CAP_S / c->t_upd);
      for (size_t g = 0; g <= HB_NGRID; g++)
        {
          const double frac = g < HB_NGRID ? HB_GRID[g] : HB_CONTROL;
          long         nmax = 0;
          int          one = 0, two = 0, tot = 0;
          double       tw = 0.0;
          for (int sg = -1; sg <= 1; sg += 2)
            for (int k = 1; k <= HB_NSTART; k++)
              {
                const double th0 = -0.5 * M_PI + M_PI * k / HB_NSTART;
                hb_pred_t    p   = hb_mean_loop (c, sg * frac, th0, cap);
                long         a   = labs (p.n);
                nmax             = a > nmax ? a : nmax;
                one += a == 1;
                two += a >= 2;
                tot++;
                if (0.5 * M_PI - fabs (th0) >= HB_TOL_SIGMA * c->sigma
                    && p.settled && p.t_last * c->t_upd > tw)
                  tw = p.t_last * c->t_upd;
              }
          printf ("%.1f/%.0f,%.5f,%.2f,%ld,%.3f,%.3f,%.3f\n", c->fc / 1e9,
                  c->esn0, c->bn, frac, nmax, (double)one / tot,
                  (double)two / tot, tw);
        }
    }
  printf ("# design_detectors: per detector at the rule bn: slope, peak, "
          "var(e)/k^2, jitter, B_eff/B_L, and the no-slip edge for the worst "
          "start and the start at 0 (in B_L; at 0, no lock point crossed)\n");
  printf ("cell,detector,k,s_peak,s_peak_deg,var_over_k2,sigma_deg,"
          "beff_over_bl,edge_worst,edge_start0\n");
  for (int i = 0; i < 8; i += 2) /* the rule bn of each cell */
    for (int det = 0; det < HB_N_DET; det++)
      {
        hb_cfg_t *c = dp_xmalloc (sizeof *c);
        hb_cfg_det (c, cs[i].fc, cs[i].esn0, 1, det);
        int pk = 0;
        for (int p = 0; p < HB_NPH; p++)
          if (c->s[p] > c->s[pk])
            pk = p;
        printf ("%.1f/%.0f,%s,%.3f,%.3f,%.0f,%.4f,%.2f,%.3f,%.3f,%.3f\n",
                c->fc / 1e9, c->esn0, HB_DET_NAME[det], c->k, c->s[pk],
                c->ph[pk] * 180.0 / M_PI, c->v / (c->k * c->k),
                c->sigma * 180.0 / M_PI, c->beff * c->t_upd / c->bn,
                hb_edge (c, 1), hb_edge (c, 0));
        fflush (stdout);
        free (c);
      }
  printf ("# design_edges_today: both bandwidths\ncell,bn,edge_worst,"
          "edge_start0\n");
  for (int i = 0; i < 8; i++)
    printf ("%.1f/%.0f,%.5f,%.3f,%.3f\n", cs[i].fc / 1e9, cs[i].esn0, cs[i].bn,
            hb_edge (&cs[i], 1), hb_edge (&cs[i], 0));
  free (cs);
  return 0;
}

/** One draw's readings. */
typedef struct
{
  double th0;                    /* start phase, rad, in (-pi, pi]          */
  long   n_meas, cross;          /* lock points crossed by the lock time;
                                    the gross crossings before it           */
  double t_lock, t_freq, t_flag; /* sustained: phase, B_L/4, .locked; s    */
  int    capped;                 /* reached HB_CAP_S without the phase test */
  size_t n_upd;
  double pin_err;  /* worst |probe phase - phase word|, cyc   */
  double pin_late; /* the same with the probe one update late */
  double sd_tail;  /* sd of the folded error past the lock, rad */
} hb_draw_t;

/* One draw: a carrier stepped `frac` B_L off the loop's 0 Hz seed, started
   at a uniform phase, one BPSK symbol an update, at the configuration's
   update SNR. Every reading is from the probes dp_costas_set_telemetry()
   registers (costas_core.c's flush), read back each update. The NCO's
   phase is the sum of the `.nco` probe (the complete command, cycles a
   sample) over the updates; the truth's minus it is the error the slips
   and the phase test read. */
static hb_draw_t
hb_draw (const hb_cfg_t *c, double frac, uint32_t seed)
{
  hb_draw_t          d   = { 0 };
  const size_t       cap = (size_t)(HB_CAP_S / c->t_upd);
  dp_costas_state_t *k   = dp_costas_create (c->bn, 0.707, 0.0, TSAMPS, 0.0);
  dp_tlm_t          *tl  = dp_tlm_create (64);
  /* Fixed, valid arguments: a refusal is a defect, not a configuration. */
  if (!k || !tl || dp_costas_set_telemetry (k, tl, "car", 1) != DP_OK)
    abort ();
  const int id_nco = dp_tlm_probe_id (tl, "car.nco"),
            id_frq = dp_tlm_probe_id (tl, "car.freq"),
            id_lkd = dp_tlm_probe_id (tl, "car.locked");
  /* Three streams a draw: the start phase, the data, the noise. */
  uint32_t ps = seed * 104729u + 7u, ds = seed * 7919u + 3u;
  d.th0 = M_PI * (2.0 * ((double)(dp_xs32 (&ps) >> 8) / 16777216.0) - 1.0);
  dp_awgn_state_t *g = dp_awgn_create (
      7919u * seed + 1u,
      dp_awgn_amplitude_for_snr (
          (float)(c->gamma_db - 10.0 * log10 ((double)TSAMPS)), 1.0f));
  if (!g)
    abort ();
  const double f0  = frac * c->bn / (double)TSAMPS; /* cycles a sample */
  uint8_t     *fph = dp_xmalloc (cap), *ffr = dp_xmalloc (cap),
          *flk      = dp_xmalloc (cap);
  double       *err = dp_xmalloc (cap * sizeof *err);
  float complex in[TSAMPS], nz[TSAMPS], out[4];
  dp_tlm_rec_t  rec[64];
  double        nco_sum = 0.0, nco_prev = 0.0; /* cycles */
  double        frq_prev = 0.0; /* the frequency this update runs at */
  size_t        run = 0, stop = cap, u = 0;
  for (; u < stop; u++)
    {
      const double sym = (dp_xs32 (&ds) & 1u) ? 1.0 : -1.0;
      (void)dp_awgn_generate (g, TSAMPS, nz, TSAMPS);
      for (size_t i = 0; i < TSAMPS; i++)
        {
          const double ph
              = d.th0 + TWOPI * f0 * (double)(u * (size_t)TSAMPS + i);
          in[i] = (float complex) (sym * cos (ph))
                  + (float complex) (sym * sin (ph)) * I + nz[i];
        }
      /* The error at this update's middle, where its prompt is centred:
         at the end of the update's samples the NCO has advanced by the
         commands of the updates before (nco_sum); half an update earlier
         it was behind that by half the frequency it ran at, the last
         update's `.freq`. The end would read a step's own advance, half an
         update of it, as error: 14 degrees at 2 B_L and bn 0.04. */
      const double truth
          = d.th0 / TWOPI + f0 * ((double)u + 0.5) * (double)TSAMPS;
      err[u] = TWOPI * (truth - (nco_sum - 0.5 * frq_prev * TSAMPS));
      (void)dp_costas_steps (k, in, TSAMPS, out, 4);
      double       nco = 0.0, frq = 0.0, lkd = 0.0;
      const size_t got = dp_tlm_read (tl, 0, rec, 64);
      for (size_t r = 0; r < got; r++)
        {
          if (rec[r].probe == id_nco)
            nco = rec[r].value;
          else if (rec[r].probe == id_frq)
            frq = rec[r].value;
          else if (rec[r].probe == id_lkd)
            lkd = rec[r].value;
        }
      nco_sum += nco * (double)TSAMPS;
      /* The pin: the summed probe, less the frequency the next update
         will run at, is the NCO's own phase word, to the float probe's
         and the word's quantization. And read one update late, it is not. */
      const double word = (double)k->nco.phase / 4294967296.0;
      const double mine = nco_sum - (double)k->nco.norm_freq * TSAMPS;
      const double late = mine - nco * TSAMPS + nco_prev * TSAMPS;
      double       e1 = mine - word, e2 = late - word;
      e1 -= floor (e1 + 0.5);
      e2 -= floor (e2 + 0.5);
      d.pin_err  = fmax (d.pin_err, fabs (e1));
      d.pin_late = fmax (d.pin_late, fabs (e2));
      nco_prev   = nco;
      frq_prev   = frq;
      fph[u]     = fabs (hb_fold (err[u])) < HB_TOL_SIGMA * c->sigma;
      ffr[u]     = fabs (frq * TSAMPS - frac * c->bn) < 0.25 * c->bn;
      flk[u]     = lkd > 0.5;
      run        = fph[u] ? run + 1 : 0;
      if (run == HB_SUSTAIN && stop == cap)
        stop = u + 1 + HB_TAIL < cap ? u + 1 + HB_TAIL : cap;
    }
  d.n_upd          = u;
  const int64_t tp = dp_ber_lock_symbol (fph, u, HB_SUSTAIN, 0.9);
  const int64_t tf = dp_ber_lock_symbol (ffr, u, HB_SUSTAIN, 0.9);
  const int64_t tk = dp_ber_lock_symbol (flk, u, HB_SUSTAIN, 0.9);
  d.capped         = tp < 0;
  d.t_lock         = tp < 0 ? NAN : (double)tp * c->t_upd;
  d.t_freq         = tf < 0 ? NAN : (double)tf * c->t_upd;
  d.t_flag         = tk < 0 ? NAN : (double)tk * c->t_upd;
  const size_t end = tp < 0 ? u - 1 : (size_t)tp;
  /* The steady jitter the band was derived from, measured: the folded
     error's sd over the updates read past the lock time. */
  if (tp >= 0 && (size_t)tp + HB_SUSTAIN < u)
    {
      double s1 = 0.0, s2 = 0.0;
      size_t m = 0;
      for (size_t v = (size_t)tp + HB_SUSTAIN; v < u; v++, m++)
        {
          const double f = hb_fold (err[v]);
          s1 += f;
          s2 += f * f;
        }
      d.sd_tail = sqrt (fmax (0.0, s2 / m - (s1 / m) * (s1 / m)));
    }
  else
    d.sd_tail = NAN;
  const long n0 = lround (d.th0 / M_PI);
  d.n_meas      = lround (err[end] / M_PI) - n0;
  long prev     = n0;
  for (size_t v = 0; v <= end; v++)
    {
      const long n = lround (err[v] / M_PI);
      d.cross += n != prev;
      prev = n;
    }
  free (fph);
  free (ffr);
  free (flk);
  free (err);
  dp_awgn_destroy (g);
  dp_tlm_destroy (tl);
  dp_costas_destroy (k);
  return d;
}

/* --half-bw: every point's draws, one row each. A point is a configuration,
   a step (the grid, then the control) and a sign; its draws are seeds 1 to
   HB_DRAWS, the same seeds at every point, so a seed's start phase, data
   and noise are the same at every step. --shard i/n takes every n-th point
   from the i-th. */
static int
hb_run (unsigned shard, unsigned shards)
{
  hb_cfg_t *cs = dp_xmalloc (8 * sizeof *cs);
  hb_cfgs (cs);
  printf ("# half_bw\ncell,bn,b_l_hz,frac,sign,seed,th0_deg,null_deg,n_pred,"
          "t_pred_s,n_meas,crossings,t_lock_s,t_freq_s,t_flag_s,capped,"
          "updates,sd_tail_deg,sigma_deg,pin_err_cyc,pin_late_cyc\n");
  unsigned point = 0;
  for (int i = 0; i < 8; i++)
    for (size_t g = 0; g <= HB_NGRID; g++)
      for (int sg = 1; sg >= -1; sg -= 2, point++)
        {
          if (point % shards != shard)
            continue;
          const hb_cfg_t *c    = &cs[i];
          const double    frac = sg * (g < HB_NGRID ? HB_GRID[g] : HB_CONTROL);
          for (uint32_t s = 1; s <= HB_DRAWS; s++)
            {
              hb_draw_t d = hb_draw (c, frac, s);
              hb_pred_t p = hb_mean_loop (c, frac, d.th0,
                                          (size_t)(HB_CAP_S / c->t_upd));
              printf ("%.1f/%.0f,%.5f,%.2f,%.2f,%+d,%u,%.2f,%.2f,%ld,%.4f,%ld,"
                      "%ld,%.4f,%.4f,%.4f,%d,%zu,%.2f,%.2f,%.2e,%.2e\n",
                      c->fc / 1e9, c->esn0, c->bn, c->bn / c->t_upd,
                      fabs (frac), sg, s, d.th0 * 180.0 / M_PI,
                      (0.5 * M_PI - fabs (hb_fold (d.th0))) * 180.0 / M_PI,
                      p.n, p.settled ? p.t_last * c->t_upd : NAN, d.n_meas,
                      d.cross, d.t_lock, d.t_freq, d.t_flag, d.capped, d.n_upd,
                      d.sd_tail * 180.0 / M_PI, c->sigma * 180.0 / M_PI,
                      d.pin_err, d.pin_late);
              fflush (stdout);
            }
        }
  free (cs);
  return 0;
}

/* The pin, for --check: on one draw at half the rule's bandwidth, the
   phase the slips are read from (the summed `.nco` probe) is the NCO's own
   phase word, and the same sum one update late is not. */
static void
hb_pin (void)
{
  hb_cfg_t *c = dp_xmalloc (sizeof *c);
  hb_cfg (c, 2.8e9, 10.0, 1);
  hb_draw_t d = hb_draw (c, 0.5, 1u);
  printf ("  half-bw pin: probe phase vs the NCO's word %.1e cycles; one "
          "update late %.1e\n",
          d.pin_err, d.pin_late);
  DP_CHECK_MSG (d.pin_err < 1e-3, "the summed .nco probe is the NCO's phase");
  DP_CHECK_MSG (d.pin_late > 1e-3, "and read one update late it is not");
  free (c);
}

int
main (int argc, char **argv)
{
  const int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  if (argc > 1 && strcmp (argv[1], "--design") == 0)
    return hb_design ();
  if (argc > 1 && strcmp (argv[1], "--half-bw") == 0)
    {
      unsigned shard = 0, shards = 1;
      if (argc > 3 && strcmp (argv[2], "--shard") == 0)
        DP_REQUIRE (sscanf (argv[3], "%u/%u", &shard, &shards) == 2
                    && shards >= 1 && shard < shards);
      return hb_run (shard, shards);
    }
  /* The same assertions in --check and in the sweep (#1566); the sweep
     draws more, so its interval is tighter: 64 of 64 bounds the success
     rate above 0.94, 16 of 16 above 0.81. */
  const int TRIALS = check ? 16 : 64;
  char      label[64];

  printf ("Costas pull-in: bound bn/m, m=%d, tsamps=%u; design point %.1f dB "
          "per update (loop SNR 20 dB at bn %.2f); lock threshold %.3f "
          "(Pfa %.0e with no carrier)\n",
          M_BPSK, TSAMPS, design_rho_db (), BN_NOMINAL, lock_threshold (),
          LOCK_PFA);
  for (g_order = 2; g_order <= 3; g_order++)
    {
      /* 1. AT and inside the bound, on BOTH signs, bare and modulated: the
         gate. Both signs asserted here is the symmetry #982 asked about. */
      printf ("[order %d] inside the bound (asserted)\n", g_order);
      for (int modulated = 0; modulated <= 1; modulated++)
        for (double frac = -1.0; frac <= 1.001; frac += 0.5)
          {
            if (fabs (frac) < 1e-9)
              continue; /* zero offset proves nothing about pull-in */
            snprintf (label, sizeof label, "%s at %+.1f of the bound",
                      modulated ? "BPSK data" : "bare tone", frac);
            cell (label,
                  hits_at (BN_NOMINAL, frac, modulated, TRIALS, N_UPDATES),
                  TRIALS, "acquires at and inside bn/m, both signs");
          }
      /* 2. The bound SCALES with bn: the law's content. The per-update SNR
         stays the design point's, so the loop SNR moves with bn: 23 dB at
         0.02, 20 dB at 0.04 (the rule) and 17 dB at 0.08, which is under
         the rule's 20 dB: a stress row, asserted all the same. */
      printf ("[order %d] the bound tracks bn (asserted)\n", g_order);
      for (double bn = BN_NOMINAL / 2.0; bn <= BN_NOMINAL * 2.001; bn *= 2.0)
        {
          snprintf (label, sizeof label, "bn %.3f (loop SNR %.0f dB) at bound",
                    bn, design_rho_db () - 10.0 * log10 (2.0 * bn));
          cell (label, hits_at (bn, 1.0, 1, TRIALS, N_UPDATES), TRIALS,
                "acquires at the bound for every bn");
        }
      /* 3. Beyond the bound: reported, never asserted (dp_sym_test.h's
         rule), at the shared budget and at 4x it, where order 3 pulls in
         further but slower (#3123). */
      printf ("[order %d] beyond the bound (reported only)\n", g_order);
      for (double frac = 2.0; frac <= 4.001; frac += 2.0)
        for (int s = -1; s <= 1; s += 2)
          for (size_t nu = N_UPDATES; nu <= 4 * N_UPDATES; nu *= 4)
            {
              snprintf (label, sizeof label, "%+.0fx the bound, %zu updates",
                        s * frac, nu);
              cell (label, hits_at (BN_NOMINAL, s * frac, 1, TRIALS / 2, nu),
                    TRIALS / 2, NULL);
            }
    }
  if (check)
    hb_pin ();
  DP_TEST_END ("validate_costas_pullin");
}
