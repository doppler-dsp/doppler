/*
 * interleaver_core.c — the object over dp_interleave.h.
 *
 * Every method here is argument checking plus one call into the header. That
 * is the whole point of the split: the permutation exists once, and the
 * object exists to hold the geometry so a caller cannot accidentally hand
 * the two ends of a link different numbers.
 */
#include "interleaver/interleaver_core.h"

#include <stdlib.h>

/* Would `rows * cols * unit_bits` overflow? Checked by division rather than
   by multiplying and looking at the result, which is the check overflowing. */
static int
block_overflows (size_t rows, size_t cols, size_t unit_bits)
{
  if (rows > (size_t)-1 / cols)
    return 1;
  return (rows * cols) > (size_t)-1 / unit_bits;
}

interleaver_state_t *
interleaver_create (size_t rows, size_t cols, size_t unit_bits)
{
  if (rows == 0 || cols == 0 || unit_bits == 0)
    return NULL;
  if (block_overflows (rows, cols, unit_bits))
    return NULL;
  interleaver_state_t *s = (interleaver_state_t *)calloc (1, sizeof *s);
  if (!s)
    return NULL;
  s->rows      = rows;
  s->cols      = cols;
  s->unit_bits = unit_bits;
  return s;
}

/* The receive face. One line, delegating -- a view needs a create_fn of its
   own and this is the whole of the difference, which is the point: a
   Deinterleaver is the same object under the name the receive side looks for,
   not a second one that could disagree about the geometry. */
interleaver_state_t *
interleaver_create_rx (size_t rows, size_t cols, size_t unit_bits)
{
  return interleaver_create (rows, cols, unit_bits);
}

void
interleaver_destroy (interleaver_state_t *state)
{
  free (state);
}

void
interleaver_reset (interleaver_state_t *state)
{
  (void)state; /* nothing is carried; see the header */
}

size_t
interleaver_get_block_bits (const interleaver_state_t *state)
{
  return state ? state->rows * state->cols * state->unit_bits : 0u;
}

size_t
interleaver_get_burst_len (const interleaver_state_t *state)
{
  return state ? state->rows : 0u;
}

size_t
interleaver_get_separation (const interleaver_state_t *state)
{
  return state ? state->cols : 0u;
}

size_t
interleaver_interleave_max_out (const interleaver_state_t *state, size_t n_in)
{
  (void)state;
  return n_in; /* a permutation is length-preserving */
}

size_t
interleaver_deinterleave_max_out (const interleaver_state_t *state,
                                  size_t                     n_in)
{
  return interleaver_interleave_max_out (state, n_in);
}

size_t
interleaver_deinterleave_soft_max_out (const interleaver_state_t *state,
                                       size_t                     n_in)
{
  return interleaver_interleave_max_out (state, n_in);
}

/* The one guard all three transforms share: a non-zero whole number of
   blocks that fits the caller's buffer. Returns the block count, or 0 for a
   refusal -- and a refusal it is, never a truncation to the largest whole
   number of blocks, because silently processing less than it was given is
   how a frame comes back short with no error anywhere. */
static size_t
whole_blocks (const interleaver_state_t *state, size_t n_in, size_t max_out)
{
  const size_t blk = interleaver_get_block_bits (state);
  if (blk == 0 || n_in == 0 || max_out < n_in)
    return 0;
  if (n_in % blk != 0)
    return 0;
  return n_in / blk;
}

size_t
interleaver_interleave (interleaver_state_t *state, const uint8_t *in,
                        size_t n_in, uint8_t *out, size_t max_out)
{
  if (!state || !in || !out)
    return 0;
  const size_t nblk = whole_blocks (state, n_in, max_out);
  if (nblk == 0)
    return 0;
  const size_t blk = interleaver_get_block_bits (state);
  for (size_t b = 0; b < nblk; b++)
    dp_interleave_u8 (in + b * blk, out + b * blk, state->rows, state->cols,
                      state->unit_bits);
  return n_in;
}

size_t
interleaver_deinterleave (interleaver_state_t *state, const uint8_t *in,
                          size_t n_in, uint8_t *out, size_t max_out)
{
  if (!state || !in || !out)
    return 0;
  const size_t nblk = whole_blocks (state, n_in, max_out);
  if (nblk == 0)
    return 0;
  const size_t blk = interleaver_get_block_bits (state);
  for (size_t b = 0; b < nblk; b++)
    dp_deinterleave_u8 (in + b * blk, out + b * blk, state->rows, state->cols,
                        state->unit_bits);
  return n_in;
}

size_t
interleaver_deinterleave_soft (interleaver_state_t *state, const float *in,
                               size_t n_in, float *out, size_t max_out)
{
  if (!state || !in || !out)
    return 0;
  const size_t nblk = whole_blocks (state, n_in, max_out);
  if (nblk == 0)
    return 0;
  const size_t blk = interleaver_get_block_bits (state);
  for (size_t b = 0; b < nblk; b++)
    dp_deinterleave_f32 (in + b * blk, out + b * blk, state->rows, state->cols,
                         state->unit_bits);
  return n_in;
}
