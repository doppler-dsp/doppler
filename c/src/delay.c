/**
 * @file delay.c
 * @brief Dual-buffer circular delay line for cf64 IQ samples.
 *
 * Internal layout:  buf[0 .. 2*capacity-1]
 *
 *   buf:  [ lower half (0..cap-1) | upper half (cap..2cap-1) ]
 *
 * Every push writes the new sample to buf[head] AND buf[head+capacity].
 * The invariant guarantees buf[head .. head+num_taps-1] is always a
 * valid, contiguous read window — the reader never needs modulo.
 *
 * head decrements with a bitmask (capacity is a power of two), so
 * ptr[0] is always the most-recently pushed sample and ptr[num_taps-1]
 * is the oldest sample in the window.
 */

#include "dp/delay.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Internal struct
 * ========================================================================= */

struct dp_delay_cf64
{
  dp_cf64_t *buf;      /* 2 × capacity elements, heap-allocated */
  size_t     head;     /* index of most-recent sample           */
  size_t     capacity; /* next power of two ≥ num_taps          */
  size_t     num_taps; /* number of taps the caller requested   */
  size_t     mask;     /* capacity - 1 (bitmask for wrap)       */
};

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

dp_delay_cf64_t *
dp_delay_cf64_create (size_t num_taps)
{
  if (num_taps == 0)
    return NULL;

  /* Round up to next power of two */
  size_t capacity = 1;
  while (capacity < num_taps)
    capacity <<= 1;

  dp_delay_cf64_t *dl = (dp_delay_cf64_t *)malloc (sizeof (*dl));
  if (!dl)
    return NULL;

  dl->buf = (dp_cf64_t *)calloc (2 * capacity, sizeof (dp_cf64_t));
  if (!dl->buf)
    {
      free (dl);
      return NULL;
    }

  dl->head     = 0;
  dl->capacity = capacity;
  dl->num_taps = num_taps;
  dl->mask     = capacity - 1;
  return dl;
}

void
dp_delay_cf64_destroy (dp_delay_cf64_t *dl)
{
  if (!dl)
    return;
  free (dl->buf);
  free (dl);
}

void
dp_delay_cf64_reset (dp_delay_cf64_t *dl)
{
  memset (dl->buf, 0, 2 * dl->capacity * sizeof (dp_cf64_t));
  dl->head = 0;
}

/* =========================================================================
 * Properties
 * ========================================================================= */

size_t
dp_delay_cf64_num_taps (const dp_delay_cf64_t *dl)
{
  return dl->num_taps;
}

size_t
dp_delay_cf64_capacity (const dp_delay_cf64_t *dl)
{
  return dl->capacity;
}

/* =========================================================================
 * Hot path
 * ========================================================================= */

void
dp_delay_cf64_push (dp_delay_cf64_t *dl, dp_cf64_t x)
{
  /* Decrement head first so ptr[0] == the sample we are about to write */
  dl->head                       = (dl->head - 1) & dl->mask;
  dl->buf[dl->head]               = x;
  dl->buf[dl->head + dl->capacity] = x;
}

const dp_cf64_t *
dp_delay_cf64_ptr (const dp_delay_cf64_t *dl)
{
  return &dl->buf[dl->head];
}

const dp_cf64_t *
dp_delay_cf64_push_ptr (dp_delay_cf64_t *dl, dp_cf64_t x)
{
  dp_delay_cf64_push (dl, x);
  return dp_delay_cf64_ptr (dl);
}

void
dp_delay_cf64_write (dp_delay_cf64_t *dl,
                     const dp_cf64_t *in,
                     size_t           n)
{
  for (size_t i = 0; i < n; i++)
    dp_delay_cf64_push (dl, in[i]);
}
