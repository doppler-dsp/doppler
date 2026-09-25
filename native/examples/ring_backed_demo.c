/**
 * ring_backed_demo.c — a ring whose samples are a file.
 *
 * dp_f32_create_backed() is dp_f32_create() over a path instead of
 * anonymous memory. The mapping is shared, so the ring's samples ARE the
 * file's contents: there is no separate write-to-disk step and no copy,
 * and the two cannot disagree. That is what lets a history outlive the
 * process that recorded it.
 *
 * Two calls matter beyond the ordinary ones:
 *
 *   - `existed` says whether the file already held a ring of this size.
 *     1 means its samples are now this ring's; 0 means it was created (or
 *     resized) and is zeroed.
 *   - dp_f32_sync() flushes to disk. Until then the samples are in the
 *     page cache, where a crash can still lose them -- so call it where a
 *     checkpoint is taken, not after every write.
 *
 * What is persisted is the SAMPLES. The read and write positions live in
 * the ring's struct, not in the file, so a re-attached ring starts empty
 * over its old contents: a caller that wants to resume records where it
 * was, beside the file, and that bookkeeping is its own.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_backed_demo
 */
#include "doppler/buffer/buffer.h"
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

enum
{
  N = 4096
};

int
main (void)
{
  const char *dir = getenv ("TMPDIR");
  char        path[512];
  snprintf (path, sizeof path, "%s/dp_ring_backed_demo.bin",
            dir && *dir ? dir : "/tmp");
  remove (path); /* a clean start, so `existed` below means something */

  /* ── first process: record, checkpoint, go away ───────────────────── */
  int       existed = -1;
  dp_f32_t *ring    = dp_f32_create_backed (N, path, &existed);
  CHECK (ring != NULL);
  CHECK (existed == 0); /* created, and zeroed */
  size_t cap = ring->capacity;

  static float block[2 * N];
  for (size_t k = 0; k < N; k++)
    {
      block[2 * k]     = (float)k;
      block[2 * k + 1] = -(float)k;
    }
  CHECK (dp_f32_write (ring, block, N));
  dp_f32_sync (ring);    /* the checkpoint: now it is on disk */
  dp_f32_destroy (ring); /* unmaps; the FILE stays             */

  /* ── second process: the history is simply there ──────────────────── */
  ring = dp_f32_create_backed (N, path, &existed);
  CHECK (ring != NULL);
  CHECK (existed == 1); /* same size: mapped as it stands */
  CHECK (ring->capacity == cap);
  CHECK (dp_f32_available (ring) == 0); /* positions are NOT in the file */

  for (size_t k = 0; k < N; k++)
    {
      CHECK (ring->data[2 * k] == (float)k);
      CHECK (ring->data[2 * k + 1] == -(float)k);
    }

  printf ("backed: %d samples written, synced, unmapped -- and read back "
          "from %s by a fresh mapping\n",
          N, path);
  dp_f32_destroy (ring);
  remove (path);
  return 0;
}
