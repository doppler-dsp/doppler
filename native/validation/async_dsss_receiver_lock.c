/**
 * @file async_dsss_receiver_lock.c
 * @brief Symbol lock through a signal's life at the 415.1-B-1 point: through
 *        a dropout, after a loss, and the hand-over on a Doppler ramp.
 *
 * The pool's design (docs/design/async-dsss-receiver.md §6) makes symbol
 * lock the receiver's own rule: without it a receiver searches, with it it
 * tracks, and losing it sends the receiver back to search. How symbol lock
 * behaves through a dropout and after a loss is measured here. The record
 * (docs/design/async-dsss-receiver-measurements.md, §6.3) renders this
 * harness's `--emit` output. How long a new signal takes to be acquired is
 * not: the pull-in validator's use-case cells measure it, and assert
 * 415.1-B-1 §7.4.1's acquisition requirement on them.
 *
 * Operating point: the 415.1-B-1 point, dp_s415_test.h -- the Gold code at
 * F_fwd x 31/(221 x 96) chips a second, 3,000 sym/s, 11 MSa/s, the
 * 275-in-1,000 code-only window -- at both ends of the band. The receiver
 * is the point's, as built (the refine on, the partials carrier window),
 * its design C/N0 the true one. Es/N0 10 dB and the 4 dB floor.
 *
 * Stimulus: the point's emitter on the scene (dp_air_test.h) -- the shipped
 * synth sending this harness's payload, the front end's converter, a
 * Doppler channel at a static offset drawn over +/-1,500 Hz -- rendered
 * WITHOUT noise, and the shipped awgn added here, sized for a unit-power
 * emitter at the C/N0 over the capture rate exactly as the scene would
 * size it. Keeping them apart is what lets a dropout cut the signal while
 * the noise runs on, so the noise floor never moves. Fed PUSH samples a
 * block.
 *
 * Every event is mid-data: the symbol it falls in, and FLIPS_SYMS data
 * symbols either side, are all data symbols, none in a frame's code-only
 * window (frame positions 545 to 729 of 1,000). A dropout waits for that
 * band before its cut; a story places its events there. Each row carries
 * the fraction of those symbol boundaries whose data flips, from the
 * scene's own truth (about one half).
 *
 * DROPOUT. A trial draws the carrier over +/-1,500 Hz, its frame start, its
 * data and noise. It is fed until it tracks with symbol lock up for
 * SETTLE_BLOCKS blocks in a row, then a further uniform 0 to JITTER_S, then
 * until its next block lands in the mid-data band, and is cut at a uniform
 * sample of that block; a draw that has not held lock that long within
 * MAX_LOCK_S, or whose lock is down at the cut, is a row marked unsettled.
 * The receiver's state at the cut is saved and the next POST_S of signal
 * and noise rendered once, so every case below resumes from the same state
 * on the same noise and differs only in the dropout (the SIGNAL is cut;
 * the noise runs on). That rests on the restore into the already-run
 * receiver being exact, so it is checked: until its signal returns, every
 * case's lock must read the for-good case's, block for block, their input
 * being the same.
 *
 *   for good       when symbol lock falls after the cut (or `-1`, held
 *                  through the whole blocks of DROP_WATCH_S)
 *   1, 2, ... code periods  whether lock stayed up through a dropout of
 *                  that length and HOLD_WATCH_S after it, every length up
 *                  to two past the drop. The profile is emitted whole,
 *                  because it need not be monotonic in the length: a lock
 *                  that rides out a long dropout can fall after a shorter
 *                  one, its slow average sinking under its threshold in
 *                  the dropout and climbing back too late after it. So the
 *                  record reports, per length, the fraction of trials that
 *                  held; `hold_ms` is only the longest held before the
 *                  first that was not, and `non_monotonic` says a longer
 *                  one held anyway
 *   RELOCK_MS      whether lock fell in a dropout of that length, and when
 *                  it is back after the signal returned (`-1` if not
 *                  within RELOCK_WATCH_S); the receiver stays tracking
 *                  throughout (lost_confirm_s = 0)
 *
 * None of the dropout's times is asserted. How soon a receiver must drop
 * symbol lock when its signal is gone is a requirement nobody has set: the
 * specification has no row for it, and the figure waits on the owner
 * (doppler#3157). They are measured and printed, each row citing that.
 *
 * RAMP. The hand-over at the point, 2.8 GHz, 4 dB Es/N0, +/-1,500 Hz, a
 * 400 Hz/s rate bound, so the search runs 140 deep. The emitter is the
 * point's on the scene, on a +400 Hz/s ramp from -1,000 and from -500 Hz,
 * plus the shipped awgn. Each draw (its own data and noise) runs twice,
 * refine true (as built) and refine false (the search's Doppler is the
 * hand-over), RAMP_S each: RAMP_TRIALS draws from each start in the sweep;
 * --check runs refine false alone, RAMP_CHECK_TRIALS draws a start, each to
 * its hand-over. Asserted in both: refine false hands over within the
 * header's bound (half a row plus the half-dwell age at the rate bound) of
 * the truth at the hand-over sample, on every draw. Printed: each arm's
 * largest hand-over error, symbol lock at 0.5 s and at 2.0 s with Wilson
 * 95% intervals, the paired draws where only one arm is locked at 0.5 s,
 * and for a draw not locked at 2.0 s whether it is on its emitter and
 * whether code lock is up -- late or wrong. The lock rates move with the
 * draw and are not asserted.
 *
 * What is asserted, the same in the sweep and in --check, is the harness's
 * own correctness, each with its source in this file: the payload covered
 * each run (dp_air_data_ended(); the scene sends a given payload by
 * construction, test_dp_air.c); every event is
 * mid-data; the restore is exact (above); at 10 dB the dropout draws settle
 * often enough to measure (at the floor how many settle is itself printed:
 * what must hold there waits on the floor brief); each lifecycle story
 * reaches symbol lock exactly when its `locks` says; and the ramp's
 * hand-over bound.
 *
 * Usage:
 *   validate_async_dsss_receiver_lock            the sweep, both carriers,
 *                                                10 and 4 dB
 *   validate_async_dsss_receiver_lock --check    2.8 GHz, 10 dB, one
 *                                                dropout trial; the ramp's
 *                                                check
 *   ... --emit                                   CSV blocks on stdout for
 *                                                the record: `# dropout`
 *                                                and `# ramp`, one row a
 *                                                trial
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
 *                                                row a story
 */
#include "async_dsss_receiver_common.h" /* trial_rng, draw_bits */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/clib_common.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_tlm_capture/dp_tlm_capture_core.h"
#include "doppler/timing/timing_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_air_test.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PUSH 4096u           /* the feed block, samples: 0.37 ms      */
#define ARRIVAL_DELAY_S 0.02 /* a story's noise alone, up to this     */
#define FLIPS_SYMS 270u      /* 90 ms of symbols each side of an event */
#define FLIPS_MIN 0.35       /* mid-data: random bits flip at about    */
#define FLIPS_MAX 0.65       /* half their boundaries                  */

#define SETTLE_BLOCKS 200u /* symbol lock up this long, then the cut */
#define MAX_LOCK_S 4.0     /* give up on a draw that never settles   */
#define JITTER_S 0.01      /* then up to this more, then the band    */
#define POST_S 0.6         /* rendered after the cut, for every case */
#define DROP_WATCH_S 0.08  /* a loss for good is watched this long   */
#define HOLD_WATCH_S 0.06  /* a held dropout: lock up this long after */
#define RELOCK_WATCH_S 0.3 /* a re-lock is waited for this long      */
#define MAX_HOLD 400u      /* the hold scan's longest dropout, periods */

static const double relock_ms[] = { 50.0, 200.0 };
#define N_RELOCK (sizeof relock_ms / sizeof relock_ms[0])

static const double esn0s[] = { 10.0, DP_S415_FLOOR_DB };
#define N_ESN0 (sizeof esn0s / sizeof esn0s[0])
static const double fcs[] = { DP_S415_FC_LO, DP_S415_FC_HI };
#define N_FC (sizeof fcs / sizeof fcs[0])
#define SWEEP_TRIALS 20u   /* settled dropout trials a cell          */
#define SWEEP_ATTEMPTS 40u /* dropout draws tried for them, at most    */

/* Samples a code period at carrier `fc`: the hold scan's unit. */
static double
period_samples (double fc)
{
  return (double)DP_S415_SF / dp_s415_chip_rate (fc) * DP_S415_FS;
}

/* Whether symbol `n` (dp_air_truth()'s count) is mid-data: it and the
   FLIPS_SYMS symbols either side all data symbols, none in its frame's
   code-only window. */
static int
mid_data (long n)
{
  if (n < 0)
    return 0;
  const long q = n % (long)DP_S415_FRAME_SYM;
  return q >= (long)(DP_S415_CODE_ONLY_SYM + FLIPS_SYMS)
         && q + (long)FLIPS_SYMS < (long)DP_S415_FRAME_SYM;
}

/* Of the symbol boundaries within FLIPS_SYMS of symbol `n`, the fraction
   where the data the scene sends flips (the truth's own bits). */
static double
flips_near (const dp_air_t *air, long n)
{
  size_t flips = 0, m = 0;
  for (long i = n - (long)FLIPS_SYMS + 1; i <= n + (long)FLIPS_SYMS; i++)
    {
      const int a = dp_air_symbol_bit (air, 0, i - 1);
      const int b = dp_air_symbol_bit (air, 0, i);
      if (a < 0 || b < 0)
        continue;
      flips += a != b;
      m++;
    }
  return m ? (double)flips / (double)m : 0.0;
}

/* The point's emitter alone, no noise, sending `bits`; `lag_s` of silence
   first, its frame `frame_off` symbols in at its first chip. */
static int
scene_init (dp_air_t *air, const uint8_t *code, double fc, double f0,
            double cn0, double lag_s, size_t frame_off, uint32_t seed,
            const uint8_t *bits, size_t n_bits)
{
  dp_air_emitter_t e = dp_s415_emitter (code, fc, f0, 0.0, cn0, lag_s, seed);
  e.frame_off        = frame_off;
  e.bits             = bits;
  e.n_bits           = n_bits;
  return dp_air_init (air, &e, 1, DP_S415_FS, NAN, 0u, PUSH);
}

/* The noise: the shipped awgn, sized for a unit-power emitter at `cn0`
   over the capture rate, as dp_air_init() sizes its own. */
static dp_awgn_state_t *
lk_noise (uint32_t seed, double cn0)
{
  return dp_awgn_create (seed * 7919u + 1u,
                         dp_awgn_amplitude_for_snr (
                             (float)(cn0 - 10.0 * log10 (DP_S415_FS)), 1.0f));
}

/* Data symbols a run of `samples` needs, with room. */
static size_t
bits_for (double samples)
{
  return (size_t)(samples / DP_S415_FS * DP_S415_SYM) + 2000u;
}

static dp_async_dsss_receiver_state_t *
lk_rx (const uint8_t *code, double fc, double cn0)
{
  return dp_s415_rx (code, fc, cn0, true, ASYNC_DSSS_RX_CARRIER_PARTIALS);
}

typedef struct
{
  int     settled;           /* tracking, symbol lock held, before MAX  */
  double  carrier_hz;        /* the trial's offset, over +/-1,500 Hz    */
  double  cut_s;             /* the cut, from the first sample          */
  double  flips;             /* data flips within FLIPS_SYMS of the cut */
  double  drop_ms;           /* cut -> lock down, gone for good; -1 held */
  size_t  n_held;            /* dropouts scanned: 1 .. n_held periods    */
  uint8_t held[MAX_HOLD];    /* held[j]: a (j+1)-period dropout held     */
  size_t  hold_periods;      /* the longest held before the first not    */
  double  period_ms;         /* one code period at this carrier, ms      */
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
      for (size_t i = 0; i < PUSH; i++)
        {
          size_t s   = b * PUSH + i;
          int    off = s >= cut && (fade == SIZE_MAX || s - cut < fade);
          blk[i]     = noip[s] + (off ? 0.0f : 1.0f) * sigp[s];
        }
      (void)dp_async_dsss_receiver_steps (rx, blk, PUSH, syms, cap);
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

/* Whole blocks covering `samples`. */
static size_t
blocks_of (size_t samples)
{
  return (samples + PUSH - 1) / PUSH;
}

static int
run_dropout (const uint8_t *code, double fc, double esn0_db, uint32_t seed,
             dropout_t *out)
{
  memset (out, 0, sizeof *out);
  out->drop_ms        = -1.0;
  uint32_t     st     = trial_rng (seed + 100000u);
  const double cn0    = dp_s415_cn0 (esn0_db);
  const double per    = period_samples (fc);
  out->period_ms      = per / DP_S415_FS * 1e3;
  const size_t lock_b = (size_t)(MAX_LOCK_S * DP_S415_FS / (double)PUSH);
  out->carrier_hz     = (2.0 * dp_uni (&st) - 1.0) * DP_S415_SPAN_HZ;
  const size_t frame0
      = (size_t)(dp_uni (&st) * (double)DP_S415_FRAME_SYM) % DP_S415_FRAME_SYM;
  const size_t jitter_b
      = (size_t)(dp_uni (&st) * JITTER_S * DP_S415_FS / PUSH);
  const size_t at = (size_t)(dp_uni (&st) * (double)PUSH);
  /* The mid-data band comes round once a frame (1/3 s): room for it. */
  const size_t band_b
      = (size_t)(DP_S415_FRAME_SYM / DP_S415_SYM * DP_S415_FS / PUSH) + 1;
  const size_t post_n = blocks_of ((size_t)(POST_S * DP_S415_FS)) * PUSH;
  const size_t max_n  = (lock_b + jitter_b + band_b + 2) * PUSH + post_n;
  const size_t n_bits = bits_for ((double)max_n);
  uint8_t     *bits   = draw_bits (&st, n_bits);

  dp_air_t air;
  DP_REQUIRE (scene_init (&air, code, fc, out->carrier_hz, cn0, 0.0, frame0,
                          seed + 100000u, bits, n_bits));
  dp_awgn_state_t                *g  = lk_noise (seed + 100000u, cn0);
  dp_async_dsss_receiver_state_t *rx = lk_rx (code, fc, cn0);
  if (!g || !rx)
    return 1;
  float complex *blk  = dp_xmalloc (PUSH * sizeof *blk);
  float complex *noi  = dp_xmalloc (PUSH * sizeof *noi);
  size_t         cap  = dp_async_dsss_receiver_steps_max_out (rx);
  cap                 = cap ? cap : PUSH;
  float complex *syms = dp_xmalloc (cap * sizeof *syms);

  /* Settle: tracking with symbol lock up SETTLE_BLOCKS in a row, then the
     jitter, then until the next block's cut sample is mid-data. */
  size_t fed = 0, run = 0, extra = 0;
  int    settled = 0, in_band = 0;
  for (size_t b = 0; b < lock_b + jitter_b + band_b; b++)
    {
      DP_REQUIRE (dp_air_next (&air, blk, PUSH));
      dp_awgn_generate (g, PUSH, noi, PUSH);
      for (size_t i = 0; i < PUSH; i++)
        blk[i] += noi[i];
      (void)dp_async_dsss_receiver_steps (rx, blk, PUSH, syms, cap);
      fed++;
      int up = dp_async_dsss_receiver_get_tracking (rx)
               && dp_async_dsss_receiver_get_locked (rx);
      run    = up ? run + 1 : 0;
      if (!settled && run >= SETTLE_BLOCKS)
        settled = 1;
      if (settled && extra++ >= jitter_b
          && mid_data (dp_air_truth (&air, 0, fed * PUSH + at).symbol))
        {
          in_band = 1;
          break;
        }
      if (!settled && b + 1 >= lock_b)
        break;
    }
  const long cut_sym = dp_air_truth (&air, 0, fed * PUSH + at).symbol;
  out->settled = settled && in_band && dp_async_dsss_receiver_get_locked (rx);
  out->cut_s   = (double)(fed * PUSH + at) / DP_S415_FS;
  out->flips   = flips_near (&air, cut_sym);
  if (!out->settled)
    goto done;

  {
    /* The state at the cut, and the stream after it, rendered once. */
    size_t sz   = dp_async_dsss_receiver_state_bytes (rx);
    void  *blob = dp_xmalloc (sz);
    dp_async_dsss_receiver_get_state (rx, blob);
    float complex *sigp = dp_xmalloc (post_n * sizeof *sigp);
    float complex *noip = dp_xmalloc (post_n * sizeof *noip);
    /* The scene renders at most a block a call (dp_air_init()'s `block`). */
    for (size_t o = 0; o < post_n; o += PUSH)
      DP_REQUIRE (dp_air_next (&air, sigp + o, PUSH));
    DP_CHECK_MSG (!dp_air_data_ended (&air),
                  "the dropout's payload covered the stream after the cut");
    dp_awgn_generate (g, post_n, noip, post_n);
    uint8_t *lk   = dp_xmalloc (post_n / PUSH);
    uint8_t *gone = dp_xmalloc (post_n / PUSH);

    /* Gone for good, over the whole blocks inside DROP_WATCH_S, so a drop
       is never read past the watch. */
    const size_t m_gone = (at + (size_t)(DROP_WATCH_S * DP_S415_FS)) / PUSH;
    DP_REQUIRE (branch (rx, blob, sigp, noip, at, SIZE_MAX, m_gone, blk, syms,
                        cap, gone)
                == 0);
    size_t m = m_gone;
    for (size_t i = 0; i < m; i++)
      if (!gone[i])
        {
          out->drop_ms
              = ((double)((i + 1) * PUSH) - (double)at) / DP_S415_FS * 1e3;
          break;
        }

    /* Every dropout of 1, 2, ... code periods, to two past the drop: past
       it the lock falls inside the dropout by construction. */
    size_t longest = (out->drop_ms >= 0.0
                          ? (size_t)ceil (out->drop_ms / out->period_ms)
                          : (size_t)ceil (DROP_WATCH_S * 1e3 / out->period_ms))
                     + 2;
    if (longest > MAX_HOLD)
      longest = MAX_HOLD;
    const size_t hold_watch = (size_t)(HOLD_WATCH_S * DP_S415_FS);
    int          failed     = 0;
    /* The restore check: until the signal returns, a case's input is the
       for-good case's, so its lock must read the same, block for block. */
    int exact = 1;
    for (size_t j = 1; j <= longest; j++)
      {
        size_t fade = (size_t)llround ((double)j * per);
        m           = blocks_of (at + fade + hold_watch);
        DP_REQUIRE (m <= post_n / PUSH);
        DP_REQUIRE (
            branch (rx, blob, sigp, noip, at, fade, m, blk, syms, cap, lk)
            == 0);
        exact &= same_until (lk, gone, m_gone, (at + fade) / PUSH);
        uint8_t h = 1;
        for (size_t i = 0; i < m; i++)
          h &= lk[i];
        out->held[j - 1] = h;
        if (!h && !failed)
          {
            failed            = 1;
            out->hold_periods = j - 1;
          }
        else if (h && failed)
          out->non_monotonic = 1;
      }
    out->n_held = longest;
    if (!failed)
      out->hold_periods = longest;

    /* The re-lock after each RELOCK_MS dropout. */
    for (size_t r = 0; r < N_RELOCK; r++)
      {
        size_t fade = (size_t)(relock_ms[r] * 1e-3 * DP_S415_FS);
        size_t back = at + fade;
        m           = blocks_of (back + (size_t)(RELOCK_WATCH_S * DP_S415_FS));
        DP_REQUIRE (m <= post_n / PUSH);
        DP_REQUIRE (
            branch (rx, blob, sigp, noip, at, fade, m, blk, syms, cap, lk)
            == 0);
        exact &= same_until (lk, gone, m_gone, back / PUSH);
        size_t first_after = blocks_of (back); /* first block past it */
        out->back_ms[r]    = -1.0;
        for (size_t i = 0; i < first_after && i < m; i++)
          out->fell[r] |= !lk[i];
        for (size_t i = first_after; i < m; i++)
          if (lk[i])
            {
              out->back_ms[r] = ((double)((i + 1) * PUSH) - (double)back)
                                / DP_S415_FS * 1e3;
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
  free (noi);
  free (blk);
  free (bits);
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  dp_air_free (&air);
  return 0;
}

static void
check_dropout (const dropout_t *d)
{
  if (!d->settled)
    return;
  DP_CHECK_MSG (d->flips >= FLIPS_MIN && d->flips <= FLIPS_MAX,
                "a dropout is cut mid-data, never data-free");
}

static void
print_dropout (double fc, double esn0_db, uint32_t seed, const dropout_t *d,
               int emit)
{
  if (emit)
    {
      printf ("%.1f,%.1f,%u,%.1f,%d,%.6f,%.3f,%.2f,%.3f,%.2f,%d,", fc / 1e9,
              esn0_db, seed, d->carrier_hz, d->settled, d->cut_s, d->flips,
              d->drop_ms, d->period_ms, (double)d->hold_periods * d->period_ms,
              d->non_monotonic);
      for (size_t r = 0; r < N_RELOCK; r++)
        printf ("%d,%.2f,", d->fell[r], d->back_ms[r]);
      for (size_t j = 0; j < d->n_held; j++)
        putchar (d->held[j] ? '1' : '0');
      putchar ('\n');
      return;
    }
  if (!d->settled)
    {
      printf ("  %.1f GHz %4.1f dB seed %2u: carrier %+7.1f Hz, not settled "
              "within %.0f s\n",
              fc / 1e9, esn0_db, seed, d->carrier_hz, MAX_LOCK_S);
      return;
    }
  printf ("  %.1f GHz %4.1f dB seed %2u: carrier %+7.1f Hz, lock falls %5.1f "
          "ms after a loss, holds %5.1f ms%s; back %5.1f ms after a %g ms "
          "dropout, %5.1f ms after %g ms; flips %.2f\n",
          fc / 1e9, esn0_db, seed, d->carrier_hz, d->drop_ms,
          (double)d->hold_periods * d->period_ms,
          d->non_monotonic ? " (NON-MONOTONIC)" : "", d->back_ms[0],
          relock_ms[0], d->back_ms[1], relock_ms[1], d->flips);
}

/* --telemetry DIR: one receiver through a signal's whole life at 2.8 GHz,
   its own telemetry captured (dp_tlm_capture) in DIR/<name>.tlm for
   scripts/plot_capture.py. A story draws its carrier, frame-free start,
   arrival, data and noise from its seed; then the signal is cut for each of
   STORY_FADES (start symbol, length s) and leaves for good at symbol
   STORY_GONE_SYM. Every event is placed by the emitter's own symbol count
   at frame position 640, inside the mid-data band. */
typedef struct
{
  const char *name;
  double      esn0_db;
  uint32_t    seed;  /* its draws */
  int         locks; /* whether it reaches symbol lock */
} story_t;

static const story_t stories[] = {
  { "lifecycle-10db", 10.0, 1u, 1 },
  { "lifecycle-4db", DP_S415_FLOOR_DB, 1u, 1 },
};
#define N_STORIES (sizeof stories / sizeof stories[0])
/* Each fade: the emitter's symbol it starts at, and its length, s. */
static const double story_fades[][2]
    = { { 3640.0, 0.010 }, { 4640.0, 0.020 }, { 5640.0, 0.050 } };
#define N_STORY_FADES (sizeof story_fades / sizeof story_fades[0])
#define STORY_GONE_SYM 6640.0
#define STORY_S 2.6

/* The receive sample the emitter's symbol `sym` starts near: its arrival
   plus that many symbol periods (the converter's and the channel's few
   samples aside; the truth checks the placement). */
static double
story_sample (size_t delay, double sym)
{
  return (double)delay + sym / DP_S415_SYM * DP_S415_FS;
}

static int
story_on (size_t n, size_t delay)
{
  if (n < delay || (double)n >= story_sample (delay, STORY_GONE_SYM))
    return 0;
  for (size_t f = 0; f < N_STORY_FADES; f++)
    {
      const double a = story_sample (delay, story_fades[f][0]);
      if ((double)n >= a && (double)n < a + story_fades[f][1] * DP_S415_FS)
        return 0;
    }
  return 1;
}

static int
run_story (const uint8_t *code, const story_t *st, const char *dir, int check,
           int emit)
{
  const int    fails_before = dp_test_fails_;
  const double fc           = DP_S415_FC_HI;
  uint32_t     rng          = trial_rng (st->seed);
  const double cn0          = dp_s415_cn0 (st->esn0_db);
  const double carrier      = (2.0 * dp_uni (&rng) - 1.0) * DP_S415_SPAN_HZ;
  const size_t delay  = (size_t)(dp_uni (&rng) * ARRIVAL_DELAY_S * DP_S415_FS);
  const size_t blocks = blocks_of ((size_t)(STORY_S * DP_S415_FS));
  const size_t n_bits = bits_for ((double)(blocks * PUSH));
  uint8_t     *bits   = draw_bits (&rng, n_bits);

  dp_air_t air;
  DP_REQUIRE (scene_init (&air, code, fc, carrier, cn0,
                          (double)delay / DP_S415_FS, 0, st->seed, bits,
                          n_bits));
  dp_awgn_state_t                *g  = lk_noise (st->seed, cn0);
  dp_async_dsss_receiver_state_t *rx = lk_rx (code, fc, cn0);
  if (!g || !rx)
    return 1;
  /* Every event mid-data: each cut and the departure, read off the
     truth at the sample it lands on. */
  for (size_t f = 0; f <= N_STORY_FADES; f++)
    {
      const double sym
          = f < N_STORY_FADES ? story_fades[f][0] : STORY_GONE_SYM;
      const long n
          = dp_air_truth (&air, 0, (uint64_t)story_sample (delay, sym)).symbol;
      const double fl = flips_near (&air, n);
      DP_CHECK_MSG (mid_data (n) && fl >= FLIPS_MIN && fl <= FLIPS_MAX,
                    "a story's every event is mid-data");
    }

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
  dp_sample_clock_init (&clk, DP_S415_FS, 0);
  dp_tlm_capture_t *cap = dp_tlm_capture_open (tlm, PUSH, path, &clk);
  DP_REQUIRE_MSG (cap != NULL, "the story's capture opens");

  float complex *sig    = dp_xmalloc (PUSH * sizeof *sig);
  float complex *blk    = dp_xmalloc (PUSH * sizeof *blk);
  size_t         cap_   = dp_async_dsss_receiver_steps_max_out (rx);
  cap_                  = cap_ ? cap_ : PUSH;
  float complex *syms   = dp_xmalloc (cap_ * sizeof *syms);
  double         t_lock = -1.0;
  for (size_t b = 0; b < blocks; b++)
    {
      dp_tlm_set_now (tlm, (uint64_t)(b * PUSH));
      DP_REQUIRE (dp_air_next (&air, sig, PUSH));
      dp_awgn_generate (g, PUSH, blk, PUSH);
      size_t on = 0;
      for (size_t i = 0; i < PUSH; i++)
        if (story_on (b * PUSH + i, delay))
          {
            blk[i] += sig[i];
            on++;
          }
      dp_tlm_emit (tlm, id_stim, (double)on / (double)PUSH);
      (void)dp_async_dsss_receiver_steps (rx, blk, PUSH, syms, cap_);
      if (t_lock < 0.0 && dp_async_dsss_receiver_get_locked (rx))
        t_lock = ((double)((b + 1) * PUSH) - (double)delay) / DP_S415_FS * 1e3;
    }
  DP_CHECK_MSG (!dp_air_data_ended (&air),
                "the story's payload covered its run");
  DP_CHECK_MSG (dp_tlm_capture_close (cap) == DP_OK,
                "the story's capture lost nothing");
  /* A story shows what its name says only while its outcome is the one it
     was picked for: a story that stopped locking (or started) is re-picked
     from its seed, not left describing another run. */
  DP_CHECK_MSG ((t_lock >= 0.0) == (st->locks != 0),
                "a story reaches symbol lock exactly when its entry says "
                "(the receiver changed: re-pick its seed)");
  if (emit)
    printf ("%s,%.1f,%u,%.1f,%.6f,%.2f\n", st->name, st->esn0_db, st->seed,
            carrier, (double)delay / DP_S415_FS, t_lock < 0.0 ? NAN : t_lock);
  else
    {
      char lock_s[32];
      if (t_lock < 0.0)
        (void)snprintf (lock_s, sizeof lock_s, "never");
      else
        (void)snprintf (lock_s, sizeof lock_s, "%.1f ms", t_lock);
      printf ("  %-24s %4.1f dB, carrier %+7.1f Hz, arrival %5.1f ms, "
              "symbol lock %s -> %s\n",
              st->name, st->esn0_db, carrier, (double)delay / DP_S415_FS * 1e3,
              lock_s, path);
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
  dp_air_free (&air);
  return 0;
}

/* ── RAMP: the hand-over with and without the refine ──────────────────── */
#define RAMP_S_EARLY 0.5
#define RAMP_S 2.0
#define RAMP_TRIALS 30u
/* --check: the asserted arm alone, each draw to its hand-over. */
#define RAMP_CHECK_TRIALS 3u
#define RAMP_ON_HZ 50.0
#define RAMP_CHUNK 16384u

typedef struct
{
  double ho_err_hz;  /* handed-over Doppler minus the truth at that sample */
  int    lock_early; /* tracking, code and symbol lock at RAMP_S_EARLY     */
  int    lock_end;   /* the same at RAMP_S                                */
  int    on_end;     /* tracked carrier within RAMP_ON_HZ of the truth     */
  int    code_end;   /* code lock up at RAMP_S                             */
} ramp_t;

/* One draw at the 415.1-B-1 point, 2.8 GHz, from `f0` Hz at +400 Hz/s. */
static int
run_ramp (const uint8_t *code, bool refine, double f0, uint32_t seed,
          int to_handover, double *bound_hz, ramp_t *r)
{
  const double fc = DP_S415_FC_HI, chip = dp_s415_chip_rate (fc);
  const double cn0                   = dp_s415_cn0 (DP_S415_FLOOR_DB);
  const double rate                  = DP_S415_RATE_HZ_S;
  dp_async_dsss_receiver_state_t *rx = dp_s415_rx (code, fc, cn0, refine, 0);
  /* The emitter on the air (native/tests/dp_air_test.h): the shipped synth
     with its window, the front end's converter, a Doppler channel on the
     ramp, the shipped noise; the truth is the channel's own offset. */
  dp_air_emitter_t e
      = dp_s415_emitter (code, fc, f0, rate, cn0, 0.0, 100u + seed);
  dp_air_t        air;
  float _Complex *x   = malloc (RAMP_CHUNK * sizeof *x);
  float _Complex *out = malloc (RAMP_CHUNK * sizeof *out);
  DP_REQUIRE (
      rx && x && out
      && dp_air_init (&air, &e, 1, DP_S415_FS, cn0, 7919u + seed, RAMP_CHUNK));
  const size_t D = rx->acq->search->coherent_bins;
  DP_REQUIRE (D == 140);
  /* The header's bound for refine false: half a row, and the half-dwell
     age at the rate bound. */
  *bound_hz         = 0.5 * rx->acq->search->doppler_res_hz
                      + rate * 0.5 * (double)D * (double)DP_S415_SF / chip;
  *r                = (ramp_t){ .ho_err_hz = NAN };
  size_t fed        = 0;
  int    early_done = 0;
  while (fed < (size_t)(RAMP_S * DP_S415_FS)
         && !(to_handover && !isnan (r->ho_err_hz)))
    {
      DP_REQUIRE (dp_air_next (&air, x, RAMP_CHUNK));
      (void)dp_async_dsss_receiver_steps (rx, x, RAMP_CHUNK, out, RAMP_CHUNK);
      fed += RAMP_CHUNK;
      if (isnan (r->ho_err_hz) && rx->state == ASYNC_DSSS_RX_TRACKING)
        r->ho_err_hz
            = dp_async_dsss_receiver_get_doppler_hz (rx)
              - dp_air_truth (&air, 0, rx->samples_in - rx->state_samples)
                    .car_hz;
      if (!early_done && fed >= (size_t)(RAMP_S_EARLY * DP_S415_FS))
        {
          early_done    = 1;
          r->lock_early = dp_async_dsss_receiver_get_tracking (rx)
                          && dp_async_dsss_receiver_get_code_locked (rx)
                          && dp_async_dsss_receiver_get_locked (rx);
        }
    }
  const async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  r->code_end = dp_async_dsss_receiver_get_code_locked (rx);
  r->lock_end = dp_async_dsss_receiver_get_tracking (rx) && r->code_end
                && dp_async_dsss_receiver_get_locked (rx);
  r->on_end   = st.state == ASYNC_DSSS_RX_TRACKING
                && fabs (st.doppler_hz - dp_air_truth (&air, 0, fed).car_hz)
                       < RAMP_ON_HZ;
  free (x);
  free (out);
  dp_air_free (&air);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

static void
ramp_rate (const char *what, size_t k, size_t n)
{
  double lo, hi;
  dp_wilson ((double)k, (double)n, &lo, &hi);
  printf ("    %-34s %2zu of %zu [%.3f, %.3f]\n", what, k, n, lo, hi);
}

/* Both arms on the same draws. Asserted: refine false hands over inside the
   header's bound on every draw. Printed: the rest. */
static int
ramp_rows (const uint8_t *code, int check, int emit)
{
  /* --check runs the asserted arm alone, RAMP_CHECK_TRIALS draws a start,
     each to its hand-over: the search hands over the same Doppler on every
     draw from one start, so a few gate it as well as the sweep's thirty.
     The sweep runs both arms to RAMP_S and prints the rates. */
  const uint32_t      trials = check ? RAMP_CHECK_TRIALS : RAMP_TRIALS;
  const int           a0     = check ? 1 : 0;
  static const double f0s[]  = { -1000.0, -500.0 };
  size_t early[2] = { 0, 0 }, end[2] = { 0, 0 }, over[2] = { 0, 0 };
  size_t only[2] = { 0, 0 }, late_wrong[2][3] = { { 0 } }, n = 0;
  double worst[2] = { 0.0, 0.0 }, bound = 0.0;
  if (emit)
    printf ("# ramp\nf0_hz,seed,refine,ho_err_hz,lock_0_5s,lock_2s,"
            "on_2s,code_2s\n");
  else
    printf ("ramp: 415.1-B-1 at 2.8 GHz, +400 Hz/s, refine true and false on "
            "the same draws\n");
  for (size_t f = 0; f < 2; f++)
    for (uint32_t s = 1; s <= trials; s++, n++)
      {
        ramp_t r[2] = { { .ho_err_hz = NAN }, { .ho_err_hz = NAN } };
        for (int a = a0; a < 2; a++) /* a: 0 refine true, 1 refine false */
          {
            DP_REQUIRE (
                run_ramp (code, a == 0, f0s[f], s, check, &bound, &r[a]) == 0);
            const double e = fabs (r[a].ho_err_hz);
            worst[a]       = isnan (e) || e > worst[a] ? e : worst[a];
            over[a] += isnan (e) || e > bound;
            early[a] += (size_t)r[a].lock_early;
            end[a] += (size_t)r[a].lock_end;
            if (!r[a].lock_end) /* not locked at RAMP_S: why */
              late_wrong[a][r[a].on_end ? (r[a].code_end ? 0 : 1) : 2]++;
            if (emit)
              printf ("%.0f,%u,%d,%.2f,%d,%d,%d,%d\n", f0s[f], s, a == 0,
                      r[a].ho_err_hz, r[a].lock_early, r[a].lock_end,
                      r[a].on_end, r[a].code_end);
          }
        if (!check && r[0].lock_early != r[1].lock_early)
          only[r[1].lock_early]++; /* [0]: only as built, [1]: only false */
        DP_CHECK_MSG (!isnan (r[1].ho_err_hz)
                          && fabs (r[1].ho_err_hz) <= bound,
                      "refine false hands over within half a row plus the "
                      "half-dwell age of the truth");
      }
  if (!emit && check)
    printf ("  refine false: hand-over error largest %.1f Hz, %zu of %zu past "
            "%.1f Hz (--check: to the hand-over; the sweep has the rates)\n",
            worst[1], over[1], n, bound);
  else if (!emit)
    {
      static const char *arm[2] = { "refine true (as built)", "refine false" };
      for (int a = 0; a < 2; a++)
        {
          printf ("  %s: hand-over error largest %.1f Hz, %zu of %zu past "
                  "%.1f Hz\n",
                  arm[a], worst[a], over[a], n, bound);
          ramp_rate ("symbol lock at 0.5 s", early[a], n);
          ramp_rate ("symbol lock at 2.0 s", end[a], n);
          printf ("    not locked at 2.0 s: %zu on the emitter with code "
                  "lock, %zu on it without, %zu off it (> %.0f Hz)\n",
                  late_wrong[a][0], late_wrong[a][1], late_wrong[a][2],
                  RAMP_ON_HZ);
        }
      printf ("  paired at 0.5 s: only refine false %zu, only as built "
              "%zu\n",
              only[1], only[0]);
    }
  return 0;
}

/* ── CODE LOCK: the statistic's moments against the detection model ─────
 *
 * #3134. The receiver sizes its code lock's look count from det's model:
 * a per-look power SNR of amp^2 x look, amp = dp_det_cn0_to_snr(C/N0, the
 * tracking rate), look the samples a look integrates -- read here off the
 * receiver's own fields, as adr_build_track_chain() reads them. The Dll
 * forms R = sqrt(2 sum |P|^2 / E|O|^2) over n_looks looks, and publishes
 * one look's two terms an epoch (".prompt_amp", ".offset_pwr"). Their
 * moments give the look SNR the statistic actually sees, E|P|^2 / E|O|^2 -
 * 1, and its loss against the model's.
 *
 * Decisions are read off ".lock" (it changes once a decision); a miss is
 * R below the drop threshold, a drop is ".locked" falling. The drops
 * independence predicts are decisions x miss^n_down; #3134 found them at
 * about 1.5 x that, the decisions sharing the CFAR reference.
 *
 * Window: from CL_SETTLE_S after code lock first declares to the draw's
 * end, while tracking. Printed and emitted, not asserted: it is the
 * evidence #3134's derivation is checked against, not a claim.
 *
 * The TERMS (`# code_lock_terms`). Each draw runs twice on the same scene,
 * once with the noise and once without (dp_air with no noise reference,
 * the emitter at unit power), and the two terms come apart:
 *   signal  E|P|^2 without noise, the look's coherent power, against
 *           (L rho)^2 with L = look / N_p partials (the Dll normalises
 *           each partial by its N_p samples, seg_norm): rho, the
 *           correlation of the received chips with the Dll's rectangular
 *           replica, is what the column reads;
 *   noise   E|O|^2 with noise less E|O|^2 without (the offset tap's
 *           signal sidelobe), against white noise's look sigma^2 / N_p^2,
 *           sigma^2 = fs / (C/N0) a complex sample for a unit-power
 *           emitter (dp_air_init()'s own sizing);
 * and the prompt's noise, E|P|^2 with noise less E|P|^2 without, beside
 * the offset tap's. Both carrier windows, since the code-period one ships.
 * The receiver needs nothing scaled for this: it wipes the carrier with a
 * unit phasor and hands one period of input straight to the Dll. */
#define CL_SETTLE_S 1.0
#define CL_DRAWS 5u
#define CL_S 20.0
#define CL_CHECK_S 5.0

typedef struct
{
  double fc, esn0;
  int    cw;
  size_t n_looks, look, n_down, epochs, decisions, misses, drops;
  double thresh_down, p2, o2, snr_meas, snr_model, loss_db, seg_norm;
} code_lock_t;

static int
run_code_lock (const uint8_t *code, double fc, double esn0, int cw, int noisy,
               uint32_t seed, double secs, code_lock_t *out)
{
  memset (out, 0, sizeof *out);
  out->fc          = fc;
  out->esn0        = esn0;
  out->cw          = cw;
  uint32_t     st  = trial_rng (seed + 200000u);
  const double cn0 = dp_s415_cn0 (esn0);
  const double f0  = (2.0 * dp_uni (&st) - 1.0) * DP_S415_SPAN_HZ;
  const size_t fr
      = (size_t)(dp_uni (&st) * (double)DP_S415_FRAME_SYM) % DP_S415_FRAME_SYM;
  dp_air_emitter_t e
      = dp_s415_emitter (code, fc, f0, 0.0, cn0, 0.0, seed + 200000u);
  e.frame_off = fr;
  dp_air_t air;
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, noisy ? cn0 : NAN,
                           seed * 7919u + 3u, PUSH));
  dp_async_dsss_receiver_state_t *rx  = dp_s415_rx (code, fc, cn0, true, cw);
  dp_tlm_t                       *tlm = dp_tlm_create (1u << 16);
  DP_REQUIRE (rx != NULL && tlm != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, tlm, "rx", 1)
              == DP_OK);
  const int id_lock = dp_tlm_probe_id (tlm, "rx.code.lock");
  const int id_lkd  = dp_tlm_probe_id (tlm, "rx.code.locked");
  const int id_p    = dp_tlm_probe_id (tlm, "rx.code.prompt_amp");
  const int id_o    = dp_tlm_probe_id (tlm, "rx.code.offset_pwr");
  DP_REQUIRE (id_lock >= 0 && id_lkd >= 0 && id_p >= 0 && id_o >= 0);

  size_t cap             = dp_async_dsss_receiver_steps_max_out (rx);
  cap                    = cap ? cap : PUSH;
  float complex *x       = dp_xmalloc (PUSH * sizeof *x);
  float complex *syms    = dp_xmalloc (cap * sizeof *syms);
  dp_tlm_rec_t  *recs    = dp_xmalloc (4096 * sizeof *recs);
  const size_t   n_blk   = (size_t)(secs * DP_S415_FS / PUSH);
  double         t_first = -1.0, last_r = NAN;
  int            last_locked = -1;
  double         sp2 = 0.0, so2 = 0.0;
  size_t         np = 0, no = 0;
  for (size_t b = 0; b < n_blk; b++)
    {
      DP_REQUIRE (dp_air_next (&air, x, PUSH));
      dp_tlm_set_now (tlm, (uint64_t)(b * PUSH));
      (void)dp_async_dsss_receiver_steps (rx, x, PUSH, syms, cap);
      const double t      = (double)(b + 1) * PUSH / DP_S415_FS;
      const int    trk    = rx->state == ASYNC_DSSS_RX_TRACKING;
      const int    locked = trk && dp_async_dsss_receiver_get_code_locked (rx);
      if (t_first < 0.0 && locked)
        t_first = t;
      const int in_win = trk && t_first >= 0.0 && t >= t_first + CL_SETTLE_S;
      size_t    n;
      while ((n = dp_tlm_read (tlm, 4096, recs, 4096)) > 0)
        for (size_t i = 0; in_win && i < n; i++)
          {
            const int    pr = (int)recs[i].probe;
            const double v  = (double)recs[i].value;
            if (pr == id_p)
              sp2 += v * v, np++;
            else if (pr == id_o)
              so2 += v, no++;
            else if (pr == id_lock && v != last_r)
              {
                out->decisions++;
                out->misses += v < rx->dll->lock.down_thresh;
                last_r = v;
              }
            else if (pr == id_lkd)
              {
                if (last_locked == 1 && v < 0.5)
                  out->drops++;
                last_locked = v >= 0.5;
              }
          }
    }
  out->n_looks     = rx->dll->n_looks;
  out->n_down      = rx->dll->lock.n_down;
  out->thresh_down = rx->dll->lock.down_thresh;
  out->seg_norm    = rx->dll->seg_norm;
  const size_t win = dp_dll_get_symbol_window (rx->dll);
  out->look        = (win ? win : 1) * (rx->tsamps / rx->segments);
  out->epochs      = np;
  out->p2          = np ? sp2 / (double)np : NAN;
  out->o2          = no ? so2 / (double)no : NAN;
  out->snr_meas    = out->p2 / out->o2 - 1.0;
  const double amp = dp_det_cn0_to_snr (cn0, rx->chip_rate * (double)rx->spc);
  out->snr_model   = amp * amp * (double)out->look;
  out->loss_db     = 10.0 * log10 (out->snr_model / out->snr_meas);
  (void)dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1);
  dp_tlm_destroy (tlm);
  free (recs);
  free (syms);
  free (x);
  dp_async_dsss_receiver_destroy (rx);
  dp_air_free (&air);
  return 0;
}

static int
code_lock_rows (const uint8_t *code, int check, int emit)
{
  static const char *wname[2] = { "partials", "code_period" };
  /* The terms, one row a draw, kept for their own block after this one. */
  enum
  {
    CL_MAX = 2 * 2 * 2 * 8
  };
  struct
  {
    code_lock_t n, c;
    uint32_t    s;
  }     *rows = dp_xmalloc (CL_MAX * sizeof *rows);
  size_t nr   = 0;
  if (emit)
    printf ("# code_lock\nfc_ghz,esn0_db,window,seed,n_looks,look,n_down,"
            "thresh_down,epochs,p2,o2,snr_look_meas,snr_look_model,loss_db,"
            "decisions,misses,miss_rate,drops,drops_indep\n");
  else
    printf ("code lock: the statistic's terms against det's model, from "
            "%.0f s after it declares (#3134; printed, not asserted)\n",
            CL_SETTLE_S);
  for (size_t f = check ? N_FC - 1 : 0; f < N_FC; f++)
    for (size_t e = 0; e < (check ? 1u : N_ESN0); e++)
      for (int cw = 0; cw < 2; cw++)
        for (uint32_t s = 1; s <= (check ? 1u : CL_DRAWS); s++)
          {
            code_lock_t  c, q;
            const double secs = check ? CL_CHECK_S : CL_S;
            DP_REQUIRE (
                run_code_lock (code, fcs[f], esn0s[e], cw, 1, s, secs, &c)
                == 0);
            DP_REQUIRE (
                run_code_lock (code, fcs[f], esn0s[e], cw, 0, s, secs, &q)
                == 0);
            DP_REQUIRE (nr < CL_MAX);
            rows[nr].n   = c;
            rows[nr].c   = q;
            rows[nr++].s = s;
            const double mr
                = c.decisions ? (double)c.misses / c.decisions : NAN;
            const double indep
                = (double)c.decisions * pow (mr, (double)c.n_down);
            if (emit)
              printf ("%.1f,%.1f,%s,%u,%zu,%zu,%zu,%.4f,%zu,%.6g,%.6g,%.5f,"
                      "%.5f,%.3f,%zu,%zu,%.5f,%zu,%.3f\n",
                      c.fc / 1e9, c.esn0, wname[cw], s, c.n_looks, c.look,
                      c.n_down, c.thresh_down, c.epochs, c.p2, c.o2,
                      c.snr_meas, c.snr_model, c.loss_db, c.decisions,
                      c.misses, mr, c.drops, indep);
            else
              printf ("  %.1f GHz %4.1f dB %-11s seed %u: n %zu, look %zu, "
                      "SNR_look %.3f measured vs %.3f modelled, loss %.2f "
                      "dB; %zu decisions, miss %.4f, %zu drops (%.2f if "
                      "independent)\n",
                      c.fc / 1e9, c.esn0, wname[cw], s, c.n_looks, c.look,
                      c.snr_meas, c.snr_model, c.loss_db, c.decisions, mr,
                      c.drops, indep);
          }
  if (emit)
    printf ("# code_lock_terms\nfc_ghz,esn0_db,window,seed,partials,"
            "signal_p2,rho,signal_db,sidelobe_o2,noise_o2,noise_p2,"
            "noise_pred,noise_o2_db,noise_p2_db\n");
  else
    printf ("code lock terms: each draw again without noise; rho is the "
            "chips' correlation with the replica, noise against white\n");
  for (size_t r = 0; r < nr; r++)
    {
      const code_lock_t *c = &rows[r].n, *q = &rows[r].c;
      const double       L   = (double)c->look / c->seg_norm;
      const double       rho = sqrt (q->p2) / L;
      const double       sig2
          = DP_S415_FS / pow (10.0, dp_s415_cn0 (c->esn0) / 10.0);
      const double npred
          = (double)c->look * sig2 / (c->seg_norm * c->seg_norm);
      const double no = c->o2 - q->o2;
      const double np = c->p2 - q->p2;
      if (emit)
        printf ("%.1f,%.1f,%s,%u,%.4f,%.6g,%.5f,%.3f,%.6g,%.6g,%.6g,%.6g,"
                "%.3f,%.3f\n",
                c->fc / 1e9, c->esn0, wname[c->cw], rows[r].s, L, q->p2, rho,
                -20.0 * log10 (rho), q->o2, no, np, npred,
                10.0 * log10 (no / npred), 10.0 * log10 (np / npred));
      else
        printf ("  %.1f GHz %4.1f dB %-11s seed %u: rho %.4f (signal %.2f dB "
                "down); noise %+.2f dB (offset) %+.2f dB (prompt) against "
                "white; sidelobe %.4g\n",
                c->fc / 1e9, c->esn0, wname[c->cw], rows[r].s, rho,
                -20.0 * log10 (rho), 10.0 * log10 (no / npred),
                10.0 * log10 (np / npred), q->o2);
    }
  free (rows);
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
  uint8_t code[DP_S415_SF];
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

  const size_t want  = check ? 1 : SWEEP_TRIALS;
  const size_t tries = check ? 4 : SWEEP_ATTEMPTS;

  if (emit)
    printf ("# dropout\nfc_ghz,esn0_db,seed,carrier_hz,settled,cut_s,flips,"
            "drop_ms,period_ms,hold_ms,non_monotonic,fell_50,back_50_ms,"
            "fell_200,back_200_ms,held\n");
  else
    printf ("dropout: 415.1-B-1, carrier over +/-%.0f Hz, the signal cut, "
            "the noise on; times printed, not asserted (doppler#3157)\n",
            DP_S415_SPAN_HZ);
  for (size_t f = check ? N_FC - 1 : 0; f < N_FC; f++)
    for (size_t e = 0; e < (check ? 1u : N_ESN0); e++)
      {
        size_t got = 0;
        for (uint32_t s = 1; s <= tries && got < want; s++)
          {
            dropout_t d;
            DP_REQUIRE (run_dropout (code, fcs[f], esn0s[e], s, &d) == 0);
            check_dropout (&d);
            print_dropout (fcs[f], esn0s[e], s, &d, emit);
            got += (size_t)d.settled;
          }
        /* At 10 dB enough settle to measure. At the floor how many do is
           itself a measurement, printed: what must hold there is the floor
           brief's (doppler#3157). */
        if (esn0s[e] >= 10.0)
          DP_CHECK_MSG (got == want,
                        "at 10 dB enough dropout draws settled to measure");
        else if (!emit)
          printf ("  %.1f GHz %4.1f dB: %zu of %zu settled in %zu draws\n",
                  fcs[f] / 1e9, esn0s[e], got, want, (size_t)tries);
      }

  DP_REQUIRE (ramp_rows (code, check, emit) == 0);
  DP_REQUIRE (code_lock_rows (code, check, emit) == 0);

  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_lock");
  DP_TEST_END ("validate_async_dsss_receiver_lock");
}
