/**
 * async_dsss_receiver_lifecycle.c -- one AsyncDsssReceiver through the life
 * of a signal, in C: noise alone, arrival, acquisition, tracking, a short
 * dropout, a long dropout, and a departure.
 *
 * This is the C twin of
 * `src/doppler/examples/async_dsss_receiver_lifecycle.py`, which shows the
 * same timeline with a figure. This file shows what the binding does FOR you
 * and therefore hides:
 *
 *   - create() takes the whole configuration in one call, and the object
 *     copies what it needs; the code array is the caller's to keep only
 *     until create() returns.
 *   - steps() writes into a buffer the CALLER owns. The receiver reports no
 *     capacity to size it by, so the caller sizes it to the largest push it
 *     will make, one code epoch.
 *   - the telemetry is the caller's too: one row per push, read from
 *     status() after the push. Nothing in the receiver keeps a history.
 *   - destroy() frees the receiver's internals; the buffers are freed here.
 *
 * **The waveform is not built here.** The emitter is the shipped
 * `wfm_synth` continuous DSSS -- the code from wfm's own Gold generator,
 * asynchronous BPSK from its own PRBS -- and the noise is the shipped awgn
 * sized from the C/N0. Nothing in this file spreads a chip, draws a bit or
 * computes a sigma.
 *
 * The payload is PRBS data on the code, never gated off, so every dropout
 * cuts the signal mid-data. The Python twin asserts that the data flips at
 * the dropouts by despreading the clean emitter; this file does not repeat
 * that check.
 *
 * Four sections are checked against the receiver's own status, after every
 * push:
 *
 *   §1  Noise alone: the receiver searches and emits nothing.
 *   §2  The signal arrives: it acquires, refines and tracks, and symbols
 *       begin only once it is tracking.
 *   §3  A 10 ms dropout: the symbol lock is up when the signal cuts, and it
 *       holds through the dropout.
 *   §4  A 50 ms dropout: the lock falls inside it while the receiver stays
 *       tracking, then returns after the signal does.
 *   §5  The emitter leaves for good: tracking is kept and the lock falls
 *       once. Nothing enters LOST, because lost_confirm_s is 0.
 *
 * The bounds are physical margins around what the run shows, not a value
 * copied from one draw. The lock-up time after tracking depends on the noise
 * draw, so the example does not assert one; it asserts the lock is up before
 * the first dropout.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_receiver_lifecycle
 */
#include "doppler/dp_complex.h"
#include <doppler/async_dsss_receiver/async_dsss_receiver_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/gold/gold_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── the operating point: Gold-1023 at 3.069 Mcps, 2700 bps BPSK ─────────
 * Two samples per chip, so one code epoch is 2,046 samples: one push. The
 * emitter sits 1500 Hz off the nominal carrier, the search covers one epoch's
 * resolution (1/T_E, about 3 kHz), and Es/N0 is 10 dB. */
#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CARRIER 1500.0
#define ESN0_DB 10.0
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
#define PFA 1e-3
#define DU (FS / (double)TE)
#define SEED_EMITTER 1u
#define SEED_NOISE 7u

/* The timeline, in code epochs: `on` carries the signal, `off` is noise
   alone. One table, read by the signal, the checks and the report. */
struct segment
{
  int    on;
  size_t epochs;
};
static const struct segment TIMELINE[] = {
  { 0, 180 }, /* 60 ms of noise: nothing to find yet */
  { 1, 900 }, /* 300 ms: acquire, refine, and track */
  { 0, 30 },  /* 10 ms dropout: short enough for the lock to hold */
  { 1, 300 }, /* 100 ms back on the air */
  { 0, 150 }, /* 50 ms dropout: long enough for the lock to fall */
  { 1, 180 }, /* 60 ms back on the air: long enough to re-lock */
  { 0, 240 }, /* the emitter leaves for good */
};
#define N_SEG (sizeof TIMELINE / sizeof TIMELINE[0])

/* The state codes status().state reports (the ASYNC_DSSS_RX_* values). */
#define SEARCHING_CODE 0
#define TRACKING_CODE 2
#define LOST_CODE 4

/** @brief One push's telemetry, read from the receiver after the push. */
struct row
{
  int    state;   /* where the receiver is */
  int    locked;  /* the symbol-lock flag */
  double metric;  /* the lock metric */
  size_t symbols; /* symbols that push emitted */
};

#define CHECK(cond, ...)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: " __VA_ARGS__);                             \
          fputc ('\n', stderr);                                               \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/** @brief The first index in [lo, hi) whose lock flag equals @p want, or
 *  @p hi when there is none. */
static size_t
first_locked (const struct row *r, size_t lo, size_t hi, int want)
{
  for (size_t i = lo; i < hi; i++)
    if (r[i].locked == want)
      return i;
  return hi;
}

/** @brief Whether every push in [lo, hi) is in the tracking state. */
static int
all_tracking (const struct row *r, size_t lo, size_t hi)
{
  for (size_t i = lo; i < hi; i++)
    if (r[i].state != TRACKING_CODE)
      return 0;
  return 1;
}

/** @brief One emitter: the shipped continuous-DSSS synth at a carrier
 *  offset, with a PRBS payload on the Gold code. Clean -- the noise is
 *  added once, at the sum. */
static dp_wfm_synth_state_t *
emitter (const uint8_t *code)
{
  dp_wfm_synth_state_t *syn
      = dp_wfm_synth_create (WFM_SYNTH_DSSS, FS, CARRIER, WFM_SYNTH_SNR_CLEAN,
                             1, SEED_EMITTER, (int)SPC, 15, 0, 0, 0.0);
  if (syn
      && dp_wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                     WFM_DSSS_DATA_PRBS, NULL, 0)
             != 0)
    {
      dp_wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

int
main (void)
{
  uint8_t          code[SF];
  dp_gold_state_t *gd = dp_gold_create (934, 350, 567, 73, 10);
  dp_gold_generate (gd, SF, code, SF);
  dp_gold_destroy (gd);

  /* The sources. The synth advances on every push, signal or not, so its
     code and data keep time with the receiver through a dropout. */
  dp_wfm_synth_state_t *syn = emitter (code);
  dp_awgn_state_t      *awg = dp_awgn_create (
      SEED_NOISE,
      dp_awgn_amplitude_for_snr ((float)(CN0_DBHZ - 10.0 * log10 (FS)), 1.0f));
  CHECK (syn && awg, "the emitter and the noise open");

  /* The receiver. create() takes its parameters in signature order, each
     named here, so an arity mismatch shows as one line rather than a count
     of commas. These are the searching flavor's defaults: no carrier
     aiding, and lost_confirm_s of 0, which never enters LOST. */
  const size_t code_len                = SF;
  const double chip_rate               = CHIP_RATE;
  const double symbol_rate             = SYM_RATE;
  const size_t spc                     = SPC;
  const int    m                       = 2; /* BPSK */
  const double cn0_dbhz                = CN0_DBHZ;
  const double pfa                     = PFA;
  const double pd                      = 0.9;
  const double doppler_uncertainty     = DU;
  const size_t segments                = 4;
  const size_t sps                     = 8;
  const int    differential            = 0;
  const double refine_max_error_db     = 0.5;
  const size_t refine_samples_per_sym  = 4;
  const double refine_design_margin_db = 14.0;
  const size_t refine_n_fft            = 64;
  const size_t refine_zero_pad         = 8;
  const bool   refine_sequential       = false;
  const size_t refine_max_n_blocks     = 100000;
  const double carrier_freq_hz         = 0.0; /* no carrier-to-code aiding */
  const double lost_confirm_s          = 0.0; /* never enters LOST */

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz, pfa, pd,
      doppler_uncertainty, segments, sps, differential, refine_max_error_db,
      refine_samples_per_sym, refine_design_margin_db, refine_n_fft,
      refine_zero_pad, refine_sequential, refine_max_n_blocks, carrier_freq_hz,
      lost_confirm_s);
  CHECK (rx != NULL, "the receiver opens");

  /* The caller's buffers. steps() reports no capacity to size its output by,
     so the output holds one push's worth of symbols: a push of TE samples
     emits at most TE symbols, which is always sufficient. The telemetry has
     one row per push of the whole timeline. */
  size_t n_pushes = 0;
  for (size_t s = 0; s < N_SEG; s++)
    n_pushes += TIMELINE[s].epochs;
  float complex *x    = malloc ((size_t)TE * sizeof *x);
  float complex *sig  = malloc ((size_t)TE * sizeof *sig);
  float complex *syms = malloc ((size_t)TE * sizeof *syms);
  struct row    *rows = malloc (n_pushes * sizeof *rows);
  CHECK (x && sig && syms && rows, "the buffers allocate");

  /* ── stream the timeline, one code epoch per push ─────────────────────── */
  size_t k = 0;
  for (size_t s = 0; s < N_SEG; s++)
    for (size_t e = 0; e < TIMELINE[s].epochs; e++, k++)
      {
        dp_wfm_synth_steps (syn, sig, TE);
        dp_awgn_generate (awg, TE, x, TE);
        if (TIMELINE[s].on)
          for (size_t i = 0; i < TE; i++)
            x[i] += sig[i];

        size_t n = dp_async_dsss_receiver_steps (rx, x, TE, syms, TE);
        async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
        rows[k].state                   = st.state;
        rows[k].locked                  = st.locked;
        rows[k].metric                  = st.lock_metric;
        rows[k].symbols                 = n;
      }

  /* ── the checks, on the rows ──────────────────────────────────────────── */
  size_t seg_at[N_SEG + 1];
  seg_at[0] = 0;
  for (size_t s = 0; s < N_SEG; s++)
    seg_at[s + 1] = seg_at[s] + TIMELINE[s].epochs;

  const size_t on_at = seg_at[1];                  /* signal arrives */
  const size_t d1 = seg_at[2], d1_end = seg_at[3]; /* 10 ms dropout */
  const size_t d2 = seg_at[4], d2_end = seg_at[5]; /* 50 ms dropout */
  const size_t back_end = seg_at[6];               /* signal back on */
  const size_t gone     = seg_at[6];               /* departure */
  const double ms       = 1e3 * TE / FS;           /* one push, ms */

  size_t first = n_pushes;
  for (size_t i = 0; i < n_pushes; i++)
    if (rows[i].state == TRACKING_CODE)
      {
        first = i;
        break;
      }
  CHECK (first < n_pushes, "the receiver never reached tracking");

  /* §1 and §2: searching first, symbols only once tracking. */
  for (size_t i = 0; i < on_at; i++)
    CHECK (rows[i].state != TRACKING_CODE,
           "tracking before the signal arrived (push %zu)", i);
  double acquire_ms = (double)(first - on_at) * ms;
  printf ("§1  noise alone for %.0f ms: searching, no symbols\n",
          (double)on_at * ms);
  printf ("§2  arrival to tracking: %.0f ms\n", acquire_ms);
  CHECK (acquire_ms > 5.0 && acquire_ms < 250.0, "arrival to tracking %.0f ms",
         acquire_ms);
  for (size_t i = 0; i < first; i++)
    CHECK (rows[i].symbols == 0, "symbols emitted before tracking (push %zu)",
           i);
  CHECK (all_tracking (rows, first, n_pushes),
         "left tracking without a release rule");
  for (size_t i = 0; i < n_pushes; i++)
    CHECK (rows[i].state != LOST_CODE, "entered LOST with lost_confirm_s = 0");

  /* §3: the lock is up before the first dropout, and holds through it. */
  CHECK (rows[d1 - 1].locked == 1, "the lock was not up before the dropout");
  CHECK (first_locked (rows, d1, d1_end, 0) == d1_end,
         "the lock fell in a 10 ms dropout");
  printf ("§3  a 10 ms dropout: the lock held\n");

  /* §4: the lock falls inside the 50 ms dropout and returns after the
     signal does, while tracking holds throughout. */
  size_t fell = first_locked (rows, d2, d2_end, 0);
  CHECK (fell < d2_end, "the lock never fell in a 50 ms dropout");
  double fall_ms = (double)(fell - d2) * ms;
  CHECK (fall_ms > 5.0 && fall_ms < 45.0,
         "the lock fell %.0f ms into the dropout", fall_ms);
  size_t back = first_locked (rows, d2_end, back_end, 1);
  CHECK (back < back_end, "the lock never returned");
  double return_ms = (double)(back - d2_end) * ms;
  CHECK (return_ms > 0.0 && return_ms < 80.0,
         "the lock returned %.0f ms after the signal", return_ms);
  printf ("§4  a 50 ms dropout: the lock fell %.0f ms in and returned %.0f ms "
          "after the signal\n",
          fall_ms, return_ms);

  /* §5: the emitter leaves for good. Tracking is kept; the lock falls once.
   */
  size_t gone_fall = first_locked (rows, gone, n_pushes, 0);
  CHECK (gone_fall < n_pushes, "the lock never fell after departure");
  double gone_ms = (double)(gone_fall - gone) * ms;
  CHECK (gone_ms > 5.0 && gone_ms < 60.0,
         "the lock fell %.0f ms after departure", gone_ms);
  CHECK (all_tracking (rows, gone, n_pushes),
         "departure moved the receiver out of tracking");
  printf ("§5  departure: the lock fell %.0f ms later; still tracking: %d\n",
          gone_ms, all_tracking (rows, gone, n_pushes));

  /* The receiver owns its internals; destroy() frees them. The buffers are
     the caller's, and the caller frees them. */
  dp_async_dsss_receiver_destroy (rx);
  free (rows);
  free (syms);
  free (sig);
  free (x);
  dp_awgn_destroy (awg);
  dp_wfm_synth_destroy (syn);
  printf ("OK\n");
  return 0;
}
