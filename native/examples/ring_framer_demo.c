/**
 * ring_framer_demo.c — any chunk in, fixed overlapping frames out, with no
 * loop of your own.
 *
 * ring_chunking_demo.c shows the ring primitives a frame consumer is built
 * from: write what fits, peek a frame, consume the hop. That loop is the same
 * in every consumer, and the order of its parts is easy to get wrong. The
 * framed face (DECLARE_DP_BUFFER_FRAMES) is that loop, once:
 *
 *   dp_f32_framer_feed()   takes input, but only as much as the caller has
 *                          room for frames from -- nothing is ever lost
 *   dp_f32_framer_next()   the next frame, zero-copy, or NULL
 *   dp_f32_framer_flush()  ends the stream with the one zero-padded row it
 *                          still owes
 *
 * The frames are a function of the INPUT STREAM, not of how it was chunked:
 * this program feeds the same stream three ways -- one chunk three times the
 * ring's size, 7 samples at a time, and with room for ONE frame per call --
 * and checks every sample of every frame against the stream itself.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_framer_demo
 */
#include "doppler/buffer/buffer.h"
#include <stdio.h>
#include <stdlib.h>

/* Stamped beside DECLARE_DP_BUFFER_VIEW in f32_buffer_core.h; this example
   builds from buffer.h alone, so it stamps the same face itself. */
DECLARE_DP_BUFFER_FRAMES (f32, float, float _Complex)

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

enum
{
  NFFT = 1000, /* frame length; does not divide the ring's capacity */
  HOP  = 250   /* 75% overlap                                       */
};

/* Row `row` of the stream `x` of `total` samples, element j: what the frame
   MUST hold, zero beyond the end of the stream. */
static float _Complex expect (const float _Complex *x, size_t total,
                              size_t row, size_t j)
{
  size_t i = row * HOP + j;
  return i < total ? x[i] : 0.0f;
}

/* Feed `x` in pieces of `chunk` samples, with room for `room` frames per
   call, and check every frame. Returns the frames produced, or -1 on a bad
   sample. `flush` ends the stream and checks the padded last row too. */
static long
run (const float _Complex *x, size_t total, size_t chunk, size_t room)
{
  dp_f32_t       *ring = dp_f32_create (1024); /* smaller than the stream */
  dp_f32_framer_t fr;
  if (!ring || dp_f32_framer_init (&fr, ring, NFFT, HOP) != DP_OK)
    return -1;

  size_t rows = 0;
  for (size_t at = 0; at < total;)
    {
      size_t n = total - at < chunk ? total - at : chunk;
      for (size_t done = 0; done < n;)
        {
          /* A short return is not a loss: offer the rest after draining. */
          done += dp_f32_framer_feed_view (&fr, x + at + done, n - done, room);
          const float _Complex *f;
          while ((f = dp_f32_framer_next_view (&fr)) != NULL)
            {
              for (size_t j = 0; j < NFFT; j++)
                if (f[j] != expect (x, total, rows, j))
                  return -1;
              rows++;
            }
        }
      at += n;
    }

  /* End of stream: the one row the last samples still owe, ON the hop grid. */
  float _Complex last[NFFT];
  if (dp_f32_framer_flush_view (&fr, last) == 1)
    {
      for (size_t j = 0; j < NFFT; j++)
        if (last[j] != expect (x, total, rows, j))
          return -1;
      rows++;
    }
  dp_f32_destroy (ring);
  return (long)rows;
}

int
main (void)
{
  const size_t    total = 3 * 1024 + 777; /* more than three rings */
  float _Complex *x     = (float _Complex *)malloc (total * sizeof *x);
  CHECK (x != NULL);
  for (size_t i = 0; i < total; i++)
    x[i] = (float)i - (float)i * I; /* I = position, Q = -position */

  const long whole = run (x, total, total, 1000000); /* one giant chunk  */
  const long drip  = run (x, total, 7, 1000000);     /* 7 at a time      */
  const long tight = run (x, total, 513, 1);         /* room for ONE row */

  CHECK (whole > 0);
  CHECK (drip == whole);
  CHECK (tight == whole);
  printf ("framer: %zu samples -> %ld rows of %d (hop %d), the same three "
          "ways: one chunk, 7 at a time, room for one row per call\n",
          total, whole, NFFT, HOP);
  free (x);
  return 0;
}
