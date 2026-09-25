/**
 * ring_chunking_demo.c — any chunk in, fixed frames out.
 *
 * The shape every FFT front end needs: input arrives in large or irregular
 * chunks, the transform wants exactly NFFT samples, possibly overlapped.
 * Feed with dp_f32_write_some(), drain with dp_f32_peek(), and alternate the
 * two -- which is also how a chunk LARGER THAN THE RING goes through it, and
 * why all-or-nothing dp_f32_write() cannot do this job at all.
 *
 *   - NFFT does not divide the ring's capacity, so frames straddle the end
 *     of the ring; the double mapping hands each one back contiguous anyway.
 *   - HOP < NFFT reads overlapped frames: consume() releases HOP, not NFFT.
 *
 * Self-validating: every sample of every frame is checked against the input
 * stream, so a dropped, repeated or torn sample anywhere exits non-zero.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_chunking_demo
 */
#include "doppler/buffer/buffer.h"
#include <stdio.h>
#include <stdlib.h>

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
  NFFT = 1000, /* frame length; does not divide the capacity */
  HOP  = 250   /* 75% overlap                                 */
};

int
main (void)
{
  dp_f32_t *ring = dp_f32_create (1024);
  CHECK (ring != NULL);

  /* One input chunk, more than three times what the ring can hold. */
  const size_t total = 3 * ring->capacity + 777;
  float       *in    = (float *)malloc (2 * total * sizeof *in);
  CHECK (in != NULL);
  for (size_t i = 0; i < total; i++)
    {
      in[2 * i]     = (float)i; /* I = stream position */
      in[2 * i + 1] = -(float)i;
    }

  size_t off = 0, frames = 0, torn = 0, across_the_end = 0;
  while (off < total)
    {
      /* Takes what fits and says how much; 0 only when the ring is full. */
      off += dp_f32_write_some (ring, in + 2 * off, total - off);

      float *frame;
      while ((frame = dp_f32_peek (ring, NFFT)) != NULL)
        {
          /* NFFT contiguous samples -- even when they wrap the ring. */
          if ((ring->tail & ring->mask) + NFFT > ring->capacity)
            across_the_end++;
          for (size_t k = 0; k < NFFT; k++)
            if (frame[2 * k] != (float)(frames * HOP + k)
                || frame[2 * k + 1] != -(float)(frames * HOP + k))
              torn++;
          dp_f32_consume (ring, HOP); /* overlap: release HOP, keep the rest */
          frames++;
        }
    }

  CHECK (torn == 0);
  CHECK (frames == (total - NFFT) / HOP + 1);
  CHECK (across_the_end > 0); /* the wrap really was exercised */
  printf ("chunking: one %zu-sample chunk through a %zu-sample ring -> %zu "
          "frames of %d (hop %d), %zu read across the end of the ring\n",
          total, ring->capacity, frames, NFFT, HOP, across_the_end);
  free (in);
  dp_f32_destroy (ring);
  return 0;
}
