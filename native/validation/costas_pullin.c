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
 */
#include "dp_rng_test.h"
#include "dp_stats_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"

#include "doppler/awgn/awgn_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_complex.h"
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

int
main (int argc, char **argv)
{
  const int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
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
  DP_TEST_END ("validate_costas_pullin");
}
