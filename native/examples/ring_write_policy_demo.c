/**
 * ring_write_policy_demo.c — two ways to write, and what each promises.
 *
 * dp_f32_write() is ALL-OR-NOTHING: a block that does not fit is refused
 * whole. Nothing is copied, the caller still holds every sample, and the
 * refusal is counted in ->dropped -- which is therefore a count of refused
 * samples, not of lost ones. Samples are lost only if the caller then
 * throws them away.
 *
 * dp_f32_write_some() takes what fits and says how much. It never refuses,
 * so it never counts a drop, and it is the only way to feed a block larger
 * than the ring.
 *
 * dp_f32_space() is the room a write is guaranteed to find, so a producer
 * that sizes its block from it is never refused at all. And
 * dp_f32_wait_status() answers "why can I not have n samples?" without
 * blocking, in one call, for a caller that wants to say so.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_write_policy_demo
 */
#include "buffer/buffer.h"
#include <stdio.h>
#include <stdlib.h>

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

int
main (void)
{
  dp_f32_t *ring = dp_f32_create (1024);
  CHECK (ring != NULL);
  size_t cap = ring->capacity; /* rounded UP from the request */
  CHECK (cap >= 1024);

  float *block = calloc (2 * (cap + 1), sizeof *block); /* I/Q interleaved */
  CHECK (block != NULL);

  /* An empty ring has room for exactly its capacity. */
  CHECK (dp_f32_space (ring) == cap);
  CHECK (dp_f32_available (ring) == 0);

  /* One sample too many: refused WHOLE, and counted. */
  CHECK (!dp_f32_write (ring, block, cap + 1));
  CHECK (dp_f32_available (ring) == 0); /* nothing was copied       */
  CHECK (ring->dropped == cap + 1);     /* refused, not lost        */

  /* The same block through write_some: the ring takes what fits. */
  CHECK (dp_f32_write_some (ring, block, cap + 1) == cap);
  CHECK (dp_f32_space (ring) == 0);
  CHECK (dp_f32_write_some (ring, block, 8) == 0); /* full: 0, not an error */
  CHECK (ring->dropped == cap + 1);                /* write_some never drops */

  /* Why can I not have n samples? Asked without blocking. */
  CHECK (dp_f32_wait_status (ring, cap) == DP_WAIT_OK);
  CHECK (dp_f32_wait_status (ring, cap + 1) == DP_WAIT_TOO_LARGE);
  dp_f32_consume (ring, cap);
  CHECK (dp_f32_wait_status (ring, 1) == DP_WAIT_PENDING); /* just not yet */

  /* A producer that sizes from space() is never refused. */
  size_t before = ring->dropped;
  for (int k = 0; k < 100; k++)
    {
      size_t n = dp_f32_space (ring) < 300 ? dp_f32_space (ring) : 300;
      CHECK (dp_f32_write (ring, block, n));
      if (dp_f32_space (ring) == 0)
        dp_f32_consume (ring, dp_f32_available (ring));
    }
  CHECK (ring->dropped == before);

  /* close() says no more is coming; what is there can still be read. */
  dp_f32_close (ring);
  CHECK (dp_f32_closed (ring));
  size_t left = dp_f32_available (ring);
  CHECK (dp_f32_wait_status (ring, left + 1) == DP_WAIT_CLOSED);
  CHECK (dp_f32_peek (ring, left) != NULL || left == 0);

  /* reset() empties AND reopens, so the mapping carries a second stream.
     ->dropped is a lifetime count and is kept. */
  dp_f32_reset (ring);
  CHECK (!dp_f32_closed (ring));
  CHECK (dp_f32_available (ring) == 0 && dp_f32_space (ring) == cap);
  CHECK (ring->dropped == before);

  printf ("write policy: capacity %zu, %zu samples refused by write(), "
          "0 by write_some(), 0 when sized from space()\n",
          cap, (size_t)ring->dropped);
  free (block);
  dp_f32_destroy (ring);
  return 0;
}
