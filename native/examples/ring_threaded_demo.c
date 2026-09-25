/**
 * ring_threaded_demo.c — one producer thread, one consumer thread, an end.
 *
 * The shape the ring was built for. The producer hands over whatever block
 * sizes it has; the consumer asks for exactly FRAME samples with the
 * blocking dp_f32_wait(), which spins until they are there and returns them
 * contiguous even when they straddle the end of the ring.
 *
 * Two things make it a complete program rather than a loop:
 *
 *   - BACKPRESSURE is the producer's. dp_f32_write() never blocks; it
 *     refuses. So the producer waits for dp_f32_space() before it writes,
 *     and nothing is ever refused.
 *   - THE END is said out loud. An empty ring cannot tell a slow producer
 *     from a finished one, so the producer calls dp_f32_close(). After that
 *     dp_f32_wait() returns NULL instead of spinning forever, and
 *     dp_f32_wait_status() says the NULL means "closed", not "interrupted".
 *     What was written before the close is still delivered first.
 *
 * Self-validating: the stream is a ramp, and every sample of every frame is
 * checked against its position.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_threaded_demo
 */
#include "doppler/buffer/buffer.h"
#include <pthread.h>
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\\n", __FILE__, __LINE__, #cond);   \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

enum
{
  FRAME = 1024,
  TOTAL = 50 * FRAME + 300 /* not a whole number of frames, on purpose */
};

static void *
producer (void *arg)
{
  dp_f32_t           *ring    = arg;
  static const size_t sizes[] = { 3000, 700, 4096, 129, 2048 };
  float               block[2 * 4096];
  size_t              sent = 0, turn = 0;

  while (sent < TOTAL)
    {
      size_t n = sizes[turn++ % 5];
      if (n > TOTAL - sent)
        n = TOTAL - sent;
      for (size_t k = 0; k < n; k++)
        {
          block[2 * k]     = (float)(sent + k); /* I = stream position */
          block[2 * k + 1] = 0.0f;
        }
      /* Wait for ROOM rather than retrying a refused write: the consumer
         can only free space, so once it is there it stays there. */
      while (dp_f32_space (ring) < n)
        ;
      dp_f32_write (ring, block, n);
      sent += n;
    }
  dp_f32_close (ring); /* the consumer's only way to tell slow from done */
  return NULL;
}

int
main (void)
{
  dp_f32_t *ring = dp_f32_create (8192);
  CHECK (ring != NULL);

  pthread_t tid;
  CHECK (pthread_create (&tid, NULL, producer, ring) == 0);

  size_t got = 0;
  float *frame;
  while ((frame = dp_f32_wait (ring, FRAME)) != NULL)
    {
      for (size_t k = 0; k < FRAME; k++)
        CHECK (frame[2 * k] == (float)(got + k));
      dp_f32_consume (ring, FRAME);
      got += FRAME;
    }

  /* NULL has more than one meaning; ask which. */
  CHECK (dp_f32_wait_status (ring, FRAME) == DP_WAIT_CLOSED);

  /* The tail is under a frame but it is still there, and still in order. */
  size_t tail = dp_f32_available (ring);
  CHECK (tail == TOTAL % FRAME);
  float *rest = dp_f32_peek (ring, tail);
  CHECK (rest != NULL);
  for (size_t k = 0; k < tail; k++)
    CHECK (rest[2 * k] == (float)(got + k));
  dp_f32_consume (ring, tail);

  pthread_join (tid, NULL);
  CHECK (ring->dropped == 0);
  printf ("threaded: %d samples -> %zu frames of %d + a tail of %zu, "
          "0 refused, ended by close()\n",
          TOTAL, got / FRAME, FRAME, tail);
  dp_f32_destroy (ring);
  return 0;
}
