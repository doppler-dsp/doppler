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
  /* max_out is the identity: a permutation neither adds nor removes. Swept
     rather than pinned at one length, because the binding asks this to SIZE
     an output buffer and asks it for whatever length arrived -- including
     lengths the transform will then refuse. A max_out that returned the
     block size, or a rounded-up multiple, would satisfy a single literal at
     an exact multiple and under-size every other call. */
  static const size_t lens[] = { 0, 1, 7, 2047, 2048, 2049, 12345 };
  for (size_t i = 0; i < sizeof lens / sizeof *lens; i++)
    {
      DP_CHECK (interleaver_interleave_max_out (il, lens[i]) == lens[i]);
      DP_CHECK (interleaver_deinterleave_max_out (il, lens[i]) == lens[i]);
      DP_CHECK (interleaver_deinterleave_soft_max_out (il, lens[i])
                == lens[i]);
    }
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

/* ── what the object adds that nothing pinned ────────────────────────── */

/* The object must apply the KERNEL's permutation, at the geometry it holds.
 * Nothing above pinned that, and the hole is not academic: transposing the
 * object's three calls into dp_interleave.h -- `state->cols, state->rows`
 * for `state->rows, state->cols` -- left the ENTIRE C suite green, all 155
 * tests. A transposed block interleave is still a permutation, still
 * inverts, still permutes each block independently, and still moves
 * something, so every assertion in this file survived it.
 *
 * Closed by comparing against the kernel rather than against the object's
 * own inverse. dp_interleave.h is the definition and `test_dp_interleave.c`
 * pins that definition against a hand-checked literal; this is the missing
 * link between the two, and it is the assertion the file header's "it checks
 * that the object AGREES with the header it wraps" always claimed. */
static int
test_the_object_applies_the_kernels_permutation (void)
{
  /* Non-square, so the transposed geometry is a DIFFERENT permutation --
     over a square block this test would pass on a transposed object and
     prove nothing, which is the vacuous-reject shape. */
  const size_t         rows = 3, cols = 4;
  interleaver_state_t *bits = interleaver_create (rows, cols, 1);
  interleaver_state_t *oct  = interleaver_create (rows, cols, 8);
  DP_REQUIRE (bits && oct);

  uint8_t in[96], obj[96], ker[96], alt[96];
  for (size_t i = 0; i < sizeof in; i++)
    in[i] = (uint8_t)(i * 17u + 3u);

  /* Both units, because `unit_bits` is the argument the object holds on the
     caller's behalf and a wrong one is invisible to a round trip. */
  const size_t         units[2] = { 1, 8 };
  interleaver_state_t *st[2]    = { bits, oct };
  for (size_t u = 0; u < 2; u++)
    {
      const size_t unit = units[u];
      const size_t blk  = interleaver_get_block_bits (st[u]);
      DP_CHECK (blk == rows * cols * unit);

      DP_CHECK (interleaver_interleave (st[u], in, blk, obj, blk) == blk);
      dp_interleave_u8 (in, ker, rows, cols, unit);
      DP_CHECK (memcmp (obj, ker, blk) == 0);
      /* and the comparison DISCRIMINATES: the transposed geometry is a
         different answer, so agreeing with the kernel is a real constraint */
      dp_interleave_u8 (in, alt, cols, rows, unit);
      DP_CHECK (memcmp (obj, alt, blk) != 0);

      DP_CHECK (interleaver_deinterleave (st[u], in, blk, obj, blk) == blk);
      dp_deinterleave_u8 (in, ker, rows, cols, unit);
      DP_CHECK (memcmp (obj, ker, blk) == 0);
      dp_deinterleave_u8 (in, alt, cols, rows, unit);
      DP_CHECK (memcmp (obj, alt, blk) != 0);
    }

  /* The soft path is a third call site and gets its own comparison: a fix
     applied to two of three is the shape the refusal test already guards. */
  {
    const size_t blk = interleaver_get_block_bits (bits);
    float        sin_[12], sobj[12], sker[12], salt[12];
    for (size_t i = 0; i < blk; i++)
      sin_[i] = (float)((double)i * -0.375 + 1.25);
    DP_CHECK (interleaver_deinterleave_soft (bits, sin_, blk, sobj, blk)
              == blk);
    dp_deinterleave_f32 (sin_, sker, rows, cols, 1);
    DP_CHECK (memcmp (sobj, sker, blk * sizeof (float)) == 0);
    dp_deinterleave_f32 (sin_, salt, cols, rows, 1);
    DP_CHECK (memcmp (sobj, salt, blk * sizeof (float)) != 0);
  }

  interleaver_destroy (bits);
  interleaver_destroy (oct);
  return 0;
}

/* The RECEIVE face. `interleaver_create_rx` had zero mentions in this file:
 * the header claims identical construction and the same refusals, and the
 * only evidence for either was that one line of C reads like it. What makes
 * the claim matter is the link -- a transmitter's Interleaver and a
 * receiver's Deinterleaver are two objects that must agree about one
 * geometry, so it is checked ACROSS the two faces and not on one. */
static int
test_the_receive_face_is_the_same_geometry (void)
{
  const size_t         rows = 3, cols = 4, unit = 2;
  interleaver_state_t *tx = interleaver_create (rows, cols, unit);
  interleaver_state_t *rx = interleaver_create_rx (rows, cols, unit);
  DP_REQUIRE (tx && rx);

  /* the same three numbers, read back through the same accessors */
  DP_CHECK (interleaver_get_block_bits (rx)
            == interleaver_get_block_bits (tx));
  DP_CHECK (interleaver_get_burst_len (rx) == interleaver_get_burst_len (tx));
  DP_CHECK (interleaver_get_separation (rx)
            == interleaver_get_separation (tx));

  /* the same refusals -- a second constructor is a second place to forget
     them, which is exactly what a delegating one-liner exists to prevent */
  DP_CHECK (interleaver_create_rx (0, 4, 1) == NULL);
  DP_CHECK (interleaver_create_rx (4, 0, 1) == NULL);
  DP_CHECK (interleaver_create_rx (4, 4, 0) == NULL);
  DP_CHECK (interleaver_create_rx ((size_t)1 << 40, (size_t)1 << 40, 8)
            == NULL);

  /* and the link: what the transmit face interleaved, the receive face
     undoes. Non-square, so a receive face built on a transposed geometry
     fails here rather than silently agreeing. */
  const size_t blk = interleaver_get_block_bits (tx);
  uint8_t      in[24], wire[24], back[24];
  for (size_t i = 0; i < blk; i++)
    in[i] = (uint8_t)(i * 11u + 5u);
  DP_CHECK (interleaver_interleave (tx, in, blk, wire, blk) == blk);
  DP_CHECK (interleaver_deinterleave (rx, wire, blk, back, blk) == blk);
  DP_CHECK (memcmp (back, in, blk) == 0);
  DP_CHECK (memcmp (wire, in, blk) != 0); /* something moved to be undone */

  interleaver_destroy (tx);
  interleaver_destroy (rx);
  return 0;
}

/* The claim the object exists for, stated in the object's OWN vocabulary.
 *
 * `burst_len` and `separation` are advertised as a link budget -- an outer
 * code correcting t units per codeword survives a burst of t * burst_len --
 * and until now nothing tied those read-backs to the permutation the object
 * applies. `test_dp_interleave.c` proves the invariant over the index map;
 * this proves the object a caller holds delivers it, at unit_bits = 8, the
 * octet case the header sells.
 *
 * Three parts, because the good case alone is not evidence: the invariant
 * holds at `burst_len`, it FAILS at one more (a bound that never bites is
 * not a bound), and without the interleaver the same burst lands inside one
 * codeword. */
static int
test_a_burst_of_burst_len_hits_each_codeword_once (void)
{
  const size_t         rows = 5, cols = 7, unit = 8;
  interleaver_state_t *il = interleaver_create (rows, cols, unit);
  DP_REQUIRE (il != NULL);
  const size_t nunits = rows * cols;
  const size_t blk    = interleaver_get_block_bits (il);
  const size_t depth  = interleaver_get_burst_len (il);
  const size_t span   = interleaver_get_separation (il);
  DP_CHECK (depth == rows && span == cols && blk == nunits * unit);

  uint8_t wire[280], back[280]; /* 5 * 7 * 8 */
  DP_REQUIRE (blk == sizeof wire);

  /* A burst of `n` consecutive wire UNITS, marked on an otherwise clean
     block and de-interleaved: where each marked unit lands says which
     codeword pays for it. Every byte of a unit is marked, so a unit that
     arrived split would be visible too. */
  for (size_t n = depth; n <= depth + 1; n++)
    for (size_t start = 0; start + n <= nunits; start++)
      {
        memset (wire, 0, blk);
        for (size_t k = 0; k < n; k++)
          memset (wire + (start + k) * unit, 1, unit);
        DP_CHECK (interleaver_deinterleave (il, wire, blk, back, blk) == blk);

        size_t worst = 0;
        for (size_t r = 0; r < rows; r++)
          {
            size_t hits = 0;
            for (size_t c = 0; c < cols; c++)
              {
                const uint8_t *u = back + (r * cols + c) * unit;
                size_t         m = 0;
                for (size_t b = 0; b < unit; b++)
                  m += u[b];
                DP_CHECK (m == 0 || m == unit); /* whole units, never split */
                hits += (m == unit);
              }
            if (hits > worst)
              worst = hits;
          }
        /* at burst_len, one per codeword; at one more, some codeword takes
           two -- the bound is exact, not conservative */
        DP_CHECK (n == depth ? worst == 1 : worst == 2);
      }

  /* The control. Un-interleaved, a burst of `burst_len` consecutive units IS
     `burst_len` consecutive input units, so it sits inside one codeword
     whenever it does not straddle a boundary -- the case that costs a frame,
     and the reason the object is applied at all. */
  size_t worst_without = 0;
  for (size_t start = 0; start + depth <= nunits; start++)
    {
      size_t hits = 0;
      for (size_t k = 0; k < depth; k++)
        hits += ((start + k) / cols == start / cols);
      if (hits > worst_without)
        worst_without = hits;
    }
  DP_CHECK (worst_without == depth);

  interleaver_destroy (il);
  return 0;
}

/* "Nothing survives between calls" -- the sentence that exempts this object
 * from the state-serialization standard every other stateful object obeys.
 * An object that quietly carried a block counter would still round-trip,
 * still refuse a partial block and still permute each block within itself;
 * what it would do is answer DIFFERENTLY for the same input depending on
 * what ran before it. That is the only observable the claim has, so it is
 * the one asserted -- across a call, a refused call, and a reset. */
static int
test_nothing_survives_between_calls (void)
{
  interleaver_state_t *fresh = interleaver_create (4, 6, 2);
  interleaver_state_t *used  = interleaver_create (4, 6, 2);
  DP_REQUIRE (fresh && used);
  const size_t blk = interleaver_get_block_bits (fresh);
  uint8_t      a[48], b[48], first[48], again[48], scratch[48];
  for (size_t i = 0; i < blk; i++)
    {
      a[i] = (uint8_t)(i * 7u + 1u);
      b[i] = (uint8_t)(i * 13u + 9u);
    }

  DP_CHECK (interleaver_interleave (fresh, b, blk, first, blk) == blk);

  /* the same input through an object that has already done work: a
     successful call, a REFUSED one, a de-interleave, and a reset */
  DP_CHECK (interleaver_interleave (used, a, blk, scratch, blk) == blk);
  DP_CHECK (interleaver_interleave (used, a, blk - 1, scratch, blk) == 0);
  DP_CHECK (interleaver_deinterleave (used, a, blk, scratch, blk) == blk);
  interleaver_reset (used);
  DP_CHECK (interleaver_interleave (used, b, blk, again, blk) == blk);
  DP_CHECK (memcmp (again, first, blk) == 0);

  /* and repeating on the SAME object is idempotent, which is what a caller
     re-sending a frame relies on */
  DP_CHECK (interleaver_interleave (used, b, blk, scratch, blk) == blk);
  DP_CHECK (memcmp (scratch, first, blk) == 0);

  interleaver_destroy (fresh);
  interleaver_destroy (used);
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
  if (test_the_object_applies_the_kernels_permutation ())
    return 1;
  if (test_the_receive_face_is_the_same_geometry ())
    return 1;
  if (test_a_burst_of_burst_len_hits_each_codeword_once ())
    return 1;
  if (test_nothing_survives_between_calls ())
    return 1;

  /* destroy(NULL) is a no-op, like free() */
  interleaver_destroy (NULL);

  DP_TEST_END ("test_interleaver_core");
}
