/*
 * test_syncword_core.c — the general marker search, and its threshold.
 *
 * `test_ccsds_tm_asm.c` already holds the search to the CCSDS marker: found
 * where it was put, at the last legal offset, in either polarity, first
 * below threshold rather than best, and never invented in random data. Since
 * `ccsds_tm_asm_find` is now `dp_syncword_find` with `0x1ACFFC1D` in it,
 * repeating those here would be two files agreeing about one kernel.
 *
 * What only THIS component can be wrong about is the two things the CCSDS
 * test cannot see:
 *
 * 1. **Generality.** Every case over there is 32 bits long, so a search that
 *    hardcoded 32 -- in the loop bound, or in the `n - d` that turns a
 *    distance into its complement's -- would pass all of it. The marker
 *    lengths below are deliberately odd, short and long.
 * 2. **The arithmetic beside the search.** `pfa` and `max_errors_for` are
 *    the answer to doppler#897, and nothing measured them. They are checked
 *    against an EXHAUSTIVE enumeration rather than against a second copy of
 *    the closed form, and then against the search itself: a formula that
 *    disagrees with the detector it is meant to size is worse than no
 *    formula, because a caller would trust it.
 */
#define _GNU_SOURCE
#include "dp_rng_test.h"
#include "dp_test.h"

#include "syncword/syncword_core.h"

#include <stdlib.h>
#include <string.h>

/* Hamming distance between two bytes read as 8 independent bits. Written as
 * a loop over bits rather than a popcount intrinsic so it shares nothing
 * with the kernel it is the oracle for. */
static unsigned
hamming8 (unsigned a, unsigned b)
{
  unsigned d = 0u, x = (a ^ b) & 0xFFu;
  for (unsigned i = 0; i < 8u; i++)
    d += (x >> i) & 1u;
  return d;
}

/* Expand a byte to 8 unpacked bits, MSB first. */
static void
byte_bits (unsigned v, uint8_t *out)
{
  for (unsigned i = 0; i < 8u; i++)
    out[i] = (uint8_t)((v >> (7u - i)) & 1u);
}

int
main (void)
{
  /* ── 1. the refusals ──────────────────────────────────────────────────────
   *
   * An empty marker matches every offset with zero errors, so a searcher
   * built from one would report frame sync immediately and forever. That is
   * not a degenerate search but a wrong one, and it is refused where a
   * caller can see it rather than at every find.
   */
  {
    DP_CHECK_MSG (syncword_create (NULL, 0) == NULL,
                  "a NULL marker must be refused");
    uint8_t m = 1u;
    DP_CHECK_MSG (syncword_create (&m, 0) == NULL,
                  "an empty marker must be refused -- it matches everywhere");
  }

  /* ── 2. the marker is COPIED ──────────────────────────────────────────────
   *
   * The header promises a searcher outlives the array it was built from,
   * which is what lets `SyncFinder(ccsds_asm_bits())` work at all: numpy
   * frees that temporary the moment the constructor returns. A searcher
   * holding the caller's pointer would search whatever landed there next,
   * and would do it silently.
   */
  {
    uint8_t m[13] = { 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0 };
    uint8_t keep[13];
    memcpy (keep, m, sizeof m);

    syncword_state_t *f = syncword_create (m, sizeof m);
    DP_REQUIRE (f != NULL);
    memset (m, 0, sizeof m); /* the caller reuses their buffer */

    uint8_t bits[60] = { 0 };
    memcpy (bits + 21, keep, sizeof keep);
    syncword_hit_t h = syncword_find (f, bits, sizeof bits, 0u);
    DP_CHECK_MSG (h.found && h.offset == 21u,
                  "the searcher must still hold the ORIGINAL marker");
    DP_CHECK (syncword_pfa (f, 0u) > 0.0); /* nbits survived too */
    syncword_destroy (f);
  }

  /* ── 3. a miss is an answer, not an absence ───────────────────────────────
   *
   * The record carries `found` rather than spelling a miss as a sentinel
   * offset, because offset 0 is a perfectly good place for a marker to be.
   * A caller reading `offset` without `found` must get something that is
   * obviously not a location.
   */
  {
    uint8_t           m[9] = { 1, 1, 0, 1, 0, 0, 1, 0, 1 };
    syncword_state_t *f    = syncword_create (m, sizeof m);
    DP_REQUIRE (f != NULL);

    uint8_t        bits[40] = { 0 };
    syncword_hit_t h        = syncword_find (f, bits, sizeof bits, 0u);
    DP_CHECK_MSG (!h.found, "a marker that is not there must not be found");
    DP_CHECK_MSG (h.offset == 0u && h.inverted == 0 && h.errors == 0u,
                  "a miss must return the record zeroed, not partly filled");

    /* Shorter than the marker: nothing to correlate against. */
    h = syncword_find (f, bits, 8u, 32u);
    DP_CHECK_MSG (!h.found, "a run shorter than the marker cannot hold one");
    syncword_destroy (f);
  }

  /* ── 4. polarity, at a length that is not 32 ──────────────────────────────
   *
   * The kernel derives an inverted match's distance as `n_marker - d`. Every
   * CCSDS case is 32 bits, so a hardcoded 32 there is invisible: it would
   * report an inverted 9-bit marker as having `32 - d` errors, which for a
   * clean inverted match is 23 -- outside every tolerance a caller would set,
   * so the frame is simply lost and nothing says why.
   */
  {
    uint8_t           m[9] = { 1, 1, 0, 1, 0, 0, 1, 0, 1 };
    syncword_state_t *f    = syncword_create (m, sizeof m);
    DP_REQUIRE (f != NULL);

    uint8_t bits[50];
    for (size_t i = 0; i < sizeof bits; i++)
      bits[i] = 1u; /* complement of the zero background */
    for (size_t i = 0; i < sizeof m; i++)
      bits[17 + i] = (uint8_t)(m[i] ^ 1u);

    syncword_hit_t h = syncword_find (f, bits, sizeof bits, 0u);
    DP_REQUIRE (h.found);
    DP_CHECK_MSG (h.offset == 17u, "an inverted marker is still at 17");
    DP_CHECK_MSG (h.inverted, "...and must be REPORTED as inverted");
    DP_CHECK_MSG (h.errors == 0u,
                  "an exactly inverted marker is zero errors from the "
                  "complement, whatever the marker's length");
    syncword_destroy (f);
  }

  /* ── 5. FIRST below threshold, at a long marker ───────────────────────────
   *
   * The promise is not a style choice: a best-match search must see the
   * whole stream before it can answer, which a synchroniser on a live
   * capture cannot do. Two markers, the earlier one worse -- and a length
   * (57) that is neither 32 nor a multiple of 8.
   */
  {
    enum
    {
      NM    = 57,
      FIRST = 30,
      BEST  = 200,
      N     = 400
    };
    uint8_t  m[NM];
    uint32_t st = 7u;
    for (size_t i = 0; i < NM; i++)
      m[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    syncword_state_t *f = syncword_create (m, NM);
    DP_REQUIRE (f != NULL);

    uint8_t bits[N] = { 0 };
    memcpy (bits + FIRST, m, NM);
    bits[FIRST + 3] ^= 1u;
    bits[FIRST + 19] ^= 1u;
    memcpy (bits + BEST, m, NM);

    syncword_hit_t h = syncword_find (f, bits, N, 2u);
    DP_REQUIRE (h.found);
    DP_CHECK_MSG (h.offset == FIRST && h.errors == 2u,
                  "the FIRST marker within tolerance wins, and reports its "
                  "own distance rather than the better one's");

    h = syncword_find (f, bits, N, 1u);
    DP_REQUIRE (h.found);
    DP_CHECK_MSG (h.offset == BEST && h.errors == 0u,
                  "tighten the tolerance and the later clean marker becomes "
                  "the first that qualifies -- a threshold, not a ranking");
    syncword_destroy (f);
  }

  /* ── 6. pfa against an EXHAUSTIVE count ───────────────────────────────────
   *
   * `2 * sum_{i<=t} C(n,i) / 2^n` summed in log space is not obviously the
   * same thing as "how many of the 2^n windows are acceptable", and the
   * difference is exactly the kind that reads plausible. So count them: for
   * an 8-bit marker every possible window fits in a loop, and the oracle
   * shares no arithmetic with the kernel -- no lgamma, no binomial, just a
   * distance and a comparison.
   *
   * The t >= 4 rows are the clamp: at 2t >= n every window is within t of
   * one polarity or the other, and the closed form's factor of two
   * double-counts. Both must land on exactly 1.
   */
  {
    const unsigned M = 0xB4u;
    uint8_t        mb[8];
    byte_bits (M, mb);
    syncword_state_t *f = syncword_create (mb, 8u);
    DP_REQUIRE (f != NULL);

    for (unsigned t = 0; t <= 8u; t++)
      {
        unsigned hits = 0u;
        for (unsigned w = 0; w < 256u; w++)
          {
            const unsigned d = hamming8 (w, M);
            if (d <= t || 8u - d <= t)
              hits++;
          }
        const double want = (double)hits / 256.0;
        DP_CHECK_NEAR (syncword_pfa (f, t), want, 1e-12);
      }
    DP_CHECK_MSG (syncword_pfa (f, 4u) == 1.0,
                  "at 2t >= n every window matches one polarity: exactly 1");
    syncword_destroy (f);
  }

  /* ── 7. the formula and the SEARCH agree ──────────────────────────────────
   *
   * A false-alarm probability that disagrees with the detector it is meant
   * to size is worse than none at all, because a caller would trust it to
   * pick `max_errors`. Nothing above connects the two: section 6 checks the
   * arithmetic against a count, and every other section checks the search
   * against a marker that is really there.
   *
   * So: draw independent random windows, run the real search over each, and
   * compare the rate it accepts them at against what `pfa` predicted. An
   * 8-bit marker is chosen because the answer is then large enough to
   * measure -- at 32 bits and a usable threshold the rate is 1e-8 and 20000
   * trials would measure zero either way.
   */
  {
    enum
    {
      TRIALS = 20000
    };
    const unsigned M = 0x6Eu;
    uint8_t        mb[8];
    byte_bits (M, mb);
    syncword_state_t *f = syncword_create (mb, 8u);
    DP_REQUIRE (f != NULL);

    for (unsigned t = 1u; t <= 2u; t++)
      {
        uint32_t st   = 20260820u + t;
        unsigned hits = 0u;
        for (unsigned k = 0; k < TRIALS; k++)
          {
            uint8_t w[8];
            for (unsigned i = 0; i < 8u; i++)
              w[i] = (uint8_t)(dp_xs32 (&st) & 1u);
            /* n_bits == 8 leaves exactly one offset, so this is a single
               accept/reject of the detector rather than a search. */
            if (syncword_find (f, w, 8u, t).found)
              hits++;
          }
        const double got  = (double)hits / (double)TRIALS;
        const double want = syncword_pfa (f, t);
        DP_CHECK_NEAR (got, want, 0.15 * want);
      }
    syncword_destroy (f);
  }

  /* ── 8. max_errors_for is the threshold, and it is TIGHT ──────────────────
   *
   * The answer to doppler#897. Two ways to be wrong and both are checked,
   * because either alone is satisfied by a constant: a value that meets the
   * bound proves nothing if a larger one would have met it too (the function
   * would be needlessly strict, and a caller would lose frames), and a value
   * one step past the bound is the false-frame rate the caller asked not to
   * have.
   */
  {
    uint8_t  m[32];
    uint32_t st = 99u;
    for (size_t i = 0; i < sizeof m; i++)
      m[i] = (uint8_t)(dp_xs32 (&st) & 1u);
    syncword_state_t *f = syncword_create (m, sizeof m);
    DP_REQUIRE (f != NULL);

    const size_t W[]    = { 32u, 96u, 4096u, 100000u };
    const double target = 1e-3;
    int          prev   = 33;
    for (size_t i = 0; i < sizeof W / sizeof W[0]; i++)
      {
        const int t = syncword_max_errors_for (f, W[i], target);
        DP_REQUIRE (t >= 0);

        const double at
            = 1.0 - pow (1.0 - syncword_pfa (f, (uint32_t)t), (double)W[i]);
        const double next
            = 1.0
              - pow (1.0 - syncword_pfa (f, (uint32_t)t + 1), (double)W[i]);
        DP_CHECK_MSG (at <= target,
                      "the returned tolerance must MEET the false-frame rate");
        DP_CHECK_MSG (next > target,
                      "...and one more must exceed it -- otherwise the "
                      "caller is being made stricter than they asked");

        DP_CHECK_MSG (t <= prev,
                      "search a longer window and the affordable tolerance "
                      "falls; it can never rise");
        prev = t;
      }

    /* Far enough past the marker's own resolution and nothing is affordable:
       even an exact match false-hits within this window more often than the
       caller allows. -1 says so rather than returning 0, which a caller
       would read as "exact matches only" and act on. */
    DP_CHECK_MSG (syncword_max_errors_for (f, (size_t)1e12, 1e-9) == -1,
                  "an unachievable rate must be reported, not rounded to 0");
    syncword_destroy (f);
  }

  DP_TEST_END ("syncword_core");
}
