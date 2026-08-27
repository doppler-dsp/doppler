/**
 * @file test_dp_interleave.c
 * @brief What dp_interleave.h claims, checked — including the claim that
 * matters, which is not the round trip.
 *
 * A round-trip test proves the permutation is invertible and proves nothing
 * about why anyone wants one. The property an interleaver exists for is that
 * a BURST of consecutive errors on the wire arrives at the decoder spread
 * out, and it is pinned here as a measured separation rather than asserted
 * in the header's prose.
 *
 * @note Uses dp_test.h, not `assert`: doppler builds Release, Release
 * defines NDEBUG, and NDEBUG compiles `assert` away.
 */
#include "dp_interleave.h"
#include "dp_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── the permutation itself ──────────────────────────────────────────── */

/* Write by rows, read by columns, spelled out on a 3x4 block small enough to
 * check by eye. Input 0..11 written as
 *
 *      0  1  2  3
 *      4  5  6  7
 *      8  9 10 11
 *
 * read by columns is 0 4 8 1 5 9 2 6 10 3 7 11. If the index map is ever
 * "simplified" into the transpose of this, every other test in the file
 * still passes -- a wrong permutation is still a permutation, and still
 * inverts. This literal is the only thing that pins the direction. */
static void
test_index_map_is_write_rows_read_columns (void)
{
  static const size_t want[12] = { 0, 3, 6, 9, 1, 4, 7, 10, 2, 5, 8, 11 };
  for (size_t i = 0; i < 12; i++)
    DP_CHECK (dp_interleave_index (i, 3, 4) == want[i]);

  /* And the output, read in order, is the column-major read above. */
  static const uint8_t in[12]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  static const uint8_t col[12] = { 0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11 };
  uint8_t              out[12];
  dp_interleave_u8 (in, out, 3, 4, 1);
  DP_CHECK (memcmp (out, col, sizeof col) == 0);
}

/* The inverse is the forward map with the two arguments exchanged. The
 * header states it as a fact about the transform; this is what makes it one,
 * over every index of a non-square block in both directions. */
static void
test_inverse_is_the_forward_map_transposed (void)
{
  const size_t rows = 5, cols = 7;
  for (size_t i = 0; i < rows * cols; i++)
    {
      DP_CHECK (dp_deinterleave_index (i, rows, cols)
                == dp_interleave_index (i, cols, rows));
      /* and it really is the inverse */
      DP_CHECK (dp_deinterleave_index (dp_interleave_index (i, rows, cols),
                                       rows, cols)
                == i);
    }
}

/* A square block is its own inverse -- the corollary the header draws, and a
 * case a caller can accidentally rely on, so it is pinned rather than left
 * to follow. */
static void
test_a_square_block_is_its_own_inverse (void)
{
  const size_t n = 6 * 6;
  uint8_t      in[36], once[36], twice[36];
  for (size_t i = 0; i < n; i++)
    in[i] = (uint8_t)(i * 7u + 3u);
  dp_interleave_u8 (in, once, 6, 6, 1);
  dp_interleave_u8 (once, twice, 6, 6, 1);
  DP_CHECK (memcmp (twice, in, n) == 0);
  /* non-square is NOT, or the test above proves nothing */
  uint8_t a[24], b[24];
  dp_interleave_u8 (in, a, 4, 6, 1);
  dp_interleave_u8 (a, b, 4, 6, 1);
  DP_CHECK (memcmp (b, in, 24) != 0);
}

/* ── round trips, over every element type and unit ───────────────────── */

static void
test_round_trip_bits_and_octets (void)
{
  const size_t rows = 8, cols = 31; /* coprime, and neither a power of two */
  for (size_t unit = 1; unit <= 8; unit++)
    {
      const size_t n  = rows * cols * unit;
      uint8_t     *in = malloc (n), *mid = malloc (n), *back = malloc (n);
      DP_CHECK (in && mid && back);
      if (in && mid && back)
        {
          for (size_t i = 0; i < n; i++)
            in[i] = (uint8_t)(i * 31u + unit);
          dp_interleave_u8 (in, mid, rows, cols, unit);
          dp_deinterleave_u8 (mid, back, rows, cols, unit);
          DP_CHECK (memcmp (back, in, n) == 0);
          /* the permutation actually moved something */
          DP_CHECK (memcmp (mid, in, n) != 0);
        }
      free (in);
      free (mid);
      free (back);
    }
}

/* The soft path, which is the one dsss_burst_receiver's llrs need. Floats
 * round-trip EXACTLY because the transform only moves them -- if this ever
 * needs a tolerance, something is arithmetic that should not be. */
static void
test_round_trip_soft_is_bit_exact (void)
{
  const size_t rows = 4, cols = 9, unit = 1;
  const size_t n = rows * cols * unit;
  float        in[36], mid[36], back[36];
  for (size_t i = 0; i < n; i++)
    in[i] = (float)((double)i * -0.37 + 1.5);
  dp_interleave_f32 (in, mid, rows, cols, unit);
  dp_deinterleave_f32 (mid, back, rows, cols, unit);
  for (size_t i = 0; i < n; i++)
    DP_CHECK (back[i] == in[i]);
  DP_CHECK (memcmp (mid, in, sizeof in) != 0);
}

/* A unit larger than one keeps each unit's elements CONTIGUOUS and in order.
 * Interleaving octets must move octets, never shuffle the bits inside one --
 * that is the whole difference between spreading a burst across codewords
 * and spreading it inside a symbol that is already wrong. */
static void
test_a_unit_moves_whole_and_unreversed (void)
{
  const size_t rows = 3, cols = 4, unit = 8;
  uint8_t      in[96], out[96];
  for (size_t u = 0; u < rows * cols; u++)
    for (size_t b = 0; b < unit; b++)
      in[u * unit + b] = (uint8_t)(u * 10u + b);
  dp_interleave_u8 (in, out, rows, cols, unit);
  for (size_t u = 0; u < rows * cols; u++)
    {
      const size_t dst = dp_interleave_index (u, rows, cols);
      for (size_t b = 0; b < unit; b++)
        DP_CHECK (out[dst * unit + b] == (uint8_t)(u * 10u + b));
    }
}

/* ── the claim the interleaver exists for ────────────────────────────── */

/* A burst of `rows` consecutive errors on the wire must arrive with every
 * error in a DIFFERENT codeword, where a codeword is one row of `cols` input
 * units. That is the property an outer code consumes: it can correct t units
 * per codeword, so it survives a burst of t * rows once interleaved.
 *
 * Note what is NOT claimed. A first draft asserted a minimum index
 * separation of `cols` between the de-interleaved burst positions, and this
 * test refused it: a burst straddling a column boundary can put two errors
 * as close as `cols - 1` apart while still landing in different codewords.
 * The separation is a consequence that holds only inside one column; the
 * one-per-codeword invariant is the property, and it holds everywhere.
 *
 * Both halves are checked, because the good case means nothing without the
 * bad one: with no interleaver the same burst sits inside ONE codeword. */
static void
test_a_burst_hits_each_codeword_at_most_once (void)
{
  const size_t rows = 8, cols = 16;
  const size_t n = rows * cols;

  for (size_t start = 0; start + rows <= n; start++)
    {
      size_t hits_per_row[8] = { 0 };
      for (size_t k = 0; k < rows; k++)
        {
          const size_t src = dp_deinterleave_index (start + k, rows, cols);
          hits_per_row[src / cols]++;
        }
      for (size_t r = 0; r < rows; r++)
        DP_CHECK (hits_per_row[r] == 1);
    }

  /* The bad case, for contrast: un-interleaved, a burst of `rows` wire
     positions IS `rows` input positions, so it lands in one codeword
     whenever it does not straddle a boundary -- the case that costs a
     frame. */
  size_t bursts_inside_one_codeword = 0;
  for (size_t start = 0; start + rows <= n; start++)
    if (start / cols == (start + rows - 1) / cols)
      bursts_inside_one_codeword++;
  DP_CHECK (bursts_inside_one_codeword > 0);

  /* And the separation claim, stated where it actually holds: two burst
     positions inside ONE column are exactly `cols` apart. */
  for (size_t k = 0; k + 1 < rows; k++)
    {
      const size_t a = dp_deinterleave_index (k, rows, cols);
      const size_t b = dp_deinterleave_index (k + 1, rows, cols);
      DP_CHECK (b - a == cols);
    }
}

/* ── geometry ────────────────────────────────────────────────────────── */

static void
test_block_units (void)
{
  DP_CHECK (dp_interleave_block_units (8, 16) == 128);
  DP_CHECK (dp_interleave_block_units (1, 1) == 1);
  /* depth 1 is the identity: one row, read back in the order written */
  uint8_t in[5] = { 9, 8, 7, 6, 5 }, out[5];
  dp_interleave_u8 (in, out, 1, 5, 1);
  DP_CHECK (memcmp (out, in, 5) == 0);
  /* and so is span 1 */
  dp_interleave_u8 (in, out, 5, 1, 1);
  DP_CHECK (memcmp (out, in, 5) == 0);
}

/* A zero-sized unit writes nothing rather than misbehaving -- the header
 * says so, and a caller computing `unit` from a configuration can reach it
 * without meaning to. */
static void
test_zero_unit_writes_nothing (void)
{
  uint8_t out[4] = { 1, 2, 3, 4 };
  uint8_t in[4]  = { 9, 9, 9, 9 };
  dp_interleave_u8 (in, out, 2, 2, 0);
  DP_CHECK (out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4);
}

int
main (void)
{
  test_index_map_is_write_rows_read_columns ();
  test_inverse_is_the_forward_map_transposed ();
  test_a_square_block_is_its_own_inverse ();
  test_round_trip_bits_and_octets ();
  test_round_trip_soft_is_bit_exact ();
  test_a_unit_moves_whole_and_unreversed ();
  test_a_burst_hits_each_codeword_at_most_once ();
  test_block_units ();
  test_zero_unit_writes_nothing ();

  DP_TEST_END ("test_dp_interleave");
}
