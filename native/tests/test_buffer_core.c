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
#include <pthread.h>
#include <stdio.h>
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

/* Consumer half of the write-then-close race below. Spins in wait() until
   the producer supplies a batch or closes the ring. */
typedef struct
{
  dp_f32_t *buf;
  float    *got;
  float     first;
} eos_race_arg_t;

static void *
eos_consumer (void *p)
{
  eos_race_arg_t *a = (eos_race_arg_t *)p;
  a->got            = dp_f32_wait (a->buf, 64);
  if (a->got)
    a->first = a->got[0];
  return NULL;
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

        It does NOT pin the re-load-after-acquire inside the closed branch:
        that needs the consumer to observe the flag while the write is still
        invisible, and when the write IS visible the loop exits before the
        branch. Sabotaging the re-load leaves this green. The argument for
        it is written in buffer.h instead, where it can be read next to the
        code it defends. */
  {
    dp_f32_t *buf = dp_f32_create (1024);
    DP_REQUIRE (buf != NULL);

    eos_race_arg_t arg = { buf, NULL, 0 };
    pthread_t      th;
    DP_REQUIRE (pthread_create (&th, NULL, eos_consumer, &arg) == 0);

    /* Let the consumer reach the spin before anything is written. */
    struct timespec nap = { 0, 50 * 1000 * 1000 };
    nanosleep (&nap, NULL);

    float chunk[64];
    for (size_t i = 0; i < 64; i++)
      chunk[i] = (float)(i + 1);
    DP_CHECK (dp_f32_write (buf, chunk, 64) == true);
    dp_f32_close (buf);

    DP_REQUIRE (pthread_join (th, NULL) == 0);
    DP_CHECK_MSG (arg.got != NULL,
                  "a consumer spinning when the producer writes-then-closes "
                  "must receive the final batch, not lose it to the flag");
    DP_CHECK_MSG (arg.first == 1.0f,
                  "and receive the samples that were actually written");
    dp_f32_destroy (buf);
  }

  DP_TEST_END ("test_buffer_core");
}
