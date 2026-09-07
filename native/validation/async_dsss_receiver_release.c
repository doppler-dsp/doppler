/**
 * @file async_dsss_receiver_release.c
 * @brief When does the tracking receiver say its emitter is gone -- and how
 *        often does it say so falsely?
 *
 * The continuous async-DSSS design (docs/design/async-dsss-receiver.md §10,
 * §11.2) releases an assigned receiver on one rule: code lock drops and
 * stays dropped for a confirm interval; symbol lock alone is a degrade.
 * The rule rests on two claims about the receiver as built, neither of
 * which anything measured: that code lock is the flag that follows PRESENCE
 * (an emitter that leaves takes its code with it, promptly), and that it
 * rides through what only disturbs the carrier -- a phase step, a deep fade
 * -- which symbol lock does not. And the confirm interval is sized from a
 * false-release budget, which needs the per-block miss probability of each
 * flag while the emitter is simply on.
 *
 * Method. The operating point: a 1023-chip Gold code at 5 Mcps, two samples
 * per chip, asynchronous BPSK data at 2700 sym/s from the shipped synth
 * (`wfm_synth` continuous DSSS with its own PRBS data -- the generator
 * wfmgen renders the waveform with), rendered clean and block by block;
 * noise from the shipped awgn generator, sized by awgn_amplitude_for_snr()
 * from the C/N0. Nothing here builds a chip, a bit or a sigma by hand. The
 * events are edits to the rendered SIGNAL block before the noise is added:
 * a switch-off drops it, a fade scales it, a phase step rotates it -- the
 * composer cannot express a fade of a continuing emitter (it rebuilds the
 * source at a segment boundary, which would restart the code), so the
 * scalar is applied here and named for what it is. The receiver is fed one
 * epoch (2046 samples) at a time and both flags are read after every block.
 * Once the receiver is tracking and symbol lock has held for SETTLE_BLOCKS
 * consecutive blocks, an event is applied at a block boundary. (Settling was
 * first keyed on both flags; the first sweep showed code lock dips for a block
 * or two about three times a second on a healthy signal, so no trial at 40
 * dB-Hz ever held it for 200 blocks -- the criterion was measuring the flag's
 * chatter, not lock.)
 *
 *   off      the emitter stops (signal amplitude 0, noise continues)
 *   fade10   the signal is 10 dB down for FADE_S seconds, then back
 *   fade20   20 dB down for FADE_S seconds, then back
 *   phase    the carrier phase steps by pi/2 and stays there
 *   cross    the emitter stops at T_OFF_S, and a second emitter -- same
 *            code, CROSS_PPM of the carrier through the shipped
 *            doppler_channel, CROSS_PPM * 5 = 7.6 chips per second
 *            relative -- has been on the air all along with its burn-in
 *            chosen so its code phase crosses the departed receiver's
 *            CROSS_AFTER_S later. Measured: the flags' returns over the
 *            watch and the receiver's chip rate over the watch's last
 *            two seconds -- a receiver that captured the neighbour runs
 *            at its chips per second (#1271: a departed receiver in the
 *            pool followed a neighbour at 44.8 for fourteen seconds, its
 *            code flag flickering sixteen times); one that coasts stands
 *            still, its code flag blipping a few times as the neighbour
 *            passes through the held phase and then no more
 *   cross-on the same neighbour at CROSS_ON_PPM (475 Hz, under a chip per
 *            second) crosses the receiver's phase while its emitter STAYS
 *            on -- the ten-minute soak's one slow crossing of two live
 *            emitters, which degraded both receivers -- watched for
 *            OFF_WATCH_S: whether the flags drop, the longest both-off
 *            run, whether they are back by the end, and the chip rate at
 *            the end (its own emitter's, zero here, or the neighbour's)
 *   on       nothing happens; the flags are watched for ON_S seconds
 *
 * Measured per trial: samples from the event to the first block with each
 * flag off (or "held" if it never dropped within the watch), whether each
 * flag is back on at the end (recovered), and -- for `off`, watched for
 * OFF_WATCH_S, several release intervals -- how many times each flag came
 * back on noise after its drop: a return restarts the receiver's release
 * clock (§10), and the aided code flag returned about once a second until
 * #1264 (a look whose window overlapped the last look's read the same
 * noise again). For `on`: the fraction of blocks
 * with each flag off, and the count of separate drop episodes -- the
 * per-look miss probability the confirm interval is sized from.
 *
 * Usage:
 *   validate_async_dsss_receiver_release            full table
 *   validate_async_dsss_receiver_release --check    two switch-off trials:
 *                                                   code lock must drop
 *                                                   within CHECK_MAX_S
 */
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

#define SETTLE_BLOCKS 200 /* both flags on for this many blocks first  */
#define MAX_LOCK_S 6.0    /* give up on a trial that never settles      */
#define POST_S 1.5        /* watch this long after a fade or a step     */
#define OFF_WATCH_S                                                           \
  8.0                    /* after a switch-off: the flags' returns on         \
                            noise over several release intervals        */
#define FADE_S 1.0       /* the fade's duration                        */
#define CARRIER_HZ 2.5e9 /* the crossing emitter's ppm -> Hz and chips */
#define CROSS_PPM                                                             \
  0.8                     /* 2 kHz off, 4 chips/s: captured without the       \
                             coast on every seed, 231 flickers on one    */
#define T_OFF_S 2.0       /* cross: the switch-off, if settled by then  */
#define CROSS_AFTER_S 0.6 /* cross: the neighbour's phase meets ours    */
#define CROSS_ON_PPM                                                          \
  0.19                  /* cross-on: 475 Hz, 0.95 chips/s -- the soak's       \
                           slow crossing of two live emitters           */
#define ON_S 3.0        /* the on-time watched for false drops        */
#define CHECK_MAX_S 0.5 /* --check: code lock must drop within this   */

enum
{
  EV_OFF,
  EV_FADE10,
  EV_FADE20,
  EV_PHASE,
  EV_CROSS,
  EV_CROSS_ON,
  EV_ON,
  N_EV
};
static const char *ev_name[N_EV]
    = { "off", "fade10", "fade20", "phase", "cross", "cross-on", "on" };

typedef struct
{
  int    settled;      /* tracking + symbol lock on before MAX_LOCK_S  */
  double t_code_s;     /* event -> code lock off, s; <0 = held         */
  double t_sym_s;      /* event -> symbol lock off, s; <0 = held       */
  double t_both_s;     /* event -> both flags off at once; <0 = never  */
  double both_max_s;   /* longest run of both-off during the watch, s  */
  int    code_back;    /* code lock on at the end of the watch         */
  int    sym_back;     /* symbol lock on at the end of the watch       */
  double p_code_off;   /* `on` only: fraction of blocks code lock off  */
  double p_sym_off;    /* `on` only: fraction of blocks symbol lock off */
  double p_both_off;   /* `on` only: fraction of blocks both flags off  */
  size_t code_drops;   /* `on` only: separate code-lock drop episodes  */
  size_t sym_drops;    /* `on` only: separate symbol-lock drop episodes */
  size_t code_returns; /* `off`: code lock back on after its drop, times */
  double rate_after;   /* `cross`: chip rate over the watch's last 2 s  */
  size_t sym_returns;  /* `off`: symbol lock back on after its drop     */
  double watch_s;      /* the watch, s                                  */
} trial_t;

/* The emitter: the shipped continuous-DSSS synth, clean (no AWGN child),
   PRBS data from its own PN register, seeded per trial. */
static wfm_synth_state_t *
make_emitter (const uint8_t *code, uint32_t seed)
{
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                          seed, (int)SPC, 15, 0, 0, 0.0);
  if (syn
      && wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                  WFM_DSSS_DATA_PRBS, NULL, 0)
             != 0)
    {
      wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

static async_dsss_receiver_state_t *
make_rx (const uint8_t *code, double cn0_dbhz)
{
  return async_dsss_receiver_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0_dbhz, 1e-2, 0.9, 500.0, 4, 8,
      0, 100.0, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
}

static int
run_trial (const uint8_t *code, int ev, double cn0_dbhz, uint32_t seed,
           trial_t *out)
{
  memset (out, 0, sizeof *out);
  out->t_code_s = out->t_sym_s = out->t_both_s = -1.0;

  const double watch_s = ev == EV_ON ? ON_S
                         : ev == EV_OFF || ev == EV_CROSS || ev == EV_CROSS_ON
                             ? OFF_WATCH_S
                             : POST_S;
  const size_t watch_blocks = (size_t)(watch_s * FS / (double)TE);
  out->watch_s              = watch_s;
  const size_t lock_blocks  = (size_t)(MAX_LOCK_S * FS / (double)TE);
  const size_t fade_blocks  = (size_t)(FADE_S * FS / (double)TE);
  const size_t max_blocks   = lock_blocks + watch_blocks + 2;

  wfm_synth_state_t *syn = make_emitter (code, seed);
  /* C/N0 to SNR over fs is the one conversion; the amplitude is the
     library's answer to "per rail or total", not a sigma derived here. */
  awgn_state_t *g = awgn_create (
      seed * 7919u + 1u,
      awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
  async_dsss_receiver_state_t *rx   = make_rx (code, cn0_dbhz);
  float complex               *sig  = malloc (TE * sizeof *sig);
  float complex               *blk  = malloc (TE * sizeof *blk);
  size_t                       cap  = async_dsss_receiver_steps_max_out (rx);
  float complex               *syms = malloc ((cap ? cap : TE) * sizeof *syms);
  if (!syn || !g || !rx || !sig || !blk || !syms)
    return 1;

  /* The crossing neighbour (EV_CROSS): its own synth through the channel
     at CROSS_PPM, burnt in so that its code phase meets the first
     emitter's at k_c = (T_OFF_S + CROSS_AFTER_S) * FS. The first emitter
     is the synth straight out (chip k/SPC at sample k); the channel's
     output k carries its input at k(1+d) - delay (its documented
     mapping), so the burn is delay - k_c * d, modulo a code period. The
     channel's output length varies by the resampler, so it is carried in
     a small FIFO and taken one block at a time. */
  wfm_synth_state_t       *synb  = NULL;
  doppler_channel_state_t *chb   = NULL;
  float complex           *bfifo = NULL, *bin = NULL, *btmp = NULL;
  size_t                   bfill = 0, bcap = 0;
  const double             xppm = ev == EV_CROSS_ON ? CROSS_ON_PPM : CROSS_PPM;
  if (ev == EV_CROSS || ev == EV_CROSS_ON)
    {
      synb = make_emitter (code, seed + 1000u);
      chb  = doppler_channel_create (FS, CARRIER_HZ, xppm, 0.0);
      if (!synb || !chb)
        return 1;
      bcap               = doppler_channel_execute_max_out (chb);
      bin                = dp_xmalloc (TE * sizeof *bin);
      btmp               = dp_xmalloc (bcap * sizeof *btmp);
      bfifo              = dp_xmalloc ((bcap + 2 * TE) * sizeof *bfifo);
      const double k_c   = (T_OFF_S + CROSS_AFTER_S) * FS;
      const double delay = doppler_channel_get_delay_samples (chb);
      double       burn  = fmod (delay - k_c * xppm * 1e-6, (double)TE);
      if (burn < 0.0)
        burn += (double)TE;
      size_t to_burn = (size_t)(burn + 0.5);
      while (to_burn > 0)
        {
          wfm_synth_steps (synb, bin, TE);
          size_t n = doppler_channel_execute (chb, bin, TE, btmp, bcap);
          size_t d = n < to_burn ? n : to_burn;
          memcpy (bfifo + bfill, btmp + d, (n - d) * sizeof *btmp);
          bfill += n - d;
          to_burn -= d;
        }
    }
  const size_t off_blocks = (size_t)(T_OFF_S * FS / (double)TE);
  double       chip_a     = 0.0;
  size_t       n_rate     = 0;

  size_t        held = 0, event_block = 0, b = 0;
  int           in_event  = 0;
  int           prev_code = 1, prev_sym = 1;
  size_t        off_code = 0, off_sym = 0, off_both = 0, watched = 0;
  size_t        both_run = 0, both_run_max = 0;
  float complex rot = 1.0f;

  for (; b < max_blocks; b++)
    {
      /* Signal for this block, then the event's edit, then noise. */
      double amp = 1.0;
      if (in_event)
        {
          size_t since = b - event_block;
          if (ev == EV_OFF || ev == EV_CROSS)
            amp = 0.0;
          else if (ev == EV_FADE10 || ev == EV_FADE20)
            amp = (since < fade_blocks)
                      ? pow (10.0, (ev == EV_FADE10 ? -10.0 : -20.0) / 20.0)
                      : 1.0;
          else if (ev == EV_PHASE)
            rot = (float complex) (0.0 + I * 1.0); /* a pi/2 step */
        }
      /* The emitter keeps rendering through every event -- its code and
         data never restart -- and the event is applied to what it rendered.
         The noise is a separate stream, so a fade scales the signal alone. */
      wfm_synth_steps (syn, sig, TE);
      awgn_generate (g, TE, blk, TE);
      const float complex gain = (float)amp * rot;
      for (size_t i = 0; i < TE; i++)
        blk[i] += gain * sig[i];
      if (ev == EV_CROSS || ev == EV_CROSS_ON)
        {
          /* The neighbour, on the air throughout, at the same level. */
          while (bfill < TE)
            {
              wfm_synth_steps (synb, bin, TE);
              size_t n = doppler_channel_execute (chb, bin, TE, btmp, bcap);
              memcpy (bfifo + bfill, btmp, n * sizeof *btmp);
              bfill += n;
            }
          for (size_t i = 0; i < TE; i++)
            blk[i] += bfifo[i];
          memmove (bfifo, bfifo + TE, (bfill - TE) * sizeof *bfifo);
          bfill -= TE;
        }

      (void)async_dsss_receiver_steps (rx, blk, TE, syms, cap ? cap : TE);
      int code_on = async_dsss_receiver_get_code_locked (rx) == 1;
      int sym_on  = async_dsss_receiver_get_locked (rx) == 1;
      int trk     = async_dsss_receiver_get_tracking (rx) == 1;

      if (!in_event)
        {
          held = (trk && sym_on) ? held + 1 : 0;
          if (held >= SETTLE_BLOCKS
              && ((ev != EV_CROSS && ev != EV_CROSS_ON)
                  || b + 1 >= off_blocks))
            {
              in_event     = 1;
              event_block  = b + 1;
              out->settled = 1;
            }
          else if (b + 1 >= lock_blocks)
            break; /* never settled */
          continue;
        }

      const double t = (double)(b + 1 - event_block) * (double)TE / FS;
      watched++;
      if (!code_on)
        {
          off_code++;
          if (out->t_code_s < 0.0)
            out->t_code_s = t;
          if (prev_code)
            out->code_drops++;
        }
      else if (!prev_code && out->t_code_s >= 0.0)
        out->code_returns++; /* back on, after it had dropped */
      if (!sym_on)
        {
          off_sym++;
          if (out->t_sym_s < 0.0)
            out->t_sym_s = t;
          if (prev_sym)
            out->sym_drops++;
        }
      else if (!prev_sym && out->t_sym_s >= 0.0)
        out->sym_returns++;
      if (!code_on && !sym_on)
        {
          off_both++;
          if (out->t_both_s < 0.0)
            out->t_both_s = t;
          both_run++;
          if (both_run > both_run_max)
            both_run_max = both_run;
        }
      else
        both_run = 0;
      prev_code = code_on;
      prev_sym  = sym_on;
      if ((ev == EV_CROSS || ev == EV_CROSS_ON)
          && watched + (size_t)(2.0 * FS / (double)TE) >= watch_blocks)
        {
          /* The watch's last two seconds: the receiver's chip rate. */
          const double c = async_dsss_receiver_get_chip_phase (rx);
          if (n_rate == 0)
            chip_a = c;
          else
            {
              double dc = fmod (c - chip_a + 1.5 * SF, (double)SF) - 0.5 * SF;
              out->rate_after = dc / ((double)n_rate * (double)TE / FS);
            }
          n_rate++;
        }
      if (watched >= watch_blocks)
        {
          out->code_back = code_on;
          out->sym_back  = sym_on;
          break;
        }
    }
  out->p_code_off = watched ? (double)off_code / (double)watched : 0.0;
  out->p_sym_off  = watched ? (double)off_sym / (double)watched : 0.0;
  out->p_both_off = watched ? (double)off_both / (double)watched : 0.0;
  out->both_max_s = (double)both_run_max * (double)TE / FS;

  free (bfifo);
  free (btmp);
  free (bin);
  if (chb)
    doppler_channel_destroy (chb);
  if (synb)
    wfm_synth_destroy (synb);
  free (syms);
  free (blk);
  free (sig);
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

static void
fmt_t (double t, char *s, size_t n)
{
  if (t < 0.0)
    snprintf (s, n, "held");
  else
    snprintf (s, n, "%.1f ms", t * 1e3);
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  if (check)
    {
      size_t returns = 0;
      for (uint32_t sd = 1; sd <= 2; sd++)
        {
          trial_t t;
          DP_REQUIRE (run_trial (code, EV_OFF, 45.0, sd, &t) == 0);
          DP_CHECK (t.settled);
          /* After a switch-off both flags must be down within CHECK_MAX_S
             and stay down: `held` (< 0) is as much a failure as late. */
          DP_CHECK (t.t_code_s >= 0.0 && t.t_code_s <= CHECK_MAX_S);
          DP_CHECK (t.t_sym_s >= 0.0 && t.t_sym_s <= CHECK_MAX_S);
          DP_CHECK (!t.code_back && !t.sym_back);
          printf ("  trial %u: code lock off at %.1f ms, symbol lock off at "
                  "%.1f ms; over %.0f s of noise code lock returned %zu "
                  "time(s), symbol lock %zu\n",
                  sd, t.t_code_s * 1e3, t.t_sym_s * 1e3, t.watch_s,
                  t.code_returns, t.sym_returns);
          returns += t.code_returns + t.sym_returns;
        }
      /* The flags' returns on noise over the two watches -- eight release
         intervals. The aided code flag used to return about once a second
         (#1264: a look whose window overlapped the last look's read the
         same noise again), 6 and 11 times per trial here, and restarted
         the receiver's release clock. Fixed, the full table measures
         0.004 per second (one in 240 s), so two watches expect 0.06 and a
         bound of two is three decades under the defect and a fluke-proof
         ceiling on the rate -- one seed here does draw two. */
      DP_CHECK_MSG (returns <= 2, "the flags all but never return on noise "
                                  "within the watches");
      /* The neighbour crossing a departed receiver's phase (#1271): the
         loops coast, so the receiver neither follows it (its chip rate
         over the watch's last two seconds stays at the departed
         emitter's, zero here) nor flickers its code flag on it. Without
         the coast the receiver captures the neighbour and runs at its
         7.6 chips per second. */
      {
        trial_t t;
        DP_REQUIRE (run_trial (code, EV_CROSS, 45.0, 3u, &t) == 0);
        DP_CHECK (t.settled);
        printf ("  cross: code lock off at %.1f ms; over %.0f s with the "
                "neighbour crossing at +%.1f s: code lock returned %zu "
                "time(s), symbol lock %zu; chip rate over the last 2 s "
                "%+.2f chips/s (the neighbour's is %+.2f)\n",
                t.t_code_s * 1e3, t.watch_s, CROSS_AFTER_S, t.code_returns,
                t.sym_returns, t.rate_after, CROSS_PPM * 1e-6 * CHIP_RATE);
        /* Measured (design section 12.19): coasting, the rate is 0.00 on
           every seed and the code flag blips 8-10 times as the neighbour
           passes through the held phase; without the coast the receiver
           follows at the neighbour's 4.00 chips per second and the flag
           flickers 230-250 times. */
        DP_CHECK_MSG (fabs (t.rate_after) < 1.0,
                      "a departed receiver does not follow a neighbour "
                      "crossing its code phase");
        DP_CHECK_MSG (t.code_returns + t.sym_returns <= 20,
                      "its code flag blips as the neighbour passes and no "
                      "more -- not the flicker of a receiver that follows");
      }
      /* The live crossing (design section 12.19): the emitter stays on
         and a neighbour 475 Hz off crosses its phase at 0.95 chips per
         second. The loops run on one flag down, so the receiver rides it
         out; held on the symbol flag alone (the rejected rule) both flags
         stayed down for the rest of the watch. */
      {
        trial_t t;
        DP_REQUIRE (run_trial (code, EV_CROSS_ON, 45.0, 3u, &t) == 0);
        DP_CHECK (t.settled);
        printf ("  cross-on: code lock off at %.1f ms, symbol lock off at "
                "%.1f ms, both off at %.1f ms (longest run %.0f ms); back "
                "at the end: code %d, symbol %d; chip rate over the last "
                "2 s %+.2f chips/s\n",
                t.t_code_s * 1e3, t.t_sym_s * 1e3, t.t_both_s * 1e3,
                t.both_max_s * 1e3, t.code_back, t.sym_back, t.rate_after);
        /* Which of the two it ends on is not the hold's to decide: two
           equal signals within half a chip and 475 Hz are one peak to the
           loops, and a trial can end on the neighbour with both flags up
           throughout (#1275). What the hold owns is that the flags do not
           drop together and stay down. */
        DP_CHECK_MSG (t.code_back && t.sym_back && t.both_max_s < 1.0,
                      "a receiver whose emitter is crossed by a live "
                      "neighbour keeps its flags through the crossing");
      }
      DP_TEST_END ("validate_async_dsss_receiver_release");
    }

  const double cn0s[]  = { 45.0, 40.0 };
  const int    n_trial = 30;
  const int    n_on    = 10;
  printf ("release: Gold-1023 at 5 Mcps, spc 2, 2700 sym/s async BPSK; "
          "block = one epoch (%.3f ms)\n\n",
          (double)TE / FS * 1e3);
  for (size_t ci = 0; ci < 2; ci++)
    {
      printf ("=== C/N0 %.0f dB-Hz (Es/N0 %.1f dB) ===\n", cn0s[ci],
              cn0s[ci] - 10.0 * log10 (SYM_RATE));
      printf ("  event    trials settled   code off: median  max   held   "
              "sym off: median  max   held   both off: median   longest "
              "both-off run max   code back   sym back\n");
      for (int ev = 0; ev < EV_ON; ev++)
        {
          double tc[64], ts[64], tb[64];
          int    nc = 0, ns = 0, nb = 0, settled = 0, held_c = 0, held_s = 0;
          int    back_c = 0, back_s = 0;
          double both_run_max = 0.0;
          size_t ret_c = 0, ret_s = 0;
          double watched = 0.0, rate_sum = 0.0;
          for (int k = 0; k < n_trial; k++)
            {
              trial_t t;
              if (run_trial (code, ev, cn0s[ci], 100u + (uint32_t)k, &t))
                return 1;
              if (!t.settled)
                continue;
              settled++;
              ret_c += t.code_returns;
              rate_sum += t.rate_after;
              ret_s += t.sym_returns;
              watched += t.watch_s;
              if (t.t_code_s < 0.0)
                held_c++;
              else
                tc[nc++] = t.t_code_s;
              if (t.t_sym_s < 0.0)
                held_s++;
              else
                ts[ns++] = t.t_sym_s;
              back_c += t.code_back;
              back_s += t.sym_back;
              if (t.t_both_s >= 0.0)
                tb[nb++] = t.t_both_s;
              if (t.both_max_s > both_run_max)
                both_run_max = t.both_max_s;
            }
          for (int i = 1; i < nb; i++)
            for (int j = i; j > 0 && tb[j - 1] > tb[j]; j--)
              {
                double x  = tb[j];
                tb[j]     = tb[j - 1];
                tb[j - 1] = x;
              }
          /* medians by insertion sort; n <= 64 */
          for (int i = 1; i < nc; i++)
            for (int j = i; j > 0 && tc[j - 1] > tc[j]; j--)
              {
                double x  = tc[j];
                tc[j]     = tc[j - 1];
                tc[j - 1] = x;
              }
          for (int i = 1; i < ns; i++)
            for (int j = i; j > 0 && ts[j - 1] > ts[j]; j--)
              {
                double x  = ts[j];
                ts[j]     = ts[j - 1];
                ts[j - 1] = x;
              }
          char mc[16], xc[16], ms[16], xs[16], mb[16], xb[16];
          fmt_t (nb ? tb[nb / 2] : -1.0, mb, sizeof mb);
          fmt_t (both_run_max, xb, sizeof xb);
          fmt_t (nc ? tc[nc / 2] : -1.0, mc, sizeof mc);
          fmt_t (nc ? tc[nc - 1] : -1.0, xc, sizeof xc);
          fmt_t (ns ? ts[ns / 2] : -1.0, ms, sizeof ms);
          fmt_t (ns ? ts[ns - 1] : -1.0, xs, sizeof xs);
          printf ("  %-8s %2d/%2d          %9s %9s  %2d/%2d   %9s %9s  "
                  "%2d/%2d   %16s   %22s   %2d/%2d       %2d/%2d\n",
                  ev_name[ev], settled, n_trial, mc, xc, held_c, settled, ms,
                  xs, held_s, settled, mb, xb, back_c, settled, back_s,
                  settled);
          if (ev == EV_CROSS || ev == EV_CROSS_ON)
            printf ("           the neighbour crossing at +%.1f s, %.2f "
                    "chips/s relative: code lock returned %zu time(s) over "
                    "%.0f s of watch, symbol lock %zu; chip rate over the "
                    "last 2 s, mean of the trials %+.2f chips/s\n",
                    CROSS_AFTER_S,
                    (ev == EV_CROSS_ON ? CROSS_ON_PPM : CROSS_PPM) * 1e-6
                        * CHIP_RATE,
                    ret_c, watched, ret_s, rate_sum / (settled ? settled : 1));
          if (ev == EV_OFF && watched > 0.0)
            printf (
                "           over %.0f s of noise after the switch-off: code "
                "lock returned %zu time(s) (%.3f per s), symbol lock %zu "
                "(%.3f per s)\n",
                watched, ret_c, (double)ret_c / watched, ret_s,
                (double)ret_s / watched);
        }
      /* The on-time: false drops. */
      double pc = 0.0, ps = 0.0, pb = 0.0, brun = 0.0;
      size_t dc = 0, ds = 0;
      int    settled = 0;
      for (int k = 0; k < n_on; k++)
        {
          trial_t t;
          if (run_trial (code, EV_ON, cn0s[ci], 500u + (uint32_t)k, &t))
            return 1;
          if (!t.settled)
            continue;
          settled++;
          pc += t.p_code_off;
          ps += t.p_sym_off;
          pb += t.p_both_off;
          if (t.both_max_s > brun)
            brun = t.both_max_s;
          dc += t.code_drops;
          ds += t.sym_drops;
        }
      if (settled)
        printf ("  on       %2d/%2d watched %.1f s each: code lock off "
                "%.2e of blocks, %zu drop episode(s); symbol lock off %.2e "
                "of blocks, %zu episode(s); both off %.2e of blocks, longest "
                "both-off run %.1f ms\n\n",
                settled, n_on, ON_S, pc / settled, dc, ps / settled, ds,
                pb / settled, brun * 1e3);
    }
  return 0;
}
