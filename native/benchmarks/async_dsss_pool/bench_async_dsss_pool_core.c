/* bench_async_dsss_pool_core.c -- what a pool of searching receivers costs
 * per input sample, at the one operating point we pin.
 *
 * The point: a 1,023-chip Gold code (the CCSDS default, wfm.Gold()) at
 * 5 Mcps, captured at 11 MSa/s. The receivers take whole samples per chip,
 * so the capture goes through one RateConverter to 10 MSa/s (2 samples a
 * chip) first, and that front end is inside every timed push: it is what a
 * caller at 11 MSa/s pays. The search is the design's: block-coherent over
 * the depth the Doppler rate allows: 3 ksym/s with a 500-symbol data-free
 * block (code_only_s) and a 500 Hz/s Doppler rate (doppler_rate), D = 150
 * code periods at 5 Mcps (the rate's 154 rounded to a 7-smooth length,
 * docs/design/async-dsss-receiver.md §9), so a tile resolves D Doppler rows
 * in slow time. Two spans:
 *
 * - native: one slow-time tile, +/- 1 / (2 T_E) = +/- 2,444 Hz;
 * - full:   +/- 50 kHz, the design's whole carrier range.
 *
 * One timed push is one dwell, D code periods at the receivers' rate, so
 * every push decides exactly one dwell and the rows compare like for like.
 *
 * The rows ask two questions. n=1 against n=4 on one thread: does a pool of
 * N idle receivers cost one search or N? Every searching receiver builds
 * the same surface on the same block grid (the ranks depend on it,
 * docs/design/async-dsss-receiver.md §5), so anything above one search is
 * repeated work. And n=4 on 4 threads against one: what do the workers buy
 * back, given push() joins them once every code period?
 *
 * Fed noise alone from the shipped awgn, so every receiver searches all
 * along; a row is recorded only if every receiver searched for every sample
 * fed (a false alarm's refine would be timed as search). The rows are
 * interleaved round by round, so their ratios are measured on one machine
 * state.
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/gold/gold_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CODE_LEN 1023
#define LEAD 2
#define ITERATIONS 10
#define CODE_ONLY_S (500.0 / 3000.0) /* 500 data-free symbols */
#define DOPPLER_RATE 500.0           /* Hz/s, the design's bound */
#define N_ROWS 5

typedef struct
{
  const char *name;
  size_t      n_rx;
  double      span_hz; /* the receivers' doppler_uncertainty, +/- */
  int         threads;
} row_t;

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* Every receiver searching for every sample fed, or a message. The
   receivers have been fed the whole periods pushed: the pool carries the
   rest to the next push. */
static int
all_searching (dp_async_dsss_pool_state_t *p, size_t n_rx, uint64_t pushed,
               const char *tag)
{
  const uint64_t fed = pushed - p->carry_len;
  for (size_t i = 0; i < n_rx; i++)
    {
      async_dsss_receiver_status_t st = dp_async_dsss_pool_status (p, i);
      if (st.state != ASYNC_DSSS_RX_SEARCHING || st.state_samples != fed
          || st.refused_hits != 0)
        {
          (void)fprintf (stderr,
                         "bench_async_dsss_pool[%s]: receiver %zu state=%d "
                         "state_samples=%llu of %llu -- not a search all "
                         "along\n",
                         tag, i, st.state,
                         (unsigned long long)st.state_samples,
                         (unsigned long long)fed);
          return 0;
        }
    }
  return 1;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };

  const double chip = 5e6, fs_in = 11e6;
  const size_t spc  = 2;
  const double fs   = chip * (double)spc; /* 10 MSa/s at the receivers */
  const double sym  = 3000.0;             /* the link's symbol rate */
  const double tile = chip / CODE_LEN;    /* 1 / T_E */
  const double cn0  = 10.0 + 10.0 * log10 (sym);

  const row_t rows[N_ROWS] = {
    { "push[native,n=1,threads=1]", 1, tile / 2.0, 1 },
    { "push[native,n=4,threads=1]", 4, tile / 2.0, 1 },
    { "push[native,n=4,threads=4]", 4, tile / 2.0, 4 },
    { "push[full,n=1,threads=1]", 1, 50e3, 1 },
    { "push[full,n=4,threads=1]", 4, 50e3, 1 },
  };

  /* The CCSDS default Gold code, as wfm.Gold() makes it. */
  uint8_t          code[CODE_LEN];
  dp_gold_state_t *gold = dp_gold_create (934, 350, 567, 73, 10);
  if (!gold || dp_gold_generate (gold, CODE_LEN, code, CODE_LEN) != CODE_LEN)
    {
      (void)fprintf (stderr, "bench_async_dsss_pool: gold failed\n");
      return 1;
    }
  dp_gold_destroy (gold);

  dp_async_dsss_pool_state_t *p[N_ROWS];
  dp_RateConverter_state_t   *rc[N_ROWS];
  uint64_t                    pushed[N_ROWS] = { 0 };
  for (int k = 0; k < N_ROWS; k++)
    {
      p[k] = dp_async_dsss_pool_create (
          code, CODE_LEN, rows[k].n_rx, chip, sym, spc, 2, cn0, 1e-3, 0.9,
          rows[k].span_hz, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0,
          2.0, false, CODE_ONLY_S, DOPPLER_RATE, rows[k].threads);
      rc[k] = dp_RateConverter_create (fs / fs_in, 0);
      if (!p[k] || !rc[k])
        {
          (void)fprintf (stderr, "bench_async_dsss_pool: %s setup failed\n",
                         rows[k].name);
          return 1;
        }
    }
  /* One dwell, D code periods at the receivers' rate, as input samples.
     The depth is the receivers' and the same at either span. */
  const size_t depth = dp_async_dsss_pool_get_coherent_bins (p[0]);
  for (int k = 1; k < N_ROWS; k++)
    if (dp_async_dsss_pool_get_coherent_bins (p[k]) != depth)
      {
        (void)fprintf (stderr,
                       "bench_async_dsss_pool: %s depth %zu, not "
                       "%zu\n",
                       rows[k].name,
                       dp_async_dsss_pool_get_coherent_bins (p[k]), depth);
        return 1;
      }
  const size_t block_in
      = (size_t)llround ((double)(depth * CODE_LEN * spc) * fs_in / fs);
  const size_t    y_cap = (size_t)ceil ((double)block_in * fs / fs_in) + 64;
  float _Complex *y[N_ROWS];
  for (int k = 0; k < N_ROWS; k++)
    if (!(y[k] = malloc (y_cap * sizeof *y[k])))
      return 1;

  const size_t     n_in = (LEAD + ITERATIONS) * block_in;
  dp_awgn_state_t *g    = dp_awgn_create (
      7919u,
      dp_awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs_in)), 1.0f));
  float _Complex *x = malloc (n_in * sizeof *x);
  if (!g || !x)
    {
      (void)fprintf (stderr, "bench_async_dsss_pool: setup failed\n");
      return 1;
    }
  (void)dp_awgn_generate (g, n_in, x, n_in);

  printf ("=== async_dsss_pool: 1,023-chip Gold at 5 Mcps, 11 MSa/s in "
          "(-> 10 MSa/s), noise, searching ===\n");
  printf ("depth D = %zu code periods; one push = one dwell = %zu input "
          "samples; %d rounds\n\n",
          depth, block_in, ITERATIONS);

  double times[N_ROWS][ITERATIONS];
  size_t pos = 0;
  for (int r = -LEAD; r < ITERATIONS; r++, pos += block_in)
    for (int k = 0; k < N_ROWS; k++)
      {
        uint64_t     t0 = jm_bench_now_ns ();
        const size_t ny
            = dp_RateConverter_execute (rc[k], x + pos, block_in, y[k], y_cap);
        (void)dp_async_dsss_pool_push (p[k], y[k], ny);
        uint64_t t1 = jm_bench_now_ns ();
        pushed[k] += ny;
        if (r >= 0)
          times[k][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int k = 0; k < N_ROWS; k++)
    {
      if (!all_searching (p[k], rows[k].n_rx, pushed[k], rows[k].name))
        return 1;
      jm_bench_add_unit (&_bench, rows[k].name, times[k], ITERATIONS, block_in,
                         JM_BENCH_SAMPLE);
      const double sec = min_sec (times[k], ITERATIONS);
      printf ("  %-28s %8.3f ms/dwell  %7.2f ns/sample  %6.2fx real time\n",
              rows[k].name, sec * 1e3, sec / (double)block_in * 1e9,
              (double)block_in / sec / fs_in);
    }

  jm_bench_write_json (&_bench, "async_dsss_pool");
  for (int k = 0; k < N_ROWS; k++)
    {
      dp_async_dsss_pool_destroy (p[k]);
      dp_RateConverter_destroy (rc[k]);
      free (y[k]);
    }
  dp_awgn_destroy (g);
  free (x);
  return 0;
}
