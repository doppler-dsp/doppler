/**
 * async_dsss_receiver_spec_demo.c -- one AsyncDsssReceiver on a link whose
 * Doppler moves the code clock as well as the carrier, in C.
 *
 * This is the C twin of
 * `src/doppler/examples/async_dsss_receiver_spec_demo.py`, which runs the same
 * captures through the same receiver and asserts the same physics, with a
 * figure. Told the RF carrier (`carrier_freq_hz`), the receiver feeds the
 * carrier offset it tracks to its code loop as the matching code-rate offset,
 * so it follows a chip clock that the Doppler stretches. With `reacquire`
 * armed, a hand-off whose carrier never comes up to symbol lock is released
 * after `lost_confirm_s` and the receiver searches again by itself.
 *
 * The link is a 1023-chip Gold code at 3.069 Mcps carrying BPSK at 2700 bps
 * on a 2.5 GHz carrier, with up to +/-50 kHz of Doppler changing by up to
 * 500 Hz/s. A pass stresses it in two ways that never coincide: the Doppler
 * ramping through zero at closest approach, and sitting at its extreme offset
 * at the ends. Each regime runs over eight noise draws.
 *
 * This file shows what the binding does for you and therefore hides:
 *
 *   - the capture is streamed. Each push of one code epoch pulls the synth's
 *     clean chips through the Doppler channel, whose output length varies
 *     from call to call as the clock stretches, and adds the noise. Nothing
 *     holds the whole capture.
 *   - steps() writes into a buffer the CALLER owns, sized to one push.
 *   - the per-push record (the state, the symbol lock) is read from status()
 *     and kept by the caller; nothing in the receiver keeps a history.
 *   - the BER meter, the lock-symbol finder, EVM and M2M4 are the same C
 *     functions the Python twin calls through its bindings.
 *
 * The waveform is not built here. The payload is doppler's PN, the emitter
 * is the shipped `wfm_synth` continuous DSSS, the Doppler is the shipped
 * `doppler_channel`, and the noise is the shipped awgn at the sigma
 * `dp_wfm_awgn_amplitude()` gives for the Es/N0. Nothing in this file spreads
 * a chip, draws a bit or computes a sigma.
 *
 * The checks, per regime:
 *
 *   - at least MIN_LOCK_FRACTION of the draws reach a sustained symbol lock;
 *   - every hand-off that locked pulled in within LOST_CONFIRM_S, so the
 *     release rule cut none short;
 *   - on a ramp that crosses zero inside the capture, every draw locked
 *     before the crossing;
 *   - the pooled BER is within MAX_LOSS_DB of coherent BPSK, and no better
 *     than it, which no receiver can be;
 *   - every locked draw's EVM is within EVM_TOL_DB of EVM_IDEAL_DB, and its
 *     M2M4 clears the input Es/N0 less the loss budget.
 *
 * It prints the same lines as the Python twin; a pair test in
 * src/doppler/tests/test_c_example_pairs.py compares them.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_receiver_spec_demo
 */
#include <doppler/async_dsss_receiver/async_dsss_receiver_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/ber_meter/ber_meter_core.h>
#include <doppler/doppler_channel/doppler_channel_core.h>
#include <doppler/dp_complex.h>
#include <doppler/gold/gold_core.h>
#include <doppler/pn/pn_core.h>
#include <doppler/snr/snr_core.h>
#include <doppler/wfm/wfm_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Setup: this scenario's numbers ───────────────────────────────────────
 * The same as the Python twin's setup block. */
#define SF 1023u
#define SPC 2u /* samples per chip: the sample rate is CHIP_RATE * SPC */
#define TE (SF * SPC)
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CARRIER_HZ 2.5e9            /* the RF carrier the Doppler is on */
#define CARRIER_UNCERTAINTY_HZ 50e3 /* one-sided: within +/-50 kHz */
#define DOPPLER_RATE_HZ_S 500.0     /* the fastest the Doppler changes */
#define ESN0_DB 8.0                 /* Es/N0 at the receiver input */
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
/* Symbol lock down this long: release, search again. Sized past the pull-in
   of a hand-off that does lock, which the run prints. */
#define LOST_CONFIRM_S 0.4

#define N_SYM 6000u /* payload symbols, 2.2 s of signal */
#define DURATION_S ((double)N_SYM / SYM_RATE)
#define LEAD_IN (20u * TE + 737u) /* noise alone first, not whole epochs */
#define N_DRAWS 8u                /* noise seeds 1..N_DRAWS */
#define PAYLOAD_LEN (N_SYM + 64u) /* it outlasts the capture */
#define PAYLOAD_SEED 1u
#define PAYLOAD_PN_LENGTH 15u

/* The checks' bounds. Each is a physical statement, not a fit to this run;
   the Python twin's validate block says why each sits where it does. Symbol
   lock is a per-draw outcome on random noise, so one draw in eight may miss;
   a missed hand-off that stays missed (no reacquire) fails it by several. */
#define MIN_LOCK_FRACTION (7.0 / 8.0)
/* The pooled BER as dB of Es/N0 lost against coherent BPSK: about 1 dB held,
   counting noise about 0.25 dB, a BER twice the receiver's fails. */
#define MAX_LOSS_DB 1.5
/* EVM normalizes by the measured total power: a perfect receiver reads
   10*log10(N/(S+N)), not -Es/N0. A locked stream sits above it by about
   0.9 dB of EVM, the implementation loss the despread no longer hides
   since its output stopped flattering it (#2495), plus about 0.1 dB of
   counting noise; twice the loss fails. */
#define EVM_IDEAL_DB (-10.0 * log10 (1.0 + pow (10.0, ESN0_DB / 10.0)))
#define EVM_TOL_DB 1.5
#define LAG_SPAN 1000 /* symbols the stream may start into the payload */
/* What "sustained" means for the symbol lock: ber_lock_symbol()'s defaults
   on the Python face. */
#define LOCK_SUSTAIN 200u
#define LOCK_MIN_FRAC 0.9

/** @brief One regime of a pass: the Doppler at the first signal sample, Hz,
 *  and its rate of change, Hz/s. */
struct regime
{
  const char *name;
  double      start_hz;
  double      rate_hz_s;
};

/* The TCA crossing starts below zero so the ramp crosses zero mid-capture. */
static const struct regime REGIMES[] = {
  { "TCA crossing", -0.5 * DOPPLER_RATE_HZ_S * DURATION_S, DOPPLER_RATE_HZ_S },
  { "+50 kHz extremum", CARRIER_UNCERTAINTY_HZ, 0.0 },
  { "-50 kHz extremum", -CARRIER_UNCERTAINTY_HZ, 0.0 },
};
#define N_REGIMES (sizeof REGIMES / sizeof REGIMES[0])

#define CHECK(cond, ...)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: " __VA_ARGS__);                             \
          fputc ('\n', stderr);                                               \
          exit (1);                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* ── The capture, streamed ─────────────────────────────────────────────── */

/** @brief Everything that makes one capture: the noise-only lead-in, the
 *  clean emitter, the Doppler channel, and the noise. */
struct capture
{
  dp_wfm_synth_state_t       *syn;
  dp_doppler_channel_state_t *ch;
  dp_awgn_state_t            *awg;
  size_t                      lead_left;  /* lead-in samples still to emit */
  size_t                      clean_left; /* clean samples still to make */
  float complex              *clean;      /* one push of clean samples */
  float complex              *stage;      /* channel output not yet used */
  size_t                      stage_cap, stage_len, stage_pos;
  float complex              *noise;
};

static void
capture_open (struct capture *c, const uint8_t *code, const uint8_t *payload,
              const struct regime *r, uint64_t seed)
{
  memset (c, 0, sizeof *c);
  /* The emitter: continuous DSSS, the payload one bit per symbol, clean. */
  const double   freq     = 0.0; /* the Doppler is the channel's */
  const int      snr_mode = 0;   /* moot: the synth adds no noise */
  const uint32_t seed_syn = 0;   /* moot: no noise, and the data is ours */
  /* moot: the synth's own PN is unused, the data is the payload */
  const int      synth_pn_length = 15, lfsr = PN_GALOIS;
  const uint64_t pn_poly = 0;
  const double   f_end   = 0.0;
  c->syn = dp_wfm_synth_create (WFM_SYNTH_DSSS, FS, freq, WFM_SYNTH_SNR_CLEAN,
                                snr_mode, seed_syn, (int)SPC, synth_pn_length,
                                pn_poly, lfsr, f_end);
  CHECK (c->syn, "the emitter opens");
  CHECK (dp_wfm_synth_set_dsss_cont (c->syn, code, SF, CHIP_RATE / SYM_RATE,
                                     WFM_DSSS_DATA_BITS, payload, PAYLOAD_LEN)
             == 0,
         "the emitter takes the payload");
  /* The Doppler, as ppm of the carrier: it stretches the chip clock and
     rotates the carrier from the one figure. */
  c->ch = dp_doppler_channel_create (FS, CARRIER_HZ,
                                     r->start_hz / CARRIER_HZ * 1e6,
                                     r->rate_hz_s / CARRIER_HZ * 1e6);
  /* The noise, added after the channel, at the Es/N0 over one symbol's
     FS / SYM_RATE samples. */
  c->awg = dp_awgn_create (
      seed, dp_wfm_awgn_amplitude (
                (float)(ESN0_DB - 10.0 * log10 (FS / SYM_RATE)), 1.0f));
  CHECK (c->ch && c->awg, "the channel and the noise open");
  c->lead_left  = LEAD_IN;
  c->clean_left = (size_t)((double)N_SYM * FS / SYM_RATE);
  c->stage_cap  = dp_doppler_channel_execute_max_out (c->ch);
  c->clean      = malloc ((size_t)TE * sizeof *c->clean);
  c->stage      = malloc (c->stage_cap * sizeof *c->stage);
  c->noise      = malloc ((size_t)TE * sizeof *c->noise);
  CHECK (c->clean && c->stage && c->noise, "the capture buffers allocate");
}

static void
capture_close (struct capture *c)
{
  dp_wfm_synth_destroy (c->syn);
  dp_doppler_channel_destroy (c->ch);
  dp_awgn_destroy (c->awg);
  free (c->clean);
  free (c->stage);
  free (c->noise);
}

/** @brief Fill @p x with the next push: lead-in zeros, then the channel's
 *  output, plus noise. Returns 0 once the signal ends inside a push, which
 *  is the end of the capture. */
static int
capture_next (struct capture *c, float complex *x)
{
  size_t k = 0;
  while (k < TE)
    {
      if (c->lead_left)
        {
          size_t take = c->lead_left < TE - k ? c->lead_left : TE - k;
          memset (x + k, 0, take * sizeof *x);
          c->lead_left -= take;
          k += take;
          continue;
        }
      if (c->stage_pos == c->stage_len)
        {
          /* The channel's output length varies by a sample from call to
             call as the stretched clock slips: that is the Doppler. */
          if (!c->clean_left)
            return 0;
          size_t n = c->clean_left < TE ? c->clean_left : TE;
          dp_wfm_synth_steps (c->syn, c->clean, n);
          c->clean_left -= n;
          c->stage_len = dp_doppler_channel_execute (c->ch, c->clean, n,
                                                     c->stage, c->stage_cap);
          c->stage_pos = 0;
          continue;
        }
      size_t avail = c->stage_len - c->stage_pos;
      size_t take  = avail < TE - k ? avail : TE - k;
      memcpy (x + k, c->stage + c->stage_pos, take * sizeof *x);
      c->stage_pos += take;
      k += take;
    }
  dp_awgn_generate (c->awg, TE, c->noise, TE);
  for (size_t i = 0; i < TE; i++)
    x[i] += c->noise[i];
  return 1;
}

/* ── One draw ──────────────────────────────────────────────────────────── */

/** @brief One draw's outcome, as the Python twin's score() returns it. */
struct draw
{
  int    locked;    /* a sustained symbol lock was reached */
  size_t released;  /* hand-offs released before the one that locked */
  double pull_in_s; /* that hand-off: start of tracking to first lock */
  double lock_s;    /* arrival to the sustained lock */
  double evm_db;
  double m2m4_db;
};

/** @brief Run one capture through one receiver, and score it into
 *  @p meter. */
static struct draw
run_draw (const uint8_t *code, const uint8_t *payload, const struct regime *r,
          uint64_t seed, dp_ber_meter_state_t *meter)
{
  struct capture cap;
  capture_open (&cap, code, payload, r, seed);

  /* The receiver. create() takes its parameters in signature order, each
     named here. The knobs a caller sets: the C/N0 sizes the detectors; the
     carrier uncertainty is one-sided; the RF carrier couples the code rate
     to the tracked carrier; the confirm time with reacquire releases a track
     whose symbol lock stays down, and searches again. The rest are the
     defaults. */
  const size_t code_len                = SF;
  const double chip_rate               = CHIP_RATE;
  const double symbol_rate             = SYM_RATE;
  const size_t spc                     = SPC;
  const int    m                       = 2; /* BPSK */
  const double cn0_dbhz                = CN0_DBHZ;
  const double pfa                     = 1e-3;
  const double pd                      = 0.9;
  const double doppler_uncertainty     = CARRIER_UNCERTAINTY_HZ;
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
  const double carrier_freq_hz         = CARRIER_HZ;
  const double lost_confirm_s          = LOST_CONFIRM_S;
  const bool   reacquire               = true;

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz, pfa, pd,
      doppler_uncertainty, segments, sps, differential, refine_max_error_db,
      refine_samples_per_sym, refine_design_margin_db, refine_n_fft,
      refine_zero_pad, refine_sequential, refine_max_n_blocks, carrier_freq_hz,
      lost_confirm_s, reacquire, 0.0, 0.0);
  CHECK (rx != NULL, "the receiver opens");

  /* The caller's buffers: one push in, one push of symbols out (a push of TE
     samples emits at most TE symbols), and the record of every push and
     every symbol, sized to the whole capture. */
  const size_t max_pushes
      = (LEAD_IN + cap.clean_left + cap.stage_cap) / TE + 1;
  size_t syms_cap = dp_async_dsss_receiver_steps_max_out (rx);
  if (syms_cap < TE)
    syms_cap = TE;
  float complex *x       = malloc ((size_t)TE * sizeof *x);
  float complex *block   = malloc (syms_cap * sizeof *block);
  float complex *syms    = malloc ((size_t)PAYLOAD_LEN * sizeof *syms);
  uint8_t       *flags   = malloc ((size_t)PAYLOAD_LEN * sizeof *flags);
  int           *state   = malloc (max_pushes * sizeof *state);
  int           *locked  = malloc (max_pushes * sizeof *locked);
  size_t        *emitted = malloc (max_pushes * sizeof *emitted);
  CHECK (x && block && syms && flags && state && locked && emitted,
         "the buffers allocate");

  /* Stream the capture, one code epoch per push, and read status() after
     each. Every symbol carries the lock flag of the push that emitted it. */
  size_t n_push = 0, n_sym = 0;
  while (n_push < max_pushes && capture_next (&cap, x))
    {
      size_t n = dp_async_dsss_receiver_steps (rx, x, TE, block, syms_cap);
      async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
      CHECK (n_sym + n <= PAYLOAD_LEN, "more symbols than the payload holds");
      for (size_t i = 0; i < n; i++)
        {
          syms[n_sym + i]  = block[i];
          flags[n_sym + i] = (uint8_t)st.locked;
        }
      n_sym += n;
      state[n_push]   = st.state;
      locked[n_push]  = st.locked;
      emitted[n_push] = n_sym; /* symbols emitted up to and including it */
      n_push++;
    }
  /* The synth sends the payload once, then silence: had it run dry, the end
     of the capture would be no signal at all. */
  CHECK (!dp_wfm_synth_data_ended (cap.syn),
         "the payload ran out before the capture ended");

  /* Score it: the window starts where the symbol lock is sustained, and the
     meter detects where the stream sits against the payload from a marker
     inside that window. BER and EVM are scored on the same window. */
  struct draw d = { 0 };
  int lo = dp_ber_lock_symbol (flags, n_sym, LOCK_SUSTAIN, LOCK_MIN_FRAC);
  if (lo >= 0)
    {
      CHECK (dp_ber_meter_align (meter, syms, n_sym, (size_t)lo + LAG_SPAN, 0,
                                 0, LAG_SPAN, 0.0),
             "the payload marker was not detected in the locked stream");
      dp_ber_meter_score (meter, syms, n_sym, (size_t)lo, n_sym);

      /* Pushes are the time base: the push that emitted symbol lo, the first
         push with the lock up, and the start of the track it came up on. */
      size_t sustained = 0;
      while (emitted[sustained] <= (size_t)lo)
        sustained++;
      size_t first_lock = 0;
      while (!locked[first_lock])
        first_lock++;
      size_t track_start = first_lock;
      while (track_start > 0
             && state[track_start - 1] == ASYNC_DSSS_RX_TRACKING)
        track_start--;
      for (size_t i = 1; i <= track_start; i++)
        if (state[i - 1] == ASYNC_DSSS_RX_TRACKING
            && state[i] != ASYNC_DSSS_RX_TRACKING)
          d.released++;

      d.locked    = 1;
      d.pull_in_s = (double)(first_lock - track_start) * TE / FS;
      d.lock_s    = ((double)(sustained + 1) * TE - LEAD_IN) / FS;
      d.evm_db    = dp_ber_evm_db (syms, n_sym, (size_t)lo, n_sym, m);
      d.m2m4_db   = dp_snr_m2m4_db (syms + lo, n_sym - (size_t)lo);
    }

  /* The receiver owns its internals; destroy() frees them. The buffers are
     the caller's. */
  dp_async_dsss_receiver_destroy (rx);
  free (emitted);
  free (locked);
  free (state);
  free (flags);
  free (syms);
  free (block);
  free (x);
  capture_close (&cap);
  return d;
}

int
main (void)
{
  /* The Gold code, from doppler's generator at its CCSDS defaults (the
     Python face's `Gold()`). */
  const uint64_t   taps_a = 934, seed_a = 350, taps_b = 567, seed_b = 73;
  const uint32_t   gold_length = 10; /* register bits: 2^10 - 1 = SF chips */
  uint8_t          code[SF];
  dp_gold_state_t *gd
      = dp_gold_create (taps_a, seed_a, taps_b, seed_b, gold_length);
  CHECK (gd != NULL, "the Gold generator opens");
  dp_gold_generate (gd, SF, code, SF);
  dp_gold_destroy (gd);

  /* The payload, one bit per symbol, from doppler's own PN: the truth the
     BER is scored against is exactly what the synth sends. */
  uint8_t        payload[PAYLOAD_LEN];
  dp_pn_state_t *pn
      = dp_pn_create_mls (0, PAYLOAD_SEED, PAYLOAD_PN_LENGTH, PN_GALOIS);
  CHECK (pn != NULL, "the payload PN opens");
  dp_pn_generate (pn, PAYLOAD_LEN, payload, PAYLOAD_LEN);
  dp_pn_destroy (pn);

  for (size_t r = 0; r < N_REGIMES; r++)
    {
      /* One meter per regime pools every draw's errors. */
      dp_ber_meter_state_t *meter = dp_ber_meter_create (2, 0, 0.0);
      CHECK (meter != NULL, "the BER meter opens");
      CHECK (dp_ber_meter_set_truth (meter, payload, PAYLOAD_LEN) == 0,
             "the meter takes the payload");

      struct draw draws[N_DRAWS];
      size_t      n_locked = 0, first_try = 0;
      double      max_pull_in = 0.0, max_lock = 0.0, evm = 0.0, m2m4 = 0.0;
      for (size_t k = 0; k < N_DRAWS; k++)
        {
          draws[k] = run_draw (code, payload, &REGIMES[r], k + 1, meter);
          if (!draws[k].locked)
            continue;
          n_locked++;
          first_try += draws[k].released == 0;
          max_pull_in = fmax (max_pull_in, draws[k].pull_in_s);
          max_lock    = fmax (max_lock, draws[k].lock_s);
          evm += draws[k].evm_db;
          m2m4 += draws[k].m2m4_db;
        }

      /* The checks. The loss is the Es/N0 coherent BPSK would need for the
         measured rate, subtracted from the Es/N0 the link has; written so a
         NaN (a rate off the theory curve: no decode) fails. */
      CHECK ((double)n_locked / N_DRAWS >= MIN_LOCK_FRACTION,
             "[%s] symbol lock on %zu of %u draws", REGIMES[r].name, n_locked,
             N_DRAWS);
      CHECK (max_pull_in < LOST_CONFIRM_S,
             "[%s] a pull-in of %.2f s reaches the confirm time",
             REGIMES[r].name, max_pull_in);
      const double zero_s = REGIMES[r].rate_hz_s != 0.0
                                ? -REGIMES[r].start_hz / REGIMES[r].rate_hz_s
                                : -1.0;
      if (zero_s > 0.0 && zero_s < DURATION_S)
        CHECK (max_lock < zero_s,
               "[%s] locked %.2f s after arrival, after the zero crossing at "
               "%.2f s",
               REGIMES[r].name, max_lock, zero_s);
      ber_interval_t ber     = dp_ber_meter_ber (meter);
      double         loss_db = ESN0_DB - dp_ber_esn0_db_for_ser (2, ber.p_hat);
      CHECK (loss_db <= MAX_LOSS_DB, "[%s] implementation loss %.2f dB",
             REGIMES[r].name, loss_db);
      CHECK (!(dp_ber_esn0_db_for_ser (2, ber.hi) > ESN0_DB),
             "[%s] BER %.2e is below coherent BPSK's: the measurement is "
             "broken",
             REGIMES[r].name, ber.p_hat);
      for (size_t k = 0; k < N_DRAWS; k++)
        if (draws[k].locked)
          {
            CHECK (fabs (draws[k].evm_db - EVM_IDEAL_DB) <= EVM_TOL_DB,
                   "[%s] EVM %.1f dB against %.1f dB ideal", REGIMES[r].name,
                   draws[k].evm_db, EVM_IDEAL_DB);
            CHECK (draws[k].m2m4_db >= ESN0_DB - MAX_LOSS_DB,
                   "[%s] M2M4 %.1f dB: not a locked stream", REGIMES[r].name,
                   draws[k].m2m4_db);
          }

      printf ("%s: symbol lock on %zu/%u draws, %zu on the first hand-off; "
              "pull-in %.2f s at most, locked %.2f s after arrival at most; "
              "BER %.1e (%zu errors in %zu bits), %.1f dB from coherent "
              "BPSK; EVM %.1f dB; M2M4 %.1f dB\n",
              REGIMES[r].name, n_locked, N_DRAWS, first_try, max_pull_in,
              max_lock, ber.p_hat, ber.errors, ber.symbols, loss_db,
              evm / (double)n_locked, m2m4 / (double)n_locked);
      dp_ber_meter_destroy (meter);
    }
  printf ("OK\n");
  return 0;
}
