/*
 * bench_stream.c — P0 transport benchmark: throughput + one-way latency.
 *
 * Measures PUSH/PULL performance at DSP-relevant block sizes over TCP
 * loopback.  PUSH/PULL has backpressure and delivers every frame exactly
 * once, making it the right baseline for pipeline work.
 *
 * TCP loopback is the conservative baseline: it exercises the kernel TCP
 * stack (no shared-memory shortcut) and matches what a real network hop
 * looks like at line rate on a fast LAN.
 *
 * One-way latency uses dp_header_t.timestamp_ns (CLOCK_REALTIME on sender)
 * vs CLOCK_REALTIME on receiver — valid because both threads share the same
 * clock.  For cross-host measurements the clocks must be PTP/NTP synced.
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
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stream/stream.h"

/* ─── constants ─────────────────────────────────────────────────────────── */
/* Endpoints default to the NATS JetStream work-queue tier on a local broker
 * (start it with `nats-server -js`). Override via the environment to bench
 * against a remote broker:
 *   DP_BENCH_FIREHOSE_EP=nats://broker:4222/firehose \
 *   DP_BENCH_REQREP_EP=nats://broker:4222/reqrep   bench_stream 4096 600 */
#define FIREHOSE_EP_DEFAULT "nats://127.0.0.1:4222/firehose"
#define REQREP_EP_DEFAULT "nats://127.0.0.1:4222/reqrep"
#define STATUS_MSG_BYTES 64 /* representative small status/control payload */
#define WARMUP_BLKS 50      /* frames before timing starts */
#define HIST_BINS 32768     /* 1 µs buckets → covers 0–32767 µs (32 ms) */
#define RECV_TIMEOUT_MS                                                       \
  2000 /* bounds a blocking recv so a producer send-                          \
        * error (e.g. a frame over the NATS max_payload)                      \
        * can't hang the bench forever */

static const char *
firehose_endpoint (void)
{
  const char *e = getenv ("DP_BENCH_FIREHOSE_EP");
  return (e && *e) ? e : FIREHOSE_EP_DEFAULT;
}

static const char *
reqrep_endpoint (void)
{
  const char *e = getenv ("DP_BENCH_REQREP_EP");
  return (e && *e) ? e : REQREP_EP_DEFAULT;
}

/* ─── portable 2-party barrier via a pair of semaphores ─────────────────── */
typedef struct
{
  sem_t a; /* producer posts when ready; consumer waits */
  sem_t b; /* consumer posts when ready; producer waits */
} bench_barrier_t;

static void
barrier_init (bench_barrier_t *br)
{
  sem_init (&br->a, 0, 0);
  sem_init (&br->b, 0, 0);
}

static void
barrier_wait (bench_barrier_t *br, int is_producer)
{
  if (is_producer)
    {
      sem_post (&br->a);
      sem_wait (&br->b);
    }
  else
    {
      sem_post (&br->b);
      sem_wait (&br->a);
    }
}

static void
barrier_destroy (bench_barrier_t *br)
{
  sem_destroy (&br->a);
  sem_destroy (&br->b);
}

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

  bench_barrier_t ready;

  /* Set once the producer has stopped sending (all frames out, or a send
   * error). The consumer reads it to know that a recv timeout means "no more
   * frames are coming" rather than "still in flight", so it can stop instead
   * of blocking forever. */
  volatile int producer_done;
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

  dp_push_t *ctx = dp_push_create (firehose_endpoint (), CF32);
  if (!ctx)
    {
      fputs ("bench_stream: push create failed\n", stderr);
      free (buf);
      s->producer_done = 1; /* let the consumer stop waiting */
      barrier_wait (&s->ready, 1);
      return NULL;
    }

  barrier_wait (&s->ready, 1);

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
  s->producer_done = 1; /* signal the consumer: no more frames are coming */
  return NULL;
}

/* ─── consumer ──────────────────────────────────────────────────────────── */
static void *
consumer (void *arg)
{
  bench_state_t *s = arg;
  /* The NATS JetStream pull consumer attaches to the work-queue stream that
   * the producer's push_create provisions, so the stream may not exist yet
   * when this thread starts (the two create concurrently). Retry until it
   * lands. */
  dp_pull_t *ctx = NULL;
  for (int attempt = 0; attempt < 100 && !ctx; attempt++)
    {
      ctx = dp_pull_create (firehose_endpoint ());
      if (!ctx)
        {
          struct timespec ts = { 0, 20L * 1000 * 1000 }; /* 20 ms */
          nanosleep (&ts, NULL);
        }
    }
  if (!ctx)
    {
      fputs ("bench_stream: pull create failed\n", stderr);
      barrier_wait (&s->ready, 0);
      return NULL;
    }

  barrier_wait (&s->ready, 0);

  /* Bound each recv so the loop can periodically re-check producer_done. */
  dp_pull_set_timeout (ctx, RECV_TIMEOUT_MS);

  size_t timed = 0;
  size_t got   = 0;
  while (got < s->num_blks)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;
      int         rc = dp_pull_recv (ctx, &msg, &hdr);
      if (rc == DP_ERR_TIMEOUT)
        {
          dp_msg_free (msg); /* NULL on timeout; free(NULL) is safe */
          /* No frame this window. If the producer has finished or errored,
           * the remaining frames will never arrive — stop instead of hanging.
           * Otherwise it is still sending, so keep waiting. */
          if (s->producer_done)
            break;
          continue;
        }
      if (rc != DP_OK)
        {
          fprintf (stderr, "bench_stream: recv error at block %zu: %s\n", got,
                   dp_strerror (rc));
          dp_msg_free (msg);
          break;
        }

      uint64_t recv_ns = now_ns ();

      if (got == WARMUP_BLKS)
        s->wall_ns_start = recv_ns;

      if (got >= WARMUP_BLKS)
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
      /* Acks the frame so the durable JetStream consumer keeps delivering
       * (MaxAckPending would otherwise stall the stream after the first
       * window of unacked frames). */
      dp_msg_ack (msg);
      dp_msg_free (msg);
      got++;
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

/* ─── status-plane REQ/REP: unloaded small-message RTT ──────────────────────
 *
 * The status/control/telemetry plane carries small messages at a low rate, so
 * its figure of merit is *unloaded* latency, not throughput. REQ/REP is
 * lock-step (one request outstanding at a time), so a ping-pong of a
 * STATUS_MSG_BYTES payload measures the genuine request→reply round trip with
 * no queueing — the "query current state" latency. One-way ≈ RTT / 2. */
static void *
replier (void *arg)
{
  bench_state_t *s   = arg;
  dp_rep_t      *rep = dp_rep_create (reqrep_endpoint ());
  if (!rep)
    {
      fputs ("bench_stream: rep create failed\n", stderr);
      barrier_wait (&s->ready, 0);
      return NULL;
    }
  barrier_wait (&s->ready, 0);

  unsigned char echo[STATUS_MSG_BYTES] = { 0 };
  for (size_t i = 0; i < s->num_blks; i++)
    {
      dp_msg_t *msg = NULL;
      size_t    sz  = 0;
      if (dp_rep_recv (rep, &msg, &sz) != DP_OK)
        {
          dp_msg_free (msg);
          break;
        }
      dp_msg_free (msg);
      if (dp_rep_send (rep, echo, STATUS_MSG_BYTES) != DP_OK)
        break;
    }
  dp_rep_destroy (rep);
  return NULL;
}

static int
run_reqrep (bench_state_t *s)
{
  pthread_t rep_tid;
  pthread_create (&rep_tid, NULL, replier, s);

  dp_req_t *req = dp_req_create (reqrep_endpoint ());
  if (!req)
    {
      fputs ("bench_stream: req create failed\n", stderr);
      barrier_wait (&s->ready, 1);
      pthread_join (rep_tid, NULL);
      return 1;
    }
  barrier_wait (&s->ready, 1);

  /* Bound the reply wait so a dead/absent replier ends the loop instead of
   * blocking forever (recv returns DP_ERR_TIMEOUT, which breaks below). */
  dp_req_set_timeout (req, RECV_TIMEOUT_MS);

  /* Core NATS REQ/REP delivers only to a subscription already registered on
   * the server. The barrier guarantees the replier's dp_rep_create returned,
   * but the SubscribeSync registration still has to propagate before the
   * first request, so let it settle. */
  struct timespec settle_ts = { 0, 200L * 1000 * 1000 }; /* 200 ms */
  nanosleep (&settle_ts, NULL);

  unsigned char payload[STATUS_MSG_BYTES] = { 0 };
  size_t        timed                     = 0;
  for (size_t i = 0; i < s->num_blks; i++)
    {
      uint64_t t0 = now_ns ();
      if (dp_req_send (req, payload, STATUS_MSG_BYTES) != DP_OK)
        break;
      dp_msg_t *msg = NULL;
      size_t    sz  = 0;
      if (dp_req_recv (req, &msg, &sz) != DP_OK)
        {
          dp_msg_free (msg);
          break;
        }
      uint64_t t1 = now_ns ();
      dp_msg_free (msg);

      if (i >= WARMUP_BLKS)
        {
          uint64_t rtt_us = (t1 - t0) / 1000ULL;
          if (rtt_us < HIST_BINS)
            s->hist[rtt_us]++;
          else
            s->lat_overflow++;
          timed++;
        }
    }
  s->timed_blks = timed;
  dp_req_destroy (req);
  pthread_join (rep_tid, NULL);

  if (timed == 0)
    {
      fputs ("bench_stream: no timed round trips\n", stderr);
      return 1;
    }

  double min_us, mean_us, p99_us, max_us;
  hist_stats (s, &min_us, &mean_us, &p99_us, &max_us);
  printf ("msg_bytes\tpings\trtt_min_us\trtt_mean_us\trtt_p99_us"
          "\trtt_max_us\toneway_mean_us\n");
  printf ("%d\t%zu\t%.1f\t%.1f\t%.1f\t%.1f\t%.2f\n", STATUS_MSG_BYTES, timed,
          min_us, mean_us, p99_us, max_us, mean_us / 2.0);
  return 0;
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
      printf (
          "Usage: bench_stream [block_size] [num_blocks]\n"
          "       bench_stream reqrep [num_pings]\n"
          "\n"
          "  block_size   CF32 samples/frame (default 4096 = 32 KB)\n"
          "  num_blocks   frames to send     (default 1000)\n"
          "  reqrep       status-plane mode: unloaded REQ/REP round-trip\n"
          "               latency for a small message (default 10000 pings)\n"
          "\n"
          "Firehose throughput sweep (PUSH/PULL):\n"
          "  for blk in 256 1024 4096 16384 65536; do\n"
          "    bench_stream $blk 600\n"
          "  done\n"
          "\n"
          "Status-plane latency:\n"
          "  bench_stream reqrep\n");
      return 0;
    }

  /* Status-plane mode: `bench_stream reqrep [num_pings]` — unloaded small-
   * message round-trip latency (no throughput; that is the firehose's job). */
  if (argc > 1 && strcmp (argv[1], "reqrep") == 0)
    {
      bench_state_t *rs = calloc (1, sizeof (*rs));
      if (!rs)
        return 1;
      rs->num_blks = (argc > 2) ? (size_t)strtoul (argv[2], NULL, 10) : 10000;
      if (rs->num_blks <= WARMUP_BLKS)
        rs->num_blks = WARMUP_BLKS + 1;
      barrier_init (&rs->ready);
      int rc = run_reqrep (rs);
      barrier_destroy (&rs->ready);
      free (rs);
      return rc;
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
  barrier_init (&s->ready);

  printf ("block_sz\tnum_blocks\ttput_mss\ttput_mbs"
          "\tlat_min_us\tlat_mean_us\tlat_p99_us\tlat_max_us\n");

  pthread_t prod_tid, cons_tid;
  pthread_create (&cons_tid, NULL, consumer, s);
  pthread_create (&prod_tid, NULL, producer, s);
  pthread_join (prod_tid, NULL);
  pthread_join (cons_tid, NULL);

  barrier_destroy (&s->ready);

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
