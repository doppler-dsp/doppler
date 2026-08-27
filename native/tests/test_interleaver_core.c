/**
 * @file test_interleaver_core.c
 * @brief What the Interleaver OBJECT adds over the permutation.
 *
 * dp_test.h, not the jm_test.h the scaffold came with: that header is
 * gitignored, so it exists only on a machine that has run `jm apply` and a
 * test including it builds here and fails in CI. `make lint`'s tests-ssot
 * gate caught it, which is the gate working.
 */
#include "dp_test.h"
#include "interleaver/interleaver_core.h"

#include <stdlib.h>
#include <string.h>

/* The object's own claims. The PERMUTATION is dp_interleave.h's and is tested
 * in test_dp_interleave.c against its index map; this file tests what the
 * object adds -- the held geometry, the whole-block contract, and the
 * refusals -- and does NOT re-test the permutation, because a second copy of
 * those assertions would pass while the object called the wrong kernel. What
 * it does check is that the object AGREES with the header it wraps. */

/* create() refuses a geometry that cannot describe a block. Each of the three
 * is checked alone, because a guard testing only the product would accept
 * rows=0 with cols=0 and unit_bits=0 as "zero" once and miss two of them. */
static int
test_create_refuses_a_degenerate_geometry (void)
{
  DP_CHECK (interleaver_create (0, 4, 1) == NULL);
  DP_CHECK (interleaver_create (4, 0, 1) == NULL);
  DP_CHECK (interleaver_create (4, 4, 0) == NULL);
  /* and the overflow guard, which is a division rather than a multiply --
     multiplying to test for overflow is the test overflowing */
  DP_CHECK (interleaver_create ((size_t)-1, 2, 1) == NULL);
  DP_CHECK (interleaver_create (2, (size_t)-1, 1) == NULL);
  DP_CHECK (interleaver_create ((size_t)1 << 40, (size_t)1 << 40, 8) == NULL);
  return 0;
}

/* The geometry read-backs are the link budget, so they are what a caller
 * sizes a code against. */
static int
test_geometry_readbacks (void)
{
  interleaver_state_t *il = interleaver_create (8, 32, 8);
  DP_REQUIRE (il != NULL);
  DP_CHECK (il->rows == 8 && il->cols == 32 && il->unit_bits == 8);
  DP_CHECK (interleaver_get_block_bits (il) == 8u * 32u * 8u);
  DP_CHECK (interleaver_get_burst_len (il) == 8);
  DP_CHECK (interleaver_get_separation (il) == 32);
  /* max_out is the identity: a permutation neither adds nor removes */
  DP_CHECK (interleaver_interleave_max_out (il, 2048) == 2048);
  DP_CHECK (interleaver_deinterleave_max_out (il, 2048) == 2048);
  DP_CHECK (interleaver_deinterleave_soft_max_out (il, 2048) == 2048);
  interleaver_destroy (il);
  return 0;
}

/* MANY blocks in one call, which is the case a single-block test cannot see:
 * a kernel that ignored the block loop would round-trip one block perfectly
 * and permute across the boundary of two. */
static int
test_round_trips_over_several_blocks (void)
{
  interleaver_state_t *il = interleaver_create (4, 6, 2);
  DP_REQUIRE (il != NULL);
  const size_t blk = interleaver_get_block_bits (il); /* 48 */
  const size_t n   = blk * 5;
  uint8_t     *in = malloc (n), *mid = malloc (n), *back = malloc (n);
  DP_REQUIRE (in && mid && back);
  for (size_t i = 0; i < n; i++)
    in[i] = (uint8_t)(i * 31u + 7u);

  DP_CHECK (interleaver_interleave (il, in, n, mid, n) == n);
  DP_CHECK (interleaver_deinterleave (il, mid, n, back, n) == n);
  DP_CHECK (memcmp (back, in, n) == 0);
  DP_CHECK (memcmp (mid, in, n) != 0); /* something actually moved */

  /* Each block is permuted WITHIN itself: block b of the output depends on
     block b of the input and on nothing else. Checked by interleaving one
     block alone and finding it at the same offset. */
  uint8_t one[48];
  DP_CHECK (interleaver_interleave (il, in + 2 * blk, blk, one, blk) == blk);
  DP_CHECK (memcmp (one, mid + 2 * blk, blk) == 0);

  free (in);
  free (mid);
  free (back);
  interleaver_destroy (il);
  return 0;
}

/* The soft path -- what a receiver uses on LLRs. Exact, because the transform
 * only moves values; if this ever needs a tolerance something is arithmetic
 * that should not be. */
static int
test_soft_deinterleave_matches_the_hard_one (void)
{
  interleaver_state_t *il = interleaver_create (5, 7, 1);
  DP_REQUIRE (il != NULL);
  const size_t n = 35;
  uint8_t      hard_in[35], hard_out[35];
  float        soft_in[35], soft_out[35];
  for (size_t i = 0; i < n; i++)
    {
      hard_in[i] = (uint8_t)(i * 13u % 251u);
      soft_in[i] = (float)hard_in[i];
    }
  DP_CHECK (interleaver_deinterleave (il, hard_in, n, hard_out, n) == n);
  DP_CHECK (interleaver_deinterleave_soft (il, soft_in, n, soft_out, n) == n);
  /* Same permutation, two element types -- the property that makes the soft
     path usable with a description written for the hard one. */
  for (size_t i = 0; i < n; i++)
    DP_CHECK (soft_out[i] == (float)hard_out[i]);
  interleaver_destroy (il);
  return 0;
}

/* A partial block is REFUSED, not padded and not truncated to the whole
 * blocks it could do. Truncating is the dangerous one: it returns a plausible
 * shorter frame and says nothing. */
static int
test_a_partial_block_is_refused (void)
{
  interleaver_state_t *il = interleaver_create (3, 4, 1);
  DP_REQUIRE (il != NULL);
  uint8_t      in[24] = { 0 }, out[24] = { 0 };
  const size_t blk = interleaver_get_block_bits (il); /* 12 */

  DP_CHECK (interleaver_interleave (il, in, blk, out, blk) == blk); /* one */
  DP_CHECK (interleaver_interleave (il, in, 2 * blk, out, 2 * blk)
            == 2 * blk);                                             /* two */
  DP_CHECK (interleaver_interleave (il, in, blk + 1, out, 24) == 0); /* 1.08 */
  DP_CHECK (interleaver_interleave (il, in, blk - 1, out, 24) == 0); /* 0.92 */
  DP_CHECK (interleaver_interleave (il, in, 0, out, 24) == 0);       /* none */
  /* and an output with no room for the whole input */
  DP_CHECK (interleaver_interleave (il, in, blk, out, blk - 1) == 0);
  /* every method refuses identically -- a guard on one of three is the shape
     where the soft path silently keeps working after the hard one is fixed */
  DP_CHECK (interleaver_deinterleave (il, in, blk + 1, out, 24) == 0);
  float sin_[24] = { 0 }, sout[24] = { 0 };
  DP_CHECK (interleaver_deinterleave_soft (il, sin_, blk + 1, sout, 24) == 0);
  /* NULL is refused rather than dereferenced */
  DP_CHECK (interleaver_interleave (il, NULL, blk, out, blk) == 0);
  DP_CHECK (interleaver_interleave (il, in, blk, NULL, blk) == 0);
  DP_CHECK (interleaver_interleave (NULL, in, blk, out, blk) == 0);
  interleaver_destroy (il);
  return 0;
}

/* reset() is a documented no-op, and the object must still work after one.
 * A reset that quietly cleared the GEOMETRY would leave every later call
 * refusing, which is the failure a "does nothing" function can still have. */
static int
test_reset_changes_nothing (void)
{
  interleaver_state_t *il = interleaver_create (2, 3, 1);
  DP_REQUIRE (il != NULL);
  uint8_t in[6] = { 1, 2, 3, 4, 5, 6 }, a[6], b[6];
  DP_CHECK (interleaver_interleave (il, in, 6, a, 6) == 6);
  interleaver_reset (il);
  DP_CHECK (interleaver_get_block_bits (il) == 6);
  DP_CHECK (interleaver_interleave (il, in, 6, b, 6) == 6);
  DP_CHECK (memcmp (a, b, 6) == 0);
  interleaver_destroy (il);
  return 0;
}

int
main (void)
{
  if (test_create_refuses_a_degenerate_geometry ())
    return 1;
  if (test_geometry_readbacks ())
    return 1;
  if (test_round_trips_over_several_blocks ())
    return 1;
  if (test_soft_deinterleave_matches_the_hard_one ())
    return 1;
  if (test_a_partial_block_is_refused ())
    return 1;
  if (test_reset_changes_nothing ())
    return 1;

  /* destroy(NULL) is a no-op, like free() */
  interleaver_destroy (NULL);

  DP_TEST_END ("test_interleaver_core");
}
