/**
 * @file async_dsss_receiver_ber.c
 * @brief The receiver's bit error rate against the BPSK bound, at the
 *        certification's operating point.
 *
 * Nothing else ties the shipped AsyncDsssReceiver to `Q(sqrt(2 Es/N0))`:
 * the certification (`async_dsss_receiver_lock.c`) measures EVM, and
 * `ber_despreader.c` pins only the synchronous, aligned despread. This
 * measures the whole receiver -- its own search, refine, hand-over, the
 * pre-despread carrier loop, the code loop, the rate converter and the
 * demodulator -- and states how far its error rate sits from the bound as
 * an IMPLEMENTATION LOSS in dB: the Es/N0 the bound would need for the
 * measured rate, subtracted from the Es/N0 applied.
 *
 * The receiver, the geometry and the stimulus are the 415.1-B-1 point's
 * (native/tests/dp_s415_test.h): Gold-1023 at both carriers, 2.2 and
 * 2.8 GHz, asynchronous BPSK at 3,000 sym/s in 1,000-symbol frames with a
 * 275-symbol code-only window, at 11 MSa/s, through the 415.1 scene
 * (native/tests/dp_air_test.h) with the harness's own i.i.d. bits, and
 * the point's receiver (dp_s415_rx()). Each burst draws its carrier offset
 * over +/-1,500 Hz.
 *
 * The measurement is `native/tests/dp_ber_test.h`'s, never re-derived:
 *
 *   - SETTLED. dp_ber_settle() takes the later of the analytic budget for
 *     the demodulator's two loops, read from the receiver's own
 *     demodulator, and the receiver's symbol lock sustained; a burst with
 *     no settled window is counted as unsettled, not scored. So the loss
 *     is the loss OVER THE BURSTS THAT SETTLED, and the bursts that do not
 *     are counted on their own: printed until a committed 415.1 sweep sets
 *     a ratchet (the
 *     report's finding `handoff_can_miss_the_carrier`, a known defect), and
 *     none may be one handed over inside the carrier
 *     loop's reliable pull-in, where the hand-over cannot be the reason.
 *   - ALIGNED. The lag and the BPSK phase are DETECTED against a marker of
 *     the burst's own truth, under a false-alarm gate, past the settling
 *     point; the marker's symbols are not scored. A second detection near
 *     the burst's end must find the same lag and phase: a timing slip or a
 *     cycle slip in between would make every later symbol a coin toss, so
 *     a burst that slipped is reported as one, never folded in quietly.
 *   - ENOUGH. Bursts run until the error target is reached (inverse
 *     binomial sampling), so the precision is fixed by the error count:
 *     the interval is exact, at DP_BER_CONF.
 *   - SANE. dp_ber_report() cross-checks the rate against EVM, the blind
 *     M2M4 Es/N0 and theory over the same windows.
 *
 * The truth array is offset to the block where the receiver began to emit:
 * the first symbol out carries a bit from about then, so the lag left to
 * detect is the chain's own delay, inside the DP_BER_LAG_SPAN search. The
 * receiver stays tracking throughout (lost_confirm_s 0).
 *
 * Usage:
 *   validate_async_dsss_receiver_ber            the sweep, every Es/N0
 *   validate_async_dsss_receiver_ber --check    the regression point
 *   ... --emit                                  CSV blocks on stdout for
 *                                               the record: `# ber`, one row
 *                                               a carrier and Es/N0, and `#
 * burst`, one row a burst
 *   ... --spec-only                             the 415.1 pool rows alone
 *   ... --spc N                                 the 415.1 scene at N samples
 *                                               a chip (default 4; 2 is the
 *                                               interpolating front end)
 */
#include "async_dsss_receiver_common.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "dp_air_test.h"
#include "dp_ber_test.h"
#include "dp_dsss_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BURST_S 6.0     /* each burst, from the signal's first sample      */
#define MAX_BURSTS 400u /* a point's cap, so a clean receiver terminates  */
#define MIN_OUT 1000u   /* fewer symbols out than this cannot settle      */
#define PUSH 4096u      /* the feed block, samples                          */
#define OUT_CAP 256u    /* symbols a push may emit: about 1.1 at 415.1    */

/* What the error rate is held to, and what is only printed (#3134, the
   three kinds of row):

   - ASSERTED, from a source: the BPSK bound, Q(sqrt(2 Es/N0)). No
     receiver beats it, so a measured rate whose interval lies wholly below
     it is a harness that mis-scores, and fails. With dp_ber_report()'s own
     validity (settled, aligned, enough errors, cross-checks agree) and the
     carrier loop's reliable pull-in below, that is all the source supports.
   - PRINTED, a measurement waiting on a requirement: the loss from the
     bound. The allowed loss is the owner's to set (doppler#3157); until
     then the row prints the loss with its interval beside the one part of
     it derived so far, the Dll replica's correlation loss rho^2/kappa
     (replica_loss_db()).
   - PRINTED until a run classifies them: bursts that never settle and
     bursts that slip. The 3.069 Mcps ratchet on unsettled bursts retired
     with that point; a new one is set from a committed 415.1 sweep, never
     from the run it would gate. */

/* The pre-despread loop is "reliable to twice" its bound,
   ASYNC_DSSS_RX_CARRIER_PULLIN_HZ, measured on the seed it is handed,
   which the refine's hand-over is. Inside that the hand-over cannot be why
   a burst did not settle, so an unsettled burst there is some other
   failure, and fails. Past it a burst may settle or not (#2409). */
#define PULLIN_RELIABLE_HZ(fc)                                                \
  (2.0 * ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (dp_s415_chip_rate (fc), DP_S415_SF))

/* The sweep: both carriers of 415.1-B-1, from its Es/N0 floor (4 dB) to
   7 dB, where a point at 200 errors still costs under a minute. */
static const double ber_fc[]   = { DP_S415_FC_LO, DP_S415_FC_HI };
static const double ber_esn0[] = { 4.0, 5.0, 6.0, 7.0 };
#define N_FC (sizeof ber_fc / sizeof ber_fc[0])
#define N_ESN0 (sizeof ber_esn0 / sizeof ber_esn0[0])
#define N_PTS (N_FC * N_ESN0)

/* --check: the band's harder end, 2.8 GHz, at 7 dB, where every
   cross-check of dp_ber_report() holds with margin, at the library's
   error target: about five bursts, the sweep's own seeds. */
#define CHECK_FC 1u
#define CHECK_ESN0 3u

/* The Dll replica's correlation loss for `code`, dB (#3134): the replica
   is the code interpolated between a quarter and three quarters of each
   chip, so with p the code's fraction of sign-changing boundaries,
   cyclically, its correlation with the chips is 1 - p/4 and its energy
   1 - p/3 (test_dll_core.c, replica_terms_follow_transitions). At 415.1
   the samples sweep every chip phase, so every correlation through it
   gives up rho^2/kappa: the prediction the loss column is read against. */
static double
replica_loss_db (const uint8_t *code, size_t sf)
{
  size_t flips = 0;
  for (size_t i = 0; i < sf; i++)
    flips += code[i] != code[(i + 1) % sf];
  const double p = (double)flips / (double)sf;
  return -10.0 * log10 ((1.0 - p / 4.0) * (1.0 - p / 4.0) / (1.0 - p / 3.0));
}

/* One burst's account. */
typedef struct
{
  double carrier_hz;
  double t_track_ms;       /* the signal's first sample -> first symbol */
  double handover_err_hz;  /* the refine's carrier then, minus truth */
  size_t n_out;            /* symbols the receiver emitted              */
  size_t settle;           /* dp_ber_settle()'s window start            */
  size_t lo, hi;           /* the scored window, receiver symbols       */
  int    settled;          /* a settled window, aligned at its start    */
  int    slipped;          /* the end's alignment is not the start's    */
  int    slip_kind;        /* 1 lag moved (timing), 2 phase flipped
                              (carrier), 3 no alignment at the end      */
  long          lag;       /* detected at the start, against `base`     */
  double        lock_frac; /* scored symbols with symbol lock up        */
  unsigned long errors;    /* this burst's errors in the window         */
  unsigned long scored;    /* and the symbols scored                    */
} burst_t;

/* The detected alignment over a marker of `truth` at `t0`, or at the first
   stretch from there toward `dir` that can fix one at all
   (dp_ber_marker_place(): a marker inside the code-only window's run of
   zeros correlates alike at every lag). */
static dp_ber_sync_t
align_at (const float complex *rx, size_t n, const uint8_t *truth,
          size_t n_truth, size_t t0, int dir, dp_ber_marker_t *mk)
{
  size_t at  = dp_ber_marker_place (truth, n_truth, 2, t0, DP_BER_SYNC_SYMS,
                                    DP_BER_LAG_SPAN, DP_BER_SYNC_PFA, dir);
  mk->sym    = NULL;
  mk->n      = DP_BER_SYNC_SYMS;
  mk->t0     = at == SIZE_MAX ? n_truth : at;
  mk->period = 0;
  mk->reps   = 0;
  return dp_ber_sync (rx, n, truth, n_truth, mk, 2, DP_BER_LAG_SPAN,
                      DP_BER_SYNC_PFA);
}

/* One burst at carrier `fc` and `esn0_db`: the 415.1 scene
   (native/tests/dp_air_test.h -- the shipped synth sending the harness's
   own i.i.d. bits in the 275-in-1,000 frame, the front end's converter to
   11 MSa/s, a Doppler channel at the carrier, the shipped noise), the
   point's receiver (dp_s415_rx()); render, receive,
   settle, align, check the end's alignment, and score into `acc`. `last`
   keeps the alignment of a scored burst for dp_ber_report(). */
static int
run_burst (const uint8_t *code, double fc, double esn0_db, uint32_t seed,
           dp_ber_t *acc, burst_t *out, dp_ber_sync_t *last)
{
  uint32_t     st  = trial_rng (seed);
  const double cn0 = dp_s415_cn0 (esn0_db);
  out->carrier_hz  = (2.0 * dp_uni (&st) - 1.0) * DP_S415_SPAN_HZ;
  const size_t fr
      = (size_t)(dp_uni (&st) * (double)DP_S415_FRAME_SYM) % DP_S415_FRAME_SYM;
  const size_t n_blk  = (size_t)(BURST_S * DP_S415_FS / PUSH);
  const size_t n_bits = (size_t)(BURST_S * DP_S415_SYM) + 400;
  uint8_t     *pay    = dp_xmalloc (n_bits);
  for (size_t i = 0; i < n_bits; i++)
    pay[i] = (uint8_t)(dp_uni (&st) < 0.5);
  dp_air_emitter_t e
      = dp_s415_emitter (code, fc, out->carrier_hz, 0.0, cn0, 0.0, seed);
  e.frame_off = fr;
  e.bits      = pay;
  e.n_bits    = n_bits;
  dp_air_t air;
  DP_REQUIRE (
      dp_air_init (&air, &e, 1, DP_S415_FS, cn0, 7919u * seed + 1u, PUSH));
  /* Whether the emitter sends `pay` needs no check of its own here: the
     truth alignment is that check, as it was on the 3.069 Mcps point,
     where the synth's own PRBS left 37 of 38 bursts unaligned and the
     38th caught as slipped. */
  const size_t n_sym = (size_t)(BURST_S * DP_S415_SYM) + 200;
  uint8_t     *bits  = dp_xmalloc (n_sym);
  for (size_t i = 0; i < n_sym; i++)
    bits[i] = (uint8_t)(dp_air_symbol_bit (&air, 0, (long)i) > 0);
  dp_async_dsss_receiver_state_t *rx = dp_s415_rx (code, fc, cn0, true);
  DP_REQUIRE (rx != NULL);

  const size_t   ocap = (size_t)(BURST_S * DP_S415_SYM) + 64 + OUT_CAP;
  float complex *blk  = dp_xmalloc (PUSH * sizeof *blk);
  float complex *syms = dp_xmalloc (ocap * sizeof *syms);
  unsigned char *lock = dp_xmalloc (ocap);
  size_t         n = 0, first_in = SIZE_MAX;
  out->handover_err_hz = NAN;
  for (size_t b = 0; b < n_blk; b++)
    {
      DP_REQUIRE (dp_air_next (&air, blk, PUSH));
      size_t room = ocap - n;
      size_t k    = dp_async_dsss_receiver_steps (
          rx, blk, PUSH, syms + n, room < OUT_CAP ? room : OUT_CAP);
      /* The symbol lock after the push, for every symbol it emitted. */
      unsigned char up = (unsigned char)dp_async_dsss_receiver_get_locked (rx);
      for (size_t j = 0; j < k; j++)
        lock[n + j] = up;
      if (k && first_in == SIZE_MAX)
        {
          /* The hand-over: the refine's carrier, frozen once tracking --
             what the carrier loops were seeded with, against the truth
             (no ramp here, so the truth is the draw's offset). */
          first_in = b * PUSH;
          out->handover_err_hz
              = dp_async_dsss_receiver_get_doppler_hz (rx) - out->carrier_hz;
        }
      n += k;
    }
  DP_CHECK_MSG (!dp_air_data_ended (&air), "the burst's data covered its run");

  out->n_out      = n;
  out->t_track_ms = first_in == SIZE_MAX
                        ? NAN
                        : (double)(first_in + PUSH) / DP_S415_FS * 1e3;
  out->settled = out->slipped = out->slip_kind = 0;
  out->settle = out->lo = out->hi = 0;
  out->lag                        = 0;
  out->lock_frac                  = NAN;
  out->errors = out->scored = 0;

  if (n >= MIN_OUT && rx->rx)
    {
      /* The analytic budget's bandwidths are the demodulator's own, read
         from the chain the receiver built rather than restated here. The
         timing loop publishes no per-symbol lock to this object, so its
         5/bn reaches the window through the budget (dp_ber_test.h's NULL
         case). */
      const double bn_t = rx->rx->l.timing.bn;
      const double bn_c = rx->rx->l.bn_carrier;
      int          ok   = 0;
      out->settle       = dp_ber_settle (bn_t, bn_c, NULL, lock, n, &ok);

      /* Truth from the symbol the emitter was on when the receiver first
         emitted: the chain's delays are a few dozen symbols, inside the
         alignment's span. */
      long base = dp_air_truth (&air, 0, first_in).symbol - 60;
      if (base < 0)
        base = 0;
      const uint8_t  *truth   = bits + base;
      const size_t    n_truth = n_sym - (size_t)base;
      const size_t    guard   = (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS;
      dp_ber_marker_t mk, mk_end;
      if (ok && out->settle + 2 * guard + 500 < n)
        {
          dp_ber_sync_t sy = align_at (syms, n, truth, n_truth,
                                       out->settle + DP_BER_LAG_SPAN, 1, &mk);
          if (sy.ok)
            {
              dp_ber_sync_t end
                  = align_at (syms, n, truth, n_truth, n - guard, -1, &mk_end);
              out->settled   = 1;
              out->lag       = sy.lag;
              out->slip_kind = !end.ok                            ? 3
                               : end.lag != sy.lag                ? 1
                               : cos (end.phase - sy.phase) < 0.0 ? 2
                                                                  : 0;
              out->slipped   = out->slip_kind != 0;
              long e0l       = (long)(mk.t0 + mk.n) - sy.lag;
              out->lo = (e0l > 0 && (size_t)e0l > out->settle) ? (size_t)e0l
                                                               : out->settle;
              out->hi = n;
              /* A burst that slipped is counted and kept out of the
                 point's tally: past the slip every symbol is a coin toss,
                 and folded in it would read the point near 0.5 and end it
                 on its error target. Its own rate is still the burst
                 row's. */
              dp_ber_t      one;
              dp_ber_t     *into = out->slipped ? &one : acc;
              unsigned long e0 = 0, s0 = 0;
              dp_ber_init (&one, 2, ULONG_MAX);
              if (!out->slipped)
                {
                  e0 = acc->errors;
                  s0 = acc->symbols;
                }
              dp_ber_score (into, syms, out->lo, out->hi, truth, n_truth, &mk,
                            &sy);
              out->errors = into->errors - e0;
              out->scored = into->symbols - s0;
              dp_ber_free (&one);
              size_t up = 0;
              for (size_t i = out->lo; i < out->hi; i++)
                up += lock[i];
              out->lock_frac = (double)up / (double)(out->hi - out->lo);
              if (!out->slipped)
                *last = sy;
            }
        }
    }

  free (lock);
  free (syms);
  free (blk);
  free (bits);
  free (pay);
  dp_async_dsss_receiver_destroy (rx);
  dp_air_free (&air);
  return 0;
}

/* ── SPEC: the receivers at the 415.1-B-1 point, as a pool runs them ─────
 *
 * docs/design/async-dsss-receiver.md §1, §4.4. Three emitters with KNOWN
 * bits on one Gold code with the 275-symbol code-only window opening every
 * 1,000 symbols: the 415.1 scene (native/tests/dp_air_test.h -- the shipped
 * synth, the front end's converter to 11 MSa/s, a DopplerChannel at the
 * carrier: -900, +100 and +1,100 Hz, 0, 300 and 600 chips apart), plus the
 * shipped awgn at Es/N0. An AsyncDsssPool of four receivers at 11 MSa/s
 * (+/-1,500 Hz, 400 Hz/s, the window, the carrier set) takes them; each
 * receiver holding an emitter at the end is scored against that emitter's
 * bits with dp_ber_test.h: settled, aligned by a detected marker, and
 * checked for a changed alignment at the end ("slipped": a carrier
 * half-cycle or a symbol it never recovered). The clean tally is the
 * unslipped receivers'.
 *
 * The sweep: 2.2 and 2.8 GHz, 4 dB (SPEC_TRIALS_FLOOR draws) and 7 dB
 * (SPEC_TRIALS_HI), SPEC_S each, printed. --check: one draw at 2.8 GHz,
 * the band's harder end, at 7 dB, SPEC_CHECK_S long
 * (about 5 s of the run), asserting that every emitter is held and aligned,
 * and none slipped; the loss is printed, as the burst sweep's is (the
 * allowed loss is doppler#3157's). The floor's rows move with the draw and
 * are printed, not asserted.
 *
 * The loss is the labelled Es/N0 less the Es/N0 the measured SER implies on
 * the BPSK bound, and the label is the emitter's C/N0 AT THE ANTENNA
 * (dp_air_test.h): it includes the front end's band, which keeps 0.917 of
 * a rectangular chip's power at 2.8 GHz (0.38 dB) and 0.946 at 2.2 (0.24).
 * The 1.5 dB margin this row once asserted retired with the three kinds
 * of row (#3134): it was a regression margin set above the run it gated,
 * not a derived bound. */
#define SPEC_K 3
#define SPEC_NRX 4
#define SPEC_PUSH 4096u
#define SPEC_S 4.0
#define SPEC_CHECK_S 2.0
#define SPEC_TRIALS_FLOOR 30
#define SPEC_TRIALS_HI 40

static const double spec_dop[SPEC_K] = { -900.0, 100.0, 1100.0 };
/* `--spc N`: the scene at N whole samples a chip (0: dp_air_test.h's
   default, 4); 2 makes the front end interpolate, for the comparison. */
static unsigned spec_spc = 0;

/* Each emitter's code offset: silence ahead of its first chip. */
static const double spec_lag_chips[SPEC_K] = { 0.0, 300.0, 600.0 };

typedef struct
{
  size_t   held, scored, unaligned, slipped;
  dp_ber_t all, clean;
} spec_t;

/* One draw: the pool on SPEC_S of the scene at `fc`, every holding
   receiver scored into `out`. */
static int
spec_draw (const uint8_t *code, double fc, double esn0, uint32_t seed,
           double secs, spec_t *out)
{
  const double chip   = dp_s415_chip_rate (fc);
  const double cn0    = dp_s415_cn0 (esn0);
  const size_t n_in   = (size_t)(secs * DP_S415_FS);
  const size_t n_bits = (size_t)(secs * DP_S415_SYM) + 400;
  uint8_t     *pay[SPEC_K], *bits[SPEC_K];
  uint32_t     st = trial_rng (seed);
  /* The emitters on the air (native/tests/dp_air_test.h): each the
     shipped synth sending its own payload with the 275-in-1,000 window,
     the front end's converter, a Doppler channel, the shipped noise. The
     payload runs through the data symbols only; `bits` is what each
     symbol carries, the truth the scorer reads. */
  dp_air_emitter_t e[SPEC_K];
  for (int k = 0; k < SPEC_K; k++)
    {
      pay[k] = dp_xmalloc (n_bits);
      for (size_t i = 0; i < n_bits; i++)
        pay[k][i] = (uint8_t)(dp_uni (&st) < 0.5);
      e[k]        = dp_s415_emitter (code, fc, spec_dop[k], 0.0, cn0,
                                     spec_lag_chips[k] / chip, 100u + seed);
      e[k].bits   = pay[k];
      e[k].n_bits = n_bits;
      e[k].spc    = spec_spc;
    }
  dp_air_t air;
  DP_REQUIRE (
      dp_air_init (&air, e, SPEC_K, DP_S415_FS, cn0, 7919u + seed, SPEC_PUSH));
  for (int k = 0; k < SPEC_K; k++)
    {
      bits[k] = dp_xmalloc (n_bits);
      for (size_t i = 0; i < n_bits; i++)
        {
          const int b = dp_air_symbol_bit (&air, k, (long)i);
          bits[k][i]  = (uint8_t)(b > 0);
        }
    }
  dp_async_dsss_pool_state_t *p
      = dp_s415_pool (code, fc, SPEC_NRX, cn0, 2.0, 0);
  DP_REQUIRE (p);
  float complex *x    = dp_xmalloc (SPEC_PUSH * sizeof *x);
  const size_t   ocap = (size_t)(secs * DP_S415_SYM) + 4096;
  float complex *tmp  = dp_xmalloc (ocap * sizeof *tmp);
  float complex *syms[SPEC_NRX];
  size_t         ns[SPEC_NRX], first_in[SPEC_NRX];
  for (int i = 0; i < SPEC_NRX; i++)
    {
      syms[i]     = dp_xmalloc (ocap * sizeof **syms);
      ns[i]       = 0;
      first_in[i] = SIZE_MAX;
    }
  for (size_t pos = 0; pos + SPEC_PUSH <= n_in; pos += SPEC_PUSH)
    {
      DP_REQUIRE (dp_air_next (&air, x, SPEC_PUSH));
      (void)dp_async_dsss_pool_push (p, x, SPEC_PUSH);
      for (int i = 0; i < SPEC_NRX; i++)
        {
          const size_t got = dp_async_dsss_pool_symbols (p, i, tmp, ocap);
          if (dp_async_dsss_pool_status (p, i).state != ASYNC_DSSS_RX_TRACKING)
            { /* only the CURRENT track's symbols */
              ns[i]       = 0;
              first_in[i] = SIZE_MAX;
              continue;
            }
          if (got && first_in[i] == SIZE_MAX)
            first_in[i] = pos + SPEC_PUSH;
          if (ns[i] + got <= ocap)
            {
              memcpy (syms[i] + ns[i], tmp, got * sizeof *tmp);
              ns[i] += got;
            }
        }
    }
  int taken[SPEC_K] = { 0 };
  for (int i = 0; i < SPEC_NRX; i++)
    {
      const async_dsss_receiver_status_t s = dp_async_dsss_pool_status (p, i);
      if (s.state != ASYNC_DSSS_RX_TRACKING || ns[i] < 2000)
        continue;
      int k = -1;
      for (int j = 0; j < SPEC_K && k < 0; j++)
        if (!taken[j] && fabs (s.doppler_hz - spec_dop[j]) < 150.0)
          k = j;
      if (k < 0)
        continue;
      taken[k] = 1;
      out->held++;
      int          ok = 0;
      const size_t settle
          = dp_ber_settle (p->rx[i]->rx->l.timing.bn,
                           p->rx[i]->rx->l.bn_carrier, NULL, NULL, ns[i], &ok);
      /* Truth from the bit emitter k was on when this receiver first
         emitted: the chain's delays are a few dozen symbols, inside the
         alignment's span. */
      long base = dp_air_truth (&air, k, first_in[i]).symbol - 60;
      if (base < 0)
        base = 0;
      const uint8_t  *truth   = bits[k] + base;
      const size_t    n_truth = n_bits - (size_t)base;
      const size_t    guard   = (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS;
      dp_ber_marker_t mk, mk_end;
      if (!ok || settle + 2 * guard + 500 >= ns[i])
        {
          out->unaligned++;
          continue;
        }
      dp_ber_sync_t sy = align_at (syms[i], ns[i], truth, n_truth,
                                   settle + DP_BER_LAG_SPAN, 1, &mk);
      if (!sy.ok)
        {
          out->unaligned++;
          continue;
        }
      dp_ber_sync_t end = align_at (syms[i], ns[i], truth, n_truth,
                                    ns[i] - guard, -1, &mk_end);
      const int     slip
          = !end.ok || end.lag != sy.lag || cos (end.phase - sy.phase) < 0.0;
      out->slipped += (size_t)slip;
      out->scored++;
      const long   e  = (long)(mk.t0 + mk.n) - sy.lag;
      const size_t lo = (e > 0 && (size_t)e > settle) ? (size_t)e : settle;
      dp_ber_score (&out->all, syms[i], lo, ns[i], truth, n_truth, &mk, &sy);
      if (!slip)
        dp_ber_score (&out->clean, syms[i], lo, ns[i], truth, n_truth, &mk,
                      &sy);
    }
  for (int i = 0; i < SPEC_NRX; i++)
    free (syms[i]);
  free (tmp);
  free (x);
  for (int k = 0; k < SPEC_K; k++)
    {
      free (bits[k]);
      free (pay[k]);
    }
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

static double
spec_loss (double esn0, unsigned long e, unsigned long n)
{
  return n ? esn0 - dp_ber_esn0_db_for_ser (2, (double)e / (double)n) : NAN;
}

/* The rows: each carrier and Es/N0. */
static int
spec_rows (const uint8_t *code, int check, int emit)
{
  static const double fcs[2]   = { DP_S415_FC_LO, DP_S415_FC_HI };
  static const double esn0s[2] = { 4.0, 7.0 };
  if (emit)
    printf ("# spec\nfc_ghz,esn0_db,draws,held,scored,unaligned,"
            "slipped,clean_errors,clean_symbols,clean_ser,clean_ser_lo,"
            "clean_ser_hi,clean_loss_db,all_errors,all_symbols,all_ser\n");
  else
    printf ("spec: 415.1-B-1, 3 emitters, a pool of 4 at 11 MSa/s, %s; "
            "loss against the BPSK bound, C/N0 at the antenna (the front "
            "end keeps 0.917 of the chip power at 2.8 GHz, 0.946 at 2.2)\n",
            check ? "2.8 GHz, 7 dB, one draw" : "4 and 7 dB");
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int e = check ? 1 : 0; e < 2; e++)
      {
        const int draws = check            ? 1
                          : esn0s[e] < 5.0 ? SPEC_TRIALS_FLOOR
                                           : SPEC_TRIALS_HI;
        spec_t    t     = { 0 };
        dp_ber_init (&t.all, 2, ULONG_MAX);
        dp_ber_init (&t.clean, 2, ULONG_MAX);
        for (int d = 1; d <= draws; d++)
          DP_REQUIRE (spec_draw (code, fcs[f], esn0s[e], (uint32_t)d,
                                 check ? SPEC_CHECK_S : SPEC_S, &t)
                      == 0);
        const dp_ber_ci_t ci
            = dp_ber_ci (t.clean.errors, t.clean.symbols, 0.95);
        const double loss
            = spec_loss (esn0s[e], t.clean.errors, t.clean.symbols);
        if (emit)
          printf ("%.1f,%.1f,%d,%zu,%zu,%zu,%zu,%lu,%lu,%.4e,%.4e,%.4e,"
                  "%.3f,%lu,%lu,%.4e\n",
                  fcs[f] / 1e9, esn0s[e], draws, t.held, t.scored, t.unaligned,
                  t.slipped, t.clean.errors, t.clean.symbols,
                  t.clean.symbols ? (double)t.clean.errors / t.clean.symbols
                                  : NAN,
                  ci.lo, ci.hi, loss, t.all.errors, t.all.symbols,
                  t.all.symbols ? (double)t.all.errors / t.all.symbols : NAN);
        else
          printf ("  %.1f GHz %.0f dB held %zu/%d, slipped %zu of "
                  "%zu, clean loss %.2f dB [%.2f, %.2f] (%lu / %lu), all "
                  "%.2f dB\n",
                  fcs[f] / 1e9, esn0s[e], t.held, SPEC_K * draws, t.slipped,
                  t.scored, loss, esn0s[e] - dp_ber_esn0_db_for_ser (2, ci.lo),
                  esn0s[e] - dp_ber_esn0_db_for_ser (2, ci.hi), t.clean.errors,
                  t.clean.symbols,
                  spec_loss (esn0s[e], t.all.errors, t.all.symbols));
        if (check)
          {
            DP_CHECK_MSG (t.held == SPEC_K && t.scored == SPEC_K,
                          "at 7 dB every emitter is held and aligned");
            DP_CHECK_MSG (t.slipped == 0, "at 7 dB no receiver slips");
          }
        dp_ber_free (&t.all);
        dp_ber_free (&t.clean);
      }
  return 0;
}

int
main (int argc, char **argv)
{
  int check = 0, emit = 0, spec_only = 0;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      spec_only |= strcmp (argv[i], "--spec-only") == 0;
      if (strcmp (argv[i], "--spc") == 0 && i + 1 < argc)
        spec_spc = (unsigned)strtoul (argv[++i], NULL, 10);
    }
  uint8_t code[DP_S415_SF];
  dp_dsss_gold_1023 (code);
  /* `--spec-only`: the 415.1 pool rows alone, for a comparison of the
     scene's front end that the other sections do not use. */
  if (spec_only)
    {
      DP_REQUIRE (spec_rows (code, check, emit) == 0);
      if (emit)
        DP_TEST_EMIT_END ("validate_async_dsss_receiver_ber --spec-only");
      DP_TEST_END ("validate_async_dsss_receiver_ber --spec-only");
    }
  const double replica_db = replica_loss_db (code, DP_S415_SF);

  if (emit)
    printf ("# burst\nfc_ghz,esn0_db,seed,carrier_hz,t_track_ms,"
            "handover_err_hz,n_out,settle,lo,hi,settled,slipped,slip_kind,lag,"
            "lock_frac,errors,scored\n");
  else
    printf ("AsyncDsssReceiver BER vs the BPSK bound at 415.1-B-1: "
            "Gold-%u, %.0f sym/s, %.1f s bursts, %lu errors a point; the "
            "Dll replica's loss %.2f dB (rho^2/kappa)\n",
            DP_S415_SF, DP_S415_SYM, BURST_S, DP_BER_TARGET_ERRORS,
            replica_db);

  dp_ber_report_t reps[N_PTS];
  size_t          n_bursts[N_PTS], n_unsettled[N_PTS], n_slipped[N_PTS];
  size_t          n_inside[N_PTS], pt_f[N_PTS], pt_e[N_PTS];
  size_t          n_pts = 0;
  for (size_t f = check ? CHECK_FC : 0; f < (check ? CHECK_FC + 1 : N_FC); f++)
    for (size_t e = check ? CHECK_ESN0 : 0;
         e < (check ? CHECK_ESN0 + 1 : N_ESN0); e++)
      {
        const size_t  q  = n_pts++;
        const double  fc = ber_fc[f], es = ber_esn0[e];
        dp_ber_t      acc;
        dp_ber_sync_t last;
        int           any = 0;
        size_t        lo = 0, hi = 0;
        pt_f[q] = f;
        pt_e[q] = e;
        dp_ber_init (&acc, 2, DP_BER_TARGET_ERRORS);
        n_bursts[q] = n_unsettled[q] = n_slipped[q] = n_inside[q] = 0;
        for (uint32_t s = 1; !dp_ber_enough (&acc) && s <= MAX_BURSTS; s++)
          {
            burst_t bu;
            DP_REQUIRE (run_burst (code, fc, es, s, &acc, &bu, &last) == 0);
            n_bursts[q]++;
            n_unsettled[q] += !bu.settled;
            n_slipped[q] += (size_t)bu.slipped;
            /* Unsettled though handed over inside the reliable pull-in --
               or with no hand-over at all (NaN fails the `>`). */
            n_inside[q]
                += !bu.settled
                   && !(fabs (bu.handover_err_hz) > PULLIN_RELIABLE_HZ (fc));
            if (bu.settled)
              {
                any = 1;
                lo  = bu.lo;
                hi  = bu.hi;
              }
            if (emit)
              printf ("%.1f,%.1f,%u,%.1f,%.2f,%.1f,%zu,%zu,%zu,%zu,%d,%d,%d,"
                      "%ld,%.4f,%lu,%lu\n",
                      fc / 1e9, es, s, bu.carrier_hz, bu.t_track_ms,
                      bu.handover_err_hz, bu.n_out, bu.settle, bu.lo, bu.hi,
                      bu.settled, bu.slipped, bu.slip_kind, bu.lag,
                      bu.lock_frac, bu.errors, bu.scored);
            else
              printf ("  %.1f GHz %4.1f dB seed %3u: carrier %+7.1f Hz, "
                      "track %7.1f ms, hand-over %+6.1f Hz off, settle "
                      "%5zu, scored %5lu, errors %4lu%s%s\n",
                      fc / 1e9, es, s, bu.carrier_hz, bu.t_track_ms,
                      bu.handover_err_hz, bu.settle, bu.scored, bu.errors,
                      bu.settled ? "" : "  UNSETTLED",
                      bu.slipped ? "  SLIPPED" : "");
          }
        reps[q] = dp_ber_report (&acc, es, any ? &last : NULL, lo, hi, any,
                                 DP_BER_CONF);
        dp_ber_free (&acc);
        const dp_ber_report_t *r = &reps[q];
        if (!emit)
          {
            char label[48];
            snprintf (label, sizeof label, "BPSK %.1f GHz @%.1f dB", fc / 1e9,
                      es);
            dp_ber_print (label, r);
            printf ("    loss %.2f dB (%.2f to %.2f), the replica's %.2f; "
                    "%zu of %zu bursts unsettled, %zu slipped\n",
                    r->loss_db, es - dp_ber_esn0_db_for_ser (2, r->ser.lo),
                    es - dp_ber_esn0_db_for_ser (2, r->ser.hi), replica_db,
                    n_unsettled[q], n_bursts[q], n_slipped[q]);
          }
        /* The same assertions in --check and the sweep, at every point:
           the asserted rows of the comment at the top. */
        if (!r->ok)
          fprintf (stderr, "  %.1f GHz %.1f dB: %s\n", fc / 1e9, es, r->why);
        DP_CHECK_MSG (r->ok, "the error rate is a result: settled, aligned, "
                             "enough errors, and its cross-checks agree");
        DP_CHECK_MSG (r->ber.hi >= r->theory_ber,
                      "the error rate is not below the BPSK bound (its "
                      "interval's upper limit reaches the bound)");
        DP_CHECK_MSG (n_inside[q] == 0,
                      "no burst handed over inside the carrier loop's "
                      "reliable pull-in fails to settle");
        /* A link works only with no slip and its end in sync (the
           owner's rule). The sweep prints the slip rate per cell, and a
           cell that slips is a gap in the report, not a verdict; the
           regression point holds it at 0. */
        if (check)
          DP_CHECK_MSG (n_slipped[q] == 0,
                        "at the regression point no burst slips");
      }

  if (emit)
    {
      /* evm_ideal_db: what the EVM meter (dp_ber_evm_db) reads on an ideal
         stream at this Es/N0, so the EVM's own loss is evm_db minus it.
         replica_loss_db: the Dll replica's rho^2/kappa, the part of the
         loss derived so far. unsettled_inside: unsettled bursts handed over
         inside pullin_hz, the reliable pull-in (must be 0). */
      printf (
          "# ber\nfc_ghz,esn0_db,bursts,unsettled,unsettled_inside,"
          "pullin_hz,slipped,slip_lo,slip_hi,symbols,errors,ber,ber_lo,ber_hi,"
          "conf,"
          "theory,loss_db,loss_lo_db,loss_hi_db,replica_loss_db,evm_db,"
          "evm_ideal_db,m2m4_db,m2m4_agree_db,ok\n");
      for (size_t q = 0; q < n_pts; q++)
        {
          const dp_ber_report_t *r  = &reps[q];
          const double           fc = ber_fc[pt_f[q]];
          const double           es = ber_esn0[pt_e[q]];
          /* The slip rate over the bursts that settled (a slip needs an
             alignment at the start), with its 95% Wilson interval
             (dp_stats_test.h; a proportion of bursts, not a rate of
             symbols): the BER beside it is over the bursts that did not
             slip, so this is the rate that conditional rate excludes. */
          double sl_lo = NAN, sl_hi = NAN;
          dp_wilson ((double)n_slipped[q],
                     (double)(n_bursts[q] - n_unsettled[q]), &sl_lo, &sl_hi);
          printf ("%.1f,%.1f,%zu,%zu,%zu,%.1f,%zu,%.4f,%.4f,%lu,%lu,%.4e,"
                  "%.4e,%.4e,%.2f,%.4e,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                  "%.1f,%d\n",
                  fc / 1e9, es, n_bursts[q], n_unsettled[q], n_inside[q],
                  PULLIN_RELIABLE_HZ (fc), n_slipped[q], sl_lo, sl_hi,
                  r->ber.symbols, r->ber.errors, r->ber.p_hat, r->ber.lo,
                  r->ber.hi, r->ber.conf, r->theory_ber, r->loss_db,
                  es - dp_ber_esn0_db_for_ser (2, r->ser.lo),
                  es - dp_ber_esn0_db_for_ser (2, r->ser.hi), replica_db,
                  r->evm_db, dp_ber_evm_ideal_db (es), r->m2m4_db,
                  DP_BER_M2M4_AGREE_DB, r->ok);
        }
    }
  DP_REQUIRE (spec_rows (code, check, emit) == 0);
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_ber");
  DP_TEST_END ("validate_async_dsss_receiver_ber");
}
