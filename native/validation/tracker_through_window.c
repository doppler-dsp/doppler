/**
 * @file tracker_through_window.c
 * @brief What do the tracking receiver's lock flags do through the frame's
 *        pure-code window -- and does the release rule ever fire on it?
 *
 * The continuous async-DSSS design (docs/design/async-dsss-receiver.md
 * §5.4, §12 step 12) gives every emitter's frame a code-only window: of
 * every F_SYM symbols the first W_SYM carry the spreading code and no data,
 * on the data clock, with a frame edge at no particular chip phase. The
 * searcher's coherent depth is what the window makes possible (§2.3); this
 * harness asks what it costs the tracker that is already assigned to that
 * emitter. Through the window the symbol clock is unobservable -- there is
 * no transition for the timing detector to see -- so it coasts, and the
 * symbol-lock detector reads a constant symbol. The design expects: code
 * lock holds, the symbol flag may drop and recovers within its dwell once
 * the data resumes, and the release rule (both flags down for longer than
 * `lost_confirm_s`, §10, §12.3) never fires. If a flag reads a pure-code
 * stretch as unhealthy, that is a detector finding to fix, not a rule to
 * loosen.
 *
 * Method. The operating point of §12.3: a 1023-chip Gold code at 5 Mcps,
 * two samples per chip, asynchronous BPSK data at 2700 sym/s from the
 * shipped synth (`wfm_synth` continuous DSSS, its own PRBS data) with the
 * shipped window (`wfm_synth_set_dsss_window`: 450 code-only symbols of
 * every 4950 -- a 1.83 s frame, a 0.17 s window); noise from the shipped
 * awgn generator sized by awgn_amplitude_for_snr() from the C/N0. The
 * receiver is the HAND-OFF flavor (`async_dsss_receiver_create_handoff`),
 * seeded once with what the searcher hands it: the shipped `acq`
 * continuous engine runs on the same received blocks until its first hit,
 * and acq_build_handoff() of that hit is the seed -- the pool's own path
 * (§8.2). Not the stimulus's nominal phase: a seed that ignores the
 * channel's own delay (0.6 chip through the resampler) leaves the code
 * loop outside its pull-in, and that was measured here first. The release
 * clock is the design's 2 s. Two
 * dynamics: `static`, no Doppler at all, and the shipped doppler_channel
 * at SPEC's -- 20 ppm of a 2.5 GHz carrier (50 kHz, the chip clock dilated
 * with it), and 0.2 ppm/s (500 Hz/s) from zero -- so that coasting through
 * the window costs the symbol clock something real;
 * the receiver's carrier-to-code aiding is on for it, as in §12.5. They are
 * two conditions, `offset` and `rate`, because SPEC's worst cases do not
 * coincide: the largest Doppler is at the horizon where the rate is nil,
 * the largest rate at closest approach where the Doppler is nil. Nothing
 * here builds a chip, a bit, a sigma, a ramp or a seed by hand.
 *
 * What the ramp measured first is not about the window (§12.9): from the
 * searcher's seed, the chain settled in 2 of 3 trials at 45 dB-Hz and 0
 * of 3 at 40 -- the refine -> track hand-over re-seeded the live code
 * loop with a phase the clock dilation had moved on from (#1249, fixed:
 * 10 of 10 at 45 dB-Hz). What remains at the floor is the refine's
 * estimate, not the window (#1252, §12.10): this harness used to give
 * the refine the retired 100 dB look-back (one dump per epoch, which
 * aliases the data lobe and keeps a third of the seed's error); on the
 * shipped 0.5 dB look-back the estimate is unbiased -- and what failed at
 * the floor was the searcher's seed (#1254, fixed): its hit's code phase
 * is the middle of its dwell, 0.9 chip behind the code at 40 dB-Hz, and
 * acq_build_handoff() now advances it by the drift over half the dwell,
 * given the carrier. `--check` pins the static condition; the channel's
 * rows are in the full table.
 *
 * The receiver is fed one epoch (2046 samples) at a time and the flags are
 * read after every block. A block is IN the window when its centre sample
 * is, by the synth's own clock (symbol = floor(chip / chips_per_symbol),
 * window when symbol mod F_SYM < W_SYM); under the ramp the received clock
 * runs 20 ppm off the emitter's, 0.4 ms -- two blocks -- over a trial,
 * which the window's 815 blocks absorb. Once tracking with symbol lock
 * held for SETTLE_BLOCKS consecutive blocks, the next N_FRAMES windows are
 * watched: per window, the fraction of blocks with each flag off, the
 * longest run of both flags off, and whether `lost` fired; after each
 * window, the pull-in -- blocks from the window's last block to each flag
 * back on (0 when it never left). Between windows the flags are watched
 * too, so the data section's own dips are told from the window's.
 *
 * Usage:
 *   validate_tracker_through_window            full table: two C/N0s,
 *                                              N_SEEDS trials of N_FRAMES
 *   validate_tracker_through_window --check    one static trial of
 *                                              CHECK_FRAMES at 45 dB-Hz;
 *                                              the design's expectations
 *                                              asserted
 *   validate_tracker_through_window --trace [ppm ppm_s [cn0 [seed]]]
 *                                              one trial through the
 *                                              channel (its Doppler and
 *                                              rate, the C/N0 and the seed
 *                                              as given), one line per
 *                                              0.2 s: the state, the
 *                                              receiver's Doppler and the
 *                                              channel's truth, the code
 *                                              rate, the lock metric
 */
#include "acq/acq_core.h"
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "awgn/awgn_core.h"
#include "clib_common.h"
#include "doppler_channel/doppler_channel_core.h"
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
#define CPS (CHIP_RATE / SYM_RATE) /* chips per symbol, 1851.85       */
#define W_SYM 450u                 /* code-only symbols per frame     */
#define F_SYM 4950u                /* frame, symbols (§5.4)           */

#define SETTLE_BLOCKS 200  /* tracking + symbol lock for this many first */
#define MAX_LOCK_S 6.0     /* give up on a trial that never settles      */
#define LOST_CONFIRM_S 2.0 /* the release rule's interval (§12.3)       */
#define N_FRAMES 10        /* windows watched per trial                 */
#define CHECK_FRAMES 3     /* --check: fewer, the same code path        */
#define N_SEEDS 3
#define CARRIER_HZ 2.5e9 /* the ramp's carrier: ppm -> Hz, and the aid  */
#define RAMP_D0_PPM 20.0 /* 50 kHz at the carrier                       */
#define RAMP_PPM_S 0.2   /* 500 Hz/s, SPEC's dynamics                   */

/* SPEC's two dynamics are two conditions because its worst cases do not
   coincide: the largest Doppler is at the horizon where the rate is nil,
   the largest rate at closest approach where the Doppler is nil. */
enum
{
  COND_STATIC, /* no channel at all                       */
  COND_OFFSET, /* 20 ppm, no rate: 50 kHz, the chip clock */
  COND_RATE,   /* 0.2 ppm/s from zero: 500 Hz/s           */
  N_COND
};
static const char *cond_name[N_COND] = { "static", "offset", "rate" };

typedef struct
{
  size_t blocks;       /* blocks in the window                        */
  size_t code_off;     /* of them, code lock off                      */
  size_t sym_off;      /* of them, symbol lock off                    */
  size_t both_off;     /* of them, both off                           */
  size_t both_run_max; /* longest both-off run inside, blocks         */
  int    lost;         /* the release rule fired inside or after      */
  size_t pull_code;    /* blocks after the window to code lock on     */
  size_t pull_sym;     /* blocks after the window to symbol lock on   */
  size_t data_blocks;  /* blocks of the data section that followed    */
  size_t data_code_off;
  size_t data_sym_off;
} frame_t;

typedef struct
{
  int      settled;  /* tracking + symbol lock before MAX_LOCK_S    */
  double   seed_s;   /* start -> the searcher's hit, s              */
  double   settle_s; /* start -> settled, s                         */
  size_t   n_frames; /* windows watched (== requested when settled) */
  frame_t *frames;
} trial_t;

/* The emitter: the shipped continuous-DSSS synth, clean (no AWGN child),
   PRBS data from its own PN register, seeded per trial, with the frame. */
static wfm_synth_state_t *
make_emitter (const uint8_t *code, uint32_t seed)
{
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                          seed, (int)SPC, 15, 0, 0, 0.0);
  /* Valid constants: the synth takes them (the caller requires `syn`). */
  (void)wfm_synth_set_dsss_cont (syn, code, SF, CPS, WFM_DSSS_DATA_PRBS, NULL,
                                 0);
  (void)wfm_synth_set_dsss_window (syn, W_SYM, F_SYM);
  return syn;
}

/* The hand-off flavor, §12.3's tracker parameters, the design's release
   interval; the carrier-to-code aid on through the channel (§12.5). */
static double g_d0_ppm      = 0.0; /* --trace: the channel's own numbers  */
static double g_ppm_s       = RAMP_PPM_S;
static size_t g_trace_every = 0; /* trace line cadence, blocks; 0 = none  */

static async_dsss_receiver_state_t *
make_rx (const uint8_t *code, double cn0_dbhz, int cond)
{
  return async_dsss_receiver_create_handoff (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0_dbhz, 1e-2, 0.9, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, false, 100000, cond != COND_STATIC ? CARRIER_HZ : 0.0,
      LOST_CONFIRM_S);
}

/* Is global sample `n` inside the code-only window, by the synth's clock? */
static int
in_window (uint64_t n)
{
  double   chip = (double)n / (double)SPC;
  uint64_t sym  = (uint64_t)floor (chip / CPS);
  return (sym % F_SYM) < W_SYM;
}

static int
run_trial (const uint8_t *code, int cond, double cn0_dbhz, uint32_t seed,
           size_t n_frames, trial_t *out)
{
  memset (out, 0, sizeof *out);
  out->frames              = calloc (n_frames, sizeof *out->frames);
  const size_t lock_blocks = (size_t)(MAX_LOCK_S * FS / (double)TE);
  /* Enough blocks for the settle plus n_frames whole frames and one more
     window's worth of slack. */
  const double frame_s = (double)F_SYM / SYM_RATE;
  const size_t max_blocks
      = lock_blocks
        + (size_t)(((double)n_frames + 1.0) * frame_s * FS / (double)TE);

  wfm_synth_state_t *syn = make_emitter (code, seed);
  awgn_state_t      *g   = awgn_create (
      seed * 7919u + 1u,
      awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
  async_dsss_receiver_state_t *rx = make_rx (code, cn0_dbhz, cond);
  /* The channel: the emitter's clean signal through it, the noise added at
     the receiver (it does not ride the emitter's clock). The table's
     conditions are fixed; --trace hands the RATE condition its own numbers. */
  const double             d0   = cond == COND_OFFSET ? RAMP_D0_PPM
                                  : cond == COND_RATE ? g_d0_ppm
                                                      : 0.0;
  const double             rate = cond == COND_RATE ? g_ppm_s : 0.0;
  doppler_channel_state_t *ch
      = cond != COND_STATIC ? doppler_channel_create (FS, CARRIER_HZ, d0, rate)
                            : NULL;
  const double   f0  = d0 * 1e-6 * CARRIER_HZ;
  float complex *sig = dp_xmalloc (TE * sizeof *sig);
  float complex *blk = dp_xmalloc (TE * sizeof *blk);
  /* The channel's output runs a few samples off TE per block; a carry
     buffer hands the receiver whole epochs. */
  float complex *fifo = dp_xmalloc (4 * TE * sizeof *fifo);
  size_t         pend = 0;
  size_t         cap  = async_dsss_receiver_steps_max_out (rx);
  float complex *syms = dp_xmalloc ((cap ? cap : TE) * sizeof *syms);
  DP_REQUIRE_MSG (syn && g && rx && out->frames && (cond == COND_STATIC || ch),
                  "the emitter, the noise, the channel and the receiver open");
  /* The searcher that seeds it: the shipped continuous engine over SPEC's
     Doppler span (one native span without a channel), fed the same blocks
     until its first hit. */
  acq_state_t *acq = acq_create_continuous (
      code, SF, SPC, CHIP_RATE, SYM_RATE, cn0_dbhz,
      cond != COND_STATIC ? 1.2 * RAMP_D0_PPM * 1e-6 * CARRIER_HZ : 0.0, 1e-3,
      0.9, 0, 1, 0.0);
  DP_REQUIRE_MSG (acq != NULL, "the searcher opens");
  /* Through the channel the code clock rides the carrier's Doppler: the
     searcher is told the carrier, as the receiver is (#1254, #1256). */
  if (cond != COND_STATIC)
    DP_REQUIRE (acq_set_carrier_freq_hz (acq, CARRIER_HZ) == DP_OK);
  int seeded = 0;

  uint64_t n        = 0; /* received samples handed to the receiver */
  size_t   held     = 0;
  int      prev_win = 0, was_in_window = 0;
  frame_t *f        = NULL; /* the window being watched, or after it */
  int      after    = 0;    /* past the window, watching the pull-in */
  size_t   since    = 0;    /* blocks since the window's last block  */
  size_t   both_run = 0;
  int      done     = 0;
  for (size_t b = 0; b < max_blocks && !done; b++)
    {
      wfm_synth_steps (syn, sig, TE);
      if (ch)
        pend += doppler_channel_execute (ch, sig, TE, fifo + pend, 2 * TE);
      else
        {
          memcpy (fifo + pend, sig, TE * sizeof *sig);
          pend += TE;
        }
      if (pend < TE)
        continue;
      memcpy (blk, fifo, TE * sizeof *blk);
      pend -= TE;
      memmove (fifo, fifo + TE, pend * sizeof *fifo);
      awgn_generate (g, TE, sig, TE);
      for (size_t i = 0; i < TE; i++)
        blk[i] += sig[i];
      n += TE;
      if (!seeded)
        {
          /* The searcher sees every block until it hits; the hit's
             hand-off record is the seed, applied at this block's end (the
             engine consumes whole epochs, and a block is one). */
          acq_result_t hit;
          if (acq_push (acq, blk, TE, &hit, 1) == 0)
            {
              if (b + 1 >= lock_blocks)
                break; /* never acquired */
              continue;
            }
          acq_handoff_t ho;
          acq_build_handoff (acq, &hit, SF, SPC, &ho);
          DP_REQUIRE_MSG (async_dsss_receiver_seed (rx, ho.chip_phase,
                                                    ho.doppler_hz_est,
                                                    ho.cn0_dbhz_est)
                              == 0,
                          "the hand-off receiver takes the searcher's seed");
          out->seed_s = (double)n / FS;
          seeded      = 1;
          if (g_trace_every)
            printf ("  seeded at %.1f ms: chip %.2f, Doppler %.1f Hz "
                    "(truth %.1f), C/N0 %.1f dB-Hz\n",
                    out->seed_s * 1e3, ho.chip_phase, ho.doppler_hz_est,
                    ch ? CARRIER_HZ
                             * (doppler_channel_scale (ch, out->seed_s) - 1.0)
                       : 0.0,
                    ho.cn0_dbhz_est);
          continue;
        }
      (void)async_dsss_receiver_steps (rx, blk, TE, syms, cap ? cap : TE);
      const int code_on = async_dsss_receiver_get_code_locked (rx) == 1;
      const int sym_on  = async_dsss_receiver_get_locked (rx) == 1;
      const int trk     = async_dsss_receiver_get_tracking (rx) == 1;
      const int lost    = async_dsss_receiver_get_lost (rx) == 1;
      const int win     = in_window (n - TE / 2);
      if (g_trace_every && b % g_trace_every == 0)
        {
          const double t = (double)n / FS;
          printf ("  t=%6.2f s  %-8s  dopp est %9.1f Hz  loop2 nco %+.6f  "
                  "err %+.3f  truth %9.1f Hz  code rate %.6f  chip %8.2f  "
                  "lock %.3f/%.3f  code %d sym %d  win %d\n",
                  t,
                  async_dsss_receiver_get_lost (rx) == 1       ? "lost"
                  : async_dsss_receiver_get_tracking (rx) == 1 ? "tracking"
                                                               : "refining",
                  async_dsss_receiver_get_doppler_hz (rx),
                  async_dsss_receiver_get_nco_freq (rx),
                  async_dsss_receiver_get_mpsk_last_error (rx),
                  ch ? CARRIER_HZ * (doppler_channel_scale (ch, t) - 1.0)
                     : 0.0,
                  async_dsss_receiver_get_code_rate (rx),
                  async_dsss_receiver_get_chip_phase (rx),
                  async_dsss_receiver_get_lock_metric (rx),
                  async_dsss_receiver_get_lock_threshold (rx), code_on, sym_on,
                  win);
        }

      if (!out->settled)
        {
          held = (trk && sym_on) ? held + 1 : 0;
          if (held >= SETTLE_BLOCKS)
            {
              out->settled  = 1;
              out->settle_s = (double)(b + 1) * (double)TE / FS;
              was_in_window = win; /* a window already open is skipped */
            }
          else if (b + 1 >= lock_blocks)
            break; /* never settled */
          prev_win = win;
          continue;
        }

      if (win && !prev_win && !was_in_window)
        {
          /* A window opens: start its record (the previous window's
             pull-in watch ends here). */
          if (out->n_frames >= n_frames)
            break;
          f        = &out->frames[out->n_frames++];
          after    = 0;
          both_run = 0;
        }
      if (!win)
        was_in_window = 0;
      if (f && win && !after)
        {
          f->blocks++;
          f->code_off += !code_on;
          f->sym_off += !sym_on;
          if (!code_on && !sym_on)
            {
              f->both_off++;
              both_run++;
              if (both_run > f->both_run_max)
                f->both_run_max = both_run;
            }
          else
            both_run = 0;
          f->lost |= lost;
        }
      else if (f && !win)
        {
          if (!after)
            {
              after = 1;
              since = 0;
            }
          since++;
          f->data_blocks++;
          f->data_code_off += !code_on;
          f->data_sym_off += !sym_on;
          f->lost |= lost;
          if (code_on && !f->pull_code)
            f->pull_code = since;
          if (sym_on && !f->pull_sym)
            f->pull_sym = since;
          if (out->n_frames >= n_frames && since > (size_t)(0.5 * FS / TE))
            break; /* the last window's pull-in watched for 0.5 s */
        }
      prev_win = win;
    }

  free (syms);
  free (fifo);
  free (blk);
  free (sig);
  doppler_channel_destroy (ch);
  acq_destroy (acq);
  async_dsss_receiver_destroy (rx);
  awgn_destroy (g);
  wfm_synth_destroy (syn);
  return 0;
}

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

static double
ms (size_t blocks)
{
  return (double)blocks * (double)TE / FS * 1e3;
}

/* One trial's windows, one line each, and the trial's totals. Returns the
   totals through `tot` for the C/N0 summary. */
static void
report_trial (const trial_t *t, uint32_t seed, frame_t *tot)
{
  printf ("  seed %u: acquired at %.1f ms, settled at %.1f ms; %zu windows\n",
          seed, t->seed_s * 1e3, t->settle_s * 1e3, t->n_frames);
  printf ("    window  blocks  code off   sym off    both off  longest "
          "both-off  lost  pull-in code   pull-in sym  | data: code off  "
          "sym off\n");
  for (size_t i = 0; i < t->n_frames; i++)
    {
      const frame_t *f = &t->frames[i];
      printf (
          "    %-6zu  %6zu  %8.4f  %8.4f  %9.4f  %10.1f ms  %4s  "
          "%9.1f ms  %9.1f ms  |  %12.4f  %8.4f\n",
          i + 1, f->blocks, (double)f->code_off / (double)f->blocks,
          (double)f->sym_off / (double)f->blocks,
          (double)f->both_off / (double)f->blocks, ms (f->both_run_max),
          f->lost ? "YES" : "no", f->pull_code ? ms (f->pull_code - 1) : 0.0,
          f->pull_sym ? ms (f->pull_sym - 1) : 0.0,
          f->data_blocks ? (double)f->data_code_off / (double)f->data_blocks
                         : 0.0,
          f->data_blocks ? (double)f->data_sym_off / (double)f->data_blocks
                         : 0.0);
      tot->blocks += f->blocks;
      tot->code_off += f->code_off;
      tot->sym_off += f->sym_off;
      tot->both_off += f->both_off;
      if (f->both_run_max > tot->both_run_max)
        tot->both_run_max = f->both_run_max;
      tot->lost |= f->lost;
      if (f->pull_code > tot->pull_code)
        tot->pull_code = f->pull_code;
      if (f->pull_sym > tot->pull_sym)
        tot->pull_sym = f->pull_sym;
      tot->data_blocks += f->data_blocks;
      tot->data_code_off += f->data_code_off;
      tot->data_sym_off += f->data_sym_off;
    }
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);
  if (argc > 1 && strcmp (argv[1], "--trace") == 0)
    {
      g_trace_every = 1000;
      /* --trace [d0_ppm ppm_s [cn0 [seed]]]: one trial through the channel,
         its Doppler and rate, the C/N0 and the seed as given (the RATE
         condition, 45 dB-Hz, 100). */
      g_d0_ppm           = argc > 3 ? atof (argv[2]) : 0.0;
      g_ppm_s            = argc > 3 ? atof (argv[3]) : RAMP_PPM_S;
      const double   cn0 = argc > 4 ? atof (argv[4]) : 45.0;
      const uint32_t sd  = argc > 5 ? (uint32_t)atoi (argv[5]) : 100u;
      trial_t        t;
      printf ("--- channel %.1f ppm, %.2f ppm/s; %.0f dB-Hz, seed %u ---\n",
              g_d0_ppm, g_ppm_s, cn0, sd);
      DP_REQUIRE (run_trial (code, COND_RATE, cn0, sd, 2, &t) == 0);
      printf ("    settled %d after %.1f ms\n", t.settled, t.settle_s * 1e3);
      free (t.frames);
      return 0;
    }

  printf ("tracker through the window: Gold-1023 at 5 Mcps, spc 2, 2700 "
          "sym/s async BPSK, %u code-only symbols of every %u (%.1f ms "
          "window, %.2f s frame); hand-off receiver, release at %.0f s; "
          "block = one epoch (%.3f ms)\n\n",
          W_SYM, F_SYM, (double)W_SYM / SYM_RATE * 1e3,
          (double)F_SYM / SYM_RATE, LOST_CONFIRM_S, (double)TE / FS * 1e3);

  const double cn0s[] = { 45.0, 40.0 };
  /* --check: 45 dB-Hz, one seed, every condition -- with #1249's
     hand-over fix all three settle in tens of ms, and the windows are as
     clean through the channel as without it; the check says so. A line of
     trace every ~4 s keeps the diagnostics in the record. */
  const size_t n_cn0   = check ? 1 : 2;
  const size_t n_seeds = check ? 1 : N_SEEDS;
  const size_t frames  = check ? CHECK_FRAMES : N_FRAMES;
  if (check)
    g_trace_every = 20000;
  for (size_t kc = 0; kc < (size_t)N_COND * n_cn0; kc++)
    {
      const int    cond = (int)(kc / n_cn0);
      const size_t ci   = kc % n_cn0;
      printf ("=== %s, C/N0 %.0f dB-Hz (Es/N0 %.1f dB) ===\n", cond_name[cond],
              cn0s[ci], cn0s[ci] - 10.0 * log10 (SYM_RATE));
      frame_t tot     = { 0 };
      int     settled = 0;
      for (size_t k = 0; k < n_seeds; k++)
        {
          trial_t t;
          DP_REQUIRE (
              run_trial (code, cond, cn0s[ci], 100u + (uint32_t)k, frames, &t)
              == 0);
          if (t.settled)
            {
              settled++;
              report_trial (&t, 100u + (uint32_t)k, &tot);
            }
          else
            printf ("  seed %u: never settled within %.0f s\n",
                    100u + (uint32_t)k, MAX_LOCK_S);
          free (t.frames);
        }
      if (tot.blocks)
        printf ("  %d of %zu trials settled; over %zu window blocks: code "
                "lock off %.4f, symbol lock off %.4f, both off %.4f, "
                "longest both-off run %.1f ms, release fired: %s; worst "
                "pull-in code %.1f ms, symbol %.1f ms; over %zu data "
                "blocks: code off %.4f, symbol off %.4f\n\n",
                settled, n_seeds, tot.blocks,
                (double)tot.code_off / (double)tot.blocks,
                (double)tot.sym_off / (double)tot.blocks,
                (double)tot.both_off / (double)tot.blocks,
                ms (tot.both_run_max), tot.lost ? "YES" : "no",
                tot.pull_code ? ms (tot.pull_code - 1) : 0.0,
                tot.pull_sym ? ms (tot.pull_sym - 1) : 0.0, tot.data_blocks,
                tot.data_blocks
                    ? (double)tot.data_code_off / (double)tot.data_blocks
                    : 0.0,
                tot.data_blocks
                    ? (double)tot.data_sym_off / (double)tot.data_blocks
                    : 0.0);
      if (check)
        {
          DP_CHECK_MSG (settled == 1 && tot.blocks > 0,
                        "the hand-off receiver settles on its seed and "
                        "the windows are seen");
          DP_CHECK_MSG (!tot.lost, "the release rule never fires on a "
                                   "pure-code window");
          /* The design's expectations (§12 step 12), pinned where the
             measurement put them -- see §12.9. */
          DP_CHECK_MSG (tot.code_off * 100 <= tot.blocks,
                        "code lock holds through the window (off in at "
                        "most 1% of its blocks)");
          DP_CHECK_MSG (ms (tot.both_run_max) < 0.5 * LOST_CONFIRM_S * 1e3,
                        "the longest both-off run stays under half the "
                        "release interval");
          DP_CHECK_MSG (tot.pull_sym == 0 || ms (tot.pull_sym - 1) <= 100.0,
                        "symbol lock is back within 100 ms of the data "
                        "resuming");
        }
    }
  if (check)
    DP_TEST_END ("validate_tracker_through_window");
  return 0;
}
