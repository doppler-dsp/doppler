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
#include "dp_sym_test.h"

#include "costas/costas_core.h"
#include <complex.h>
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
acquires (double bn, double f0, float sigma, int modulated, uint32_t seed)
{
  costas_state_t *c = costas_create (bn, 0.707, 0.0, TSAMPS, 0.0);
  if (!c)
    return 0;
  uint32_t      ds = seed * 7919u + 1u, ns = seed;
  float complex in[TSAMPS];
  float complex out[4];
  double        sym = 1.0;

  for (size_t u = 0; u < N_UPDATES; u++)
    {
      if (modulated)
        sym = (dp_xs32 (&ds) & 1u) ? 1.0 : -1.0;
      for (size_t i = 0; i < TSAMPS; i++)
        {
          size_t k  = u * (size_t)TSAMPS + i;
          double ph = TWOPI * f0 * (double)k;
          /* Named locals, not two draws in one expression: the evaluation
             order would be the compiler's, so gcc and clang would see
             different noise off the same seed. */
          float n_re = (float)dp_gauss (&ns);
          float n_im = (float)dp_gauss (&ns);
          in[i]      = (float complex) (sym * cos (ph))
                       + (float complex) (sym * sin (ph)) * I + sigma * n_re
                       + sigma * n_im * I;
        }
      (void)costas_steps (c, in, TSAMPS, out, 4);
    }

  /* costas_get_norm_freq is cycles per SAMPLE, the same units as f0. The
     tolerance is a tenth of the bound: tight enough that a loop parked at the
     wrong rate fails, loose enough that steady-state jitter does not. */
  double tol = fabs (seed_offset_cyc_per_sample (bn, M_BPSK, 0.1, TSAMPS));
  int    ok  = fabs (costas_get_norm_freq (c) - f0) < tol
               && costas_get_lock_metric (c) > 0.5;
  costas_destroy (c);
  return ok;
}

/** Success fraction over `n` independent noise draws at one offset. */
static double
success_frac (double bn, double frac, float sigma, int modulated, int n)
{
  int hits = 0;
  for (int t = 0; t < n; t++)
    hits
        += acquires (bn, seed_offset_cyc_per_sample (bn, M_BPSK, frac, TSAMPS),
                     sigma, modulated, (uint32_t)(101 + 17 * t));
  return (double)hits / (double)n;
}

int
main (int argc, char **argv)
{
  int         check  = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int         fail   = 0;
  const int   TRIALS = 16;
  const float SIGMA  = 0.30f;

  printf ("Costas pull-in — bound = bn/m, m=%d, tsamps=%u\n", M_BPSK, TSAMPS);
  printf ("  bn=%.3f -> bound %.3e cyc/sample\n\n", BN_NOMINAL,
          seed_offset_cyc_per_sample (BN_NOMINAL, M_BPSK, 1.0, TSAMPS));

  /* ── 1. AT and inside the bound, both signs. This is the gate. ────────── */
  printf ("Inside the bound (asserted), %d draws each\n", TRIALS);
  for (int modulated = 0; modulated <= 1; modulated++)
    {
      for (double frac = -1.0; frac <= 1.001; frac += 0.5)
        {
          if (fabs (frac) < 1e-9)
            continue; /* zero offset proves nothing about pull-in */
          double f = success_frac (BN_NOMINAL, frac, SIGMA, modulated, TRIALS);
          printf ("  %-11s frac=%+.1f  acquired %.0f%%\n",
                  modulated ? "BPSK data" : "bare tone", frac, 100.0 * f);
          if (check && f < 1.0)
            fail = 1;
        }
    }

  /* ── 2. The bound SCALES with bn — the law's actual content. ──────────── */
  printf ("\nThe bound tracks bn (asserted): same fraction, different bn\n");
  for (double bn = BN_NOMINAL / 2.0; bn <= BN_NOMINAL * 2.001; bn *= 2.0)
    {
      double f = success_frac (bn, 1.0, SIGMA, 1, TRIALS);
      printf ("  bn=%.3f  offset=%.3e cyc/sample  acquired %.0f%%\n", bn,
              seed_offset_cyc_per_sample (bn, M_BPSK, 1.0, TSAMPS), 100.0 * f);
      if (check && f < 1.0)
        fail = 1;
    }

  /* ── 3. SYMMETRY: +k and -k must behave alike (doppler#982). ──────────── */
  printf ("\nSign symmetry inside the bound (asserted)\n");
  for (double frac = 0.5; frac <= 1.001; frac += 0.5)
    {
      double pos = success_frac (BN_NOMINAL, +frac, SIGMA, 1, TRIALS);
      double neg = success_frac (BN_NOMINAL, -frac, SIGMA, 1, TRIALS);
      printf ("  frac=%.1f  +%.0f%% / -%.0f%%\n", frac, 100.0 * pos,
              100.0 * neg);
      if (check && fabs (pos - neg) > 1e-9)
        fail = 1;
    }

  /* ── 4. The envelope BEYOND the bound — reported, never asserted. ─────── */
  printf ("\nBeyond the bound (reported only — dp_sym_test.h's rule)\n");
  for (double frac = 2.0; frac <= 6.001; frac += 2.0)
    {
      /* Half the draws: this section asserts nothing, so its precision buys
         nothing but runtime. */
      double pos = success_frac (BN_NOMINAL, +frac, SIGMA, 1, TRIALS / 2);
      double neg = success_frac (BN_NOMINAL, -frac, SIGMA, 1, TRIALS / 2);
      printf ("  frac=%.0fx  +%.0f%% / -%.0f%%\n", frac, 100.0 * pos,
              100.0 * neg);
    }

  if (fail)
    {
      fprintf (stderr, "Costas pull-in deviates from the bn/m bound — FAIL\n");
      return 1;
    }
  if (check)
    printf ("\nPASS: acquires at and inside bn/m on both signs; the bound "
            "scales with bn; +k and -k agree\n");
  return 0;
}
