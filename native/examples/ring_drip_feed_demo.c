/**
 * ring_drip_feed_demo.c — accumulate small arrivals until a frame is there.
 *
 * Samples arrive in pieces smaller than a frame, on ONE thread. The blocking
 * dp_f32_wait() would deadlock here -- the thread that would produce the
 * samples is the one waiting for them -- so the question "is there a frame
 * yet?" is asked with dp_f32_peek(), which never blocks: it returns the
 * frame, contiguous and zero-copy, or NULL.
 *
 * Self-validating: exits non-zero if a frame is missing, late, or starts at
 * the wrong sample. (Checks are explicit rather than assert(): a Release
 * build defines NDEBUG, and an example that validates nothing is a listing.)
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_drip_feed_demo
 */
#include "buffer/buffer.h"
#include <stdio.h>

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
  PIECE    = 100,  /* complex samples per arrival          */
  FRAME    = 1024, /* complex samples the consumer wants   */
  ARRIVALS = 25    /* 2500 samples in all: two full frames */
};

int
main (void)
{
  dp_f32_t *ring = dp_f32_create (4096);
  CHECK (ring != NULL);

  /* A ramp, so a frame's first sample says where in the stream it starts. */
  float  piece[2 * PIECE];
  size_t sent = 0, frames = 0;

  for (int arrival = 0; arrival < ARRIVALS; arrival++)
    {
      for (size_t k = 0; k < PIECE; k++)
        {
          piece[2 * k]     = (float)(sent + k); /* I = stream position */
          piece[2 * k + 1] = 0.0f;
        }
      CHECK (dp_f32_write_some (ring, piece, PIECE) == PIECE);
      sent += PIECE;

      /* NULL until FRAME samples have accumulated; then the frame itself. */
      float *frame = dp_f32_peek (ring, FRAME);
      if (frame)
        {
          CHECK (frame[0] == (float)(frames * FRAME)); /* starts on time  */
          CHECK (frame[2 * (FRAME - 1)]
                 == (float)(frames * FRAME + FRAME - 1));
          dp_f32_consume (ring, FRAME);
          frames++;
        }
    }

  CHECK (frames == (size_t)(ARRIVALS * PIECE) / FRAME);
  CHECK (dp_f32_available (ring) == (size_t)(ARRIVALS * PIECE) % FRAME);
  printf ("drip-feed: %d arrivals of %d -> %zu frames of %d, %zu left over\n",
          ARRIVALS, PIECE, frames, FRAME, dp_f32_available (ring));
  dp_f32_destroy (ring);
  return 0;
}
