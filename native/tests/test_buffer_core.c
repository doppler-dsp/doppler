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
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

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

int
main (void)
{
  int    _fails = 0;
  size_t page   = dp__page_size ();

  /* ── invalid sizes are rejected ─────────────────────────────────── */
  {
    CHECK (dp_f32_create (0) == NULL);  /* zero */
    CHECK (dp_f32_create (3) == NULL);  /* not a power of two */
    CHECK (dp_f32_create (96) == NULL); /* not a power of two */
  }

  /* ── sub-page request rounds up to a whole, power-of-two page ────── */
  {
    /* elem = bytes per complex sample: f32=8, f64=16, i16=4. */
    dp_f32_t *a = dp_f32_create (1);
    CHECK (a != NULL);
    CHECK ((a->capacity & (a->capacity - 1)) == 0);
    CHECK (a->capacity * 8 >= page);
    CHECK ((a->capacity * 8) % page == 0);
    dp_f32_destroy (a);

    dp_f64_t *b = dp_f64_create (1);
    CHECK (b != NULL);
    CHECK ((b->capacity & (b->capacity - 1)) == 0);
    CHECK (b->capacity * 16 >= page);
    CHECK ((b->capacity * 16) % page == 0);
    dp_f64_destroy (b);

    dp_i16_t *c = dp_i16_create (1);
    CHECK (c != NULL);
    CHECK ((c->capacity & (c->capacity - 1)) == 0);
    CHECK (c->capacity * 4 >= page);
    CHECK ((c->capacity * 4) % page == 0);
    dp_i16_destroy (c);
  }

  /* ── a request that already spans a page is NOT over-rounded ─────── */
  {
    /* page/8 complex samples is exactly one page for f32; page and elem are
     * powers of two, so this is itself a power of two and must pass through
     * unchanged. */
    size_t    exact = page / 8;
    dp_f32_t *a     = dp_f32_create (exact);
    CHECK (a != NULL);
    CHECK (a->capacity == exact);
    dp_f32_destroy (a);

    /* Two pages — also unchanged. */
    dp_f32_t *b = dp_f32_create (exact * 2);
    CHECK (b != NULL);
    CHECK (b->capacity == exact * 2);
    dp_f32_destroy (b);
  }

  /* ── mirror wraps correctly after rounding (f32) ─────────────────── */
  {
    dp_f32_t *buf = dp_f32_create (1); /* rounds up to the page minimum */
    CHECK (buf != NULL);
    size_t cap = buf->capacity;

    PRIME_TO (f32, float, buf, cap - 2); /* head = tail = cap - 2 */

    /* Four interleaved I/Q samples written at index cap-2 straddle the wrap
     * at `cap`; the double-mapping must hand them back contiguously. */
    float in[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    CHECK (dp_f32_write (buf, in, 4) == true);
    float *view = dp_f32_wait (buf, 4);
    for (int i = 0; i < 8; i++)
      CHECK (view[i] == in[i]);
    dp_f32_consume (buf, 4);
    dp_f32_destroy (buf);
  }

  /* ── mirror wraps correctly after rounding (i16 IQ path) ─────────── */
  {
    dp_i16_t *buf = dp_i16_create (1);
    CHECK (buf != NULL);
    size_t cap = buf->capacity;

    PRIME_TO (i16, int16_t, buf, cap - 2);

    int16_t in[8] = { 10, 11, 20, 21, 30, 31, 40, 41 };
    CHECK (dp_i16_write (buf, in, 4) == true);
    int16_t *view = dp_i16_wait (buf, 4);
    for (int i = 0; i < 8; i++)
      CHECK (view[i] == in[i]);
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
        CHECK (dp_f32_write (buf, chunk, n) == true);
        written += n;
      }
    /* Now full: one more sample must be dropped. */
    CHECK (dp_f32_write (buf, chunk, 1) == false);
    CHECK (buf->dropped == 1);
    dp_f32_destroy (buf);
  }

  if (_fails)
    {
      fprintf (stderr, "test_buffer_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_buffer_core PASSED\n");
  return 0;
}
