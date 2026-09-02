/**
 * @file dll_aid_jitter.c
 * @brief The code loop on the symbol-aided window against the per-epoch
 *        look-back: closed-loop jitter and pull-in at the continuous
 *        async-DSSS operating point.
 *
 * `dll_set_symbol_period()` (docs/design/async-dsss-receiver.md §3.7) lifts
 * the DLL's max-power window search to the symbol scale. Its first use was
 * the lock detector's looks (§12.4); this harness measures its second: the
 * code discriminator itself runs on the winning window -- six of the 7.24
 * partials a symbol spans, coherent, leaving the transition partial and
 * the slack out -- and the loop steers once per symbol, its filter
 * re-timed to that interval so `bn` keeps its per-epoch meaning. Two
 * questions, and the answer to each is a number §12.5 states rather than
 * a hope:
 *
 *   jitter    the closed-loop code-phase error's standard deviation, in
 *             chips, per mode, against C/N0 -- and where each mode loses
 *             the code (an error of more than half a chip from the
 *             converged phase for a measured epoch);
 *   pull-in   from a start a fraction of a chip off, at the two design
 *             C/N0s: how many of ten trials converge, and how long the
 *             median takes.
 *
 * What it found (§12.5): the aided loop is tighter above 45 dB-Hz, where
 * the per-epoch look-back's own handling of the data transitions is what
 * sets the per-epoch jitter, and 1.2-1.4x looser at the 40 dB-Hz floor,
 * where the noise sets it and the aided window's 17% of unused partials
 * cost more than its coherence buys; it pulls in about 20% faster at both.
 * The loop GAIN is pinned elsewhere: test_dll_core.c 6c requires the two
 * modes' settling transients under a code-rate step to agree, which a
 * filter left at its per-epoch gains while updating once per symbol fails.
 *
 * Method. The operating point of §12: a 1023-chip Gold code (CCSDS #365)
 * at 5 Mcps, two samples per chip, asynchronous BPSK data at 2700 sym/s
 * (7.24 partials per symbol at four partials per epoch) from the shipped
 * continuous-DSSS synth with its own PRBS data; noise from the shipped
 * awgn generator sized by awgn_amplitude_for_snr() from the C/N0. The DLL
 * is the receiver's own (`bn 0.002`, half-chip spacing, four partials per
 * epoch) and is fed one epoch at a time; after every block its tracked
 * code phase is compared with the truth the generator implies -- samples
 * fed over samples per chip -- wrapped to half a period. Nothing here
 * builds a chip, a bit or a sigma by hand.
 *
 * The converged phase carries a constant: the synth's shaping delay and
 * the DLL's own phase convention. It is measured once on a clean stream
 * per mode and printed; the two modes agreeing on it is itself a claim (the
 * aided discriminator's zero is the same code phase), and it is the target
 * pull-in is measured against and the reference the loss-of-code count
 * uses.
 *
 * Usage:
 *   validate_dll_aid_jitter            the full tables
 *   validate_dll_aid_jitter --check    the spot checks CTest runs: the two
 *                                      modes agree on the phase, neither
 *                                      loses the code, the aided jitter
 *                                      within its measured relation at 45
 *                                      and 40 dB-Hz, and pull-in at 40
 */
#include "awgn/awgn_core.h"
#include "dll/dll_core.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define SEGMENTS 4u /* the receiver's partials per epoch           */
#define BN 0.002    /* the receiver's DLL bandwidth, per epoch      */
#define P_SYM ((double)SEGMENTS * CHIP_RATE / ((double)SF * SYM_RATE))

#define SETTLE_EP 3000 /* 5/bn is 2500; the loop and the aid's EMAs   */
#define MEAS_EP 12000  /* ~150 independent jitter samples at 1/bn     */
#define CHECK_MEAS_EP 6000
#define PULL_EP 3000   /* pull-in watched for this long               */
#define PULL_TOL 0.1   /* chips from the converged phase = pulled in   */
#define LOSS_CHIPS 0.5 /* farther than this from it = the code is lost */
#define N_PULL 10
#define CHECK_PULL 5

typedef struct
{
  double mean;    /* mean wrapped error over the measured epochs, chips */
  double sigma;   /* its standard deviation, chips                      */
  double p_lost;  /* fraction of measured epochs > LOSS_CHIPS from ref  */
  double rate;    /* final code_rate                                    */
  long   pull_ep; /* first epoch from which |err - ref| < PULL_TOL held;
                     -1 = never                                         */
} run_t;

typedef struct
{
  double   cn0_dbhz;  /* >= 100 renders clean                          */
  int      aided;     /* dll_set_symbol_period on                       */
  double   init_chip; /* the DLL's starting code phase, chips           */
  double   ref;       /* converged phase to measure against; NAN = own  */
  uint32_t seed;
  int      settle_ep, meas_ep;
} cfg_t;

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

/* The emitter: the shipped continuous-DSSS synth, clean (no AWGN child),
   PRBS data from its own PN register, seeded per trial. */
static wfm_synth_state_t *
make_emitter (const uint8_t *code, uint32_t seed)
{
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                          seed, (int)SPC, 15, 0, 0, 0.0);
  if (syn
      && wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                  WFM_DSSS_DATA_PRBS, NULL, 0)
             != 0)
    {
      wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

static double
wrap_chips (double e)
{
  e = fmod (e, (double)SF);
  if (e > 0.5 * SF)
    e -= (double)SF;
  else if (e <= -0.5 * SF)
    e += (double)SF;
  return e;
}

static int
run (const uint8_t *code, const cfg_t *c, run_t *out)
{
  memset (out, 0, sizeof *out);
  out->pull_ep           = -1;
  wfm_synth_state_t *syn = make_emitter (code, c->seed);
  /* C/N0 to SNR over fs is the one conversion; the amplitude is the
     library's answer to "per rail or total", not a sigma derived here. */
  awgn_state_t *g
      = c->cn0_dbhz < WFM_SYNTH_SNR_CLEAN
            ? awgn_create (c->seed * 7919u + 1u,
                           awgn_amplitude_for_snr (
                               (float)(c->cn0_dbhz - 10.0 * log10 (FS)), 1.0f))
            : NULL;
  dll_state_t *d
      = dll_create (code, SF, SPC, c->init_chip, BN, 0.707, 0.5, SEGMENTS);
  if (!syn || !d || (c->cn0_dbhz < WFM_SYNTH_SNR_CLEAN && !g))
    return 1;
  if (c->aided && dll_set_symbol_period (d, P_SYM) != DP_OK)
    return 1;
  float complex *blk = malloc (TE * sizeof *blk);
  float complex *nz  = malloc (TE * sizeof *nz);
  float complex *prt = malloc (TE * sizeof *prt);
  double *err = malloc ((size_t)(c->settle_ep + c->meas_ep) * sizeof *err);
  if (!blk || !nz || !prt || !err)
    return 1;

  const long total = c->settle_ep + c->meas_ep;
  uint64_t   fed   = 0; /* samples the DLL has consumed */
  for (long b = 0; b < total; b++)
    {
      wfm_synth_steps (syn, blk, TE);
      if (g)
        {
          awgn_generate (g, TE, nz, TE);
          for (size_t i = 0; i < TE; i++)
            blk[i] += nz[i];
        }
      (void)dll_steps (d, blk, TE, prt, TE);
      fed += TE;
      /* The truth the generator implies at this sample count: samples fed
         over samples per chip. */
      err[b] = wrap_chips (dll_get_code_phase (d) - (double)fed / (double)SPC);
    }
  /* Statistics over the measured tail. */
  double m = 0.0, m2 = 0.0;
  for (long b = c->settle_ep; b < total; b++)
    m += err[b];
  m /= (double)c->meas_ep;
  double ref = isnan (c->ref) ? m : c->ref;
  /* The mean about the reference, wrapped there rather than at zero, so a
     loop sitting near the period's edge is not split across it. */
  m = 0.0;
  for (long b = c->settle_ep; b < total; b++)
    m += wrap_chips (err[b] - ref);
  m           = ref + m / (double)c->meas_ep;
  size_t lost = 0;
  for (long b = c->settle_ep; b < total; b++)
    {
      double e = wrap_chips (err[b] - m);
      m2 += e * e;
      if (fabs (wrap_chips (err[b] - ref)) > LOSS_CHIPS)
        lost++;
    }
  out->mean   = wrap_chips (m);
  out->sigma  = sqrt (m2 / (double)c->meas_ep);
  out->p_lost = (double)lost / (double)c->meas_ep;
  out->rate   = dll_get_code_rate (d);
  /* Pull-in: the first epoch after the last one outside PULL_TOL. */
  long last_out = -1;
  for (long b = 0; b < total; b++)
    if (fabs (wrap_chips (err[b] - ref)) >= PULL_TOL)
      last_out = b;
  out->pull_ep = last_out + 1 < total ? last_out + 1 : -1;

  free (err);
  free (prt);
  free (nz);
  free (blk);
  dll_destroy (d);
  awgn_destroy (g);
  wfm_synth_destroy (syn);
  return 0;
}

static cfg_t
base_cfg (double cn0, int aided, uint32_t seed)
{
  cfg_t c;
  memset (&c, 0, sizeof c);
  c.cn0_dbhz  = cn0;
  c.aided     = aided;
  c.ref       = NAN;
  c.seed      = seed;
  c.settle_ep = SETTLE_EP;
  c.meas_ep   = MEAS_EP;
  return c;
}

static const char *mode_name[2] = { "per-epoch", "aided" };

/* The converged phase per mode on a clean stream (the constant every other
   number is measured against), and the check that both modes converge to
   the same code phase. */
static int
converged_phase (const uint8_t *code, double ref[2])
{
  for (int a = 0; a < 2; a++)
    {
      cfg_t c     = base_cfg (WFM_SYNTH_SNR_CLEAN, a, 1u);
      c.settle_ep = SETTLE_EP;
      c.meas_ep   = 1000;
      run_t r;
      if (run (code, &c, &r))
        return 1;
      ref[a] = r.mean;
      printf ("  %-9s converged phase %+.4f chips (sigma %.5f, code rate "
              "%.7f)\n",
              mode_name[a], r.mean, r.sigma, r.rate);
    }
  return 0;
}

static double
median (double *v, int n)
{
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--)
      {
        double x = v[j];
        v[j]     = v[j - 1];
        v[j - 1] = x;
      }
  return n ? v[n / 2] : -1.0;
}

/* Pull-in from `delta` chips off at `cn0`, `n` trials: converged count and
   the median time to converge, in ms. */
static int
pull_in (const uint8_t *code, double cn0, int aided, double delta, double ref,
         int n, int *n_ok, double *med_ms)
{
  double ms[N_PULL];
  int    ok = 0;
  for (int k = 0; k < n; k++)
    {
      cfg_t c     = base_cfg (cn0, aided, 200u + (uint32_t)k);
      c.init_chip = ref + delta;
      c.ref       = ref;
      c.settle_ep = 0;
      c.meas_ep   = PULL_EP;
      run_t r;
      if (run (code, &c, &r))
        return 1;
      if (r.pull_ep >= 0)
        ms[ok++] = (double)r.pull_ep * (double)TE / FS * 1e3;
    }
  *n_ok   = ok;
  *med_ms = median (ms, ok);
  return 0;
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  printf ("DLL on the symbol-aided window vs the per-epoch look-back: "
          "Gold-1023 at 5 Mcps, spc %u, %.0f sym/s async BPSK "
          "(%.2f partials/symbol), bn %.3f, %u partials/epoch; block = one "
          "epoch (%.3f ms)\n\n",
          SPC, SYM_RATE, P_SYM, BN, SEGMENTS, (double)TE / FS * 1e3);

  printf ("converged phase, clean:\n");
  double ref[2];
  if (converged_phase (code, ref))
    return 1;

  if (check)
    {
      /* Both discriminators zero at the same code phase. */
      DP_CHECK (fabs (wrap_chips (ref[1] - ref[0])) < 0.05);
      /* Jitter at the two design C/N0s: neither loop loses the code, and
         the aided loop's jitter stays within its measured relation to the
         per-epoch loop's (§12.5: about equal at 45 dB-Hz, 1.2-1.4x at
         40). A window placed a partial off -- straddling a transition in
         most symbols -- reads 2x and more here, so the bound is a defect
         gate, not a ratchet. */
      const double cn0s[2]  = { 45.0, 40.0 };
      const double bound[2] = { 1.25, 1.6 };
      for (int ci = 0; ci < 2; ci++)
        {
          run_t r[2];
          for (int a = 0; a < 2; a++)
            {
              cfg_t c   = base_cfg (cn0s[ci], a, 11u);
              c.ref     = ref[a];
              c.meas_ep = CHECK_MEAS_EP;
              DP_REQUIRE (run (code, &c, &r[a]) == 0);
              DP_CHECK (r[a].p_lost == 0.0);
              DP_CHECK (fabs (r[a].rate - 1.0) < 1e-4);
            }
          printf ("  %.0f dB-Hz: jitter per-epoch %.4f, aided %.4f chips "
                  "(ratio %.2f, bound %.2f)\n",
                  cn0s[ci], r[0].sigma, r[1].sigma, r[1].sigma / r[0].sigma,
                  bound[ci]);
          DP_CHECK (r[1].sigma <= bound[ci] * r[0].sigma);
        }
      /* Pull-in from half a chip at the floor: every trial converges in
         both modes, and the aided loop is no slower than the per-epoch
         one (measured 20% faster). */
      int    n_ok[2];
      double med[2];
      for (int a = 0; a < 2; a++)
        {
          DP_REQUIRE (pull_in (code, 40.0, a, 0.5, ref[a], CHECK_PULL,
                               &n_ok[a], &med[a])
                      == 0);
          printf ("  pull-in 0.5 chip at 40 dB-Hz, %s: %d/%d, median %.0f "
                  "ms\n",
                  mode_name[a], n_ok[a], CHECK_PULL, med[a]);
          DP_CHECK (n_ok[a] == CHECK_PULL);
        }
      DP_CHECK (med[1] <= 1.1 * med[0]);
      DP_TEST_END ("validate_dll_aid_jitter");
    }

  /* ── jitter against C/N0 ── */
  const double cn0s[] = { 50.0, 45.0, 42.0, 40.0, 38.0, 36.0, 34.0 };
  const int    ncn0   = (int)(sizeof cn0s / sizeof cn0s[0]);
  printf ("\njitter over %d epochs after %d settling (chips):\n", MEAS_EP,
          SETTLE_EP);
  printf ("  C/N0    Es/N0     per-epoch: sigma   bias   lost   rate       "
          "aided: sigma   bias   lost   rate       ratio\n");
  for (int ci = 0; ci < ncn0; ci++)
    {
      run_t r[2];
      for (int a = 0; a < 2; a++)
        {
          cfg_t c = base_cfg (cn0s[ci], a, 11u);
          c.ref   = ref[a];
          if (run (code, &c, &r[a]))
            return 1;
        }
      printf ("  %4.0f   %5.1f            %.4f  %+.3f  %5.1f%%  %.6f        "
              "%.4f  %+.3f  %5.1f%%  %.6f   %.2f\n",
              cn0s[ci], cn0s[ci] - 10.0 * log10 (SYM_RATE), r[0].sigma,
              wrap_chips (r[0].mean - ref[0]), 100.0 * r[0].p_lost, r[0].rate,
              r[1].sigma, wrap_chips (r[1].mean - ref[1]), 100.0 * r[1].p_lost,
              r[1].rate, r[1].sigma / r[0].sigma);
    }

  /* ── pull-in ── */
  const double deltas[] = { 0.25, 0.5, 0.75 };
  const double pcn0s[]  = { 45.0, 40.0 };
  printf ("\npull-in, %d trials each, within %.2f chips of the converged "
          "phase and staying, watched %.0f ms:\n",
          N_PULL, PULL_TOL, (double)PULL_EP * TE / FS * 1e3);
  printf ("  C/N0   start (chips)   per-epoch: converged  median ms   "
          "aided: converged  median ms\n");
  for (int ci = 0; ci < 2; ci++)
    for (int di = 0; di < 3; di++)
      {
        int    n_ok[2];
        double med[2];
        for (int a = 0; a < 2; a++)
          if (pull_in (code, pcn0s[ci], a, deltas[di], ref[a], N_PULL,
                       &n_ok[a], &med[a]))
            return 1;
        printf ("  %4.0f   %5.2f                   %2d/%2d     %6.0f          "
                "%2d/%2d     %6.0f\n",
                pcn0s[ci], deltas[di], n_ok[0], N_PULL, med[0], n_ok[1],
                N_PULL, med[1]);
      }

  return 0;
}
