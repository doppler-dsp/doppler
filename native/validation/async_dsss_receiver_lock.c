/**
 * @file async_dsss_receiver_lock.c
 * @brief Symbol lock through a signal's life, at the parallel receivers'
 *        operating point: from a signal's arrival to tracking and to symbol
 *        lock, and symbol lock through a dropout and after a loss.
 *
 * The parallel-receivers design (docs/design/async-dsss-parallel.md) makes
 * symbol lock the receiver's own rule: without it a receiver searches, with
 * it it tracks, and losing it sends the receiver back to search. Two things
 * that rule rests on are measured here, at that design's operating point:
 * how long a new signal takes to be tracked and then locked (its §5), and
 * how symbol lock behaves through a dropout and after a loss (its §3).
 * The record (docs/design/async-dsss-parallel-measurements.md, §3.1 and
 * §5.1) renders this harness's `--emit` output.
 *
 * Operating point. A 1023-chip Gold code (CCSDS #365) at 3.069 Mcps, two
 * samples a chip, so one code epoch -- the feed block -- is 2,046 samples,
 * 333.3 us, and 1/T_E is 3 kHz; asynchronous BPSK data at 2,700 sym/s. The
 * receiver is the searching flavor at the binding's defaults (pfa 1e-3, the
 * 0.5 dB look-back, the refine's 64-point FFT), its own search over
 * +/-1/T_E and its design C/N0 the true one. Es/N0 10 dB and 5 dB, the
 * design's floor (C/N0 44.3 and 39.3 dB-Hz).
 *
 * Stimulus. The emitter is the shipped continuous-DSSS synth
 * (dp_dsss_cont_emitter), rendered clean block by block, and the noise the
 * shipped awgn generator, sized by dp_awgn_amplitude_for_snr() from the
 * C/N0: an arrival or a dropout is an edit to the signal block before the
 * noise is added, so the noise floor never moves. The data is the
 * harness's own bits (dp_bit()), one per symbol, so where the data
 * transitions is KNOWN: every measurement is taken mid-data, never in a
 * data-free stretch, and each row carries the fraction of its symbol
 * boundaries that flip around its event, counted from the bits themselves
 * (about one half). Nothing here builds a chip, a bit or a sigma by hand.
 *
 * ARRIVAL. A trial draws the carrier uniformly over +/-1/T_E, the arrival
 * uniformly over 0 to ARRIVAL_DELAY_S of noise (so the signal starts at any
 * sample of a feed block and any code phase against it), and its own data
 * and noise; the receiver is fed ARRIVAL_S past the arrival. Recorded,
 * from the arrival sample, to one block: when the receiver first reads
 * tracking; the refine stage's carrier there (get_doppler_hz(), frozen once
 * tracking) minus the truth -- the residual the carrier loop must pull in,
 * which `refine_bias.c` measures on its own and doppler#982 found handed
 * over outside the loop's bound; when symbol lock first comes; and whether
 * it is up at the end.
 *
 * DROPOUT. A trial draws the carrier uniformly over +/-1/T_E, as ARRIVAL
 * does (a fixed 1,500 Hz, the first sweep's, sits on the edge between two
 * of the search's 3 kHz tiles), and its own data and noise. It is fed
 * until it tracks with symbol lock up for SETTLE_BLOCKS blocks in a row,
 * then a further uniform 0 to JITTER_S, then cut at a uniform sample of
 * the next block; a draw that has not held lock that long within
 * MAX_LOCK_S, or whose lock is down at the cut, is a row marked unsettled
 * (not every draw locks: ARRIVAL measures which). The receiver's state at
 * the cut is saved and the next POST_S of signal and noise rendered once,
 * so every case below resumes from the same state on the same noise and
 * differs only in the dropout (the SIGNAL is cut; the noise runs on). That
 * rests on the restore into the already-run receiver being exact, so it is
 * checked: until its signal returns, every case's lock must read the
 * for-good case's, block for block, their input being the same.
 *
 *   for good       when symbol lock falls after the cut (or `-1`, held
 *                  through the whole blocks of DROP_WATCH_S)
 *   1, 2, ... epochs  whether lock stayed up through a dropout of that
 *                  length and HOLD_WATCH_S after it, every length up to
 *                  two past the drop (past the drop it falls inside the
 *                  dropout, by construction). The profile is emitted
 *                  whole, because it is NOT monotonic in the length: a
 *                  lock that rides out 8 ms can fall after 7, the slow
 *                  lock average (1/30 a symbol) sinking under its
 *                  threshold in the dropout and climbing back too late
 *                  after it. So the record reports, per length, the
 *                  fraction of trials that held; `hold_ms` is only the
 *                  longest held before the first that was not, and
 *                  `non_monotonic` says a longer one held anyway
 *   RELOCK_MS      whether lock fell in a dropout of that length, and when
 *                  it is back after the signal returned (`-1` if not
 *                  within RELOCK_WATCH_S); the receiver stays tracking
 *                  throughout (lost_confirm_s = 0)
 *
 * What is asserted, the same in the sweep and in --check: the emitter
 * sends the harness's bits (BITS mode, every one of them) and each run's
 * bits covered it; every row is mid-data (its flips within
 * FLIPS_MIN..FLIPS_MAX); the restore is exact (above); at 10 dB a signal
 * is tracked within CHECK_TRACK_MS of arriving; in every settled dropout
 * trial a loss for good drops symbol lock within DROP_WATCH_S; enough
 * dropout draws settle to measure; and each lifecycle story reaches symbol
 * lock exactly when its `locks` says. The rest -- lock times, seed errors,
 * holds, re-locks, the 5 dB
 * arrival and dropout rows -- is measured and printed, not asserted: a
 * fraction that moves with the draw is a finding for the record, not a
 * gate.
 *
 * The asserted bounds must hold on every toolchain (CI's image is one of
 * them), so none sits on an edge one draw can cross; a toolchain that
 * crosses one is a finding for the record, not a bound to widen here.
 *
 * A hold of LOCK_N_DOWN symbols
 * is NOT a property of the detector, though it reads like one: the first
 * sweep asserted it and four 5 dB trials failed it, the average sunk in
 * the dropout and its recovery after supplying the run of low symbols.
 *
 * Usage:
 *   validate_async_dsss_receiver_lock            the sweep, 10 and 5 dB
 *   validate_async_dsss_receiver_lock --check    10 dB, three arrivals and
 *                                                one dropout trial
 *   ... --emit                                   CSV blocks on stdout for
 *                                                the record: `# arrival`
 *                                                and `# dropout`, one row
 *                                                a trial
 *   ... --telemetry DIR                          the lifecycle stories:
 *                                                DIR/<story>.tlm, one
 *                                                receiver's own probes
 *                                                through arrival, three
 *                                                fades and departure
 *   ... --telemetry DIR --check                  the same, checked, and
 *                                                removed once they pass
 *                                                (the ctest form)
 *   ... --telemetry DIR --emit                   the same, with a `# story`
 *                                                CSV block on stdout, one
 *                                                row a story, in the
 *                                                arrival row's formats
 */
#include "async_dsss_receiver_common.h" /* SF..EPOCH_MS, make_rx, ... */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_tlm_capture/dp_tlm_capture_core.h"
#include "doppler/timing/timing_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRIVAL_S 2.0        /* each arrival trial's run              */
#define ARRIVAL_DELAY_S 0.02 /* noise alone for up to this, first     */
#define FLIPS_SYMS 270u      /* 100 ms of symbols each side of an event */
#define FLIPS_MIN 0.35       /* mid-data: random bits flip at about    */
#define FLIPS_MAX 0.65       /* half their boundaries                  */
#define CHECK_TRACK_MS 100.0 /* 10 dB: tracked within this of arrival */

#define SETTLE_BLOCKS 200u /* symbol lock up this long, then the cut */
#define MAX_LOCK_S 4.0     /* give up on a draw that never settles   */
#define JITTER_S 0.01      /* then up to this more, then a random sample */
#define POST_S 0.6         /* rendered after the cut, for every case */
#define DROP_WATCH_S 0.08  /* a loss for good must drop lock by this */
#define HOLD_WATCH_S 0.06  /* a held dropout: lock up this long after */
#define RELOCK_WATCH_S 0.3 /* a re-lock is waited for this long      */
#define MAX_HOLD 400u      /* the hold scan's longest dropout, epochs */

#define QUALITY_SETTLE_S 0.2 /* after symbol lock first comes up     */
#define QUALITY_S 1.0        /* the measured window: 2,700 symbols   */
#define QUALITY_TRIALS 5u    /* a sweep's draws per Es/N0            */
#define FALSE_S 2.0          /* noise fed to a receiver seeded on it */

/* The EVM allowance, at each Es/N0, as a loss over the meter's ideal
   (dp_ber_evm_ideal_db()). A locked receiver's error is the ideal's scaled
   by its implementation loss, plus a floor: EVM^2 = k * ideal + floor in
   linear power, so the loss it shows is 10*log10(k + floor/ideal) -- about
   1 dB from k alone at 10 dB, over 2 dB at 20, where the floor joins it.
   The loss is held to a BAND between two such lines, fitted to the full
   sweep's asserted draws (10, 15, 20 dB, five each):
     - the upper, k 0.83 dB with a floor of -23.0 dB, through the worst
       draws at 10 and 20 dB;
     - the lower, k 0.78 dB with a floor of -27.3 dB, under the least
       draws at 10 and 15 dB;
   each widened by EVM_MARGIN_DB, the estimator's own counting noise: over
   QUALITY_S of symbols (2,700) one sigma of an EVM is 4.34/sqrt(2700) =
   0.08 dB, and 0.3 dB is past 3.5 sigma. The fit is the sweep at the
   commit that divides the Dll's output by a slowly tracked level
   (doppler#2495), on
   swiftgo-ai9465 (AMD Ryzen AI 9 465, gcc 14.2.0): bands of +0.55 to
   +1.32 dB at 10 dB, +0.69 to +1.68 at 15 and +1.10 to +2.65 at 20.

   Before that, the model was the floor alone, EVM^2 = ideal + floor (k = 1,
   a floor of -24.4 dB), bounded below only by -EVM_MARGIN_DB. That fitted
   a metric the Dll's per-period output normalization flattered by about
   1 dB: it compressed the in-phase error, so the EVM read near the ideal
   while the error rate sat 0.9 dB from the bound. The lower line is what
   sees that defect come back: under the normalization as it shipped, the
   spot check's draw read +0.28 dB at 10 dB and +0.00 at 15, both under the
   band, and fails (restored as a sabotage, it does). At 20 dB the floor
   dominates and its +1.27 is inside the band, so 20 dB alone could not
   tell it. An EVM under the band is
   never a better receiver by itself: either the meter flatters or the
   receiver really improved, and the latter is a re-fit from a new sweep,
   the same as a raise. */
#define EVM_K_HI_DB 0.830
#define EVM_FLOOR_HI_DB -22.98
#define EVM_K_LO_DB 0.779
#define EVM_FLOOR_LO_DB -27.34
#define EVM_MARGIN_DB 0.3

/* 10*log10(k + floor/ideal) at this Es/N0, k and floor in dB. */
static double
evm_loss_line_db (double esn0_db, double k_db, double floor_db)
{
  return 10.0
         * log10 (
             pow (10.0, k_db / 10.0)
             + pow (10.0, (floor_db - dp_ber_evm_ideal_db (esn0_db)) / 10.0));
}

/* The most loss a locked receiver may show at this Es/N0. */
static double
evm_loss_max_db (double esn0_db)
{
  return evm_loss_line_db (esn0_db, EVM_K_HI_DB, EVM_FLOOR_HI_DB)
         + EVM_MARGIN_DB;
}

/* The least: its implementation loss is not hidden. */
static double
evm_loss_min_db (double esn0_db)
{
  return evm_loss_line_db (esn0_db, EVM_K_LO_DB, EVM_FLOOR_LO_DB)
         - EVM_MARGIN_DB;
}
#define FALSE_TRIALS 5u

static const double quality_esn0s[] = { 20.0, 15.0, 10.0, 7.0, 5.0 };
#define N_QUALITY_ESN0 (sizeof quality_esn0s / sizeof quality_esn0s[0])

static const double relock_ms[] = { 50.0, 200.0 };
#define N_RELOCK (sizeof relock_ms / sizeof relock_ms[0])

static const double esn0s[] = { 10.0, 5.0 };
#define N_ESN0 (sizeof esn0s / sizeof esn0s[0])
#define SWEEP_TRIALS 20u   /* arrival trials, and settled dropout trials */
#define SWEEP_ATTEMPTS 40u /* dropout draws tried for them, at most    */

/* Require that the emitter really sends the harness's bits: a synth left
   on its own PRBS would make the flip count, read off `bits`, describe a
   stream nobody sent. */
static int
sends_bits (const dp_wfm_synth_state_t *syn, size_t n_bits)
{
  return syn->data_mode == WFM_DSSS_DATA_BITS && syn->n_bits == n_bits;
}

/* Of the `FLIPS_SYMS` symbol boundaries either side of symbol `k` (or from
   `k` on, `after`), the fraction where the data flips. */
static double
flips_near (const uint8_t *bits, size_t n_bits, size_t k, int after)
{
  size_t lo = after ? k + 1 : (k > FLIPS_SYMS ? k - FLIPS_SYMS + 1 : 1);
  size_t hi = k + FLIPS_SYMS;
  if (hi >= n_bits)
    hi = n_bits - 1;
  size_t flips = 0, n = 0;
  for (size_t i = lo; i <= hi; i++, n++)
    flips += bits[i] != bits[i - 1];
  return n ? (double)flips / (double)n : 0.0;
}

typedef struct
{
  double carrier_hz;
  double delay_s;
  double t_track_ms;  /* arrival -> first block tracking; NAN: never   */
  double seed_err_hz; /* the refine's carrier at tracking, minus truth */
  double t_lock_ms;   /* arrival -> symbol lock first up; NAN: never   */
  int    locked_end;  /* symbol lock up ARRIVAL_S after the arrival    */
  double flips;       /* data flips over the first 100 ms of signal    */
} arrival_t;

static int
run_arrival (const uint8_t *code, double esn0_db, uint32_t seed,
             arrival_t *out)
{
  uint32_t     st     = trial_rng (seed);
  const double cn0    = cn0_of (esn0_db);
  out->carrier_hz     = (2.0 * dp_uni (&st) - 1.0) * SPAN;
  const size_t delay  = (size_t)(dp_uni (&st) * ARRIVAL_DELAY_S * FS);
  out->delay_s        = (double)delay / FS;
  const size_t blocks = blocks_for (delay + (size_t)(ARRIVAL_S * FS));
  const size_t n_bits = symbol_at (blocks * TE) + 2;
  uint8_t     *bits   = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn
      = dp_dsss_cont_emitter (code, SF, SPC, FS, out->carrier_hz, CPS, seed,
                              WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g  = make_noise (seed, cn0);
  dp_async_dsss_receiver_state_t *rx = make_rx (code, cn0);
  if (!syn || !g || !rx)
    return 1;
  DP_REQUIRE_MSG (sends_bits (syn, n_bits),
                  "the arrival's emitter sends the harness's bits");
  float complex *sig  = dp_xmalloc (TE * sizeof *sig);
  float complex *blk  = dp_xmalloc (TE * sizeof *blk);
  size_t         cap  = dp_async_dsss_receiver_steps_max_out (rx);
  float complex *syms = dp_xmalloc ((cap ? cap : TE) * sizeof *syms);

  out->t_track_ms = out->seed_err_hz = out->t_lock_ms = NAN;
  for (size_t b = 0; b < blocks; b++)
    {
      /* Signal for this block, gated off before the arrival, then the
         noise: the emitter renders from sample 0 either way, so its code
         and data run on the same clock as every other trial's. */
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, blk, TE);
      for (size_t i = 0; i < TE; i++)
        if (b * TE + i >= delay)
          blk[i] += sig[i];
      (void)dp_async_dsss_receiver_steps (rx, blk, TE, syms, cap ? cap : TE);
      const double t_ms = ((double)((b + 1) * TE) - (double)delay) / FS * 1e3;
      if (isnan (out->t_track_ms) && dp_async_dsss_receiver_get_tracking (rx))
        {
          out->t_track_ms = t_ms;
          out->seed_err_hz
              = dp_async_dsss_receiver_get_doppler_hz (rx) - out->carrier_hz;
        }
      if (isnan (out->t_lock_ms) && dp_async_dsss_receiver_get_locked (rx))
        out->t_lock_ms = t_ms;
    }
  /* The bits are sent once (doppler#1718): had they run dry, the tail of
     the run would have measured silence. */
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the arrival's data covered its run");
  out->locked_end = dp_async_dsss_receiver_get_locked (rx);
  out->flips      = flips_near (bits, n_bits, symbol_at (delay), 1);

  free (syms);
  free (blk);
  free (sig);
  free (bits);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  dp_wfm_synth_destroy (syn);
  return 0;
}

typedef struct
{
  int     settled;           /* tracking, symbol lock held, before MAX  */
  double  carrier_hz;        /* the trial's carrier, over +/-1/T_E      */
  double  cut_s;             /* the cut, from the emitter's first sample */
  double  flips;             /* data flips within 100 ms of the cut      */
  double  drop_ms;           /* cut -> lock down, gone for good; -1 held */
  size_t  n_held;            /* dropouts scanned: 1 .. n_held epochs     */
  uint8_t held[MAX_HOLD];    /* held[j]: a (j+1)-epoch dropout held      */
  size_t  hold_epochs;       /* the longest held before the first not    */
  int     non_monotonic;     /* a longer one held after that             */
  int     fell[N_RELOCK];    /* lock fell inside the RELOCK_MS dropout   */
  double  back_ms[N_RELOCK]; /* signal back -> lock back; -1: not      */
} dropout_t;

/* Resume `rx` from `blob` and feed the rendered post-cut stream with the
   signal cut for `fade` samples from sample `cut` of it (SIZE_MAX: for
   good), `m` blocks; symbol lock after each into `locked`. Non-zero when
   the blob is refused. */
static int
branch (dp_async_dsss_receiver_state_t *rx, const void *blob,
        const float complex *sigp, const float complex *noip, size_t cut,
        size_t fade, size_t m, float complex *blk, float complex *syms,
        size_t cap, uint8_t *locked)
{
  if (dp_async_dsss_receiver_set_state (rx, blob) != DP_OK)
    return 1;
  for (size_t b = 0; b < m; b++)
    {
      for (size_t i = 0; i < TE; i++)
        {
          size_t s   = b * TE + i;
          int    off = s >= cut && (fade == SIZE_MAX || s - cut < fade);
          blk[i]     = noip[s] + (off ? 0.0f : 1.0f) * sigp[s];
        }
      (void)dp_async_dsss_receiver_steps (rx, blk, TE, syms, cap);
      locked[b] = (uint8_t)dp_async_dsss_receiver_get_locked (rx);
    }
  return 0;
}

/* Whether `lk` reads `gone` over every block that ends before the signal
   returns (the first `before` blocks, at most the `n_gone` recorded). */
static int
same_until (const uint8_t *lk, const uint8_t *gone, size_t n_gone,
            size_t before)
{
  for (size_t i = 0; i < before && i < n_gone; i++)
    if (lk[i] != gone[i])
      return 0;
  return 1;
}

static int
run_dropout (const uint8_t *code, double esn0_db, uint32_t seed,
             dropout_t *out)
{
  memset (out, 0, sizeof *out);
  out->drop_ms          = -1.0;
  uint32_t     st       = trial_rng (seed + 100000u);
  const double cn0      = cn0_of (esn0_db);
  const size_t lock_b   = (size_t)(MAX_LOCK_S * FS / (double)TE);
  out->carrier_hz       = (2.0 * dp_uni (&st) - 1.0) * SPAN;
  const size_t jitter_b = (size_t)(dp_uni (&st) * JITTER_S * FS / TE);
  const size_t at       = (size_t)(dp_uni (&st) * (double)TE);
  const size_t post_n   = (size_t)(POST_S * FS / (double)TE) * TE;
  const size_t max_n    = (lock_b + jitter_b + 2) * TE + post_n;
  const size_t n_bits   = symbol_at (max_n) + 2;
  uint8_t     *bits     = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn = dp_dsss_cont_emitter (
      code, SF, SPC, FS, out->carrier_hz, CPS, seed + 100000u,
      WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g  = make_noise (seed + 100000u, cn0);
  dp_async_dsss_receiver_state_t *rx = make_rx (code, cn0);
  if (!syn || !g || !rx)
    return 1;
  DP_REQUIRE_MSG (sends_bits (syn, n_bits),
                  "the dropout's emitter sends the harness's bits");
  float complex *sig  = dp_xmalloc (TE * sizeof *sig);
  float complex *blk  = dp_xmalloc (TE * sizeof *blk);
  size_t         cap  = dp_async_dsss_receiver_steps_max_out (rx);
  float complex *syms = dp_xmalloc ((cap ? cap : TE) * sizeof *syms);
  cap                 = cap ? cap : TE;

  /* Settle: tracking with symbol lock up SETTLE_BLOCKS in a row, then
     the jitter, then the cut at sample `at` of the next block. */
  size_t fed = 0, run = 0, extra = 0;
  int    settled = 0;
  for (size_t b = 0; b < lock_b + jitter_b; b++)
    {
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, blk, TE);
      for (size_t i = 0; i < TE; i++)
        blk[i] += sig[i];
      (void)dp_async_dsss_receiver_steps (rx, blk, TE, syms, cap);
      fed++;
      int up = dp_async_dsss_receiver_get_tracking (rx)
               && dp_async_dsss_receiver_get_locked (rx);
      run    = up ? run + 1 : 0;
      if (!settled && run >= SETTLE_BLOCKS)
        settled = 1;
      if (settled && extra++ >= jitter_b)
        break;
      if (!settled && b + 1 >= lock_b)
        break;
    }
  out->settled = settled && dp_async_dsss_receiver_get_locked (rx);
  out->cut_s   = (double)(fed * TE + at) / FS;
  out->flips   = flips_near (bits, n_bits, symbol_at (fed * TE + at), 0);
  if (!out->settled)
    goto done;

  {
    /* The state at the cut, and the stream after it, rendered once. */
    size_t sz   = dp_async_dsss_receiver_state_bytes (rx);
    void  *blob = dp_xmalloc (sz);
    dp_async_dsss_receiver_get_state (rx, blob);
    float complex *sigp = dp_xmalloc (post_n * sizeof *sigp);
    float complex *noip = dp_xmalloc (post_n * sizeof *noip);
    dp_wfm_synth_steps (syn, sigp, post_n);
    DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                  "the dropout's data covered the stream after the cut");
    dp_awgn_generate (g, post_n, noip, post_n);
    uint8_t *lk   = dp_xmalloc (post_n / TE);
    uint8_t *gone = dp_xmalloc (post_n / TE);

    /* Gone for good, over the whole blocks inside DROP_WATCH_S, so a drop
       is never read past the watch. */
    const size_t m_gone = (at + (size_t)(DROP_WATCH_S * FS)) / TE;
    DP_REQUIRE (branch (rx, blob, sigp, noip, at, SIZE_MAX, m_gone, blk, syms,
                        cap, gone)
                == 0);
    size_t m = m_gone;
    for (size_t i = 0; i < m; i++)
      if (!gone[i])
        {
          out->drop_ms = ((double)((i + 1) * TE) - (double)at) / FS * 1e3;
          break;
        }

    /* Every dropout of 1, 2, ... epochs, to two past the drop: past it
       the lock falls inside the dropout by construction. */
    size_t longest
        = (out->drop_ms >= 0.0 ? (size_t)ceil (out->drop_ms / EPOCH_MS)
                               : (size_t)ceil (DROP_WATCH_S * 1e3 / EPOCH_MS))
          + 2;
    if (longest > MAX_HOLD)
      longest = MAX_HOLD;
    const size_t hold_watch = (size_t)(HOLD_WATCH_S * FS);
    int          failed     = 0;
    /* The restore check: until the signal returns, a case's input is the
       for-good case's, so its lock must read the same, block for block. */
    int exact = 1;
    for (size_t j = 1; j <= longest; j++)
      {
        size_t fade = j * TE;
        m           = blocks_for (at + fade + hold_watch);
        DP_REQUIRE (
            branch (rx, blob, sigp, noip, at, fade, m, blk, syms, cap, lk)
            == 0);
        exact &= same_until (lk, gone, m_gone, (at + fade) / TE);
        uint8_t h = 1;
        for (size_t i = 0; i < m; i++)
          h &= lk[i];
        out->held[j - 1] = h;
        if (!h && !failed)
          {
            failed           = 1;
            out->hold_epochs = j - 1;
          }
        else if (h && failed)
          out->non_monotonic = 1;
      }
    out->n_held = longest;
    if (!failed)
      out->hold_epochs = longest;

    /* The re-lock after each RELOCK_MS dropout. */
    for (size_t r = 0; r < N_RELOCK; r++)
      {
        size_t fade = (size_t)(relock_ms[r] * 1e-3 * FS);
        size_t back = at + fade;
        m           = blocks_for (back + (size_t)(RELOCK_WATCH_S * FS));
        DP_REQUIRE (
            branch (rx, blob, sigp, noip, at, fade, m, blk, syms, cap, lk)
            == 0);
        exact &= same_until (lk, gone, m_gone, back / TE);
        size_t first_after = blocks_for (back); /* first block past it */
        out->back_ms[r]    = -1.0;
        for (size_t i = 0; i < first_after && i < m; i++)
          out->fell[r] |= !lk[i];
        for (size_t i = first_after; i < m; i++)
          if (lk[i])
            {
              out->back_ms[r]
                  = ((double)((i + 1) * TE) - (double)back) / FS * 1e3;
              break;
            }
      }
    DP_CHECK_MSG (exact, "every case's lock reads the for-good case's until "
                         "its signal returns: the restore is exact");
    free (gone);
    free (lk);
    free (noip);
    free (sigp);
    free (blob);
  }

done:
  free (syms);
  free (blk);
  free (sig);
  free (bits);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  dp_wfm_synth_destroy (syn);
  return 0;
}

/* QUALITY. A trial draws the carrier over +/-1/T_E and its own data and
   noise, as ARRIVAL does, with the signal on from sample 0. It is fed
   until symbol lock first comes up, QUALITY_SETTLE_S more, and then every
   symbol steps() returns over QUALITY_S is kept -- all of them, never
   gated on the flag, because the receiver never gates its output. Scored
   by the shipped estimators: EVM over the WHOLE window against the
   stream's own hard BPSK decisions (dp_test_evm_db_hard_range), its loss
   over what that meter reads on an ideal stream (dp_ber_evm_ideal_db --
   the meter scales to unit total power, so the ideal is not -Es/N0), and
   the fraction of the window's blocks with symbol lock up. A draw that has no
   lock within MAX_LOCK_S is a row marked unsettled. */
typedef struct
{
  int    settled;
  double carrier_hz;
  double t_lock_ms; /* signal start -> symbol lock first up          */
  size_t n_sym;     /* symbols scored                                 */
  double evm_db;    /* EVM against the stream's own hard decisions    */
  double loss_db;   /* evm_db minus dp_ber_evm_ideal_db(Es/N0)        */
  double lock_frac; /* the window's blocks with symbol lock up        */
} quality_t;

static int
run_quality (const uint8_t *code, double esn0_db, uint32_t seed,
             quality_t *out)
{
  uint32_t     st     = trial_rng (seed);
  const double cn0    = cn0_of (esn0_db);
  out->carrier_hz     = (2.0 * dp_uni (&st) - 1.0) * SPAN;
  const size_t max_b  = blocks_for ((size_t)(MAX_LOCK_S * FS));
  const size_t win_b  = blocks_for ((size_t)(QUALITY_S * FS));
  const size_t set_b  = blocks_for ((size_t)(QUALITY_SETTLE_S * FS));
  const size_t n_bits = symbol_at ((max_b + set_b + win_b) * TE) + 2;
  uint8_t     *bits   = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn
      = dp_dsss_cont_emitter (code, SF, SPC, FS, out->carrier_hz, CPS, seed,
                              WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g  = make_noise (seed, cn0);
  dp_async_dsss_receiver_state_t *rx = make_rx (code, cn0);
  if (!syn || !g || !rx)
    return 1;
  DP_REQUIRE_MSG (sends_bits (syn, n_bits),
                  "the quality trial's emitter sends the harness's bits");
  float complex *sig  = dp_xmalloc (TE * sizeof *sig);
  float complex *blk  = dp_xmalloc (TE * sizeof *blk);
  const size_t   cap  = TE; /* < 1 symbol a code period here (#2469) */
  const size_t   wcap = (size_t)(QUALITY_S * SYM_RATE) + 64 + cap;
  float complex *syms = dp_xmalloc (wcap * sizeof *syms);
  float complex *junk = dp_xmalloc (cap * sizeof *junk);

  out->settled   = 0;
  out->t_lock_ms = out->evm_db = out->loss_db = out->lock_frac = NAN;
  out->n_sym                                                   = 0;
  size_t start = 0, locked_blocks = 0;
  for (size_t b = 0; b < max_b + set_b + win_b; b++)
    {
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, blk, TE);
      for (size_t i = 0; i < TE; i++)
        blk[i] += sig[i];
      int    in_win = start && b >= start;
      size_t room   = wcap - out->n_sym;
      /* Before the window the symbols are not scored: a scratch buffer
         takes them (never the input, which steps() is still reading). */
      size_t k = dp_async_dsss_receiver_steps (
          rx, blk, TE, in_win ? syms + out->n_sym : junk,
          in_win ? (room < cap ? room : cap) : cap);
      if (in_win)
        {
          out->n_sym += k;
          locked_blocks += (size_t)dp_async_dsss_receiver_get_locked (rx);
          if (b + 1 == start + win_b)
            break;
        }
      if (isnan (out->t_lock_ms) && dp_async_dsss_receiver_get_locked (rx))
        {
          out->t_lock_ms = (double)((b + 1) * TE) / FS * 1e3;
          start          = b + 1 + set_b;
        }
      if (!start && b + 1 >= max_b)
        break;
    }
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the quality trial's data covered its run");
  if (start && out->n_sym > 0)
    {
      out->settled   = 1;
      out->evm_db    = dp_test_evm_db_hard_range (syms, 0, out->n_sym, 2);
      out->loss_db   = out->evm_db - dp_ber_evm_ideal_db (esn0_db);
      out->lock_frac = (double)locked_blocks / (double)win_b;
    }
  free (junk);
  free (syms);
  free (blk);
  free (sig);
  free (bits);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  dp_wfm_synth_destroy (syn);
  return 0;
}

/* FALSE LOCK. The lock flag's false-alarm side: a receiver seeded onto
   NOISE ALONE (seed() takes what a hit would hand over, here a false
   alarm's), sized for the Es/N0 it would have expected, is fed FALSE_S
   and never released (lost_confirm_s 0), so its loops run on noise the
   whole time. Recorded: the fraction of blocks with symbol lock up, and
   how many times it was declared. */
typedef struct
{
  size_t blocks;    /* blocks fed                                    */
  size_t up;        /* of them, symbol lock up                       */
  double frac;      /* up / blocks                                   */
  size_t declares;  /* rising edges of the flag                       */
  double code_frac; /* blocks with code lock up, for comparison       */
} falselock_t;

static int
run_falselock (const uint8_t *code, double esn0_db, uint32_t seed,
               falselock_t *out)
{
  const double                    cn0 = cn0_of (esn0_db);
  dp_awgn_state_t                *g   = make_noise (seed + 1000u, cn0);
  dp_async_dsss_receiver_state_t *rx  = make_rx (code, cn0);
  if (!g || !rx)
    return 1;
  DP_REQUIRE (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  float complex *blk  = dp_xmalloc (TE * sizeof *blk);
  float complex *syms = dp_xmalloc (TE * sizeof *syms);
  const size_t   nb   = blocks_for ((size_t)(FALSE_S * FS));
  size_t         up = 0, cup = 0, tracked = 0;
  int            was = 0;
  out->declares      = 0;
  for (size_t b = 0; b < nb; b++)
    {
      dp_awgn_generate (g, TE, blk, TE);
      (void)dp_async_dsss_receiver_steps (rx, blk, TE, syms, TE);
      int l = dp_async_dsss_receiver_get_locked (rx);
      up += (size_t)l;
      cup += (size_t)dp_async_dsss_receiver_get_code_locked (rx);
      tracked += (size_t)dp_async_dsss_receiver_get_tracking (rx);
      out->declares += (size_t)(l && !was);
      was = l;
    }
  /* Precondition: it held a track on the noise, or the flag never had a
     chance to come up. */
  DP_CHECK_MSG (tracked > nb / 2, "the false-lock receiver tracked noise");
  out->blocks    = nb;
  out->up        = up;
  out->frac      = (double)up / (double)nb;
  out->code_frac = (double)cup / (double)nb;
  free (syms);
  free (blk);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  return 0;
}

/* The assertions the sweep and --check share. */
static void
check_arrival (double esn0_db, const arrival_t *a)
{
  DP_CHECK_MSG (a->flips >= FLIPS_MIN && a->flips <= FLIPS_MAX,
                "an arrival is measured mid-data, never data-free");
  if (esn0_db >= 10.0)
    DP_CHECK_MSG (!isnan (a->t_track_ms) && a->t_track_ms > 0.0
                      && a->t_track_ms <= CHECK_TRACK_MS,
                  "at 10 dB a signal is tracked within CHECK_TRACK_MS");
}

static void
check_dropout (const dropout_t *d)
{
  DP_CHECK_MSG (d->flips >= FLIPS_MIN && d->flips <= FLIPS_MAX,
                "a dropout is cut mid-data, never data-free");
  if (!d->settled)
    return;
  DP_CHECK_MSG (d->drop_ms > 0.0 && d->drop_ms <= DROP_WATCH_S * 1e3,
                "a loss for good drops symbol lock within DROP_WATCH_S");
}

static void
print_arrival (double esn0_db, uint32_t seed, const arrival_t *a, int emit)
{
  if (emit)
    printf ("%.1f,%u,%.1f,%.6f,%.2f,%.1f,%.2f,%d,%.3f\n", esn0_db, seed,
            a->carrier_hz, a->delay_s, a->t_track_ms, a->seed_err_hz,
            a->t_lock_ms, a->locked_end, a->flips);
  else
    printf ("  %4.1f dB seed %2u: carrier %+7.1f Hz, tracking %6.1f ms, "
            "seed error %+7.1f Hz, symbol lock %7.1f ms, locked at end %d, "
            "flips %.2f\n",
            esn0_db, seed, a->carrier_hz, a->t_track_ms, a->seed_err_hz,
            a->t_lock_ms, a->locked_end, a->flips);
}

static void
print_dropout (double esn0_db, uint32_t seed, const dropout_t *d, int emit)
{
  if (emit)
    {
      printf ("%.1f,%u,%.1f,%d,%.6f,%.3f,%.2f,%.2f,%d,", esn0_db, seed,
              d->carrier_hz, d->settled, d->cut_s, d->flips, d->drop_ms,
              (double)d->hold_epochs * EPOCH_MS, d->non_monotonic);
      for (size_t r = 0; r < N_RELOCK; r++)
        printf ("%d,%.2f,", d->fell[r], d->back_ms[r]);
      for (size_t j = 0; j < d->n_held; j++)
        putchar (d->held[j] ? '1' : '0');
      putchar ('\n');
      return;
    }
  if (!d->settled)
    {
      printf ("  %4.1f dB seed %2u: carrier %+7.1f Hz, not settled within "
              "%.0f s\n",
              esn0_db, seed, d->carrier_hz, MAX_LOCK_S);
      return;
    }
  printf ("  %4.1f dB seed %2u: carrier %+7.1f Hz, lock falls %5.1f ms "
          "after a loss, holds "
          "%5.1f ms%s; back %5.1f ms after a %g ms dropout, %5.1f ms after "
          "%g ms; flips %.2f\n",
          esn0_db, seed, d->carrier_hz, d->drop_ms,
          (double)d->hold_epochs * EPOCH_MS,
          d->non_monotonic ? " (NON-MONOTONIC)" : "", d->back_ms[0],
          relock_ms[0], d->back_ms[1], relock_ms[1], d->flips);
}

/* --telemetry DIR: one receiver through a signal's whole life, its own
   telemetry captured (dp_tlm_capture) in DIR/<name>.tlm for
   scripts/plot_capture.py. A story takes an arrival trial's draws -- its
   carrier, arrival, bits and noise -- so its first second is that trial,
   sample for sample, and its arrival is the one the record counts; then
   the signal is cut for each of STORY_FADES (start, length, s) and leaves
   for good at STORY_GONE_S, every event mid-data. */
typedef struct
{
  const char *name;
  double      esn0_db;
  uint32_t    seed;  /* the arrival trial it continues */
  int         locks; /* whether that trial reaches symbol lock */
} story_t;

/* A third story, seed 5 at 5 dB, once showed a hand-off that never locks
   (its refine 444 Hz off); since the Dll's output level (#2495) that draw's
   refine lands 106 Hz off and locks, and no 5 dB arrival trial misses, so
   it is gone. A story that never locks needs a draw that does not. */
static const story_t stories[] = {
  { "lifecycle-10db", 10.0, 1u, 1 }, /* locks within 120 ms */
  { "lifecycle-5db", 5.0, 13u, 1 },  /* locks within 160 ms */
};
#define N_STORIES (sizeof stories / sizeof stories[0])
static const double story_fades[][2]
    = { { 1.0, 0.010 }, { 1.3, 0.020 }, { 1.6, 0.050 } };
#define N_STORY_FADES (sizeof story_fades / sizeof story_fades[0])
#define STORY_GONE_S 2.0
#define STORY_S 2.4

static int
story_on (size_t n, size_t delay)
{
  if (n < delay || (double)n >= STORY_GONE_S * FS)
    return 0;
  for (size_t f = 0; f < N_STORY_FADES; f++)
    {
      const double a = story_fades[f][0] * FS;
      if ((double)n >= a && (double)n < a + story_fades[f][1] * FS)
        return 0;
    }
  return 1;
}

static int
run_story (const uint8_t *code, const story_t *st, const char *dir, int check,
           int emit)
{
  const int    fails_before = dp_test_fails_;
  uint32_t     rng          = trial_rng (st->seed);
  const double cn0          = cn0_of (st->esn0_db);
  const double carrier      = (2.0 * dp_uni (&rng) - 1.0) * SPAN;
  const size_t delay        = (size_t)(dp_uni (&rng) * ARRIVAL_DELAY_S * FS);
  const size_t blocks       = blocks_for ((size_t)(STORY_S * FS));
  const size_t n_bits       = symbol_at (blocks * TE) + 2;
  uint8_t     *bits         = draw_bits (&rng, n_bits);

  dp_wfm_synth_state_t *syn
      = dp_dsss_cont_emitter (code, SF, SPC, FS, carrier, CPS, st->seed,
                              WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g  = make_noise (st->seed, cn0);
  dp_async_dsss_receiver_state_t *rx = make_rx (code, cn0);
  if (!syn || !g || !rx)
    return 1;
  DP_REQUIRE_MSG (sends_bits (syn, n_bits),
                  "the story's emitter sends the harness's bits");
  /* Every event mid-data: each cut and the departure. */
  for (size_t f = 0; f < N_STORY_FADES; f++)
    {
      double fl = flips_near (bits, n_bits,
                              symbol_at ((size_t)(story_fades[f][0] * FS)), 0);
      DP_CHECK_MSG (fl >= FLIPS_MIN && fl <= FLIPS_MAX,
                    "a story's dropout is cut mid-data");
    }
  double fl
      = flips_near (bits, n_bits, symbol_at ((size_t)(STORY_GONE_S * FS)), 0);
  DP_CHECK_MSG (fl >= FLIPS_MIN && fl <= FLIPS_MAX,
                "a story's signal leaves mid-data");

  /* Probes first: the capture sizes its ring from the probe table. */
  char path[1024];
  (void)snprintf (path, sizeof path, "%s/%s.tlm", dir, st->name);
  dp_tlm_t *tlm = dp_tlm_create (1u << 16);
  DP_REQUIRE (tlm != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, tlm, "rx", 1)
              == DP_OK);
  /* The stimulus beside the receiver's own probes, so a plot can shade
     where the signal was off (scripts/plot_capture.py --shade stim.on):
     the fraction of each block that carried it, once a block, on the
     block's own stamp -- 0 is off throughout, 1 on throughout. */
  const int id_stim = dp_tlm_probe (tlm, "stim.on", 1);
  DP_REQUIRE (id_stim >= 0);
  dp_sample_clock_t clk;
  dp_sample_clock_init (&clk, FS, 0);
  dp_tlm_capture_t *cap = dp_tlm_capture_open (tlm, TE, path, &clk);
  DP_REQUIRE_MSG (cap != NULL, "the story's capture opens");

  float complex *sig    = dp_xmalloc (TE * sizeof *sig);
  float complex *blk    = dp_xmalloc (TE * sizeof *blk);
  size_t         cap_   = dp_async_dsss_receiver_steps_max_out (rx);
  float complex *syms   = dp_xmalloc ((cap_ ? cap_ : TE) * sizeof *syms);
  double         t_lock = -1.0;
  for (size_t b = 0; b < blocks; b++)
    {
      dp_tlm_set_now (tlm, (uint64_t)(b * TE));
      dp_wfm_synth_steps (syn, sig, TE);
      dp_awgn_generate (g, TE, blk, TE);
      size_t on = 0;
      for (size_t i = 0; i < TE; i++)
        if (story_on (b * TE + i, delay))
          {
            blk[i] += sig[i];
            on++;
          }
      dp_tlm_emit (tlm, id_stim, (double)on / (double)TE);
      (void)dp_async_dsss_receiver_steps (rx, blk, TE, syms, cap_ ? cap_ : TE);
      if (t_lock < 0.0 && dp_async_dsss_receiver_get_locked (rx))
        t_lock = ((double)((b + 1) * TE) - (double)delay) / FS * 1e3;
    }
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the story's data covered its run");
  DP_CHECK_MSG (dp_tlm_capture_close (cap) == DP_OK,
                "the story's capture lost nothing");
  /* The story is the trial its seed names, so its outcome is that
     trial's: a story that stopped locking (or started) no longer shows
     what its name says. */
  DP_CHECK_MSG ((t_lock >= 0.0) == (st->locks != 0),
                "a story reaches symbol lock exactly when its name says "
                "(the receiver changed: re-pick its seed from the record's "
                "arrival trials)");
  /* --emit: the arrival row's own formats, so the record can hold each
     story to the sweep's trial of the same draws. */
  if (emit)
    printf ("%s,%.1f,%u,%.1f,%.6f,%.2f\n", st->name, st->esn0_db, st->seed,
            carrier, (double)delay / FS, t_lock < 0.0 ? NAN : t_lock);
  else
    {
      char lock_s[32];
      if (t_lock < 0.0)
        (void)snprintf (lock_s, sizeof lock_s, "never");
      else
        (void)snprintf (lock_s, sizeof lock_s, "%.1f ms", t_lock);
      printf ("  %-24s %4.1f dB, carrier %+7.1f Hz, arrival %5.1f ms, "
              "symbol lock %s -> %s\n",
              st->name, st->esn0_db, carrier, (double)delay / FS * 1e3, lock_s,
              path);
    }
  dp_tlm_capture_destroy (cap);
  /* --check is the ctest form: a passing story leaves nothing behind
     (scripts/check_test_leaks.py); a failing one keeps its capture as the
     evidence. */
  if (check && dp_test_fails_ == fails_before)
    {
      char meta[1040];
      (void)snprintf (meta, sizeof meta, "%s-meta", path);
      DP_CHECK (remove (path) == 0 && remove (meta) == 0);
    }
  (void)dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1);
  dp_tlm_destroy (tlm);
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
  int         check = 0, emit = 0;
  const char *tlm_dir = NULL;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--telemetry") == 0 && i + 1 < argc)
        tlm_dir = argv[++i];
    }
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);
  if (tlm_dir)
    {
      if (emit)
        printf ("# story\nname,esn0_db,seed,carrier_hz,delay_s,t_lock_ms\n");
      else
        printf ("telemetry: %zu lifecycle stories into %s\n", N_STORIES,
                tlm_dir);
      for (size_t k = 0; k < N_STORIES; k++)
        DP_REQUIRE (run_story (code, &stories[k], tlm_dir, check, emit) == 0);
      if (emit)
        DP_TEST_EMIT_END ("validate_async_dsss_receiver_lock --telemetry");
      DP_TEST_END ("validate_async_dsss_receiver_lock --telemetry");
    }

  const size_t n_esn0 = check ? 1 : N_ESN0;
  const size_t n_arr  = check ? 3 : SWEEP_TRIALS;
  const size_t want   = check ? 1 : SWEEP_TRIALS;
  const size_t tries  = check ? 4 : SWEEP_ATTEMPTS;

  if (emit)
    printf ("# arrival\nesn0_db,seed,carrier_hz,delay_s,t_track_ms,"
            "seed_err_hz,t_lock_ms,locked_end,flips\n");
  else
    printf ("arrival: %.3f Mcps, +/-%.0f Hz search, %.1f s a trial\n",
            CHIP_RATE * 1e-6, SPAN, ARRIVAL_S);
  for (size_t e = 0; e < n_esn0; e++)
    for (uint32_t s = 1; s <= n_arr; s++)
      {
        arrival_t a;
        DP_REQUIRE (run_arrival (code, esn0s[e], s, &a) == 0);
        check_arrival (esn0s[e], &a);
        print_arrival (esn0s[e], s, &a, emit);
      }

  if (emit)
    printf ("# dropout\nesn0_db,seed,carrier_hz,settled,cut_s,flips,drop_ms,"
            "hold_ms,non_monotonic,fell_50,back_50_ms,fell_200,back_200_ms,"
            "held\n");
  else
    printf ("dropout: carrier over +/-%.0f Hz, the signal cut, the noise "
            "on\n",
            SPAN);
  for (size_t e = 0; e < n_esn0; e++)
    {
      size_t got = 0;
      for (uint32_t s = 1; s <= tries && got < want; s++)
        {
          dropout_t d;
          DP_REQUIRE (run_dropout (code, esn0s[e], s, &d) == 0);
          check_dropout (&d);
          print_dropout (esn0s[e], s, &d, emit);
          got += (size_t)d.settled;
        }
      DP_CHECK_MSG (got == want, "enough dropout draws settled to measure");
    }

  /* QUALITY: --check is one draw at every Es/N0, the sweep five; both
     assert the same at 10 dB and above and measure the rest. */
  const size_t n_q  = N_QUALITY_ESN0;
  const size_t q_tr = check ? 1 : QUALITY_TRIALS;
  /* The EVM band's two lines, for the report to quote rather than invert
     from the bounds the quality rows carry. */
  if (emit)
    printf ("# evm_model\nk_hi_db,floor_hi_db,k_lo_db,floor_lo_db,"
            "margin_db\n%.3f,%.2f,%.3f,%.2f,%.2f\n",
            EVM_K_HI_DB, EVM_FLOOR_HI_DB, EVM_K_LO_DB, EVM_FLOOR_LO_DB,
            EVM_MARGIN_DB);
  if (emit)
    printf ("# quality\nesn0_db,seed,carrier_hz,settled,t_lock_ms,n_sym,"
            "evm_db,ideal_db,loss_db,loss_min_db,loss_max_db,lock_frac\n");
  else
    printf ("quality: symbol lock, then %.1f s of every symbol\n", QUALITY_S);
  for (size_t e = 0; e < n_q; e++)
    {
      const double es = quality_esn0s[e];
      for (uint32_t s = 1; s <= q_tr; s++)
        {
          quality_t q;
          DP_REQUIRE (run_quality (code, es, s, &q) == 0);
          /* Asserted at 10 dB and above. Below, a draw need not lock (the
             sweep's 5 dB seed 3 does not), and the row carries no
             allowance (NaN). */
          const int    asserted = es >= 10.0;
          const double lo       = asserted ? evm_loss_min_db (es) : NAN;
          const double hi       = asserted ? evm_loss_max_db (es) : NAN;
          if (emit)
            printf ("%.1f,%u,%.1f,%d,%.2f,%zu,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f\n",
                    es, s, q.carrier_hz, q.settled, q.t_lock_ms, q.n_sym,
                    q.evm_db, dp_ber_evm_ideal_db (es), q.loss_db, lo, hi,
                    q.lock_frac);
          else if (asserted)
            printf ("  %4.1f dB seed %u: carrier %+7.1f Hz, lock %7.1f ms, "
                    "%zu symbols, EVM %6.2f dB, loss %+.2f dB (allowed %+.2f "
                    "to %+.2f), locked %.3f\n",
                    es, s, q.carrier_hz, q.t_lock_ms, q.n_sym, q.evm_db,
                    q.loss_db, lo, hi, q.lock_frac);
          else
            printf ("  %4.1f dB seed %u: carrier %+7.1f Hz, lock %7.1f ms, "
                    "%zu symbols, EVM %6.2f dB, loss %+.2f dB, locked %.3f\n",
                    es, s, q.carrier_hz, q.t_lock_ms, q.n_sym, q.evm_db,
                    q.loss_db, q.lock_frac);
          if (asserted)
            {
              DP_CHECK_MSG (q.settled, "at 10 dB and above a draw locks");
              DP_CHECK_MSG (q.settled && q.loss_db <= hi,
                            "a locked receiver's EVM is within its "
                            "allowance over the meter's ideal");
              DP_CHECK_MSG (q.settled && q.loss_db >= lo,
                            "a locked receiver's EVM does not read better "
                            "than its implementation loss allows");
              DP_CHECK_MSG (q.settled && q.lock_frac == 1.0,
                            "symbol lock holds through the window");
            }
        }
    }

  /* FALSE LOCK on noise at each design Es/N0: --check one draw, the
     sweep five. */
  if (emit)
    printf ("# falselock\nesn0_db,seed,blocks,up,frac,declares,code_frac\n");
  else
    printf ("false lock: a receiver seeded on noise, %.1f s\n", FALSE_S);
  for (size_t e = 0; e < N_ESN0; e++)
    for (uint32_t s = 1; s <= (check ? 1u : FALSE_TRIALS); s++)
      {
        falselock_t f;
        DP_REQUIRE (run_falselock (code, esn0s[e], s, &f) == 0);
        if (emit)
          printf ("%.1f,%u,%zu,%zu,%.4f,%zu,%.4f\n", esn0s[e], s, f.blocks,
                  f.up, f.frac, f.declares, f.code_frac);
        else
          printf ("  %4.1f dB seed %u: symbol lock up %.4f of blocks, "
                  "declared %zu times; code lock %.4f\n",
                  esn0s[e], s, f.frac, f.declares, f.code_frac);
      }

  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_lock");
  DP_TEST_END ("validate_async_dsss_receiver_lock");
}
