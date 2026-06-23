/*
 * bench_stream.c — P0 transport benchmark: throughput + one-way latency.
 *
 * Measures PUSH/PULL performance at DSP-relevant block sizes over ipc://
 * (same-host, lowest ZMQ overhead).  PUSH/PULL has backpressure and delivers
 * every frame exactly once, making it the right baseline for pipeline work.
 *
 * One-way latency uses dp_header_t.timestamp_ns (CLOCK_REALTIME on sender)
 * vs CLOCK_REALTIME on receiver — valid because both threads share the same
 * clock.  For cross-host measurements use a REQ/REP echo and halve the RTT.
 *
 * Usage:
 *   bench_stream [block_size] [num_blocks]
 *
 *   block_size   CF32 samples per frame (default 4096 = 32 KB)
 *   num_blocks   frames to send        (default 1000)
 *
 * Output (TSV, one row per run):
 *   block_sz  num_blocks  tput_mss  tput_mbs
 *             lat_min_us  lat_mean_us  lat_p99_us  lat_max_us
 *
 * Build:
 *   cmake --build build --target bench_stream
 *   ./build/native/benchmarks/bench_stream 8192 2000
 *
 * P0 sweep:
 *   for blk in 256 1024 4096 16384 65536; do
 *     bench_stream $blk 600
 *   done
 */

#define _POSIX_C_SOURCE 200809L
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stream/stream.h"

/* ─── constants ─────────────────────────────────────────────────────────── */
#define ENDPOINT "ipc:///tmp/dp_bench_stream.ipc"
#define WARMUP_BLKS 50  /* frames before timing starts */
#define HIST_BINS 32768 /* 1 µs buckets → covers 0–32767 µs (32 ms) */

/* ─── shared state ──────────────────────────────────────────────────────── */
typedef struct
{
  size_t block_sz;
  size_t num_blks;

  uint64_t hist[HIST_BINS];
  uint64_t lat_overflow;
  size_t   timed_blks;

  uint64_t wall_ns_start;
  uint64_t wall_ns_end;

  pthread_barrier_t ready;
} bench_state_t;

/* ─── helpers ───────────────────────────────────────────────────────────── */
static uint64_t
now_ns (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ─── producer ──────────────────────────────────────────────────────────── */
static void *
producer (void *arg)
{
  bench_state_t  *s   = arg;
  size_t          n   = s->block_sz;
  float _Complex *buf = malloc (n * sizeof (float _Complex));

  for (size_t i = 0; i < n; i++)
    buf[i] = cosf (2.0f * (float)M_PI * (float)i / (float)n)
             + sinf (2.0f * (float)M_PI * (float)i / (float)n)
                   * (float _Complex)_Complex_I;

  dp_push_t *ctx = dp_push_create (ENDPOINT, CF32);
  if (!ctx)
    {
      fputs ("bench_stream: push create failed\n", stderr);
      free (buf);
      pthread_barrier_wait (&s->ready);
      return NULL;
    }

  pthread_barrier_wait (&s->ready);

  for (size_t i = 0; i < s->num_blks; i++)
    {
      int rc = dp_push_send_cf32 (ctx, buf, n, 1e6, 2.4e9);
      if (rc != DP_OK)
        {
          fprintf (stderr, "bench_stream: send error at block %zu: %s\n", i,
                   dp_strerror (rc));
          break;
        }
    }

  dp_push_destroy (ctx);
  free (buf);
  return NULL;
}

/* ─── consumer ──────────────────────────────────────────────────────────── */
static void *
consumer (void *arg)
{
  bench_state_t *s   = arg;
  dp_pull_t     *ctx = dp_pull_create (ENDPOINT);
  if (!ctx)
    {
      fputs ("bench_stream: pull create failed\n", stderr);
      pthread_barrier_wait (&s->ready);
      return NULL;
    }

  pthread_barrier_wait (&s->ready);

  size_t timed = 0;
  for (size_t i = 0; i < s->num_blks; i++)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;
      int         rc = dp_pull_recv (ctx, &msg, &hdr);
      if (rc != DP_OK)
        {
          fprintf (stderr, "bench_stream: recv error at block %zu: %s\n", i,
                   dp_strerror (rc));
          dp_msg_free (msg);
          break;
        }

      uint64_t recv_ns = now_ns ();

      if (i == WARMUP_BLKS)
        s->wall_ns_start = recv_ns;

      if (i >= WARMUP_BLKS)
        {
          int64_t lat_ns = (int64_t)(recv_ns - hdr.timestamp_ns);
          if (lat_ns < 0)
            lat_ns = 0;
          uint64_t lat_us = (uint64_t)lat_ns / 1000ULL;
          if (lat_us < HIST_BINS)
            s->hist[lat_us]++;
          else
            s->lat_overflow++;
          timed++;
        }

      s->wall_ns_end = recv_ns;
      dp_msg_free (msg);
    }

  s->timed_blks = timed;
  dp_pull_destroy (ctx);
  return NULL;
}

/* ─── histogram stats ───────────────────────────────────────────────────── */
static void
hist_stats (const bench_state_t *s, double *min_us, double *mean_us,
            double *p99_us, double *max_us)
{
  size_t   total      = s->timed_blks;
  uint64_t sum        = 0;
  uint64_t p99_thresh = (uint64_t)(total * 99ULL / 100ULL);
  uint64_t cum        = 0;

  *min_us = -1;
  *max_us = 0;
  *p99_us = 0;

  for (int b = 0; b < HIST_BINS; b++)
    {
      if (!s->hist[b])
        continue;
      if (*min_us < 0)
        *min_us = (double)b;
      *max_us = (double)b;
      sum += (uint64_t)b * s->hist[b];
      cum += s->hist[b];
      if (*p99_us == 0 && cum >= p99_thresh)
        *p99_us = (double)b;
    }

  if (s->lat_overflow)
    *max_us = (double)HIST_BINS;

  *mean_us = total > 0 ? (double)sum / (double)total : 0.0;
  if (*min_us < 0)
    *min_us = 0;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int
main (int argc, char *argv[])
{
  size_t block_sz = 4096;
  size_t num_blks = 1000;

  if (argc > 1
      && (strcmp (argv[1], "-h") == 0 || strcmp (argv[1], "--help") == 0))
    {
      printf ("Usage: bench_stream [block_size] [num_blocks]\n"
              "\n"
              "  block_size   CF32 samples/frame (default 4096 = 32 KB)\n"
              "  num_blocks   frames to send     (default 1000)\n"
              "\n"
              "P0 sweep:\n"
              "  for blk in 256 1024 4096 16384 65536; do\n"
              "    bench_stream $blk 600\n"
              "  done\n");
      return 0;
    }

  if (argc > 1)
    block_sz = (size_t)strtoul (argv[1], NULL, 10);
  if (argc > 2)
    num_blks = (size_t)strtoul (argv[2], NULL, 10);

  if (block_sz == 0 || num_blks <= WARMUP_BLKS)
    {
      fprintf (stderr,
               "bench_stream: block_size > 0 and num_blocks > %d required\n",
               WARMUP_BLKS);
      return 1;
    }

  bench_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return 1;
  s->block_sz = block_sz;
  s->num_blks = num_blks;
  pthread_barrier_init (&s->ready, NULL, 2);

  printf ("block_sz\tnum_blocks\ttput_mss\ttput_mbs"
          "\tlat_min_us\tlat_mean_us\tlat_p99_us\tlat_max_us\n");

  pthread_t prod_tid, cons_tid;
  pthread_create (&cons_tid, NULL, consumer, s);
  pthread_create (&prod_tid, NULL, producer, s);
  pthread_join (prod_tid, NULL);
  pthread_join (cons_tid, NULL);

  pthread_barrier_destroy (&s->ready);

  if (s->timed_blks == 0)
    {
      fputs ("bench_stream: no timed frames received\n", stderr);
      free (s);
      return 1;
    }

  double wall_s        = (double)(s->wall_ns_end - s->wall_ns_start) / 1e9;
  double total_samples = (double)s->block_sz * (double)s->timed_blks;
  double tput_mss      = total_samples / wall_s / 1e6;
  double tput_mbs
      = total_samples * sizeof (float _Complex) / wall_s / (1024.0 * 1024.0);

  double min_us, mean_us, p99_us, max_us;
  hist_stats (s, &min_us, &mean_us, &p99_us, &max_us);

  printf ("%zu\t%zu\t%.2f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n", s->block_sz,
          s->timed_blks, tput_mss, tput_mbs, min_us, mean_us, p99_us, max_us);

  if (s->lat_overflow)
    fprintf (stderr, "bench_stream: %llu frames exceeded %d µs\n",
             (unsigned long long)s->lat_overflow, HIST_BINS);

  free (s);
  return 0;
}
