/* bench_async_dsss_receiver_core.c — the held cell receiver, per sample.
 *
 * The receiver a pool's searcher drives (the cell mode, design section
 * 12.22-12.24, #1283): seeded at the code's chip 0, no refine, the Dll
 * coasting and steered once an interval. What it costs per input sample
 * while it holds the code is what each assigned slot adds to a pool push
 * (bench_async_dsss_pool_core.c), against the 100 ns-per-sample budget of
 * design section 6.4.
 *
 * The stimulus is an async DSSS signal: the code runs continuously on the
 * sample clock and the BPSK data changes at its own symbol rate, so a
 * symbol is not a whole number of code periods. Each timed round feeds the
 * NEXT contiguous block. The receiver must be tracking AND code-locked
 * before the timed rounds and still after them, or nothing is recorded: a
 * receiver that lost the code would time a different path and read as this
 * one.
 *
 * Two waveforms, the two ends of the code-length axis: a 7-chip code
 * (28-sample period), where the per-period work weighs most per sample, and
 * the continuous async-DSSS operating point (section 6.1), a 1,023-chip
 * code at 5 Mcps and 2 samples per chip, the top of the 2-5 Mcps range.
 *
 * Not timed here (doppler#2386): acquisition and refinement, a one-time
 * transient rather than a rate, and the searching flavor's steady state,
 * which reads the same per sample as this one.
 */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Samples per timed round. Before the first one the receiver is fed a
   contiguous lead-in covering its pull-in (ASYNC_DSSS_RX_CELL_PULLIN
   steering intervals) and two intervals more, and never under
   MIN_LEAD_BLOCKS: at 5 Mcps the pull-in alone is about 19 blocks, and a
   round timed inside it would time the pull-in (see lead_blocks()). */
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
  size_t         periods; /* the cell's steering interval, code periods */
} wf_t;

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

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
   where the cell is seeded), times a BPSK bit that changes every symbol.
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

/* Whole blocks covering the pull-in and two steering intervals more. */
static size_t
lead_blocks (const wf_t *w)
{
  const size_t interval = w->periods * w->sf * w->spc; /* samples */
  const size_t need     = (ASYNC_DSSS_RX_CELL_PULLIN + 2u) * interval;
  const size_t blocks   = (need + BLOCK - 1) / BLOCK;
  return blocks < MIN_LEAD_BLOCKS ? MIN_LEAD_BLOCKS : blocks;
}

/* Tracking and holding the code, or a message naming what was lost. */
static int
held (const dp_async_dsss_receiver_state_t *rx, const wf_t *w,
      const char *when)
{
  int trk = dp_async_dsss_receiver_get_tracking (rx);
  int cl  = dp_async_dsss_receiver_get_code_locked (rx);
  if (trk == 1 && cl == 1)
    return 1;
  (void)fprintf (stderr,
                 "bench_async_dsss_receiver%s: %s, tracking=%d "
                 "code_locked=%d -- a timing would measure a receiver that "
                 "is not holding the code\n",
                 w->tag, when, trk, cl);
  return 0;
}

static int
run_waveform (jm_bench_t *bench, const wf_t *w)
{
  const size_t    lead = lead_blocks (w);
  const size_t    n    = (lead + ITERATIONS) * (size_t)BLOCK;
  float _Complex *x    = build_capture (w, n);
  if (!x)
    return 1;

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create_cell (
      w->code, w->sf, w->chip_rate, w->sym_rate, w->spc, 2, 70.0, 1e-2, 0.9, 4,
      8, 0, 0.0, 0.0, w->periods, ASYNC_DSSS_RX_CELL_GAIN,
      ASYNC_DSSS_RX_CELL_PULLIN);
  if (!rx || dp_async_dsss_receiver_seed (rx, 0.0, 0.0, 70.0) != DP_OK)
    {
      (void)fprintf (stderr,
                     "bench_async_dsss_receiver%s: the cell mode did not "
                     "open or seed\n",
                     w->tag);
      return 1;
    }
  size_t cap = dp_async_dsss_receiver_steps_max_out (rx);
  if (cap == 0 || cap > BLOCK)
    cap = BLOCK;
  float _Complex *out = malloc (cap * sizeof *out);
  if (!out)
    return 1;

  volatile size_t sink = 0;
  size_t          pos  = 0;
  for (size_t b = 0; b < lead; b++, pos += BLOCK)
    sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, cap);
  if (!held (rx, w, "after the lead-in"))
    return 1;

  double times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++, pos += BLOCK)
    {
      uint64_t t0 = jm_bench_now_ns ();
      sink += dp_async_dsss_receiver_steps (rx, x + pos, BLOCK, out, cap);
      uint64_t t1 = jm_bench_now_ns ();
      times[r]    = jm_bench_elapsed_sec (t0, t1);
    }
  if (!held (rx, w, "after the timed rounds"))
    return 1;

  char name[JM_BENCH_NAME_LEN];
  (void)snprintf (name, sizeof name, "steps[cell,%s]", w->tag);
  jm_bench_add_unit (bench, name, times, ITERATIONS, BLOCK, JM_BENCH_SAMPLE);
  double sec = min_sec (times, ITERATIONS);
  printf ("  %-28s %8.3f ms/block  %7.2f ns/sample  %6.2fx real time on "
          "one core\n",
          name, sec * 1e3, sec / (double)BLOCK * 1e9,
          (double)BLOCK / sec / (w->chip_rate * (double)w->spc));

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

  const wf_t waveforms[] = {
    { "sf=7", CODE7, 7, 4, 1.0e6, 35714.29, 100 },
    { "sf=1023,5Mcps", code1023, 1023, 2, 5.0e6, 2700.0, 154 },
  };
  printf ("=== async_dsss_receiver benchmark: the held cell ===\n");
  printf ("block = %d samples, %d rounds after a lead-in past the "
          "pull-in\n\n",
          BLOCK, ITERATIONS);
  for (size_t i = 0; i < sizeof waveforms / sizeof waveforms[0]; i++)
    if (run_waveform (&_bench, &waveforms[i]) != 0)
      return 1;

  jm_bench_write_json (&_bench, "async_dsss_receiver");
  return 0;
}
