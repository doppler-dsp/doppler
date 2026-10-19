/**
 * async_dsss_receiver_lifecycle.c -- one AsyncDsssReceiver through a signal's
 * life, in C: searching, acquisition and tracking, a short fade, a departure
 * that releases the emitter, a reset, and a returning emitter acquired afresh.
 *
 * This is the C twin of
 * `src/doppler/examples/async_dsss_receiver_lifecycle.py`, which runs the same
 * timeline and asserts the same physics, with a figure. This file shows what
 * the binding does for you and therefore hides:
 *
 *   - create() takes the whole configuration in one call, and the object
 *     copies what it needs; the code array is the caller's to keep only
 *     until create() returns.
 *   - steps() writes into a buffer the CALLER owns. The receiver reports no
 *     capacity to size it by, so the caller sizes it to the largest push it
 *     will make, one code epoch.
 *   - the telemetry is the receiver's: set_telemetry() registers its probes
 *     on a dp_tlm context the CALLER owns, and a memory capture keeps every
 *     record. The receiver keeps no history; the capture is the history,
 *     and the rows below are read from it.
 *   - the receiver decides that an emitter has gone (LOST); the caller
 *     decides what to do about it, here reset(), which returns it to
 *     searching.
 *   - destroy() frees the receiver's internals; the buffers are freed here.
 *
 * The waveform is not built here. The emitter is the shipped `wfm_synth`
 * continuous DSSS -- the Gold code from wfm's own generator, asynchronous
 * BPSK from its own PRBS -- and the noise is the shipped awgn at the sigma
 * dp_wfm_awgn_amplitude() gives for the Es/N0. That is wfm's face of the awgn
 * primitive, dp_awgn_amplitude_for_snr(), and the function the Python twin
 * calls as wfm_awgn_amplitude. Nothing in this file spreads a chip, draws a
 * bit or computes a sigma.
 * The payload is the synth's own PRBS, sent on the code throughout.
 *
 * The checks are against the receiver's own telemetry, one row a push:
 *
 *   §1  Noise alone: the receiver searches and emits nothing.
 *   §2  The signal arrives: it acquires and tracks, and symbols begin only
 *       once it is tracking.
 *   §3  A 10 ms fade: in this run, tracking and the symbol lock hold through
 *       it. Lock-up time varies by draw.
 *   §4  A 0.3 s departure: the receiver goes LOST, no earlier than its confirm
 *       interval; the caller resets it, and the reset receiver is searching.
 *   §5  A returning emitter: it is acquired afresh, emits no symbols until it
 *       tracks, and emits them once it does.
 *
 * The bounds are physical where a bound exists (the confirm interval, and no
 * symbols before tracking). The rest are generous windows around one run.
 *
 * Both twins run one draw: the same seeds (SEED_EMITTER, SEED_NOISE), the
 * same sources and the same sigma, so they print the same five lines.
 * src/doppler/tests/test_c_example_pairs.py runs both and requires that.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_receiver_lifecycle
 */
#include "doppler/dp_complex.h"
#include <doppler/async_dsss_receiver/async_dsss_receiver_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/dp_tlm/dp_tlm_core.h>
#include <doppler/dp_tlm_capture/dp_tlm_capture_core.h>
#include <doppler/gold/gold_core.h>
#include <doppler/timing/timing_core.h>
#include <doppler/wfm/wfm_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Setup: this scenario's numbers ─────────────────────────────────────────
 * Gold-1023 at 3.069 Mcps, 2700 bps BPSK, two samples per chip, so one code
 * epoch is 2,046 samples: one push. The emitter sits 1500 Hz off nominal, the
 * caller's carrier uncertainty is 5 kHz either side, and Es/N0 is 10 dB. */
#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CARRIER 1500.0
#define CARRIER_UNCERTAINTY 5000.0
#define ESN0_DB 10.0
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
#define PFA 1e-3
#define LOST_CONFIRM_S 0.1
/* The Python twin's draw: the synth's default seed, and noise seed 2. */
#define SEED_EMITTER 0u
#define SEED_NOISE 2u

/* The timeline, in code epochs: `on` carries the signal, `off` is noise
   alone. One table, read by the signal, the checks and the report. */
struct segment
{
  int    on;
  size_t epochs;
};
static const struct segment TIMELINE[] = {
  { 0, 180 },  /* 60 ms of noise: nothing to find yet */
  { 1, 4500 }, /* 1.5 s: acquire, track, and let the lock come up */
  { 0, 30 },   /* 10 ms fade: shorter than the lock detectors' memory */
  { 1, 300 },  /* 100 ms back on the air */
  { 0, 900 },  /* 0.3 s: the emitter leaves, longer than the confirm time */
  { 1, 4500 }, /* a returning emitter, on the air for 1.5 s */
};
#define N_SEG (sizeof TIMELINE / sizeof TIMELINE[0])

/** @brief One push, as the receiver's records left it. */
struct row
{
  int    state;       /* where the receiver is: "rx.state" */
  int    locked;      /* the symbol-lock flag: "rx.sym.locked" */
  int    code_locked; /* the code-lock flag: "rx.code.locked" */
  double metric;      /* the lock metric: "rx.sym.lock" */
  size_t symbols; /* symbols that push emitted: its "rx.sym.lock" records */
  double handover_hz; /* the Doppler the chain was built from */
};

/** @brief The rows, from the capture's records.
 *
 *  Each probe reports at its own rate: the state once a push, the symbol
 *  lock once a symbol, the code lock once a discriminator update. Records
 *  arrive in emission order, each stamped with its push's first sample, so a
 *  push's row is the values the records left as the next push began, carried
 *  over pushes a probe wrote nothing in. Returns the pushes that reported a
 *  state. */
static size_t
rows_from (const dp_tlm_t *tlm, const dp_tlm_rec_t *rec, size_t n_rec,
           struct row *rows, size_t n_pushes)
{
  const int  id_state  = dp_tlm_probe_id (tlm, "rx.state");
  const int  id_locked = dp_tlm_probe_id (tlm, "rx.sym.locked");
  const int  id_code   = dp_tlm_probe_id (tlm, "rx.code.locked");
  const int  id_metric = dp_tlm_probe_id (tlm, "rx.sym.lock");
  const int  id_ho     = dp_tlm_probe_id (tlm, "rx.handover_hz");
  struct row cur       = { 0, 0, 0, 0.0, 0, 0.0 };
  size_t     k = 0, states = 0;
  for (size_t i = 0; i < n_rec; i++)
    {
      for (; k < rec[i].n / TE && k < n_pushes; k++, cur.symbols = 0)
        rows[k] = cur;
      const int   id = rec[i].probe;
      const float v  = rec[i].value;
      if (id == id_state)
        {
          cur.state = (int)v;
          states++;
        }
      else if (id == id_locked)
        cur.locked = v > 0.5f;
      else if (id == id_code)
        cur.code_locked = v > 0.5f;
      else if (id == id_metric)
        {
          cur.metric = v;
          cur.symbols++; /* the metric reports once an emitted symbol */
        }
      else if (id == id_ho)
        cur.handover_hz = v;
    }
  for (; k < n_pushes; k++, cur.symbols = 0)
    rows[k] = cur;
  return states;
}

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

/** @brief The first index in [lo, hi) in the @p want state, or @p hi. */
static size_t
first_state (const struct row *r, size_t lo, size_t hi, int want)
{
  for (size_t i = lo; i < hi; i++)
    if (r[i].state == want)
      return i;
  return hi;
}

/** @brief Whether every push in [lo, hi) is in the tracking state. */
static int
all_tracking (const struct row *r, size_t lo, size_t hi)
{
  for (size_t i = lo; i < hi; i++)
    if (r[i].state != ASYNC_DSSS_RX_TRACKING)
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
  CHECK (gd != NULL, "the Gold generator opens");
  dp_gold_generate (gd, SF, code, SF);
  dp_gold_destroy (gd);

  /* The sources. The synth advances on every push, signal or not, so its
     code and data keep time with the receiver through every cut. */
  dp_wfm_synth_state_t *syn = emitter (code);
  dp_awgn_state_t      *awg = dp_awgn_create (
      SEED_NOISE, dp_wfm_awgn_amplitude (
                      (float)(ESN0_DB - 10.0 * log10 (FS / SYM_RATE)), 1.0f));
  CHECK (syn && awg, "the emitter and the noise open");

  /* The receiver. create() takes its parameters in signature order, each
     named here, so an arity mismatch shows as one line rather than a count
     of commas. The caller's carrier uncertainty is one-sided, and the release
     rule is armed by the confirm interval. Everything else is the default. */
  const size_t code_len                = SF;
  const double chip_rate               = CHIP_RATE;
  const double symbol_rate             = SYM_RATE;
  const size_t spc                     = SPC;
  const int    m                       = 2; /* BPSK */
  const double cn0_dbhz                = CN0_DBHZ;
  const double pfa                     = PFA;
  const double pd                      = 0.9;
  const double doppler_uncertainty     = CARRIER_UNCERTAINTY;
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
  const double lost_confirm_s          = LOST_CONFIRM_S;

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz, pfa, pd,
      doppler_uncertainty, segments, sps, differential, refine_max_error_db,
      refine_samples_per_sym, refine_design_margin_db, refine_n_fft,
      refine_zero_pad, refine_sequential, refine_max_n_blocks, carrier_freq_hz,
      lost_confirm_s, false);
  CHECK (rx != NULL, "the receiver opens");

  /* The telemetry. set_telemetry() registers every probe the receiver and
     its loops publish, under "rx."; the memory capture opens after it, since
     the probes set its bound, and keeps every record. dp_tlm_set_now() at
     each push stamps the records with the push's first sample, and drains
     the push before. */
  dp_tlm_t *tlm = dp_tlm_create (1u << 12);
  CHECK (tlm != NULL, "the telemetry context opens");
  CHECK (dp_async_dsss_receiver_set_telemetry (rx, tlm, "rx", 1) == DP_OK,
         "the receiver's telemetry attaches");
  dp_sample_clock_t clk;
  dp_sample_clock_init (&clk, FS, 0);
  dp_tlm_capture_t *cap = dp_tlm_capture_open_memory (tlm, TE, &clk);
  CHECK (cap != NULL, "the capture opens");

  /* The caller's buffers. steps_max_out() returns 0 for this receiver, which
     means "allocate x_len": a push of TE samples emits at most TE symbols. So
     the output is sized to the larger of the reported capacity and one push,
     which is sufficient in both readings. The rows are one per push of the
     whole timeline. */
  size_t n_pushes = 0;
  for (size_t s = 0; s < N_SEG; s++)
    n_pushes += TIMELINE[s].epochs;
  size_t syms_cap = dp_async_dsss_receiver_steps_max_out (rx);
  if (syms_cap < TE)
    syms_cap = TE;
  float complex *x    = malloc ((size_t)TE * sizeof *x);
  float complex *sig  = malloc ((size_t)TE * sizeof *sig);
  float complex *syms = malloc (syms_cap * sizeof *syms);
  struct row    *rows = malloc (n_pushes * sizeof *rows);
  CHECK (x && sig && syms && rows, "the buffers allocate");

  /* ── stream the timeline, one code epoch per push ─────────────────────── */
  size_t k = 0, emitted = 0;
  for (size_t s = 0; s < N_SEG; s++)
    for (size_t e = 0; e < TIMELINE[s].epochs; e++, k++)
      {
        dp_wfm_synth_steps (syn, sig, TE);
        dp_awgn_generate (awg, TE, x, TE);
        if (TIMELINE[s].on)
          for (size_t i = 0; i < TE; i++)
            x[i] += sig[i];

        dp_tlm_set_now (tlm, (uint64_t)k * TE);
        emitted += dp_async_dsss_receiver_steps (rx, x, TE, syms, syms_cap);
        /* The receiver has decided the emitter is gone. The caller acts on
           it: reset() returns the receiver to searching. */
        if (dp_async_dsss_receiver_get_lost (rx))
          dp_async_dsss_receiver_reset (rx);
      }
  /* The last push's boundary. DP_OK means no record was dropped. */
  CHECK (dp_tlm_capture_close (cap) == DP_OK, "the capture lost records");
  const size_t states = rows_from (tlm, dp_tlm_capture_records (cap),
                                   dp_tlm_capture_count (cap), rows, n_pushes);
  CHECK (states == n_pushes, "%zu of %zu pushes reported a state", states,
         n_pushes);
  /* The lock metric reports once an emitted symbol: its records count them. */
  size_t counted = 0;
  for (size_t i = 0; i < n_pushes; i++)
    counted += rows[i].symbols;
  CHECK (counted == emitted,
         "%zu symbols counted from the records, %zu "
         "emitted",
         counted, emitted);

  /* ── the checks, on the rows ──────────────────────────────────────────── */
  size_t seg_at[N_SEG + 1];
  seg_at[0] = 0;
  for (size_t s = 0; s < N_SEG; s++)
    seg_at[s + 1] = seg_at[s] + TIMELINE[s].epochs;

  const size_t on_at  = seg_at[1]; /* the first arrival */
  const size_t fade   = seg_at[2]; /* the short fade */
  const size_t depart = seg_at[4]; /* the departure */
  const size_t back   = seg_at[5]; /* the return */

  const double ms = 1e3 * TE / FS; /* one push, ms */

  /* §1 and §2: searching first, symbols only once tracking. */
  size_t first = first_state (rows, 0, n_pushes, ASYNC_DSSS_RX_TRACKING);
  CHECK (first < n_pushes, "the receiver never reached tracking");
  for (size_t i = 0; i < on_at; i++)
    CHECK (rows[i].state == ASYNC_DSSS_RX_SEARCHING,
           "left searching before the signal arrived (push %zu)", i);
  double acquire_ms = (double)(first - on_at) * ms;
  printf ("§1  noise alone for %.0f ms: searching, no symbols\n",
          (double)on_at * ms);
  size_t lock_up = first_locked (rows, on_at, n_pushes, 1);
  printf ("§2  arrival to tracking: %.0f ms, handed over at %.0f Hz (the "
          "emitter at %.0f Hz); symbol lock up %.0f ms after arrival\n",
          acquire_ms, rows[first].handover_hz, CARRIER,
          (double)(lock_up - on_at) * ms);
  CHECK (acquire_ms > 5.0 && acquire_ms < 250.0, "arrival to tracking %.0f ms",
         acquire_ms);
  /* The refine hands the tracker a Doppler inside the caller's
     uncertainty. */
  CHECK (fabs (rows[first].handover_hz) <= CARRIER_UNCERTAINTY,
         "handed over at %.0f Hz, outside the search",
         rows[first].handover_hz);
  for (size_t i = 0; i < first; i++)
    CHECK (rows[i].symbols == 0, "symbols emitted before tracking (push %zu)",
           i);

  /* §3: the lock is up before the fade, and from the first tracking push to
     the departure the receiver tracks, the lock stays up, and nothing enters
     LOST. The fade is inside that stretch, so one check covers it. */
  CHECK (rows[fade - 1].locked == 1, "the lock was not up before the fade");
  CHECK (all_tracking (rows, first, depart),
         "left tracking before the departure");
  CHECK (first_locked (rows, fade, depart, 0) == depart,
         "the symbol lock fell before the departure");
  CHECK (first_state (rows, first, depart, ASYNC_DSSS_RX_LOST) == depart,
         "entered LOST before the departure");
  printf ("§3  a %.0f ms fade: tracking and the symbol lock held\n",
          (double)(seg_at[3] - fade) * ms);

  /* §4: the departure releases the emitter. LOST comes only after the confirm
     interval, since both flags must be down for longer than that first. */
  size_t released = first_state (rows, depart, back, ASYNC_DSSS_RX_LOST);
  CHECK (released < back, "the departure never released the emitter");
  double release_ms = (double)(released - depart) * ms;
  CHECK ((double)(released - depart) * TE / FS >= LOST_CONFIRM_S,
         "released %.0f ms after departure, before the confirm interval",
         release_ms);
  /* Both flags are down for the whole confirm interval before the release. */
  const size_t confirm = (size_t)ceil (LOST_CONFIRM_S / ((double)TE / FS));
  for (size_t i = released - confirm; i < released; i++)
    CHECK (!rows[i].locked && !rows[i].code_locked,
           "a lock flag was up during the confirm interval (push %zu)", i);
  for (size_t i = released + 1; i < back; i++)
    CHECK (rows[i].state == ASYNC_DSSS_RX_SEARCHING,
           "left searching during the gap after the reset (push %zu)", i);
  printf ("§4  departure: released %.0f ms later, after the %.0f ms confirm "
          "interval\n",
          release_ms, 1e3 * LOST_CONFIRM_S);

  /* §5: the returning emitter is acquired afresh. It emits no symbols until
     it tracks, and it tracks again within the bound. */
  size_t reacq = first_state (rows, back, n_pushes, ASYNC_DSSS_RX_TRACKING);
  CHECK (reacq < n_pushes, "the returning emitter was never acquired");
  for (size_t i = released + 1; i < reacq; i++)
    CHECK (rows[i].symbols == 0,
           "symbols emitted before the returning emitter was tracked (push "
           "%zu)",
           i);
  double return_ms = (double)(reacq - back) * ms;
  CHECK (return_ms > 5.0 && return_ms < 250.0,
         "re-acquired %.0f ms after the return", return_ms);
  size_t syms_after = 0;
  for (size_t i = reacq; i < n_pushes; i++)
    syms_after += rows[i].symbols;
  CHECK (syms_after > 0, "no symbols after re-acquiring");
  printf ("§5  the return: re-acquired %.0f ms after it began; %zu symbols "
          "since\n",
          return_ms, syms_after);

  /* The receiver owns its internals; destroy() frees them. The buffers, the
     capture and the context are the caller's, and the caller frees them,
     the receiver first: it borrows the context until then. */
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_capture_destroy (cap);
  dp_tlm_destroy (tlm);
  free (rows);
  free (syms);
  free (sig);
  free (x);
  dp_awgn_destroy (awg);
  dp_wfm_synth_destroy (syn);
  printf ("OK\n");
  return 0;
}
