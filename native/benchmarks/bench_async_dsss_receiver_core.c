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
 * cost the parallel design multiplies (docs/design/async-dsss-parallel.md:
 * every receiver searches the whole range), and the row is recorded only if
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
 * Not timed here: refinement, a one-time transient rather than a rate.
 */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
      code, sf, chip, sym, spc, 2, cn0, 1e-3, 0.9, span, 4, 8, 0, 0.5, 4, 14.0,
      64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      code, w.sf, w.chip_rate, w.sym_rate, w.spc, 2, cn0, 1e-3, 0.9, span, 4,
      8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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

  jm_bench_write_json (&_bench, "async_dsss_receiver");
  return 0;
}
