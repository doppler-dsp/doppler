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
 * and noise; the receiver is fed ARRIVAL_S. Recorded, from the arrival
 * sample, to one block: when the receiver first reads tracking; the refine
 * stage's carrier there (get_doppler_hz(), frozen once tracking) minus the
 * truth -- the residual the carrier loop must pull in, which
 * `refine_bias.c` measures on its own and doppler#982 found handed over
 * outside the loop's bound; when symbol lock first comes; whether it is up
 * at the end; and the SNR the receiver's own symbols carry over its last
 * M2M4_SYMS (dp_test_m2m4_snr_db, truth-free), which tells a lock on a
 * pulled-in carrier from one that is not.
 *
 * DROPOUT. A trial, at 1,500 Hz on its own data and noise, is fed until it
 * tracks with symbol lock up for SETTLE_BLOCKS blocks in a row (or gives
 * up at MAX_LOCK_S, a row marked unsettled: not every draw locks, which
 * ARRIVAL measures), then a further uniform 0 to JITTER_S, then cut at a
 * uniform sample of the next block. The receiver's state there is saved and
 * the next POST_S of signal and noise rendered once, so every case below
 * resumes from the same state on the same noise and differs only in the
 * dropout (the SIGNAL is cut; the noise runs on):
 *
 *   for good       when symbol lock falls after the cut (or `-1`, held
 *                  through DROP_WATCH_S)
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
 * What is asserted, the same in the sweep and in --check: every row is
 * mid-data (its flips within FLIPS_MIN..FLIPS_MAX); at 10 dB a signal is
 * tracked within CHECK_TRACK_MS of arriving (measured 44-47 ms, every
 * draw); and in every settled dropout trial a loss for good drops symbol
 * lock within DROP_WATCH_S (measured 11-43 ms). The rest -- lock times,
 * seed errors, holds, re-locks, the 5 dB rows -- is measured and printed,
 * not asserted: a fraction that moves with the draw is a finding for the
 * record, not a gate. A hold of LOCK_N_DOWN symbols is NOT a property of
 * the detector, though it reads like one: the first sweep asserted it and
 * four 5 dB trials held 2.7-4.7 ms, the average sunk in the dropout and
 * its recovery after supplying the run of low symbols.
 *
 * Usage:
 *   validate_async_dsss_receiver_lock            the sweep, 10 and 5 dB
 *   validate_async_dsss_receiver_lock --check    10 dB, three arrivals and
 *                                                one dropout trial
 *   ... --emit                                   CSV blocks on stdout for
 *                                                the record: `# arrival`
 *                                                and `# dropout`, one row
 *                                                a trial
 */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CPS (CHIP_RATE / SYM_RATE) /* chips a symbol, 1136.7 */
#define SPAN (FS / (double)TE)     /* 1/T_E: 3,000 Hz                */
#define EPOCH_MS (1e3 * (double)TE / FS)

#define ARRIVAL_S 2.0        /* each arrival trial's run              */
#define ARRIVAL_DELAY_S 0.02 /* noise alone for up to this, first     */
#define M2M4_SYMS 1000u      /* the receiver's last symbols, scored   */
#define FLIPS_SYMS 270u      /* 100 ms of symbols each side of an event */
#define FLIPS_MIN 0.35       /* mid-data: random bits flip at about    */
#define FLIPS_MAX 0.65       /* half their boundaries                  */
#define CHECK_TRACK_MS 100.0 /* 10 dB: tracked within this of arrival */

#define DROPOUT_HZ 1500.0
#define SETTLE_BLOCKS 200u /* symbol lock up this long, then the cut */
#define MAX_LOCK_S 4.0     /* give up on a draw that never settles   */
#define JITTER_S 0.01      /* then up to this more, then a random sample */
#define POST_S 0.6         /* rendered after the cut, for every case */
#define DROP_WATCH_S 0.08  /* a loss for good must drop lock by this */
#define HOLD_WATCH_S 0.06  /* a held dropout: lock up this long after */
#define RELOCK_WATCH_S 0.3 /* a re-lock is waited for this long      */
#define MAX_HOLD 400u      /* the hold scan's longest dropout, epochs */

static const double relock_ms[] = { 50.0, 200.0 };
#define N_RELOCK (sizeof relock_ms / sizeof relock_ms[0])

static const double esn0s[] = { 10.0, 5.0 };
#define N_ESN0 (sizeof esn0s / sizeof esn0s[0])
#define SWEEP_TRIALS 20u   /* arrival trials, and settled dropout trials */
#define SWEEP_ATTEMPTS 40u /* dropout draws tried for them, at most    */

static double
cn0_of (double esn0_db)
{
  return esn0_db + 10.0 * log10 (SYM_RATE);
}

/* The receiver: the searching flavor at the binding's defaults
   (objects/async_dsss_receiver.toml), its own search over +/-1/T_E. */
static dp_async_dsss_receiver_state_t *
make_rx (const uint8_t *code, double cn0_dbhz)
{
  return dp_async_dsss_receiver_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0_dbhz, 1e-3, 0.9, SPAN, 4, 8,
      0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0);
}

/* The noise: the shipped awgn, its amplitude the library's answer for a
   unit-power emitter at this C/N0 over fs. */
static dp_awgn_state_t *
make_noise (uint32_t seed, double cn0_dbhz)
{
  return dp_awgn_create (
      seed * 7919u + 1u,
      dp_awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
}

/* A trial's own stream: adjacent seeds share their first xorshift draws,
   so the golden-ratio mix spreads them (dp_rng_test.h's note). */
static uint32_t
trial_rng (uint32_t seed)
{
  uint32_t st = seed * 0x9e3779b9u;
  for (int w = 0; w < 4; w++)
    (void)dp_xs32 (&st);
  return st;
}

/* The data, one 0/1 bit a symbol, from the trial's stream. */
static uint8_t *
draw_bits (uint32_t *st, size_t n)
{
  uint8_t *bits = dp_xmalloc (n);
  for (size_t i = 0; i < n; i++)
    bits[i] = dp_bit (st) < 0 ? 1u : 0u;
  return bits;
}

/* The symbol carrying sample `n` of the emitter's stream: its chip over
   the chips a symbol, the synth's own clock to within one symbol. */
static size_t
symbol_at (size_t n)
{
  return (size_t)((double)n / (double)SPC / CPS);
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
  int    locked_end;  /* symbol lock up after the last block           */
  double m2m4_db;     /* SNR of the last M2M4_SYMS symbols; NAN: fewer */
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
  const size_t blocks = (size_t)(ARRIVAL_S * FS / (double)TE);
  const size_t n_bits = symbol_at (blocks * TE) + 2;
  uint8_t     *bits   = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn
      = dp_dsss_cont_emitter (code, SF, SPC, FS, out->carrier_hz, CPS, seed,
                              WFM_DSSS_DATA_BITS, bits, n_bits);
  dp_awgn_state_t                *g   = make_noise (seed, cn0);
  dp_async_dsss_receiver_state_t *rx  = make_rx (code, cn0);
  float complex                  *sig = dp_xmalloc (TE * sizeof *sig);
  float complex                  *blk = dp_xmalloc (TE * sizeof *blk);
  size_t         cap  = dp_async_dsss_receiver_steps_max_out (rx);
  float complex *syms = dp_xmalloc ((cap ? cap : TE) * sizeof *syms);
  float complex *last = dp_xmalloc (M2M4_SYMS * sizeof *last);
  if (!syn || !g || !rx)
    return 1;

  out->t_track_ms = out->seed_err_hz = out->t_lock_ms = NAN;
  size_t n_last = 0, w = 0;
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
      size_t n
          = dp_async_dsss_receiver_steps (rx, blk, TE, syms, cap ? cap : TE);
      for (size_t i = 0; i < n; i++, w = (w + 1) % M2M4_SYMS)
        {
          last[w] = syms[i];
          if (n_last < M2M4_SYMS)
            n_last++;
        }
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
  out->m2m4_db
      = n_last == M2M4_SYMS ? dp_test_m2m4_snr_db (last, M2M4_SYMS) : NAN;
  out->flips = flips_near (bits, n_bits, symbol_at (delay), 1);

  free (last);
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

static size_t
blocks_for (size_t samples)
{
  return (samples + TE - 1) / TE;
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
  const size_t jitter_b = (size_t)(dp_uni (&st) * JITTER_S * FS / TE);
  const size_t at       = (size_t)(dp_uni (&st) * (double)TE);
  const size_t post_n   = (size_t)(POST_S * FS / (double)TE) * TE;
  const size_t max_n    = (lock_b + jitter_b + 2) * TE + post_n;
  const size_t n_bits   = symbol_at (max_n) + 2;
  uint8_t     *bits     = draw_bits (&st, n_bits);

  dp_wfm_synth_state_t *syn = dp_dsss_cont_emitter (
      code, SF, SPC, FS, DROPOUT_HZ, CPS, seed + 100000u, WFM_DSSS_DATA_BITS,
      bits, n_bits);
  dp_awgn_state_t                *g   = make_noise (seed + 100000u, cn0);
  dp_async_dsss_receiver_state_t *rx  = make_rx (code, cn0);
  float complex                  *sig = dp_xmalloc (TE * sizeof *sig);
  float complex                  *blk = dp_xmalloc (TE * sizeof *blk);
  size_t         cap  = dp_async_dsss_receiver_steps_max_out (rx);
  float complex *syms = dp_xmalloc ((cap ? cap : TE) * sizeof *syms);
  if (!syn || !g || !rx)
    return 1;
  cap = cap ? cap : TE;

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
    uint8_t *lk = dp_xmalloc (post_n / TE);

    /* Gone for good. */
    size_t m = blocks_for (at + (size_t)(DROP_WATCH_S * FS));
    DP_REQUIRE (
        branch (rx, blob, sigp, noip, at, SIZE_MAX, m, blk, syms, cap, lk)
        == 0);
    for (size_t i = 0; i < m; i++)
      if (!lk[i])
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
    for (size_t j = 1; j <= longest; j++)
      {
        size_t fade = j * TE;
        m           = blocks_for (at + fade + hold_watch);
        DP_REQUIRE (
            branch (rx, blob, sigp, noip, at, fade, m, blk, syms, cap, lk)
            == 0);
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
    printf ("%.1f,%u,%.1f,%.6f,%.2f,%.1f,%.2f,%d,%.2f,%.3f\n", esn0_db, seed,
            a->carrier_hz, a->delay_s, a->t_track_ms, a->seed_err_hz,
            a->t_lock_ms, a->locked_end, a->m2m4_db, a->flips);
  else
    printf ("  %4.1f dB seed %2u: carrier %+7.1f Hz, tracking %6.1f ms, "
            "seed error %+7.1f Hz, symbol lock %7.1f ms, locked at end %d, "
            "m2m4 %5.1f dB, flips %.2f\n",
            esn0_db, seed, a->carrier_hz, a->t_track_ms, a->seed_err_hz,
            a->t_lock_ms, a->locked_end, a->m2m4_db, a->flips);
}

static void
print_dropout (double esn0_db, uint32_t seed, const dropout_t *d, int emit)
{
  if (emit)
    {
      printf ("%.1f,%u,%d,%.6f,%.3f,%.2f,%.2f,%d,", esn0_db, seed, d->settled,
              d->cut_s, d->flips, d->drop_ms,
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
      printf ("  %4.1f dB seed %2u: not settled within %.0f s\n", esn0_db,
              seed, MAX_LOCK_S);
      return;
    }
  printf ("  %4.1f dB seed %2u: lock falls %5.1f ms after a loss, holds "
          "%5.1f ms%s; back %5.1f ms after a %g ms dropout, %5.1f ms after "
          "%g ms; flips %.2f\n",
          esn0_db, seed, d->drop_ms, (double)d->hold_epochs * EPOCH_MS,
          d->non_monotonic ? " (NON-MONOTONIC)" : "", d->back_ms[0],
          relock_ms[0], d->back_ms[1], relock_ms[1], d->flips);
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

  const size_t n_esn0 = check ? 1 : N_ESN0;
  const size_t n_arr  = check ? 3 : SWEEP_TRIALS;
  const size_t want   = check ? 1 : SWEEP_TRIALS;
  const size_t tries  = check ? 4 : SWEEP_ATTEMPTS;

  if (emit)
    printf ("# arrival\nesn0_db,seed,carrier_hz,delay_s,t_track_ms,"
            "seed_err_hz,t_lock_ms,locked_end,m2m4_db,flips\n");
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
    printf ("# dropout\nesn0_db,seed,settled,cut_s,flips,drop_ms,hold_ms,"
            "non_monotonic,fell_50,back_50_ms,fell_200,back_200_ms,held\n");
  else
    printf ("dropout: %.0f Hz, the signal cut, the noise on\n", DROPOUT_HZ);
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

  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_lock");
  DP_TEST_END ("validate_async_dsss_receiver_lock");
}
