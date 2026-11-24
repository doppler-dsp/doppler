/* bench_async_dsss_receiver_core.c — the receiver, per sample, in the two
 * states it spends its life in.
 *
 * The stimulus is an async DSSS signal: the code runs continuously on the
 * sample clock and the BPSK data changes at its own symbol rate, so a
 * symbol is not a whole number of code periods. Each timed round feeds the
 * NEXT contiguous block.
 *
 * SEARCHING, while it searches: the specification
 * (docs/design/async-dsss-receiver.md) configuration the certification
 * measures, with its own search over
 * +/-1/T_E, fed noise alone from the shipped awgn. That is the per-sample
 * cost a pool of searching receivers multiplied
 * (docs/design/async-dsss-receiver.md §9), and the row is recorded only if
 * the receiver searched for every sample fed -- a false alarm's refine, even
 * one refused back to search, would be timed as the search.
 *
 * And TRACKING, the mode a receiver spends most of
 * its time in while signals are present: the same specification configuration,
 * seeded at chip 0 on the specification capture at its design Es/N0 (10 dB,
 * the shipped awgn), run through its refine and pull-in until it tracks with
 * code AND symbol lock, then timed. This is the full closed-loop chain: the
 * Dll's own code loop closed every period, beside the carrier loop, the
 * resampler and the MpskReceiver's carrier and timing loops and matched
 * filter. It records only if the receiver stayed in tracking for every timed
 * sample with both locks up at either end.
 *
 * And CREATE, one receiver per call, on a five-tile search: what each new
 * receiver costs before its first sample, the Doppler-offset sidelobe sweep
 * included (run_create()).
 *
 * And SEARCH and TRACK again at the CCSDS 415.1-B-1 specification point,
 * 11 MSa/s in, at 2.2 and 2.8 GHz (run_spec_search(), run_spec_track()).
 *
 * Not timed here: refinement, a one-time transient rather than a rate.
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_air_test.h"
#include "dp_s415_test.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Samples per timed round, the rounds, and the untimed blocks the search
   row feeds first. */
#define BLOCK 65536
#define MIN_LEAD_BLOCKS 4
#define ITERATIONS 20

typedef struct
{
  const char    *tag; /* row suffix */
  const uint8_t *code;
  size_t         sf;
  size_t         spc;
  double         chip_rate;
  double         sym_rate;
} wf_t;

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

static uint32_t
xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

/* The capture: the code on the global sample index (chip 0 at sample 0,
   where the tracking row seeds), times a BPSK bit that changes every symbol.
   Built here rather than pulled from native/tests's dp_dsss_capture(): a
   benchmark that includes a test header acquires a dependency on the test
   tree's build wiring. */
static float _Complex *
build_capture (const wf_t *w, size_t n)
{
  const double    tsym = w->chip_rate * (double)w->spc / w->sym_rate;
  float _Complex *x    = malloc (n * sizeof *x);
  if (!x)
    return NULL;
  uint32_t lfsr = 0xA57Bu;
  size_t   sym  = (size_t)-1;
  float    a    = 1.0f;
  for (size_t k = 0; k < n; k++)
    {
      size_t si = (size_t)((double)k / tsym);
      if (si != sym)
        {
          lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          a    = (lfsr & 1u) ? -1.0f : 1.0f;
          sym  = si;
        }
      x[k] = a * (w->code[(k / w->spc) % w->sf] ? -1.0f : 1.0f);
    }
  return x;
}

/* Searching for EVERY sample fed so far, or a message naming what broke.
   The state alone is not enough: a false alarm that refines and is then
   refused at the hand-over comes back to SEARCHING within the run, and its
   refine would be timed as search. Entering a state zeroes state_samples,
   so `state_samples == fed` says it never left; a refused hit (one that
   never refines) still counts in refused_hits, which must not move. */
static int
searching (const dp_async_dsss_receiver_state_t *rx, uint64_t fed,
           uint64_t refused0, const char *when)
{
  async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  if (st.state == ASYNC_DSSS_RX_SEARCHING && st.state_samples == fed
      && st.refused_hits == refused0)
    return 1;
  (void)fprintf (stderr,
                 "bench_async_dsss_receiver[search]: %s, state=%d, "
                 "state_samples=%llu of %llu fed, refused_hits=%llu (was "
                 "%llu) -- a timing would measure a receiver that did not "
                 "search all along\n",
                 when, st.state, (unsigned long long)st.state_samples,
                 (unsigned long long)fed, (unsigned long long)st.refused_hits,
                 (unsigned long long)refused0);
  return 0;
}

/* The searching flavor at the specification point, while it searches: the
   specification configuration, a 1,023-chip code at 3.069 Mcps and 2 samples a
   chip, 2,700 sym/s, its own search over +/-1/T_E and designed for Es/N0 10
   dB, the rest at the binding's defaults. The input is the shipped awgn at
   that C/N0's noise level for a unit-power emitter, and no emitter.

   The configuration MIRRORS make_rx() in
   native/validation/async_dsss_receiver_lock.c, the receiver the
   certification measures; change one, change the other. It is not shared
   because a bench takes no dependency on the test tree's headers (see
   build_capture()), and a new dp_*_test.h member carries its own
   meta-test (native/tests/README.md). A false alarm at pfa 1e-3 over the
   run's ~770 epochs of search is possible; searching() refuses the row
   if one ever hands off, rather than timing it. */
static int
run_searching (jm_bench_t *bench, const uint8_t *code)
{
  const double chip = 3.069e6, sym = 2700.0;
  const size_t sf = 1023, spc = 2;
  const double fs   = chip * (double)spc;
  const double span = fs / (double)(sf * spc); /* 1/T_E */
  const double cn0  = 10.0 + 10.0 * log10 (sym);
  const size_t lead = MIN_LEAD_BLOCKS;
  const size_t n    = (lead + ITERATIONS) * (size_t)BLOCK;

  dp_awgn_state_t *g = dp_awgn_create (
      7919u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs)), 1.0f));
  float _Complex                 *x  = malloc (n * sizeof *x);
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, sf, chip, sym, chip * (double)spc, 2, cn0, 1e-3, 0.9, span, 4, 8,
      0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0, true);
  float _Complex *out = malloc ((size_t)BLOCK * sizeof *out);
  if (!g || !x || !rx || !out)
    {
      (void)fprintf (stderr, "bench_async_dsss_receiver[search]: the "
                             "receiver or its noise did not open\n");
      return 1;
    }
  (void)dp_awgn_generate (g, n, x, n);

  const uint64_t  refused0 = dp_async_dsss_receiver_status (rx).refused_hits;
  volatile size_t sink     = 0;
  size_t          pos      = 0;
  for (size_t b = 0; b < lead; b++, pos += BLOCK)
    sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, BLOCK);
  if (!searching (rx, pos, refused0, "after the lead-in"))
    return 1;

  double times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++, pos += BLOCK)
    {
      uint64_t t0 = jm_bench_now_ns ();
      sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, BLOCK);
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
    }
  if (!searching (rx, pos, refused0, "after the timed rounds"))
    return 1;

  const char *name = "steps[search,sf=1023,3.069Mcps]";
  jm_bench_add_unit (bench, name, times, ITERATIONS, BLOCK, JM_BENCH_SAMPLE);
  double sec = min_sec (times, ITERATIONS);
  printf ("  %-32s %8.3f ms/block  %7.2f ns/sample  %6.2fx real time on "
          "one core\n",
          name, sec * 1e3, sec / (double)BLOCK * 1e9,
          (double)BLOCK / sec / fs);

  (void)sink;
  dp_async_dsss_receiver_destroy (rx);
  dp_awgn_destroy (g);
  free (x);
  free (out);
  return 0;
}

/* CREATE, one receiver per call (#2752 step 3.4): the searching flavor's
   specification configuration with a five-tile search, +/-5 kHz, and a
   4 ms code-only window (CODE_ONLY_S), the receivers' Gold-1023 point where
   create sweeps the code's Doppler-offset sidelobe (dp_correlator_dsl_db(),
   read at create so the sweep never stalls a push). Step 3.4 moves that sweep
   onto the one delay-Doppler map, so rule 2 judges create as well as push. The
   row records only if the search is the one named: five window tiles and a
   deep block, without which create skips the sweep. Destroy is untimed. */
#define CODE_ONLY_S 0.004 /* a code-only window of 11 epochs: a deep block */

static int
run_create (jm_bench_t *bench, const uint8_t *code)
{
  const double chip = 3.069e6, sym = 2700.0;
  const size_t sf = 1023, spc = 2;
  const double cn0 = 10.0 + 10.0 * log10 (sym);
  double       times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      uint64_t                        t0 = jm_bench_now_ns ();
      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
          code, sf, chip, sym, chip * (double)spc, 2, cn0, 1e-3, 0.9, 5000.0,
          4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false,
          CODE_ONLY_S, 0.0, true);
      uint64_t t1 = jm_bench_now_ns ();
      if (!rx || rx->acq->search->window_bins != 5
          || !(rx->acq->search->coherent_bins > 1))
        {
          (void)fprintf (stderr,
                         "bench_async_dsss_receiver[create]: the receiver did "
                         "not open on a five-tile deep search (window_bins "
                         "%zu, coherent_bins %zu) -- the row would time a "
                         "create that skips the sidelobe sweep\n",
                         rx ? rx->acq->search->window_bins : 0,
                         rx ? rx->acq->search->coherent_bins : 0);
          return 1;
        }
      times[r] = jm_bench_elapsed_sec (t0, t1);
      dp_async_dsss_receiver_destroy (rx);
    }
  const char *name = "create[search,sf=1023,3.069Mcps,W=5]";
  jm_bench_add_unit (bench, name, times, ITERATIONS, 1, JM_BENCH_CALL);
  printf ("  %-32s %8.3f ms/create\n", name,
          min_sec (times, ITERATIONS) * 1e3);
  return 0;
}

/* Tracking with both locks, or a message naming what was lost. */
static int
tracking (const dp_async_dsss_receiver_state_t *rx, const char *when)
{
  async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  int cl = dp_async_dsss_receiver_get_code_locked (rx);
  int sl = dp_async_dsss_receiver_get_locked (rx);
  if (st.state == ASYNC_DSSS_RX_TRACKING && cl == 1 && sl == 1)
    return 1;
  (void)fprintf (stderr,
                 "bench_async_dsss_receiver[track]: %s, state=%d "
                 "code_locked=%d locked=%d -- a timing would measure a "
                 "receiver that is not tracking with both locks\n",
                 when, st.state, cl, sl);
  return 0;
}

/* Tracking with the code lock up, or a message naming what was lost: the
   requirement of the specification rows after the timed rounds. The code
   flag is the presence flag and is sized through the detection module. The
   symbol flag is NOT required here: on main its rule is the literal 0.5 up
   after 30 symbols, 0.3 down after 15, with no rates behind it (#3253), and
   at the certified 4 dB Es/N0 its metric lives at 0.3 to 0.55, straddling
   both thresholds, so it drops in some timed windows and not in others on
   any head. The row prints it instead, with the fraction of timed blocks it
   was up. When the
   sized symbol lock lands (#3253), these rows require both flags again and
   this function goes. */
static int
tracking_code_locked (const dp_async_dsss_receiver_state_t *rx,
                      const char                           *when)
{
  async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  int cl = dp_async_dsss_receiver_get_code_locked (rx);
  if (st.state == ASYNC_DSSS_RX_TRACKING && cl == 1)
    return 1;
  (void)fprintf (stderr,
                 "bench_async_dsss_receiver[spec track]: %s, state=%d "
                 "code_locked=%d -- a timing would measure a receiver "
                 "that is not tracking with the code lock\n",
                 when, st.state, cl);
  return 0;
}

/* The searching flavor's closed-loop tracking at the specification point: the
   specification configuration run_searching() times (and make_rx() in
   native/validation/async_dsss_receiver_lock.c measures), seeded at chip 0
   on the specification capture, fed until it tracks with both locks -- through
   the refine and the pull-in, bounded at MAX_LOCK_S -- then TRACK_SETTLE
   blocks more, then timed. Recorded only if it was tracking for every timed
   sample (state_samples grew by exactly the samples timed: no release, no
   re-entry) with both locks up after. */
#define MAX_LOCK_S 4.0
#define TRACK_SETTLE 2u
static int
run_tracking (jm_bench_t *bench, const uint8_t *code)
{
  const wf_t       w = { "sf=1023,3.069Mcps", code, 1023, 2, 3.069e6, 2700.0 };
  const double     fs   = w.chip_rate * (double)w.spc;
  const double     span = fs / (double)(w.sf * w.spc); /* 1/T_E */
  const double     cn0  = 10.0 + 10.0 * log10 (w.sym_rate);
  const size_t     maxb = (size_t)(MAX_LOCK_S * fs) / BLOCK + 1;
  const size_t     n    = (maxb + TRACK_SETTLE + ITERATIONS) * (size_t)BLOCK;
  float _Complex  *x    = build_capture (&w, n);
  float _Complex  *nz   = malloc (n * sizeof *nz);
  dp_awgn_state_t *g    = dp_awgn_create (
      7919u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs)), 1.0f));
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, w.sf, w.chip_rate, w.sym_rate, w.chip_rate * (double)w.spc, 2, cn0,
      1e-3, 0.9, span, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0,
      false, 0.0, 0.0, true);
  float _Complex *out = malloc ((size_t)BLOCK * sizeof *out);
  if (!x || !nz || !g || !rx || !out
      || dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) != DP_OK)
    {
      (void)fprintf (stderr, "bench_async_dsss_receiver[track]: the "
                             "receiver did not open or seed\n");
      return 1;
    }
  /* At the design Es/N0 (10 dB), the shipped awgn, as the certification
     measures it: the searching flavor's code-lock detector is sized for its
     design C/N0 and does not declare on a noiseless capture. */
  (void)dp_awgn_generate (g, n, nz, n);
  for (size_t k = 0; k < n; k++)
    x[k] += nz[k];
  free (nz);
  dp_awgn_destroy (g);

  volatile size_t sink = 0;
  size_t          pos = 0, b = 0;
  for (; b < maxb; b++, pos += BLOCK)
    {
      sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, BLOCK);
      if (dp_async_dsss_receiver_get_tracking (rx)
          && dp_async_dsss_receiver_get_code_locked (rx)
          && dp_async_dsss_receiver_get_locked (rx))
        {
          pos += BLOCK; /* the block just fed: break skips the increment */
          break;
        }
    }
  if (b == maxb)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[track]: no lock within "
                     "MAX_LOCK_S (%.0f s) -- both code and symbol lock are "
                     "needed before timing\n",
                     MAX_LOCK_S);
      return 1;
    }
  for (size_t k = 0; k < TRACK_SETTLE; k++, pos += BLOCK)
    sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, BLOCK);
  if (!tracking (rx, "after the lead-in"))
    return 1;
  const uint64_t s0 = dp_async_dsss_receiver_status (rx).state_samples;

  double times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++, pos += BLOCK)
    {
      uint64_t t0 = jm_bench_now_ns ();
      sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, BLOCK);
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
    }
  if (!tracking (rx, "after the timed rounds"))
    return 1;
  const uint64_t s1 = dp_async_dsss_receiver_status (rx).state_samples;
  if (s1 - s0 != (uint64_t)ITERATIONS * BLOCK)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[track]: tracking for %llu of "
                     "the %llu samples timed -- it left tracking in between\n",
                     (unsigned long long)(s1 - s0),
                     (unsigned long long)ITERATIONS * BLOCK);
      return 1;
    }

  const char *name = "steps[track,sf=1023,3.069Mcps]";
  jm_bench_add_unit (bench, name, times, ITERATIONS, BLOCK, JM_BENCH_SAMPLE);
  double sec = min_sec (times, ITERATIONS);
  printf ("  %-32s %8.3f ms/block  %7.2f ns/sample  %6.2fx real time on "
          "one core\n",
          name, sec * 1e3, sec / (double)BLOCK * 1e9,
          (double)BLOCK / sec / fs);

  (void)sink;
  dp_async_dsss_receiver_destroy (rx);
  free (x);
  free (out);
  return 0;
}

/* ── At the 415.1-B-1 specification point ─────────────────────────────────
 * The receiver as docs/design/async-dsss-receiver.md §1 specifies it: a
 * 1,023-chip code at fc x 31/21216 chips a second (the CCSDS 415.1-B-1
 * forward-link coherence ratio), 3,000 sym/s, fed 11 MSa/s directly (its
 * search resamples to the 2,048-sample epoch; refine and tracking run at
 * 11 MSa/s), +/-1,500 Hz, the residual's rate R_env as the rate bound
 * (dp_s415_r_env(), 11 Hz/s; v0.85.0 and earlier measured a 400 Hz/s
 * bound, which searched shallower), a 275-symbol code-only
 * window and the carrier set, so the code rate is aided from the carrier.
 * The design C/N0 is 4 dB Es/N0 (38.77 dB-Hz), the shipped awgn. Two
 * carriers, the band's ends: 2.2 GHz (3.215 Mcps) and 2.8 GHz (4.091 Mcps).
 *
 * The emitter is the CCSDS default Gold code through the on-air pool rows'
 * chain (spec_src_init()): the library's continuous DSSS synth, a
 * RateConverter to 11 MSa/s, then a DopplerChannel at the carrier at
 * -1,000 Hz, so its clocks dilate with the carrier as the
 * receiver's aiding assumes: the on-air pool rows' chain. It is made a block
 * at a time, outside the timed call. The emitter holds its Doppler: the
 * tracking cost does not depend on a ramp (9.0 to 9.9 ns a sample with and
 * without +400 Hz/s, measured when these rows were written), and a row must
 * lock to be timed at all.
 *
 * SEARCH is fed noise alone, one dwell (D code periods of input) a round, so
 * every round carries one decide pass: a round shorter than a dwell would
 * let the fastest-round statistic skip it. TRACK lets the receiver search,
 * hand over, refine and pull in on its own (no seed), bounded at
 * SPEC_MAX_LOCK_S, then times 65,536-sample rounds with both locks up and
 * no state change, as run_tracking(). */
#define SPEC_MAX_LOCK_S 3.0
#define SPEC_ITERS 10

typedef struct
{
  const char *tag; /* "2.2GHz" */
  double      carrier_hz;
} spec_pt_t;

/* The spec-point stream at 11 MSa/s, a block at a time: the use-case
   scene (native/tests/dp_air_test.h -- the library's continuous DSSS synth
   with PRBS data and a 275-symbol code-only window opening every
   1,000-symbol frame, the front end's converter to 11 MSa/s, a
   DopplerChannel 1 kHz off, the shipped AWGN); `amp` 0 is noise alone. */
typedef struct
{
  dp_air_t air;
} spec_src_t;

static void
spec_src_free (spec_src_t *s)
{
  dp_air_free (&s->air);
}

static int
spec_src_init (spec_src_t *s, const uint8_t *code, double fc, double cn0,
               double fs_in, float amp, size_t block)
{
  dp_air_emitter_t e = dp_s415_emitter (
      code, fc, -1000.0, 0.0,
      cn0 + (amp > 0.0f ? 20.0 * log10 ((double)amp) : 0.0), 0.0, 100u);
  return dp_air_init (&s->air, &e, amp > 0.0f ? 1 : 0, fs_in, cn0, 7919u,
                      block);
}

static int
spec_src_next (spec_src_t *s, float _Complex *x, size_t n)
{
  return dp_air_next (&s->air, x, n);
}

/* On ONE thread: the receiver's speed goal is one thread, and a search
   that fans its block across a worker pool would otherwise turn these
   rows into wall-time rows on a many-core box. */
static dp_async_dsss_receiver_state_t *
spec_rx (const uint8_t *code, double fc)
{
  dp_async_dsss_receiver_state_t *rx = dp_s415_rx (code, fc, true);
  if (rx)
    (void)dp_async_dsss_receiver_set_threads (rx, 1);
  return rx;
}

static void
spec_print (const char *name, double sec, size_t n, double fs_in)
{
  printf ("  %-32s %8.3f ms/round  %7.2f ns/sample  %6.2fx real time on "
          "one core\n",
          name, sec * 1e3, sec / (double)n * 1e9, (double)n / sec / fs_in);
}

static int
run_spec_search (jm_bench_t *bench, const uint8_t *code, const spec_pt_t *pt)
{
  const double fs_in = DP_S415_FS, chip = dp_s415_chip_rate (pt->carrier_hz);
  const double cn0                   = dp_s415_cn0 (DP_S415_FLOOR_DB);
  dp_async_dsss_receiver_state_t *rx = spec_rx (code, pt->carrier_hz);
  if (!rx)
    return 1;
  const size_t    D     = dp_async_dsss_receiver_get_coherent_bins (rx);
  const size_t    block = (size_t)llround ((double)D * 1023.0 / chip * fs_in);
  float _Complex *x     = malloc (block * sizeof *x);
  float _Complex *out   = malloc (block * sizeof *out);
  spec_src_t      src;
  if (!x || !out
      || !spec_src_init (&src, code, pt->carrier_hz, cn0, fs_in, 0.0f, block))
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[spec search %s]: "
                     "setup failed\n",
                     pt->tag);
      return 1;
    }
  const uint64_t  refused0 = dp_async_dsss_receiver_status (rx).refused_hits;
  volatile size_t sink     = 0;
  uint64_t        pos      = 0;
  for (int b = 0; b < 2; b++, pos += block)
    {
      (void)spec_src_next (&src, x, block);
      sink += dp_async_dsss_receiver_steps (rx, x, block, out, block);
    }
  double times[SPEC_ITERS];
  for (int r = 0; r < SPEC_ITERS; r++, pos += block)
    {
      (void)spec_src_next (&src, x, block);
      uint64_t t0 = jm_bench_now_ns ();
      sink += dp_async_dsss_receiver_steps (rx, x, block, out, block);
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
    }
  if (!searching (rx, pos, refused0, "after the timed rounds"))
    return 1;
  char name[64];
  (void)snprintf (name, sizeof name, "steps[search,415.1,%s]", pt->tag);
  jm_bench_add_unit (bench, name, times, SPEC_ITERS, block, JM_BENCH_SAMPLE);
  spec_print (name, min_sec (times, SPEC_ITERS), block, fs_in);
  printf ("    D = %zu, dwell %.1f ms = %zu input samples\n", D,
          (double)block / fs_in * 1e3, block);
  (void)sink;
  dp_async_dsss_receiver_destroy (rx);
  spec_src_free (&src);
  free (x);
  free (out);
  return 0;
}

static int
run_spec_track (jm_bench_t *bench, const uint8_t *code, const spec_pt_t *pt)
{
  const double fs_in = DP_S415_FS;
  const double cn0   = dp_s415_cn0 (DP_S415_FLOOR_DB);
  const size_t maxb  = (size_t)(SPEC_MAX_LOCK_S * fs_in) / BLOCK;
  dp_async_dsss_receiver_state_t *rx  = spec_rx (code, pt->carrier_hz);
  float _Complex                 *x   = malloc ((size_t)BLOCK * sizeof *x);
  float _Complex                 *out = malloc ((size_t)BLOCK * sizeof *out);
  spec_src_t                      src;
  if (!rx || !x || !out
      || !spec_src_init (&src, code, pt->carrier_hz, cn0, fs_in, 1.0f, BLOCK))
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[spec track %s]: "
                     "setup failed\n",
                     pt->tag);
      return 1;
    }
  volatile size_t sink = 0;
  size_t          b    = 0;
  for (; b < maxb; b++)
    {
      (void)spec_src_next (&src, x, BLOCK);
      sink += dp_async_dsss_receiver_steps (rx, x, BLOCK, out, BLOCK);
      if (dp_async_dsss_receiver_get_tracking (rx)
          && dp_async_dsss_receiver_get_code_locked (rx)
          && dp_async_dsss_receiver_get_locked (rx))
        break;
    }
  if (b == maxb)
    {
      async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[spec track %s]: no lock "
                     "within %.0f s (state=%d code_locked=%d locked=%d, "
                     "%.0f Hz)\n",
                     pt->tag, SPEC_MAX_LOCK_S, st.state,
                     dp_async_dsss_receiver_get_code_locked (rx),
                     dp_async_dsss_receiver_get_locked (rx),
                     dp_async_dsss_receiver_get_doppler_hz (rx));
      return 1;
    }
  for (size_t k = 0; k < TRACK_SETTLE; k++)
    {
      (void)spec_src_next (&src, x, BLOCK);
      sink += dp_async_dsss_receiver_steps (rx, x, BLOCK, out, BLOCK);
    }
  if (!tracking (rx, "after the lead-in"))
    return 1;
  const uint64_t s0 = dp_async_dsss_receiver_status (rx).state_samples;
  double         times[ITERATIONS];
  int            symbol_up_blocks = 0;
  for (int r = 0; r < ITERATIONS; r++)
    {
      (void)spec_src_next (&src, x, BLOCK);
      uint64_t t0 = jm_bench_now_ns ();
      sink += dp_async_dsss_receiver_steps (rx, x, BLOCK, out, BLOCK);
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
      symbol_up_blocks += dp_async_dsss_receiver_get_locked (rx) == 1;
    }
  if (!tracking_code_locked (rx, "after the timed rounds"))
    return 1;
  if (dp_async_dsss_receiver_status (rx).state_samples - s0
      != (uint64_t)ITERATIONS * BLOCK)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver[spec track %s]: it "
                     "left tracking in between\n",
                     pt->tag);
      return 1;
    }
  char name[64];
  (void)snprintf (name, sizeof name, "steps[track,415.1,%s]", pt->tag);
  jm_bench_add_unit (bench, name, times, ITERATIONS, BLOCK, JM_BENCH_SAMPLE);
  spec_print (name, min_sec (times, ITERATIONS), BLOCK, fs_in);
  printf ("    locked after %.2f s, tracking at %.0f Hz\n",
          (double)(b + 1) * BLOCK / fs_in,
          dp_async_dsss_receiver_get_doppler_hz (rx));
  printf ("    symbol lock %s at the end, up in %d of %d timed blocks "
          "(printed, not required: #3253)\n",
          dp_async_dsss_receiver_get_locked (rx) == 1 ? "up" : "DOWN",
          symbol_up_blocks, ITERATIONS);
  (void)sink;
  dp_async_dsss_receiver_destroy (rx);
  spec_src_free (&src);
  free (x);
  free (out);
  return 0;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };

  /* A synthetic 1023-chip code: a real Gold code's exact chips do not
     matter for a cost benchmark (bench_acq_core.c makes the same choice),
     and using one keeps this executable off the gold component's link
     line. */
  static uint8_t code1023[1023];
  uint32_t       cseed = 7;
  for (size_t c = 0; c < 1023; c++)
    code1023[c] = (uint8_t)(xorshift32 (&cseed) & 1u);

  printf ("=== async_dsss_receiver benchmark: the tracking receiver and "
          "the search ===\n");
  printf ("block = %d samples, %d rounds\n\n", BLOCK, ITERATIONS);
  if (run_tracking (&_bench, code1023) != 0)
    return 1;
  if (run_searching (&_bench, code1023) != 0)
    return 1;
  if (run_create (&_bench, code1023) != 0)
    return 1;
  printf ("\n=== at the 415.1-B-1 specification point: 11 MSa/s in, "
          "3,000 sym/s, 4 dB Es/N0 ===\n");
  static const spec_pt_t pts[]
      = { { "2.2GHz", DP_S415_FC_LO }, { "2.8GHz", DP_S415_FC_HI } };
  /* The CCSDS default Gold code, as wfm.Gold() makes it. */
  static uint8_t   gold[1023];
  dp_gold_state_t *gs = dp_gold_create (934, 350, 567, 73, 10);
  if (!gs || dp_gold_generate (gs, 1023, gold, 1023) != 1023)
    return 1;
  dp_gold_destroy (gs);
  for (size_t i = 0; i < sizeof pts / sizeof *pts; i++)
    if (run_spec_search (&_bench, gold, &pts[i]) != 0
        || run_spec_track (&_bench, gold, &pts[i]) != 0)
      return 1;

  jm_bench_write_json (&_bench, "async_dsss_receiver");
  return 0;
}
