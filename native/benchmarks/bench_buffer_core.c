/* bench_buffer_core.c -- what the double mapping is worth.
 *
 * `DECLARE_DP_BUFFER` builds a ring whose backing pages are mapped TWICE,
 * back to back, so the region after the last sample is the region before
 * the first one. The payoff is stated in the header and never measured:
 * a batch that straddles the wrap is still contiguous, so `write` is one
 * `memcpy` regardless of where the head sits, and `wait` hands back a
 * pointer a SIMD loop can run straight off without a copy or a split.
 *
 * The ordinary ring cannot do either. It splits a straddling batch into
 * two memcpys, and a consumer wanting one contiguous run has to copy into
 * scratch. So the claim being made here is not "this is fast" -- it is
 * "the position of the head does not matter", and that is a claim a
 * benchmark can actually settle.
 *
 * The test is two chunk sizes against one power-of-two capacity:
 *
 *   chunk 1024 divides the capacity, so every batch starts at the same
 *   handful of offsets and NO batch ever straddles the wrap.
 *   chunk 1000 does not, so the start offset walks and most batches do.
 *
 * Same bytes moved, same arithmetic, same number of calls per sample. If
 * the two rows agree, the mapping is doing what it says. A gap would mean
 * straddling costs something after all -- a page boundary, a TLB entry,
 * or a memcpy that is not taking the fast path it looks like it takes.
 *
 * Both element widths are here because the buffer is a macro instantiated
 * per type, and f64 moves twice the bytes for the same sample count: it
 * is the row that says whether the cost is per sample or per byte.
 *
 * NOT measured here, and worth saying so: contention. This is one thread
 * alternating producer and consumer, which is the sequencing the acquire /
 * release pairs are written for but not the case they exist for. A real
 * two-thread throughput number needs a different harness and belongs with
 * whoever adds one.
 */
#include "doppler/buffer/buffer.h"
#include "doppler/dp_thread.h"
#include "dp_bench.h"
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 200
#define CAPACITY 8192 /* complex samples; power of two, fits in L2 */
#define TOTAL 262144  /* samples pushed through per timed round    */

#define N_CHUNK 2
/* 1024 divides CAPACITY (never straddles); 1000 does not (usually does). */
static const size_t chunks[N_CHUNK] = { 1024, 1000 };
#define REF_IDX 0

enum
{
  KIND_F32,
  KIND_F64,
  N_KIND
};

static const char *kind_name[N_KIND] = { "f32", "f64" };

#define N_CFG (N_CHUNK * N_KIND)

/* The streaming row: chunks that neither divide the frame nor fit a whole
   number of times into the ring go IN through write_some(), and fixed frames
   come OUT through peek() -- the loop every single-threaded consumer of this
   ring runs (acq, detector, detector2d, burst_capture). It is read against
   write_wait_consume[f32,chunk=1024]: same samples, same per-sample read.

   Measured 2026-09-20 (Zen 5, pinned, -O3 x86-64-v2), and it is two effects:
     1.03x  with STREAM_CHUNK == STREAM_FRAME -- the loop itself: the drain
            ends on one peek() that returns NULL per chunk, ~10 ns against a
            ~340 ns frame. That is the price of not knowing the chunk size.
     1.08x  at STREAM_CHUNK 3000 -- the other 5% is cache geometry, not the
            API: 24 KB is written before any of it is read back, where the
            reference row reads each 8 KB while it is still hot.
   So the non-blocking surface is not free, it is ~3% at this frame size,
   and the ratio printed below is expected to read ~1.08, not 1.00. */
#define STREAM_CHUNK 3000
#define STREAM_FRAME 1024

/* Read the batch so the contiguity of the returned pointer is load-bearing
   rather than decorative. The sum is returned to keep it alive. */
#define DRAIN(name, type, buf, chunk, acc)                                    \
  do                                                                          \
    {                                                                         \
      const type *_p = dp_##name##_wait ((buf), (chunk));                     \
      /* wait() may now return NULL -- end of stream, or interrupted. This    \
         bench closes neither, so it cannot happen here; the guard is what    \
         keeps that a fact rather than an assumption. */                      \
      if (!_p)                                                                \
        break;                                                                \
      for (size_t _k = 0; _k < (chunk) * 2; _k++)                             \
        (acc) += (double)_p[_k];                                              \
      dp_##name##_consume ((buf), (chunk));                                   \
    }                                                                         \
  while (0)

/* ── two threads ──────────────────────────────────────────────────────────
   Every row above is ONE thread alternating roles, which is the sequencing
   the acquire/release pairs are written for and not the case they exist
   for. This is that case: a producer on its own thread against a blocking
   wait(), at a SMALL frame -- so the cost is the ring's calls and the
   cross-core traffic on its two indices, not memcpy. It is also where a
   bounded consume() would show if it cost anything: the bound reads the
   producer's index on the consumer's release path. It does not, because
   the wait() just before it has already loaded that line. */
#define TWO_FRAME 64
#define TWO_TOTAL (8u * 1024u * 1024u) /* samples per timed round */
#define TWO_ROUNDS 20

DP_THREAD_FN (two_producer, p)
{
  dp_f32_t    *ring = (dp_f32_t *)p;
  static float block[2 * TWO_FRAME];
  size_t       sent = 0;
  while (sent < TWO_TOTAL)
    {
      if (dp_f32_space (ring) < TWO_FRAME)
        continue; /* write() never blocks: backpressure is ours */
      dp_f32_write (ring, block, TWO_FRAME);
      sent += TWO_FRAME;
    }
  dp_f32_close (ring);
  DP_THREAD_RETURN;
}

int
main (void)
{
  jm_bench_t    _bench = { 0 };
  uint64_t      t0, t1;
  static double t[N_CFG][ITERATIONS];
  static double t_stream[ITERATIONS];       /* write_some + peek, f32   */
  static double t_i16[N_CHUNK][ITERATIONS]; /* write/wait/consume, i16 */
  dp_f32_t     *b32   = dp_f32_create (CAPACITY);
  dp_f64_t     *b64   = dp_f64_create (CAPACITY);
  dp_i16_t     *b16   = dp_i16_create (CAPACITY);
  float        *srcs  = NULL; /* STREAM_CHUNK samples, for the stream row */
  int16_t      *src16 = NULL;
  float        *src32 = NULL;
  double       *src64 = NULL;
  double        acc   = 0.0;
  char          name[72];

  if (!b32 || !b64 || !b16)
    {
      fprintf (stderr, "buffer create failed\n");
      return 1;
    }

  src32 = malloc ((size_t)2 * chunks[0] * sizeof *src32);
  src64 = malloc ((size_t)2 * chunks[0] * sizeof *src64);
  srcs  = malloc ((size_t)2 * STREAM_CHUNK * sizeof *srcs);
  src16 = malloc ((size_t)2 * chunks[0] * sizeof *src16);
  if (!src32 || !src64 || !srcs || !src16)
    return 1;
  for (size_t i = 0; i < 2 * STREAM_CHUNK; i++)
    srcs[i] = (float)(i & 0xff);
  for (size_t i = 0; i < 2 * chunks[0]; i++)
    src16[i] = (int16_t)(i & 0xff);
  for (size_t i = 0; i < 2 * chunks[0]; i++)
    {
      src32[i] = (float)(i & 0xff);
      src64[i] = (double)(i & 0xff);
    }

  printf ("=== buffer (VM double-mapped ring, capacity %d samples) ===\n",
          CAPACITY);
  printf ("%d samples per round, %d rounds, min over rounds\n\n", TOTAL,
          ITERATIONS);

  DP_BENCH_SETTLE ({
    (void)dp_f32_write (b32, src32, chunks[0]);
    DRAIN (f32, float, b32, chunks[0], acc);
  });

  /* Rounds outside, configurations inside. The whole result is the
     straddling row read against the aligned one, so a thermal step must
     not land on one of them alone. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CHUNK; c++)
      {
        const size_t chunk = chunks[c];

        t0 = jm_bench_now_ns ();
        for (size_t done = 0; done < TOTAL; done += chunk)
          {
            (void)dp_f32_write (b32, src32, chunk);
            DRAIN (f32, float, b32, chunk, acc);
          }
        t1                          = jm_bench_now_ns ();
        t[c * N_KIND + KIND_F32][r] = jm_bench_elapsed_sec (t0, t1);

        t0 = jm_bench_now_ns ();
        for (size_t done = 0; done < TOTAL; done += chunk)
          {
            (void)dp_f64_write (b64, src64, chunk);
            DRAIN (f64, double, b64, chunk, acc);
          }
        t1                          = jm_bench_now_ns ();
        t[c * N_KIND + KIND_F64][r] = jm_bench_elapsed_sec (t0, t1);

        t0 = jm_bench_now_ns ();
        for (size_t done = 0; done < TOTAL; done += chunk)
          {
            (void)dp_i16_write (b16, src16, chunk);
            DRAIN (i16, int16_t, b16, chunk, acc);
          }
        t1          = jm_bench_now_ns ();
        t_i16[c][r] = jm_bench_elapsed_sec (t0, t1);

        if (c == REF_IDX)
          {
            /* Interleaved with its reference row, in the same round. */
            size_t fed = 0;
            dp_f32_reset (b32);
            t0 = jm_bench_now_ns ();
            while (fed < TOTAL)
              {
                size_t off = 0;
                while (off < STREAM_CHUNK)
                  {
                    const float *f;
                    off += dp_f32_write_some (b32, srcs + 2 * off,
                                              STREAM_CHUNK - off);
                    while ((f = dp_f32_peek (b32, STREAM_FRAME)) != NULL)
                      {
                        for (size_t k = 0; k < (size_t)STREAM_FRAME * 2; k++)
                          acc += (double)f[k];
                        dp_f32_consume (b32, STREAM_FRAME);
                      }
                  }
                fed += STREAM_CHUNK;
              }
            t1          = jm_bench_now_ns ();
            t_stream[r] = jm_bench_elapsed_sec (t0, t1);
            dp_f32_reset (b32); /* leave no remainder for the next row */
          }
      }

  for (int c = 0; c < N_CHUNK; c++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "write_wait_consume[%s,chunk=%zu]",
                        kind_name[k], chunks[c]);
        dp_bench_record (&_bench, name, t[c * N_KIND + k], ITERATIONS, TOTAL,
                         "sample");
      }

  for (int c = 0; c < N_CHUNK; c++)
    {
      (void)snprintf (name, sizeof name, "write_wait_consume[i16,chunk=%zu]",
                      chunks[c]);
      dp_bench_record (&_bench, name, t_i16[c], ITERATIONS, TOTAL, "sample");
    }
  (void)snprintf (name, sizeof name,
                  "write_some_peek_consume[f32,in=%d,frame=%d]", STREAM_CHUNK,
                  STREAM_FRAME);
  dp_bench_record (&_bench, name, t_stream, ITERATIONS,
                   (TOTAL + STREAM_CHUNK - 1) / STREAM_CHUNK * STREAM_CHUNK,
                   "sample");

  printf (
      "\n  the streaming loop (write_some + peek), against write + wait:\n");
  printf (
      "    f32  in=%d frame=%d over chunk=%zu   %.2fx per sample\n",
      STREAM_CHUNK, STREAM_FRAME, chunks[REF_IDX],
      (dp_bench_min (t_stream, ITERATIONS)
       / (double)((TOTAL + STREAM_CHUNK - 1) / STREAM_CHUNK * STREAM_CHUNK))
          / (dp_bench_min (t[REF_IDX * N_KIND + KIND_F32], ITERATIONS)
             / (double)TOTAL));

  printf ("\n  straddling the wrap, against never straddling it:\n");
  for (int k = 0; k < N_KIND; k++)
    printf ("    %-4s chunk=%zu over chunk=%zu   %.2fx\n", kind_name[k],
            chunks[1], chunks[REF_IDX],
            dp_bench_min (t[1 * N_KIND + k], ITERATIONS)
                / dp_bench_min (t[REF_IDX * N_KIND + k], ITERATIONS));
  printf ("  1.00x is the double mapping doing its job: the head's\n"
          "  position does not change what a batch costs, which is the\n"
          "  entire reason the pages are mapped twice. Anything else is\n"
          "  a copy or a split that the contiguity was supposed to remove.\n");

  {
    static double t_two[TWO_ROUNDS];
    size_t        got = 0;
    for (int r = 0; r < TWO_ROUNDS; r++)
      {
        dp_f32_t   *ring = dp_f32_create (65536);
        dp_thread_t th;
        float      *v;
        if (!ring || dp_thread_create (&th, two_producer, ring) != 0)
          return 1;
        t0 = jm_bench_now_ns ();
        while ((v = dp_f32_wait (ring, TWO_FRAME)) != NULL)
          {
            acc += v[0];
            dp_f32_consume (ring, TWO_FRAME);
            got += TWO_FRAME;
          }
        t1       = jm_bench_now_ns ();
        t_two[r] = jm_bench_elapsed_sec (t0, t1);
        dp_thread_join (th);
        dp_f32_destroy (ring);
      }
    if (got != (size_t)TWO_ROUNDS * TWO_TOTAL)
      {
        fprintf (stderr, "two-thread: %zu samples, expected %zu\n", got,
                 (size_t)TWO_ROUNDS * TWO_TOTAL);
        return 1; /* a throughput for a stream that lost samples is a lie */
      }
    (void)snprintf (name, sizeof name,
                    "two_thread_write_wait_consume[f32,frame=%d]", TWO_FRAME);
    dp_bench_record (&_bench, name, t_two, TWO_ROUNDS, TWO_TOTAL, "sample");
    printf ("\n  two threads, frame=%d:   %.1f ns/frame   %.0f MSa/s\n",
            TWO_FRAME,
            1e9 * dp_bench_min (t_two, TWO_ROUNDS)
                / (double)(TWO_TOTAL / TWO_FRAME),
            (double)TWO_TOTAL / dp_bench_min (t_two, TWO_ROUNDS) / 1e6);
  }

  printf ("\n  (checksum %.0f -- keeps the drain from being optimised out)\n",
          acc);

  free (src32);
  free (src64);
  free (srcs);
  free (src16);
  dp_f32_destroy (b32);
  dp_f64_destroy (b64);
  dp_i16_destroy (b16);
  jm_bench_write_json (&_bench, "buffer");
  return 0;
}
