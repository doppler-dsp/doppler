/**
 * async_dsss_receiver_spec_demo.c -- one AsyncDsssReceiver on the CCSDS
 * 415.1-B-1 forward link, in C.
 *
 * This is the C twin of
 * `src/doppler/examples/async_dsss_receiver_spec_demo.py`, which runs the same
 * captures through the same receiver and makes the same checks, with a
 * figure. The link: BPSK at 3,000 symbols a second, spread by a 1,023-chip
 * Gold code whose chip rate is the carrier times 31/21216, the data
 * asynchronous to the code. The receiver takes 11 MSa/s and searches a
 * Doppler within +/-1,500 Hz that moves at up to R_env, 11 Hz/s: the
 * residual the link's ephemeris pre-compensation leaves (the design page's
 * §1.8). Told the RF
 * carrier (`carrier_freq_hz`), it feeds the carrier offset it tracks to its
 * code loop as the matching code-rate offset, so it follows the chip clock
 * the Doppler stretches.
 *
 * It runs at both ends of the band, 2.2 and 2.8 GHz, each a capture whose
 * Doppler ramps at R_env from -13.75 Hz through zero to +13.75 Hz, at Es/N0 =
 * 10 dB, 6 dB above the link's 4 dB floor. It demonstrates the receiver; it
 * does not certify it: performance at the floor is measured by the
 * receiver's validators (docs/design/async-dsss-receiver-measurements.md).
 *
 * This file shows what the binding does for you and therefore hides:
 *
 *   - the capture is streamed. Each push pulls the synth's clean chips
 *     through the rate converter and the Doppler channel, whose output
 *     lengths vary from call to call, and adds the noise. Nothing holds the
 *     whole capture.
 *   - steps() writes into a buffer the CALLER owns, sized to one push.
 *   - the per-push record (the state, the symbol lock) is read from status()
 *     and kept by the caller; nothing in the receiver keeps a history.
 *   - the BER meter, the lock-symbol finder, EVM and M2M4 are the same C
 *     functions the Python twin calls through its bindings.
 *
 * The waveform is not built here. The payload is doppler's PN, the emitter
 * is the shipped `wfm_synth` continuous DSSS, the rate converter and the
 * Doppler are the shipped `RateConverter` and `doppler_channel`, and the
 * noise is the shipped awgn at the sigma `dp_wfm_awgn_amplitude()` gives for
 * the Es/N0. Nothing in this file spreads a chip or computes a sigma.
 *
 * The checks, per carrier:
 *
 *   - at least MIN_LOCK_FRACTION of the draws reach a sustained symbol lock;
 *   - every hand-off that locked pulled in within LOST_CONFIRM_S, so the
 *     release rule cut none short;
 *   - every draw locked before the Doppler's zero crossing;
 *   - the pooled BER is at most MAX_BER: the frames decode.
 *
 * It prints the same lines as the Python twin; a pair test in
 * src/doppler/tests/test_c_example_pairs.py compares them.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_receiver_spec_demo
 */
#include <doppler/RateConverter/RateConverter_core.h>
#include <doppler/async_dsss_receiver/async_dsss_receiver_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/ber_meter/ber_meter_core.h>
#include <doppler/ccsds/ccsds_415.h>
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

/* ── Setup: the 415.1-B-1 forward link ────────────────────────────────────
 * The point's numbers are the library's (doppler/ccsds/ccsds_415.h), named
 * here as the Python twin names them. */
#define SF DP_CCSDS_415_SF
#define SYM_RATE DP_CCSDS_415_SYM
#define FS DP_CCSDS_415_FS
#define CODE_ONLY_SYM                                                         \
  DP_CCSDS_415_CODE_ONLY_SYM /* each frame opens with these */
#define FRAME_SYM DP_CCSDS_415_FRAME_SYM
#define DOPPLER_UNCERTAINTY_HZ DP_CCSDS_415_SPAN_HZ /* one-sided */
/* The fastest the Doppler changes: R_env, the residual's rate (the design
   page's §1.8). */
#define DOPPLER_RATE_HZ_S DP_CCSDS_415_R_ENV_HZ_S
/* Es/N0 at the receiver input: 6 dB above the link's 4 dB floor, so the
   despread symbols show as two clusters. A demonstration, not a validator. */
#define ESN0_DB 10.0
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
/* Symbol lock down this long: release, search again. */
#define LOST_CONFIRM_S 0.4

#define PUSH 4096u  /* samples a push into the receiver */
#define CHUNK 8192u /* emitter samples a pass through the rate converter */
#define DURATION_S 2.5
#define N_SYM ((size_t)(DURATION_S * SYM_RATE))
#define LEAD_IN (20u * PUSH + 737u) /* noise alone first */
#define N_DRAWS 4u                  /* noise seeds 1..N_DRAWS */
#define PAYLOAD_LEN (N_SYM + 64u)   /* it outlasts the capture */
#define PAYLOAD_SEED 1u
#define PAYLOAD_PN_LENGTH 15u
/* The Doppler at the signal's first sample; it ramps at DOPPLER_RATE_HZ_S
   through zero at 1.25 s, to +13.75 Hz. */
#define START_HZ (-13.75)

/* The checks' bounds; the Python twin's validate block says why each sits
   where it does. Symbol lock is a per-draw outcome on random noise, so one
   draw in four may miss. */
#define MIN_LOCK_FRACTION (3.0 / 4.0)
/* "Decoded": the pooled BER over the locked windows. */
#define MAX_BER 1e-3
#define LAG_SPAN 1000 /* symbols the stream may start into the payload */
/* What "sustained" means for the symbol lock: ber_lock_symbol()'s defaults
   on the Python face. */
#define LOCK_SUSTAIN 200u
#define LOCK_MIN_FRAC 0.9

/** @brief One carrier: its name, its Hz, and the scene's numbers there
 *  (dp_ccsds_415_scene(): the chip rate, the rate converter's ratio from
 *  the emitter's two samples a chip, and the Doppler at the start and its
 *  rate in ppm of the carrier, the unit the channel takes), filled in by
 *  main(). */
struct regime
{
  const char          *name;
  double               carrier_hz;
  dp_ccsds_415_scene_t s;
};

/* The scene's numbers are computed once, by the library, with fast math
   off: the Python twin reads the same bits through
   doppler.ccsds.ccsds415_scene, and the pair test holds the two to each
   other through the line main() prints. */
static struct regime REGIMES[] = {
  { .name = "2.2 GHz", .carrier_hz = DP_CCSDS_415_FC_LO },
  { .name = "2.8 GHz", .carrier_hz = DP_CCSDS_415_FC_HI },
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
 *  clean emitter at two samples a chip, the rate converter to FS, the
 *  Doppler channel, and the noise. */
struct capture
{
  dp_wfm_synth_state_t       *syn;
  dp_RateConverter_state_t   *rc;
  dp_doppler_channel_state_t *ch;
  dp_awgn_state_t            *awg;
  size_t                      lead_left;  /* lead-in samples still to emit */
  size_t                      clean_left; /* clean samples still to make */
  float complex              *clean;      /* one chunk of clean samples */
  float complex              *conv;       /* that chunk at FS */
  float complex              *stage;      /* channel output not yet used */
  size_t                      conv_cap, stage_cap, stage_len, stage_pos;
  float complex              *noise;
};

static void
capture_open (struct capture *c, const uint8_t *code, const uint8_t *payload,
              const struct regime *r, uint64_t seed)
{
  memset (c, 0, sizeof *c);
  const double fs2 = 2.0 * r->s.chip_rate; /* the emitter's two a chip */
  /* The emitter: continuous DSSS, the payload one bit per symbol, clean. */
  const double   freq     = 0.0; /* the Doppler is the channel's */
  const int      snr_mode = 0;   /* moot: the synth adds no noise */
  const uint32_t seed_syn = 0;   /* moot: no noise, and the data is ours */
  /* moot: the synth's own PN is unused, the data is the payload */
  const int      synth_pn_length = 15, lfsr = PN_GALOIS;
  const uint64_t pn_poly = 0;
  const double   f_end   = 0.0;
  c->syn = dp_wfm_synth_create (WFM_SYNTH_DSSS, fs2, freq, WFM_SYNTH_SNR_CLEAN,
                                snr_mode, seed_syn, 2, synth_pn_length,
                                pn_poly, lfsr, f_end);
  CHECK (c->syn, "the emitter opens");
  CHECK (dp_wfm_synth_set_dsss_cont (c->syn, code, SF,
                                     r->s.chip_rate / SYM_RATE,
                                     WFM_DSSS_DATA_BITS, payload, PAYLOAD_LEN)
             == 0,
         "the emitter takes the payload");
  c->rc = dp_RateConverter_create (r->s.ratio, 0);
  /* The Doppler, as ppm of the carrier: it stretches the chip clock and
     rotates the carrier from the one figure. */
  c->ch = dp_doppler_channel_create (FS, r->carrier_hz, r->s.start_ppm,
                                     r->s.rate_ppm_s);
  /* The noise, added after the channel, at the Es/N0 over one symbol's
     FS / SYM_RATE samples. */
  c->awg = dp_awgn_create (
      seed, dp_wfm_awgn_amplitude (
                (float)(ESN0_DB - 10.0 * log10 (FS / SYM_RATE)), 1.0f));
  CHECK (c->rc && c->ch && c->awg, "the converter, channel and noise open");
  c->lead_left  = LEAD_IN;
  c->clean_left = (size_t)((double)N_SYM * fs2 / SYM_RATE);
  c->conv_cap   = 2u * CHUNK + 64u;
  c->stage_cap  = 2u * CHUNK + 64u;
  c->clean      = malloc ((size_t)CHUNK * sizeof *c->clean);
  c->conv       = malloc (c->conv_cap * sizeof *c->conv);
  c->stage      = malloc (c->stage_cap * sizeof *c->stage);
  c->noise      = malloc ((size_t)PUSH * sizeof *c->noise);
  CHECK (c->clean && c->conv && c->stage && c->noise,
         "the capture buffers allocate");
}

static void
capture_close (struct capture *c)
{
  dp_wfm_synth_destroy (c->syn);
  dp_RateConverter_destroy (c->rc);
  dp_doppler_channel_destroy (c->ch);
  dp_awgn_destroy (c->awg);
  free (c->clean);
  free (c->conv);
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
  while (k < PUSH)
    {
      if (c->lead_left)
        {
          size_t take = c->lead_left < PUSH - k ? c->lead_left : PUSH - k;
          memset (x + k, 0, take * sizeof *x);
          c->lead_left -= take;
          k += take;
          continue;
        }
      if (c->stage_pos == c->stage_len)
        {
          /* The converter's and the channel's output lengths vary by a
             sample from call to call: a fractional rate, and the stretched
             clock slipping, which is the Doppler. */
          if (!c->clean_left)
            return 0;
          size_t n = c->clean_left < CHUNK ? c->clean_left : CHUNK;
          dp_wfm_synth_steps (c->syn, c->clean, n);
          c->clean_left -= n;
          size_t m     = dp_RateConverter_execute (c->rc, c->clean, n, c->conv,
                                                   c->conv_cap);
          c->stage_len = dp_doppler_channel_execute (c->ch, c->conv, m,
                                                     c->stage, c->stage_cap);
          c->stage_pos = 0;
          continue;
        }
      size_t avail = c->stage_len - c->stage_pos;
      size_t take  = avail < PUSH - k ? avail : PUSH - k;
      memcpy (x + k, c->stage + c->stage_pos, take * sizeof *x);
      c->stage_pos += take;
      k += take;
    }
  dp_awgn_generate (c->awg, PUSH, c->noise, PUSH);
  for (size_t i = 0; i < PUSH; i++)
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

/** @brief Where the BER meter's marker starts in the payload: the first
 *  symbol at least LAG_SPAN into the locked stream that is data, never a
 *  frame's data-free window, which is all one symbol and marks nothing. */
static size_t
marker_at (size_t lo)
{
  size_t t0 = lo + LAG_SPAN;
  if (t0 % FRAME_SYM < CODE_ONLY_SYM)
    t0 += CODE_ONLY_SYM - t0 % FRAME_SYM;
  return t0;
}

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
     to the tracked carrier; the data-free window the search integrates
     across, and the Doppler rate that bounds it; the confirm time with
     reacquire releases a track whose symbol lock stays down, and searches
     again. The rest are the defaults. */
  const size_t code_len                = SF;
  const double chip_rate               = r->s.chip_rate;
  const double symbol_rate             = SYM_RATE;
  const double sample_rate             = FS;
  const int    m                       = 2; /* BPSK */
  const double cn0_dbhz                = CN0_DBHZ;
  const double pfa                     = 1e-3;
  const double pd                      = 0.9;
  const double doppler_uncertainty     = DOPPLER_UNCERTAINTY_HZ;
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
  const double carrier_freq_hz         = r->carrier_hz;
  const double lost_confirm_s          = LOST_CONFIRM_S;
  const bool   reacquire               = true;
  const double code_only_s             = CODE_ONLY_SYM / SYM_RATE;
  const double doppler_rate            = DOPPLER_RATE_HZ_S;
  const bool   refine                  = true;
  /* The carrier loop forms its error over one coherent code-period window.
     It can because a symbol, 333 us, is at least a code period, 250 to
     318 us across the band, so some window of one period is free of data
     transitions; create() refuses a shorter symbol. */
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, code_len, chip_rate, symbol_rate, sample_rate, m, cn0_dbhz, pfa,
      pd, doppler_uncertainty, segments, sps, differential,
      refine_max_error_db, refine_samples_per_sym, refine_design_margin_db,
      refine_n_fft, refine_zero_pad, refine_sequential, refine_max_n_blocks,
      carrier_freq_hz, lost_confirm_s, reacquire, code_only_s, doppler_rate,
      refine);
  CHECK (rx != NULL, "the receiver opens");

  /* The caller's buffers: one push in, one push of symbols out, and the
     record of every push and every symbol, sized to the whole capture. */
  const size_t max_pushes
      = (LEAD_IN + cap.clean_left * 2u + cap.stage_cap) / PUSH + 1;
  size_t syms_cap = dp_async_dsss_receiver_steps_max_out (rx);
  if (syms_cap < PUSH)
    syms_cap = PUSH;
  float complex *x       = malloc ((size_t)PUSH * sizeof *x);
  float complex *block   = malloc (syms_cap * sizeof *block);
  float complex *syms    = malloc ((size_t)PAYLOAD_LEN * sizeof *syms);
  uint8_t       *flags   = malloc ((size_t)PAYLOAD_LEN * sizeof *flags);
  int           *state   = malloc (max_pushes * sizeof *state);
  int           *locked  = malloc (max_pushes * sizeof *locked);
  size_t        *emitted = malloc (max_pushes * sizeof *emitted);
  CHECK (x && block && syms && flags && state && locked && emitted,
         "the buffers allocate");

  /* Stream the capture, one push at a time, and read status() after each.
     Every symbol carries the lock flag of the push that emitted it. */
  size_t n_push = 0, n_sym = 0;
  while (n_push < max_pushes && capture_next (&cap, x))
    {
      size_t n = dp_async_dsss_receiver_steps (rx, x, PUSH, block, syms_cap);
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
         "the payload ran out before the run ended");

  /* Score it: the window starts where the symbol lock is sustained, and the
     meter detects where the stream sits against the payload from a marker.
     BER and EVM are scored on the same window. */
  struct draw d = { 0 };
  int64_t lo = dp_ber_lock_symbol (flags, n_sym, LOCK_SUSTAIN, LOCK_MIN_FRAC);
  if (lo >= 0)
    {
      CHECK (dp_ber_meter_align (meter, syms, n_sym, marker_at ((size_t)lo), 0,
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
      d.pull_in_s = (double)(first_lock - track_start) * PUSH / FS;
      d.lock_s    = ((double)(sustained + 1) * PUSH - LEAD_IN) / FS;
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
  /* The point and each carrier's scene, at %a so the twins compare the
     bits, not a rounding of them. */
  printf ("point: sf %u fs %a sym %a span %a r_env %a frame %u code_only %u\n",
          SF, FS, SYM_RATE, DOPPLER_UNCERTAINTY_HZ, DOPPLER_RATE_HZ_S,
          FRAME_SYM, CODE_ONLY_SYM);
  for (size_t r = 0; r < N_REGIMES; r++)
    {
      REGIMES[r].s = dp_ccsds_415_scene (REGIMES[r].carrier_hz, 2.0, START_HZ);
      printf ("%s: chip_rate %a ratio %a start_ppm %a rate_ppm_s %a\n",
              REGIMES[r].name, REGIMES[r].s.chip_rate, REGIMES[r].s.ratio,
              REGIMES[r].s.start_ppm, REGIMES[r].s.rate_ppm_s);
    }

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

  /* The payload, one bit per symbol, from doppler's own PN, each frame's
     first CODE_ONLY_SYM symbols zero (the data-free window the search looks
     in): the truth the BER is scored against is exactly what was sent. */
  uint8_t        payload[PAYLOAD_LEN];
  dp_pn_state_t *pn
      = dp_pn_create_mls (0, PAYLOAD_SEED, PAYLOAD_PN_LENGTH, PN_GALOIS);
  CHECK (pn != NULL, "the payload PN opens");
  dp_pn_generate (pn, PAYLOAD_LEN, payload, PAYLOAD_LEN);
  dp_pn_destroy (pn);
  for (size_t i = 0; i < PAYLOAD_LEN; i++)
    if (i % FRAME_SYM < CODE_ONLY_SYM)
      payload[i] = 0;

  const double zero_s = -START_HZ / DOPPLER_RATE_HZ_S;
  for (size_t r = 0; r < N_REGIMES; r++)
    {
      /* One meter per carrier pools every draw's errors. */
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

      /* The checks. */
      CHECK ((double)n_locked / N_DRAWS >= MIN_LOCK_FRACTION,
             "[%s] symbol lock on %zu of %u draws", REGIMES[r].name, n_locked,
             N_DRAWS);
      CHECK (max_pull_in < LOST_CONFIRM_S,
             "[%s] a pull-in of %.2f s reaches the confirm time",
             REGIMES[r].name, max_pull_in);
      CHECK (max_lock < zero_s,
             "[%s] locked %.2f s after arrival, after the zero crossing at "
             "%.2f s",
             REGIMES[r].name, max_lock, zero_s);
      ber_interval_t ber = dp_ber_meter_ber (meter);
      /* Written so a NaN (no symbols scored: no decode) fails. */
      CHECK (ber.p_hat <= MAX_BER, "[%s] BER %.2e over %.0e", REGIMES[r].name,
             ber.p_hat, MAX_BER);

      printf ("%s: symbol lock on %zu/%u draws, %zu on the first hand-off; "
              "pull-in %.2f s at most, locked %.2f s after arrival at most; "
              "BER %.1e (%zu errors in %zu bits); EVM %.1f dB; M2M4 %.1f "
              "dB\n",
              REGIMES[r].name, n_locked, N_DRAWS, first_try, max_pull_in,
              max_lock, ber.p_hat, ber.errors, ber.symbols,
              evm / (double)n_locked, m2m4 / (double)n_locked);
      dp_ber_meter_destroy (meter);
    }
  printf ("OK\n");
  return 0;
}
