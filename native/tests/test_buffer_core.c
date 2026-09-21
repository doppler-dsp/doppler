/*
 * C-level tests for the VM-mirrored ring buffer (buffer/buffer.h).
 *
 * The buffer is a header-only macro (DECLARE_DP_BUFFER) with no _core.c, and
 * the Python tests only run on Linux in normal CI — so this is the only gate
 * that exercises the buffer on macOS arm64 (16 KiB pages), where issue #66
 * lived. The focus is the page-aware sizing: a sub-page request must round up
 * to a whole-page, power-of-two capacity, and the double-mapping must still
 * wrap correctly afterwards.
 */
#include "buffer/buffer.h"
#include "dp_test.h"
#include "dp_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Advance head and tail to `target` while keeping occupancy near zero, using
 * a small fixed scratch so the prime works for any (possibly 16 KiB-page)
 * capacity without a large stack buffer. */
#define PRIME_TO(name, type, buf, target)                                     \
  do                                                                          \
    {                                                                         \
      type   _scratch[128] = { 0 }; /* 64 complex samples */                  \
      size_t _pos          = 0;                                               \
      while (_pos < (target))                                                 \
        {                                                                     \
          size_t _chunk = (target) - _pos;                                    \
          if (_chunk > 64)                                                    \
            _chunk = 64;                                                      \
          dp_##name##_write ((buf), _scratch, _chunk);                        \
          (void)dp_##name##_wait ((buf), _chunk);                             \
          dp_##name##_consume ((buf), _chunk);                                \
          _pos += _chunk;                                                     \
        }                                                                     \
    }                                                                         \
  while (0)

/* How many times the write-then-close race below is run. One attempt
   catches a missing re-load-after-acquire only ~5% of the time (measured:
   2 of 40 with it deleted), because the branch needs a window one spin
   iteration wide. Independent attempts compound: 0.95^200 ~= 3e-5. */
#define EOS_RACE_ATTEMPTS 200

/* Consumer half of the write-then-close race below. Spins in wait() until
   the producer supplies a batch or closes the ring. */
typedef struct
{
  dp_f32_t *buf;
  float    *got;
  float     first;
} eos_race_arg_t;

DP_THREAD_FN (eos_consumer, p)
{
  eos_race_arg_t *a = (eos_race_arg_t *)p;
  a->got            = dp_f32_wait (a->buf, 64);
  if (a->got)
    a->first = a->got[0];
  DP_THREAD_RETURN;
}

/* The two sides of the "never over-reports" stress below. Each sizes its
   call ONLY from its own side's count -- space() for the producer,
   available() for the consumer -- and records the first time the ring
   disagrees with what that count promised. */
#define SIDED_TOTAL 2000000u

typedef struct
{
  dp_f32_t *buf;
  size_t    refused; /* producer: write() said no to a space()-sized block */
  size_t    missing; /* consumer: peek() said no to available() samples    */
  size_t    wrong;   /* consumer: a sample out of order                     */
  volatile int stop; /* either side gave up: the other must not wait on it  */
} sided_arg_t;

/* A count that over-reports makes its side fail EVERY call, so without a
   bound the stress does not fail, it hangs -- and a test that hangs under
   the defect it exists to catch is a defect in the test. */
#define SIDED_GIVE_UP 1000u

DP_THREAD_FN (sided_producer, p)
{
  sided_arg_t *a = (sided_arg_t *)p;
  /* The WHOLE of space(), every time: a block capped below it would leave
     slack, and an over-report hides in slack. Capacity is 4096 here on
     every page size (32 KiB of f32 spans a page everywhere). */
  static float block[2 * 4096];
  size_t       sent = 0;
  while (sent < SIDED_TOTAL && !a->stop)
    {
      size_t n = dp_f32_space (a->buf);
      if (n == 0)
        continue;
      if (n > SIDED_TOTAL - sent)
        n = SIDED_TOTAL - sent;
      for (size_t k = 0; k < n; k++)
        block[2 * k] = (float)((sent + k) & 0xFFFF);
      if (dp_f32_write (a->buf, block, n))
        sent += n;
      else if (++a->refused >= SIDED_GIVE_UP)
        a->stop = 1;
    }
  dp_f32_close (a->buf);
  DP_THREAD_RETURN;
}

DP_THREAD_FN (sided_consumer, p)
{
  sided_arg_t *a   = (sided_arg_t *)p;
  size_t       got = 0;
  while (got < SIDED_TOTAL && !a->stop)
    {
      size_t n = dp_f32_available (a->buf);
      if (n == 0)
        {
          if (dp_f32_closed (a->buf) && dp_f32_available (a->buf) == 0)
            break;
          continue;
        }
      float *v = dp_f32_peek (a->buf, n);
      if (!v)
        {
          if (++a->missing >= SIDED_GIVE_UP)
            a->stop = 1;
          continue;
        }
      for (size_t k = 0; k < n; k++)
        if (v[2 * k] != (float)((got + k) & 0xFFFF))
          a->wrong++;
      dp_f32_consume (a->buf, n);
      got += n;
    }
  DP_THREAD_RETURN;
}

int
main (void)
{
  size_t page = dp__page_size ();

  /* ── invalid sizes are rejected ─────────────────────────────────── */
  {
    DP_CHECK (dp_f32_create (0) == NULL);  /* zero */
    DP_CHECK (dp_f32_create (3) == NULL);  /* not a power of two */
    DP_CHECK (dp_f32_create (96) == NULL); /* not a power of two */
  }

  /* ── an unsatisfiable wait returns instead of spinning (#1335) ───── */
  {
    /* n > capacity can never be met: the ring holds at most `capacity`, so
       head - tail cannot reach n whatever the producer does. The spin loop
       has exits for end-of-stream and for an interrupt and none for this,
       so without the guard this hangs forever at 100% CPU.

       NOTE for anyone sabotaging the guard to check this test bites: it
       will HANG rather than fail, because the failure being pinned is a
       non-return. The Python half (TestWaitBeyondCapacity) runs the same
       call on a thread with a join deadline and so fails fast instead. */
    dp_f32_t *a = dp_f32_create (1024);
    DP_CHECK (a != NULL);
    DP_CHECK (dp_f32_wait (a, a->capacity + 1) == NULL);
    DP_CHECK (!dp_f32_closed (a)); /* and NOT by pretending it is EOF */
    dp_f32_destroy (a);

    dp_f64_t *b = dp_f64_create (1024);
    DP_CHECK (b != NULL);
    DP_CHECK (dp_f64_wait (b, b->capacity + 1) == NULL);
    dp_f64_destroy (b);

    dp_i16_t *c = dp_i16_create (1024);
    DP_CHECK (c != NULL);
    DP_CHECK (dp_i16_wait (c, c->capacity + 1) == NULL);
    dp_i16_destroy (c);
  }

  /* ── sub-page request rounds up to a whole, power-of-two page ────── */
  {
    /* elem = bytes per complex sample: f32=8, f64=16, i16=4. */
    dp_f32_t *a = dp_f32_create (1);
    DP_CHECK (a != NULL);
    DP_CHECK ((a->capacity & (a->capacity - 1)) == 0);
    DP_CHECK (a->capacity * 8 >= page);
    DP_CHECK ((a->capacity * 8) % page == 0);
    dp_f32_destroy (a);

    dp_f64_t *b = dp_f64_create (1);
    DP_CHECK (b != NULL);
    DP_CHECK ((b->capacity & (b->capacity - 1)) == 0);
    DP_CHECK (b->capacity * 16 >= page);
    DP_CHECK ((b->capacity * 16) % page == 0);
    dp_f64_destroy (b);

    dp_i16_t *c = dp_i16_create (1);
    DP_CHECK (c != NULL);
    DP_CHECK ((c->capacity & (c->capacity - 1)) == 0);
    DP_CHECK (c->capacity * 4 >= page);
    DP_CHECK ((c->capacity * 4) % page == 0);
    dp_i16_destroy (c);
  }

  /* ── a request that already spans a page is NOT over-rounded ─────── */
  {
    /* page/8 complex samples is exactly one page for f32; page and elem are
     * powers of two, so this is itself a power of two and must pass through
     * unchanged. */
    size_t    exact = page / 8;
    dp_f32_t *a     = dp_f32_create (exact);
    DP_CHECK (a != NULL);
    DP_CHECK (a->capacity == exact);
    dp_f32_destroy (a);

    /* Two pages — also unchanged. */
    dp_f32_t *b = dp_f32_create (exact * 2);
    DP_CHECK (b != NULL);
    DP_CHECK (b->capacity == exact * 2);
    dp_f32_destroy (b);
  }

  /* ── mirror wraps correctly after rounding (f32) ─────────────────── */
  {
    dp_f32_t *buf = dp_f32_create (1); /* rounds up to the page minimum */
    DP_CHECK (buf != NULL);
    size_t cap = buf->capacity;

    PRIME_TO (f32, float, buf, cap - 2); /* head = tail = cap - 2 */

    /* Four interleaved I/Q samples written at index cap-2 straddle the wrap
     * at `cap`; the double-mapping must hand them back contiguously. */
    float in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    DP_CHECK (dp_f32_write (buf, in, 4) == true);
    float *view = dp_f32_wait (buf, 4);
    for (int i = 0; i < 8; i++)
      DP_CHECK (view[i] == in[i]);
    dp_f32_consume (buf, 4);
    dp_f32_destroy (buf);
  }

  /* ── mirror wraps correctly after rounding (i16 IQ path) ─────────── */
  {
    dp_i16_t *buf = dp_i16_create (1);
    DP_CHECK (buf != NULL);
    size_t cap = buf->capacity;

    PRIME_TO (i16, int16_t, buf, cap - 2);

    int16_t in[8] = { 10, 11, 20, 21, 30, 31, 40, 41 };
    DP_CHECK (dp_i16_write (buf, in, 4) == true);
    int16_t *view = dp_i16_wait (buf, 4);
    for (int i = 0; i < 8; i++)
      DP_CHECK (view[i] == in[i]);
    dp_i16_consume (buf, 4);
    dp_i16_destroy (buf);
  }

  /* ── full-then-overflow drops and counts ─────────────────────────── */
  {
    dp_f32_t *buf = dp_f32_create (1);
    size_t    cap = buf->capacity;
    PRIME_TO (f32, float, buf, 0); /* no-op; head = tail = 0 */

    /* Fill to capacity in 64-sample chunks (avoids a cap-sized stack array),
     * leaving the data in place so the buffer ends up full. */
    float  chunk[128] = { 0 };
    size_t written    = 0;
    while (written < cap)
      {
        size_t n = cap - written;
        if (n > 64)
          n = 64;
        DP_CHECK (dp_f32_write (buf, chunk, n) == true);
        written += n;
      }
    /* Now full: one more sample must be dropped. */
    DP_CHECK (dp_f32_write (buf, chunk, 1) == false);
    DP_CHECK (buf->dropped == 1);
    dp_f32_destroy (buf);
  }

  /* ---- end of stream ------------------------------------------------- */
  /* The defect this closes: wait() was an unbounded busy-spin with no exit.
     A producer that stopped left the consumer spinning forever at 100% CPU,
     and because the loop read no flag, no signal handler could rescue it.
     These four cases are the whole contract. */
  {
    dp_f32_t *buf = dp_f32_create (1024);
    DP_REQUIRE (buf != NULL);

    /* 1. An open, empty ring with data available returns it as before --
          closing must not change the ordinary path. */
    float chunk[128];
    for (size_t i = 0; i < 128; i++)
      chunk[i] = (float)i;
    DP_CHECK (dp_f32_write (buf, chunk, 64) == true);
    DP_CHECK (dp_f32_wait (buf, 64) != NULL);
    dp_f32_consume (buf, 64);

    /* 2. Closed and drained -> NULL, promptly, instead of spinning. */
    DP_CHECK (dp_f32_closed (buf) == 0);
    dp_f32_close (buf);
    DP_CHECK (dp_f32_closed (buf) != 0);
    DP_CHECK_MSG (dp_f32_wait (buf, 64) == NULL,
                  "a closed, drained ring ends the wait rather than "
                  "spinning forever on a producer that has finished");
    dp_f32_destroy (buf);
  }

  {
    /* 3. Closed with a FULL batch still buffered must hand it back, not
          discard it. The ordering in wait() exists for this: a producer
          that writes its last samples and closes must not lose them to a
          consumer that noticed the flag first. */
    dp_f32_t *buf = dp_f32_create (1024);
    DP_REQUIRE (buf != NULL);
    float chunk[128];
    for (size_t i = 0; i < 128; i++)
      chunk[i] = (float)i;
    DP_CHECK (dp_f32_write (buf, chunk, 64) == true);
    dp_f32_close (buf);
    DP_CHECK_MSG (dp_f32_wait (buf, 64) != NULL,
                  "closing must not discard samples already written");
    /* NB: this case returns before the loop body runs at all -- a full
       batch is available on entry, so the closed check inside the loop is
       never reached. The ordering that check exists for is exercised by
       the threaded case below, not here. Sabotaging the in-loop ordering
       leaves THIS assertion green, which is how that was found. */
    dp_f32_consume (buf, 64);
    DP_CHECK (dp_f32_wait (buf, 64) == NULL);
    dp_f32_destroy (buf);
  }

  {
    /* 4. Interrupted -> NULL, on a ring nobody closed. This is the case the
          old spin could not express at all. */
    dp_f32_t *buf = dp_f32_create (1024);
    DP_REQUIRE (buf != NULL);
    dp_interrupt ();
    DP_CHECK_MSG (dp_f32_wait (buf, 64) == NULL,
                  "an interrupt ends the wait even on an open ring");
    DP_CHECK_MSG (dp_f32_closed (buf) == 0,
                  "and does so WITHOUT closing the ring -- the caller tells "
                  "end-of-stream from interrupted by asking which happened");
    dp_resume ();
    dp_f32_destroy (buf);
  }

  /* 5. A consumer already spinning inside wait() when the producer writes
        its last batch and closes must receive those samples. This is the
        realistic shape -- the ring exists for producer/consumer threads --
        and it does cover the loop body, which the single-threaded cases
        never enter.

        Repeated, because ONE attempt does not pin the re-load-after-
        acquire inside the closed branch. That branch needs the consumer's
        head-load to land before the producer's write and its closed-load
        after the close -- a window one spin-iteration wide -- so a single
        race hits it only sometimes. Measured, rather than assumed: with
        the re-load deleted, one attempt goes red in 2 of 40 runs.

        A 5% detector is not a gate; it is a flake wearing one. But the
        attempts are independent, so repetition compounds them --
        0.95^EOS_RACE_ATTEMPTS, which is about 3e-5 at 200. That turns the
        argument written in buffer.h into something the suite enforces,
        which is what this file is for. */
  {
    /* 64 COMPLEX samples, so 128 floats. `n` is in samples and each is an
       I/Q pair, so a float[64] here is a 256-byte over-read that the
       fixture -- not the ring -- was committing. */
    float chunk[128];
    for (size_t i = 0; i < 128; i++)
      chunk[i] = (float)(i + 1);

    int lost = 0, wrong = 0;
    for (int attempt = 0; attempt < EOS_RACE_ATTEMPTS; attempt++)
      {
        dp_f32_t *buf = dp_f32_create (1024);
        DP_REQUIRE (buf != NULL);

        eos_race_arg_t arg = { buf, NULL, 0 };
        dp_thread_t    th;
        DP_REQUIRE (dp_thread_create (&th, eos_consumer, &arg) == 0);

        /* Long enough for the consumer to reach the spin, short enough
           that the loop above stays affordable. */
        dp_thread_sleep_us (200);

        DP_CHECK (dp_f32_write (buf, chunk, 64) == true);
        dp_f32_close (buf);

        DP_REQUIRE (dp_thread_join (th) == 0);
        if (arg.got == NULL)
          lost++;
        else if (arg.first != 1.0f)
          wrong++;
        dp_f32_destroy (buf);
      }

    DP_CHECK_MSG (lost == 0,
                  "a consumer spinning when the producer writes-then-closes "
                  "must receive the final batch, not lose it to the flag");
    DP_CHECK_MSG (wrong == 0,
                  "and receive the samples that were actually written");
    dp_f32_destroy (dp_f32_create (1024)); /* keep the create/destroy pair */
  }

  /* The macro generates a type each time it is instantiated, so f64 and
     i16 have their own copies of every function -- exercised here, because
     "it works for f32" says nothing about the others. */
  {
    dp_f64_t *b64 = dp_f64_create (1024);
    DP_REQUIRE (b64 != NULL);
    double d[128] = { 0 };
    DP_CHECK (dp_f64_write (b64, d, 64) == true);
    DP_CHECK (dp_f64_closed (b64) == 0);
    dp_f64_close (b64);
    DP_CHECK (dp_f64_closed (b64) != 0);
    DP_CHECK (dp_f64_wait (b64, 64) != NULL);
    dp_f64_consume (b64, 64);
    DP_CHECK (dp_f64_wait (b64, 64) == NULL);
    dp_f64_destroy (b64);

    dp_i16_t *b16 = dp_i16_create (1024);
    DP_REQUIRE (b16 != NULL);
    int16_t s[128] = { 0 };
    DP_CHECK (dp_i16_write (b16, s, 64) == true);
    DP_CHECK (dp_i16_closed (b16) == 0);
    dp_i16_close (b16);
    DP_CHECK (dp_i16_closed (b16) != 0);
    DP_CHECK (dp_i16_wait (b16, 64) != NULL);
    dp_i16_consume (b16, 64);
    DP_CHECK (dp_i16_wait (b16, 64) == NULL);
    dp_i16_destroy (b16);
  }

  /* ── many rings live at once, each with a real mirror (#1360) ────────
     The Windows allocator used to probe for a hole only as long as ONE
     view, so whether the mirror fit depended on what happened to follow
     it -- which ASLR changes every run. It failed as a NULL from create(),
     seen as crashes in six unrelated suites. Holding many rings open,
     interleaved with heap blocks, is what fills the address space around
     each probe; and a mirror that is not the same memory is checked
     directly, since a ring that merely allocates proves nothing. */
  {
    enum
    {
      N_RINGS = 96
    };
    dp_f32_t *rings[N_RINGS];
    void     *heap[N_RINGS];
    int       null_rings = 0, bad_mirror = 0;
    for (size_t i = 0; i < N_RINGS; i++)
      {
        rings[i] = dp_f32_create ((size_t)8192 << (i % 4)); /* 64K..512K */
        heap[i]  = malloc (4096 + 4096 * (i % 7));
        if (!rings[i])
          {
            null_rings++;
            continue;
          }
        /* Two floats per complex sample, so the mirror starts 2*capacity
           floats in. */
        size_t mirror              = 2 * rings[i]->capacity;
        rings[i]->data[0]          = (float)i + 0.5f;
        rings[i]->data[mirror - 1] = -(float)i;
        if (rings[i]->data[mirror] != (float)i + 0.5f
            || rings[i]->data[2 * mirror - 1] != -(float)i)
          bad_mirror++;
      }
    DP_CHECK (null_rings == 0);
    DP_CHECK (bad_mirror == 0);
    for (size_t i = 0; i < N_RINGS; i++)
      {
        if (rings[i])
          dp_f32_destroy (rings[i]);
        free (heap[i]);
      }
  }

  /* ══ The non-blocking surface: space / write_some / peek / reset ═══════
     Every C consumer of this ring is single-threaded, and until these
     existed each one rebuilt them from head/tail/mask by hand. */

  /* ── space is the producer's free room, and write() agrees with it ─── */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    float src[2 * 64] = { 0 };
    DP_CHECK (dp_f32_space (b) == b->capacity);
    DP_CHECK (dp_f32_write (b, src, 64));
    DP_CHECK (dp_f32_space (b) == b->capacity - 64);
    DP_CHECK (dp_f32_space (b) + dp_f32_available (b) == b->capacity);
    dp_f32_consume (b, 64);
    DP_CHECK (dp_f32_space (b) == b->capacity);
    dp_f32_destroy (b);
  }

  /* ── write_some takes what fits, reports it, and never counts a drop ── */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    size_t cap = b->capacity;
    float *src = (float *)calloc (2 * (cap + 100), sizeof *src);
    DP_CHECK (src != NULL);
    for (size_t i = 0; i < 2 * (cap + 100); i++)
      src[i] = (float)i;

    /* More than the ring can ever hold: all-or-nothing write() refuses it
       forever; write_some takes exactly `capacity` of it. */
    DP_CHECK (!dp_f32_write (b, src, cap + 100));
    size_t refused = b->dropped;
    DP_CHECK (refused == cap + 100);
    DP_CHECK (dp_f32_write_some (b, src, cap + 100) == cap);
    DP_CHECK (b->dropped == refused); /* nothing refused: nothing counted */
    DP_CHECK (dp_f32_space (b) == 0);
    DP_CHECK (dp_f32_write_some (b, src, 1) == 0); /* full */
    DP_CHECK (b->dropped == refused); /* a FULL ring is not a refusal either */
    /* ...and it wrote the FIRST `cap` samples, in order. */
    float *v = dp_f32_peek (b, cap);
    DP_CHECK (v != NULL);
    DP_CHECK (v[0] == 0.0f && v[1] == 1.0f);
    DP_CHECK (v[2 * cap - 1] == (float)(2 * cap - 1));
    free (src);
    dp_f32_destroy (b);
  }

  /* ── peek never blocks: NULL until n are there, then wait()'s pointer ─ */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    float src[2 * 32];
    for (size_t i = 0; i < 2 * 32; i++)
      src[i] = (float)i;
    /* On an EMPTY, OPEN ring wait() would spin forever; this returns. */
    DP_CHECK (dp_f32_peek (b, 64) == NULL);
    DP_CHECK (dp_f32_write_some (b, src, 32) == 32);
    DP_CHECK (dp_f32_peek (b, 64) == NULL); /* half a frame: still no */
    DP_CHECK (dp_f32_write_some (b, src, 32) == 32);
    float *pk = dp_f32_peek (b, 64);
    DP_CHECK (pk != NULL);
    DP_CHECK (pk == dp_f32_wait (b, 64));  /* the same zero-copy pointer */
    DP_CHECK (dp_f32_available (b) == 64); /* and it consumed nothing */
    DP_CHECK (dp_f32_peek (b, b->capacity + 1) == NULL); /* never */
    dp_f32_destroy (b);
  }

  /* ── the chunking loop: ANY chunk in, fixed frames out, across the wrap ─
     The pattern write_some()'s header documents, run as written -- with a
     chunk larger than the ring, and a frame size that does not divide the
     capacity, so frames straddle the mirror seam. Every output sample is
     checked against the input stream, so a dropped, repeated or torn
     sample anywhere fails. */
  {
    dp_f32_t *b = dp_f32_create (1024);
    DP_CHECK (b != NULL);
    const size_t total = 3 * b->capacity + 777; /* > capacity, on purpose */
    const size_t nfft  = 1000;                  /* does not divide it     */
    float       *in    = (float *)malloc (2 * total * sizeof *in);
    DP_CHECK (in != NULL);
    for (size_t i = 0; i < 2 * total; i++)
      in[i] = (float)i;
    size_t off = 0, frames = 0, bad = 0, seam = 0;
    while (off < total)
      {
        size_t w = dp_f32_write_some (b, in + 2 * off, total - off);
        off += w;
        float *f;
        while ((f = dp_f32_peek (b, nfft)) != NULL)
          {
            size_t base = frames * nfft; /* stream position of f[0] */
            if ((b->tail & b->mask) + nfft > b->capacity)
              seam++; /* this frame crosses the mirror seam */
            for (size_t k = 0; k < 2 * nfft; k++)
              if (f[k] != (float)(2 * base + k))
                bad++;
            dp_f32_consume (b, nfft);
            frames++;
          }
        if (w == 0 && dp_f32_peek (b, nfft) == NULL)
          break; /* cannot progress: would be a bug, not a hang */
      }
    DP_CHECK (off == total);
    DP_CHECK (frames == total / nfft);
    DP_CHECK (bad == 0);
    DP_CHECK (seam > 0); /* the wrap was actually exercised */
    DP_CHECK (dp_f32_available (b) == total % nfft); /* the remainder */
    free (in);
    dp_f32_destroy (b);
  }

  /* ── overlapped frames: consume(hop) with hop < the peeked frame ────── */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    float src[2 * 256];
    for (size_t i = 0; i < 2 * 256; i++)
      src[i] = (float)i;
    DP_CHECK (dp_f32_write_some (b, src, 256) == 256);
    const size_t nfft = 64, hop = 16;
    size_t       n = 0, bad = 0;
    float       *f;
    while ((f = dp_f32_peek (b, nfft)) != NULL)
      {
        if (f[0] != (float)(2 * n * hop)) /* frame n starts at n*hop */
          bad++;
        dp_f32_consume (b, hop);
        n++;
      }
    DP_CHECK (bad == 0);
    DP_CHECK (n == (256 - nfft) / hop + 1); /* 13 overlapped frames */
    dp_f32_destroy (b);
  }

  /* ── wait_status: one owner of the precedence ──────────────────────── */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    float src[2 * 64] = { 0 };
    DP_CHECK (dp_f32_wait_status (b, 64) == DP_WAIT_PENDING);
    DP_CHECK (dp_f32_wait_status (b, b->capacity + 1) == DP_WAIT_TOO_LARGE);
    DP_CHECK (dp_f32_write (b, src, 64));
    DP_CHECK (dp_f32_wait_status (b, 64) == DP_WAIT_OK);

    /* Interrupted, with too few samples. */
    dp_interrupt ();
    DP_CHECK (dp_f32_wait_status (b, 128) == DP_WAIT_INTERRUPTED);
    /* Readable samples win over an interrupt... */
    DP_CHECK (dp_f32_wait_status (b, 64) == DP_WAIT_OK);
    /* ...too-large wins over everything... */
    DP_CHECK (dp_f32_wait_status (b, b->capacity + 1) == DP_WAIT_TOO_LARGE);
    /* ...and closed wins over interrupted. */
    dp_f32_close (b);
    DP_CHECK (dp_f32_wait_status (b, 128) == DP_WAIT_CLOSED);
    DP_CHECK (dp_f32_wait_status (b, 64) == DP_WAIT_OK); /* still drains */
    dp_resume ();

    /* It agrees with wait() on every case wait() can return from. */
    DP_CHECK (dp_f32_wait (b, 64) != NULL);
    DP_CHECK (dp_f32_wait (b, 128) == NULL);
    DP_CHECK (dp_f32_wait (b, b->capacity + 1) == NULL);
    dp_f32_destroy (b);
  }

  /* ── reset empties AND reopens; dropped is a lifetime count ─────────── */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_CHECK (b != NULL);
    float src[2 * 64] = { 0 };
    DP_CHECK (dp_f32_write (b, src, 64));
    DP_CHECK (!dp_f32_write (b, src, b->capacity)); /* refused: counted */
    size_t dropped = b->dropped;
    DP_CHECK (dropped == b->capacity);
    dp_f32_close (b);
    dp_f32_reset (b);
    DP_CHECK (dp_f32_available (b) == 0);
    DP_CHECK (dp_f32_space (b) == b->capacity);
    DP_CHECK (!dp_f32_closed (b));    /* reopened */
    DP_CHECK (b->dropped == dropped); /* kept */
    DP_CHECK (dp_f32_wait_status (b, 64) == DP_WAIT_PENDING);
    DP_CHECK (dp_f32_write (b, src, 64)); /* and usable again */
    dp_f32_destroy (b);
  }

  /* ── all three widths carry the surface (they are one macro) ────────── */
  {
    dp_f64_t *d = dp_f64_create (4096);
    dp_i16_t *q = dp_i16_create (4096);
    DP_CHECK (d != NULL && q != NULL);
    double  sd[2 * 8] = { 0 };
    int16_t sq[2 * 8] = { 1, -1, 2, -2, 3, -3, 4, -4 };
    DP_CHECK (dp_f64_peek (d, 8) == NULL && dp_i16_peek (q, 8) == NULL);
    DP_CHECK (dp_f64_write_some (d, sd, 8) == 8);
    DP_CHECK (dp_i16_write_some (q, sq, 8) == 8);
    DP_CHECK (dp_f64_space (d) == d->capacity - 8);
    int16_t *iq = dp_i16_peek (q, 8);
    DP_CHECK (iq != NULL && iq[0] == 1 && iq[1] == -1 && iq[7] == -4);
    DP_CHECK (dp_f64_wait_status (d, 8) == DP_WAIT_OK);
    DP_CHECK (dp_i16_wait_status (q, 9) == DP_WAIT_PENDING);
    dp_f64_reset (d);
    dp_i16_reset (q);
    DP_CHECK (dp_f64_available (d) == 0 && dp_i16_available (q) == 0);
    dp_f64_destroy (d);
    dp_i16_destroy (q);
  }

  /* ══ Claims the header makes that nothing above asserted ══════════════
     Found by the claim inventory (doppler#1438). Each was sabotaged in a
     copy of buffer.h and seen to fail before being trusted. */

  /* ── a REFUSED write takes nothing and disturbs nothing ───────────────
     "Nothing is dropped here: the write is REFUSED, whole." The counts were
     pinned; the CONTENTS were not -- a write that copied as much as fit and
     then reported false satisfied every assertion above. */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_REQUIRE (b != NULL);
    size_t cap  = b->capacity;
    float *keep = (float *)calloc (2 * cap, sizeof *keep);
    float *junk = (float *)calloc (2 * cap, sizeof *junk);
    DP_REQUIRE (keep != NULL && junk != NULL);
    for (size_t i = 0; i < 2 * cap; i++)
      {
        keep[i] = (float)(i + 1);
        junk[i] = -1.0f;
      }
    /* Advance so the refused block WOULD straddle the wrap, then leave
       exactly 10 samples of room. */
    PRIME_TO (f32, float, b, cap - 5);
    DP_CHECK (dp_f32_write (b, keep, cap - 10));
    DP_CHECK (dp_f32_space (b) == 10);

    DP_CHECK (!dp_f32_write (b, junk, 11)); /* one too many: refused */
    DP_CHECK (dp_f32_available (b) == cap - 10);
    DP_CHECK (dp_f32_space (b) == 10);

    /* Look at the free room BEFORE refilling it. Writing over it first --
       which this test used to do -- erases exactly what a leaky refusal
       leaves behind: that version stayed green with write() copying a
       sample on its way to returning false. The room follows the readable
       samples contiguously (that is the mirror), and PRIME_TO left zeros. */
    float *held = dp_f32_peek (b, cap - 10);
    DP_REQUIRE (held != NULL);
    size_t touched = 0;
    for (size_t i = 0; i < 2 * 10; i++)
      touched += (held[2 * (cap - 10) + i] != 0.0f);
    DP_CHECK_MSG (touched == 0, "a refused write must not reach the ring");

    /* And what was there is intact: fill the room, read everything back. */
    DP_CHECK (dp_f32_write (b, keep + 2 * (cap - 10), 10));
    float *v = dp_f32_peek (b, cap);
    DP_REQUIRE (v != NULL);
    size_t bad = 0;
    for (size_t i = 0; i < 2 * cap; i++)
      bad += (v[i] != keep[i]);
    DP_CHECK_MSG (bad == 0, "nor disturb what was already in it");
    free (keep);
    free (junk);
    dp_f32_destroy (b);
  }

  /* ── reset returns the POSITIONS to zero, not merely the counts ───────
     "Both positions return to zero." available()==0 and space()==capacity
     hold for ANY head==tail, so they pinned the counts and not the claim.
     The position is observable: the next write lands at data[0]. */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_REQUIRE (b != NULL);
    PRIME_TO (f32, float, b, 777); /* anywhere but zero */
    float one[2] = { 42.0f, -42.0f };
    dp_f32_reset (b);
    DP_CHECK (dp_f32_write (b, one, 1));
    DP_CHECK (b->data[0] == 42.0f && b->data[1] == -42.0f);
    DP_CHECK (dp_f32_peek (b, 1) == b->data);
    dp_f32_destroy (b);
  }

  /* ── a file-backed ring: the samples ARE the file ─────────────────────
     Nothing here was asserted at the ring's level at all; it was exercised
     only through burst_capture. */
  {
    const char *path = "dp_test_buffer_core_backed.bin";
    remove (path);

    DP_CHECK (dp_f32_create_backed (1000, path, NULL) == NULL); /* not 2^k */
    DP_CHECK (dp_f32_create_backed (0, path, NULL) == NULL);

    int       existed = -1;
    dp_f32_t *b       = dp_f32_create_backed (4096, path, &existed);
    DP_REQUIRE (b != NULL);
    DP_CHECK (existed == 0); /* created */
    size_t cap = b->capacity;
    DP_CHECK (b->data[0] == 0.0f && b->data[2 * cap - 1] == 0.0f); /* zeroed */

    float *src = (float *)calloc (2 * cap, sizeof *src);
    DP_REQUIRE (src != NULL);
    for (size_t i = 0; i < 2 * cap; i++)
      src[i] = (float)(i + 1);
    DP_CHECK (dp_f32_write (b, src, cap));
    dp_f32_sync (b);
    dp_f32_destroy (b);

    /* "the FILE's size is capacity * sizeof(type) * 2" */
    FILE *f = fopen (path, "rb");
    DP_REQUIRE (f != NULL);
    DP_CHECK (fseek (f, 0, SEEK_END) == 0);
    DP_CHECK ((size_t)ftell (f) == cap * sizeof (float) * 2);
    fclose (f);

    /* Same size: mapped AS IT STANDS -- and the positions are not in it. */
    existed = -1;
    b       = dp_f32_create_backed (4096, path, &existed);
    DP_REQUIRE (b != NULL);
    DP_CHECK (existed == 1);
    DP_CHECK (b->capacity == cap);
    DP_CHECK (dp_f32_available (b) == 0);
    size_t bad = 0;
    for (size_t i = 0; i < 2 * cap; i++)
      bad += (b->data[i] != src[i]);
    DP_CHECK_MSG (bad == 0, "a re-attached ring must hold what was written");
    /* ...through BOTH mappings: the mirror is rebuilt over the file. */
    DP_CHECK (b->data[2 * cap] == src[0]);
    dp_f32_destroy (b);

    /* A different size is NOT that ring: recreated, and zeroed. */
    existed = -1;
    b       = dp_f32_create_backed (2 * cap, path, &existed);
    DP_REQUIRE (b != NULL);
    DP_CHECK (existed == 0);
    DP_CHECK (b->capacity == 2 * cap);
    bad = 0;
    for (size_t i = 0; i < 4 * cap; i++)
      bad += (b->data[i] != 0.0f);
    DP_CHECK_MSG (bad == 0, "a resized file must come back zeroed");
    dp_f32_destroy (b);
    remove (path);
    free (src);

    /* sync() on an anonymous ring is a no-op, not a crash. */
    dp_f32_t *anon = dp_f32_create (4096);
    DP_REQUIRE (anon != NULL);
    dp_f32_sync (anon);
    DP_CHECK (dp_f32_space (anon) == anon->capacity);
    dp_f32_destroy (anon);
  }

  /* ── consume() refuses what is not there; write() stays in the mapping ─
     consume() used to be unbounded, so a caller COULD push the tail past the
     head -- and then the room computed from the two indices is larger than
     the ring. write() believed it and copied past the mapping: from the
     Python face, consume(1e6) then write(5e5 samples) was a SIGSEGV. Two
     independent guards now, each pinned here on its own. */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_REQUIRE (b != NULL);
    size_t cap = b->capacity;
    float *big = (float *)calloc (2 * (cap + 1), sizeof *big);
    DP_REQUIRE (big != NULL);

    /* 1. The release refuses, and releases NOTHING when it does. */
    DP_CHECK (dp_f32_write (b, big, 100));
    DP_CHECK (dp_f32_consume (b, 101) == DP_ERR_INVALID);
    DP_CHECK (dp_f32_available (b) == 100); /* not 0, and not -1 */
    DP_CHECK (dp_f32_space (b) == cap - 100);
    DP_CHECK (dp_f32_consume (b, 100) == DP_OK);
    DP_CHECK (dp_f32_available (b) == 0);
    DP_CHECK (dp_f32_consume (b, 0) == DP_OK);          /* a no-op      */
    DP_CHECK (dp_f32_consume (b, 1) == DP_ERR_INVALID); /* empty ring   */
    DP_CHECK (dp_f32_consume (b, (size_t)-1) == DP_ERR_INVALID);
    DP_CHECK (dp_f32_space (b) == cap); /* the counts still describe a ring */

    /* 2. A memcpy must not depend on (1). Put the indices in the state the
          old consume() could reach -- by hand, since the API no longer can
          -- and ask for more than the ring could ever hold. Under ASan an
          overrun here is a report; without it, the refusal is observable. */
    b->tail = b->head + 1000;
    DP_CHECK (dp_f32_space (b) > cap); /* the premise: the count is a lie */
    size_t before = b->dropped;
    DP_CHECK (!dp_f32_write (b, big, cap + 1));
    DP_CHECK (b->dropped == before + cap + 1);
    DP_CHECK (dp_f32_write_some (b, big, cap + 1) <= cap);
    free (big);
    dp_f32_destroy (b);
  }

  /* ── space() and available() never over-report, with the other side live
     "a lower bound that never goes stale in the unsafe direction" -- the
     whole reason a caller may size a block from them. Each side here sizes
     EVERY call from its own count and nothing else, over a few hundred
     wraps: a write() refused, or a peek() that comes back NULL, is that
     count having promised more than was there. */
  {
    dp_f32_t *b = dp_f32_create (4096);
    DP_REQUIRE (b != NULL);
    DP_REQUIRE (b->capacity == 4096); /* the producer's block is sized to it */
    sided_arg_t arg = { b, 0, 0, 0, 0 };
    dp_thread_t prod, cons;
    DP_REQUIRE (dp_thread_create (&cons, sided_consumer, &arg) == 0);
    DP_REQUIRE (dp_thread_create (&prod, sided_producer, &arg) == 0);
    DP_REQUIRE (dp_thread_join (prod) == 0);
    DP_REQUIRE (dp_thread_join (cons) == 0);
    DP_CHECK_MSG (arg.refused == 0, "space() promised room write() denied");
    DP_CHECK_MSG (arg.missing == 0, "available() promised samples peek() "
                                    "could not give");
    DP_CHECK_MSG (arg.wrong == 0, "and every sample arrived in order");
    DP_CHECK (b->dropped == 0);
    DP_CHECK (dp_f32_available (b) == 0);
    dp_f32_destroy (b);
  }

  DP_TEST_END ("test_buffer_core");
}
