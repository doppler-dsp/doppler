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
 * The receiver, the geometry and the stimulus are the certification's
 * (async_dsss_receiver_common.h): Gold-1023 at 3.069 Mcps, two samples a
 * chip, asynchronous BPSK at 2,700 sym/s, the searching flavor at the
 * binding's defaults, the shipped continuous-DSSS synth sending the
 * harness's own i.i.d. bits, and the shipped awgn sized from the C/N0 for
 * that Es/N0. Each burst draws its carrier over +/-1/T_E, as the
 * certification's draws do.
 *
 * The measurement is `native/tests/dp_ber_test.h`'s, never re-derived:
 *
 *   - SETTLED. dp_ber_settle() takes the later of the analytic budget for
 *     the demodulator's two loops, read from the receiver's own
 *     demodulator, and the receiver's symbol lock sustained; a burst with
 *     no settled window is counted as unsettled, not scored. So the loss
 *     is the loss OVER THE BURSTS THAT SETTLED, and the bursts that do not
 *     are gated on their own: their count is a ratchet per point (F3, a
 *     known defect), and none may be one handed over inside the carrier
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
 *                                               an Es/N0, and `# burst`, one
 *                                               row a burst
 */
#include "async_dsss_receiver_common.h"
#include "doppler/dp_complex.h"
#include "dp_ber_test.h"
#include "dp_dsss_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BURST_S 6.0     /* each burst, from the signal's first sample      */
#define MAX_BURSTS 400u /* a point's cap, so a clean receiver terminates  */
#define MIN_OUT 1000u   /* fewer symbols out than this cannot settle      */

/* The most implementation loss the record claims over the bursts that
   settle, dB, asserted on the loss the interval's LOWER limit implies --
   the receiver's best defensible rate -- so counting noise cannot flake
   it. The sweep's point losses are 0.65 to 0.89 dB from 5 to 8 dB, their
   lower limits 0.52 to 0.68; at 7 dB and 200 errors the lower limit sits
   about 0.17 dB under the point. So an unchanged receiver reads about 0.7
   at most, 0.4 dB of room for another toolchain's draws, and a change that
   adds about 0.5 dB fails the spot check. Where the loss goes, stage by
   stage, is validate_async_dsss_receiver_loss (#2495). */
#define LOSS_MAX_DB 1.1

/* A burst that never settles is not scored, so the loss above is
   conditional on settling and cannot see a receiver that stops settling.
   F3 is why some do not: a first hand-off that lands off the carrier and,
   with `reacquire` off, never pulls in. It is a known defect (#2409), and
   where the searching path's pull-in edge lies is that issue's question.
   In the committed sweep every burst settles, handed over at most 329 Hz
   off; before the Dll stopped dividing each period by its own magnitude
   (#2495), seven of
   45 did not, handed over 343 to 492 Hz off.

   So the count is a RATCHET: unsettled_max below, per Es/N0, is the
   committed sweep's count at its fixed seeds plus one for another
   toolchain's draws, and it may only shrink. The cap is a count, not a
   rate, and a point runs until its errors: a receiver whose loss improves
   runs more bursts and can trip it at an unchanged settle rate. That is a
   re-record moment (the sweep, then this table), not a regression.

   And one floor. The pre-despread loop is "reliable to twice" its bound,
   ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (60 Hz here), measured on the seed it is
   handed, which the refine's hand-over is. Inside that the hand-over
   cannot be why a burst did not settle, so an unsettled burst there is
   some other failure, and fails. Past it a burst may settle or not: 13 of
   the sweep's 48 settled bursts were handed over past 120 Hz. */
#define PULLIN_RELIABLE_HZ                                                    \
  (2.0 * ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (CHIP_RATE, SF))

/* The sweep: from the design's Es/N0 floor (5 dB) to where a point still
   costs seconds (8 dB, BER ~7e-4). Above it the certification's EVM rows
   characterise the receiver; below the floor the C test
   `_test_awgn_esn0_floor` pins that it stops decoding. unsettled_max is
   the ratchet above: the sweep's 0, 0, 0 and 0 unsettled, plus one. */
static const double esn0s[]         = { 5.0, 6.0, 7.0, 8.0 };
static const size_t unsettled_max[] = { 1, 1, 1, 1 };
#define N_ESN0 (sizeof esn0s / sizeof esn0s[0])

/* --check: one point of the sweep, by index, where every cross-check of
   dp_ber_report() holds with margin (the blind M2M4 reads the Es/N0 less
   about the implementation loss, as a circular error makes it), at the
   library's error target: about 11 bursts, the sweep's own seeds. */
static const size_t check_points[] = { 2 }; /* 7 dB */

/* One burst's account. */
typedef struct
{
  double        carrier_hz;
  double        t_track_ms; /* the signal's first sample -> first symbol */
  double        handover_err_hz; /* the refine's carrier then, minus truth */
  size_t        n_out;     /* symbols the receiver emitted              */
  size_t        settle;    /* dp_ber_settle()'s window start            */
  size_t        lo, hi;    /* the scored window, receiver symbols       */
  int           settled;   /* a settled window, aligned at its start    */
  int           slipped;   /* the end's alignment is not the start's    */
  long          lag;       /* detected at the start, against `base`     */
  double        lock_frac; /* scored symbols with symbol lock up        */
  unsigned long errors;    /* this burst's errors in the window         */
  unsigned long scored;    /* and the symbols scored                    */
} burst_t;

/* The detected alignment over a marker of `truth` at `t0`. */
static dp_ber_sync_t
align_at (const float complex *rx, size_t n, const uint8_t *truth,
          size_t n_truth, size_t t0, dp_ber_marker_t *mk)
{
  mk->sym    = NULL;
  mk->n      = DP_BER_SYNC_SYMS;
  mk->t0     = t0;
  mk->period = 0;
  mk->reps   = 0;
  return dp_ber_sync (rx, n, truth, n_truth, mk, 2, DP_BER_LAG_SPAN,
                      DP_BER_SYNC_PFA);
}

/* One burst at `esn0_db`: render, receive, settle, align, check the end's
   alignment, and score into `acc`. `last` keeps the alignment of a scored
   burst for dp_ber_report(). */
static int
run_burst (const uint8_t *code, double esn0_db, uint32_t seed, dp_ber_t *acc,
           burst_t *out, dp_ber_sync_t *last)
{
  uint32_t     st     = trial_rng (seed);
  const double cn0    = cn0_of (esn0_db);
  out->carrier_hz     = (2.0 * dp_uni (&st) - 1.0) * SPAN;
  const size_t n_blk  = blocks_for ((size_t)(BURST_S * FS));
  const size_t n_bits = symbol_at (n_blk * TE) + 2;
  uint8_t     *bits   = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn
      = dp_dsss_cont_emitter (code, SF, SPC, FS, out->carrier_hz, CPS, seed,
                              WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g  = make_noise (seed, cn0);
  dp_async_dsss_receiver_state_t *rx = make_rx (code, cn0);
  /* Whether the emitter sends `bits` needs no check of its own here: the
     truth alignment is that check. Sabotaged onto the synth's own PRBS, 37
     of 38 bursts found no alignment, and the 38th, a false one (#2496),
     was caught at its end as slipped, at a rate of 0.50: the point fails
     on the slip and the loss. */
  if (!syn || !g || !rx)
    return 1;

  const size_t   cap  = TE; /* < 1 symbol a code period here (#2469) */
  const size_t   ocap = (size_t)(BURST_S * SYM_RATE) + 64 + cap;
  float complex *sig  = dp_xmalloc (TE * sizeof *sig);
  float complex *blk  = dp_xmalloc (TE * sizeof *blk);
  float complex *syms = dp_xmalloc (ocap * sizeof *syms);
  unsigned char *lock = dp_xmalloc (ocap);
  size_t         n = 0, first_blk = SIZE_MAX;
  out->handover_err_hz = NAN;
  for (size_t b = 0; b < n_blk; b++)
    {
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, blk, TE);
      for (size_t i = 0; i < TE; i++)
        blk[i] += sig[i];
      size_t room = ocap - n;
      size_t k    = dp_async_dsss_receiver_steps (rx, blk, TE, syms + n,
                                                  room < cap ? room : cap);
      /* The symbol lock after the push, for every symbol it emitted. */
      unsigned char up = (unsigned char)dp_async_dsss_receiver_get_locked (rx);
      for (size_t j = 0; j < k; j++)
        lock[n + j] = up;
      if (k && first_blk == SIZE_MAX)
        {
          /* The hand-over: the refine's carrier, frozen once tracking --
             what the carrier loops were seeded with, against the truth. */
          first_blk = b;
          out->handover_err_hz
              = dp_async_dsss_receiver_get_doppler_hz (rx) - out->carrier_hz;
        }
      n += k;
    }
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the burst's data covered its run");

  out->n_out      = n;
  out->t_track_ms = first_blk == SIZE_MAX
                        ? NAN
                        : (double)((first_blk + 1) * TE) / FS * 1e3;
  out->settled = out->slipped = 0;
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

      /* Truth from the bit the first emitted symbol's block began on. */
      const size_t    base    = symbol_at (first_blk * TE);
      const uint8_t  *truth   = bits + base;
      const size_t    n_truth = n_bits - base;
      const size_t    guard   = (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS;
      dp_ber_marker_t mk, mk_end;
      if (ok && out->settle + 2 * guard + 500 < n)
        {
          dp_ber_sync_t sy = align_at (syms, n, truth, n_truth,
                                       out->settle + DP_BER_LAG_SPAN, &mk);
          if (sy.ok)
            {
              dp_ber_sync_t end
                  = align_at (syms, n, truth, n_truth, n - guard, &mk_end);
              out->settled = 1;
              out->lag     = sy.lag;
              out->slipped = !end.ok || end.lag != sy.lag
                             || cos (end.phase - sy.phase) < 0.0;
              long e       = (long)(mk.t0 + mk.n) - sy.lag;
              out->lo      = (e > 0 && (size_t)e > out->settle) ? (size_t)e
                                                                : out->settle;
              out->hi      = n;
              unsigned long e0 = acc->errors, s0 = acc->symbols;
              dp_ber_score (acc, syms, out->lo, out->hi, truth, n_truth, &mk,
                            &sy);
              out->errors = acc->errors - e0;
              out->scored = acc->symbols - s0;
              size_t up   = 0;
              for (size_t i = out->lo; i < out->hi; i++)
                up += lock[i];
              out->lock_frac = (double)up / (double)(out->hi - out->lo);
              *last          = sy;
            }
        }
    }

  free (lock);
  free (syms);
  free (blk);
  free (sig);
  free (bits);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  dp_wfm_synth_destroy (syn);
  return 0;
}

int
main (int argc, char **argv)
{
  int check = 0, emit = 0;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
    }
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);
  /* The points by index into esn0s, so --check reads the sweep's own
     ratchet entry for its point. */
  static const size_t all_points[] = { 0, 1, 2, 3 };
  const size_t       *ix           = check ? check_points : all_points;
  const size_t        n_pts
      = check ? sizeof check_points / sizeof check_points[0] : N_ESN0;

  if (emit)
    printf ("# burst\nesn0_db,seed,carrier_hz,t_track_ms,handover_err_hz,"
            "n_out,settle,lo,hi,settled,slipped,lag,lock_frac,errors,"
            "scored\n");
  else
    printf ("AsyncDsssReceiver BER vs the BPSK bound: Gold-%u, %.3f Mcps, "
            "%.0f sym/s, %.1f s bursts, %lu errors a point\n",
            SF, CHIP_RATE * 1e-6, SYM_RATE, BURST_S, DP_BER_TARGET_ERRORS);

  dp_ber_report_t reps[N_ESN0];
  size_t          n_bursts[N_ESN0], n_unsettled[N_ESN0], n_slipped[N_ESN0];
  size_t          n_inside[N_ESN0];
  for (size_t e = 0; e < n_pts; e++)
    {
      const double  es = esn0s[ix[e]];
      dp_ber_t      acc;
      dp_ber_sync_t last;
      int           any = 0;
      size_t        lo = 0, hi = 0;
      dp_ber_init (&acc, 2, DP_BER_TARGET_ERRORS);
      n_bursts[e] = n_unsettled[e] = n_slipped[e] = n_inside[e] = 0;
      for (uint32_t s = 1; !dp_ber_enough (&acc) && s <= MAX_BURSTS; s++)
        {
          burst_t bu;
          DP_REQUIRE (run_burst (code, es, s, &acc, &bu, &last) == 0);
          n_bursts[e]++;
          n_unsettled[e] += !bu.settled;
          n_slipped[e] += (size_t)bu.slipped;
          /* Unsettled though handed over inside the reliable pull-in -- or
             with no hand-over at all (NaN fails the `>`): not F3. */
          n_inside[e] += !bu.settled
                         && !(fabs (bu.handover_err_hz) > PULLIN_RELIABLE_HZ);
          if (bu.settled)
            {
              any = 1;
              lo  = bu.lo;
              hi  = bu.hi;
            }
          if (emit)
            printf ("%.1f,%u,%.1f,%.2f,%.1f,%zu,%zu,%zu,%zu,%d,%d,%ld,%.4f,"
                    "%lu,%lu\n",
                    es, s, bu.carrier_hz, bu.t_track_ms, bu.handover_err_hz,
                    bu.n_out, bu.settle, bu.lo, bu.hi, bu.settled, bu.slipped,
                    bu.lag, bu.lock_frac, bu.errors, bu.scored);
          else
            printf ("  %4.1f dB seed %3u: carrier %+7.1f Hz, track %7.1f ms, "
                    "hand-over %+6.1f Hz off, settle %5zu, scored %5lu, "
                    "errors %4lu%s%s\n",
                    es, s, bu.carrier_hz, bu.t_track_ms, bu.handover_err_hz,
                    bu.settle, bu.scored, bu.errors,
                    bu.settled ? "" : "  UNSETTLED",
                    bu.slipped ? "  SLIPPED" : "");
        }
      reps[e] = dp_ber_report (&acc, es, any ? &last : NULL, lo, hi, any,
                               DP_BER_CONF);
      dp_ber_free (&acc);
      if (!emit)
        {
          char label[32];
          snprintf (label, sizeof label, "BPSK @%.1f dB", es);
          dp_ber_print (label, &reps[e]);
        }

      /* The same assertions in --check and the sweep, at every point. The
         loss is the one the interval's LOWER limit implies; !(<=), so a
         NaN (no errors, or a rate outside the bracket) fails. */
      const dp_ber_report_t *r = &reps[e];
      const double loss_lo     = es - dp_ber_esn0_db_for_ser (2, r->ser.lo);
      if (!r->ok)
        fprintf (stderr, "  %.1f dB: %s\n", es, r->why);
      DP_CHECK_MSG (r->ok, "the error rate is a result: settled, aligned, "
                           "enough errors, and its cross-checks agree");
      DP_CHECK_MSG (n_slipped[e] == 0, "no burst's alignment moved between "
                                       "its start and its end");
      DP_CHECK_MSG (loss_lo <= LOSS_MAX_DB,
                    "the error rate over the settled bursts is within "
                    "LOSS_MAX_DB of the BPSK bound (the 99% interval's "
                    "lower limit)");
      DP_CHECK_MSG (n_unsettled[e] <= unsettled_max[ix[e]],
                    "no more bursts fail to settle than the ratchet allows "
                    "(F3, #2409)");
      DP_CHECK_MSG (n_inside[e] == 0,
                    "no burst handed over inside the carrier loop's reliable "
                    "pull-in fails to settle");
    }

  if (emit)
    {
      /* evm_ideal_db: what the EVM meter (dp_ber_evm_db) reads on an ideal
         stream at this Es/N0, so the EVM's own loss is evm_db minus it.
         unsettled_max: the ratchet; unsettled_inside: unsettled bursts
         handed over inside pullin_hz, the reliable pull-in (must be 0). */
      printf ("# ber\nesn0_db,bursts,unsettled,unsettled_max,"
              "unsettled_inside,pullin_hz,slipped,symbols,errors,ber,ber_lo,"
              "ber_hi,conf,theory,loss_db,loss_lo_db,loss_hi_db,loss_max_db,"
              "evm_db,evm_ideal_db,m2m4_db,m2m4_agree_db,ok\n");
      for (size_t e = 0; e < n_pts; e++)
        {
          const dp_ber_report_t *r  = &reps[e];
          const double           es = esn0s[ix[e]];
          printf ("%.1f,%zu,%zu,%zu,%zu,%.1f,%zu,%lu,%lu,%.4e,%.4e,%.4e,"
                  "%.2f,%.4e,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%.1f,%d\n",
                  es, n_bursts[e], n_unsettled[e], unsettled_max[ix[e]],
                  n_inside[e], PULLIN_RELIABLE_HZ, n_slipped[e],
                  r->ber.symbols, r->ber.errors, r->ber.p_hat, r->ber.lo,
                  r->ber.hi, r->ber.conf, r->theory_ber, r->loss_db,
                  es - dp_ber_esn0_db_for_ser (2, r->ser.lo),
                  es - dp_ber_esn0_db_for_ser (2, r->ser.hi), LOSS_MAX_DB,
                  r->evm_db, dp_ber_evm_ideal_db (es), r->m2m4_db,
                  DP_BER_M2M4_AGREE_DB, r->ok);
        }
    }
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_ber");
  DP_TEST_END ("validate_async_dsss_receiver_ber");
}
