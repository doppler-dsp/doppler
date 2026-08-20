/*
 * test_ccsds_tm_rand.c — the CCSDS pseudo-randomisers, each held to the
 * sequence the standard prints for it.
 *
 * This test exists in the shape it does because coding is unusually easy to
 * test vacuously. `randomise` is its own inverse, so a round trip returns the
 * input for ANY self-consistent LFSR — a swapped tap, a Fibonacci/Galois
 * mixup, the wrong preset — and a round-trip test would pass for all of them
 * while producing a frame no receiver on earth could derandomise.
 *
 * The check that bites is the published prefix. 131.0-B-6 10.4.3 note 2
 * prints the first 40 bits of BOTH sequences, and only one tap arrangement
 * reproduces each.
 *
 * ## There are two, and which one is the default matters
 *
 *   10.4.1  h(x) = x^17 + x^14 + 1              131071 bits   the `shall`
 *   10.4.2  h(x) = x^8 + x^7 + x^5 + x^3 + 1       255 bits   legacy only
 *
 * B-6 moved the default to the long one and keeps the short one "for
 * backward compatibility with legacy systems", noting it "may introduce
 * spectral lines at 1/255 of the symbol rate" and "could not guarantee full
 * compliance with ITU power flux density limits". Both are pinned here,
 * because shipping the wrong one is a waveform no current receiver
 * derandomises and is invisible to every check except these.
 *
 * The data used is ZEROS, deliberately. A PN payload is already maximally
 * random, so XORing a randomiser onto one produces something that looks
 * exactly as random whether the randomiser ran, ran wrong, or did not run at
 * all — the absence is invisible. Zeros make the sequence itself the output.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "ccsds_tm/ccsds_tm.h"

#include <string.h>

/* 131.0-B-6 10.4.3 note 2, the printed prefix of the DEFAULT sequence:
 *   0001 1100 0111 0001 1011 1001 0001 1011 1010 1001 . . . .
 * The leftmost bit is the one XORed with the first bit of the codeblock. */
static const uint8_t published40_17[40] = {
  0, 0, 0, 1, 1, 1, 0, 0, /* 1C */
  0, 1, 1, 1, 0, 0, 0, 1, /* 71 */
  1, 0, 1, 1, 1, 0, 0, 1, /* B9 */
  0, 0, 0, 1, 1, 0, 1, 1, /* 1B */
  1, 0, 1, 0, 1, 0, 0, 1  /* A9 */
};

/* ...and of the legacy one, unchanged since B-3 10.4.2:
 *   1111 1111 0100 1000 0000 1110 1100 0000 1001 1010 . . . .
 * i.e. FF 48 0E C0 9A, MSB-first. */
static const uint8_t published40_8[40] = {
  1, 1, 1, 1, 1, 1, 1, 1, /* FF */
  0, 1, 0, 0, 1, 0, 0, 0, /* 48 */
  0, 0, 0, 0, 1, 1, 1, 0, /* 0E */
  1, 1, 0, 0, 0, 0, 0, 0, /* C0 */
  1, 0, 0, 1, 1, 0, 1, 0  /* 9A */
};

/* One randomiser's whole contract, checked the same way for both.
 *
 * Shared rather than written twice because the properties are the standard's,
 * not either generator's: a second copy would let the two drift, and the one
 * nobody edited would be the one that stopped checking. */
static int
check_generator (const ccsds_tm_rand_t *r, const char *name,
                 const uint8_t *published, size_t period, size_t ones_wanted,
                 uint8_t *scratch, size_t scratch_len)
{
  DP_CHECK_MSG (r->period == period, name);

  /* ── 1. the sequence itself, against the printed prefix ───────────────── */
  {
    uint8_t seq[40];
    ccsds_tm_rand_seq_with (r, seq, sizeof seq);
    DP_CHECK_MSG (memcmp (seq, published, 40) == 0,
                  "the first 40 bits must match what 10.4.3 prints");
  }

  /* ── 2. randomising zeros yields the sequence ─────────────────────────── */
  {
    uint8_t bits[40] = { 0 };
    ccsds_tm_randomise_with (r, bits, 40);
    DP_CHECK_MSG (memcmp (bits, published, 40) == 0,
                  "randomising zeros must emit the sequence verbatim");
  }

  /* ── 3. it repeats after its period, and not before ────────────────────────
   *
   * The balance check is here because the period checks are not enough on
   * their own, and this test learned that the hard way: an early degree-8
   * implementation drove the register to the all-zero fixed point, and a
   * dead sequence repeats with EVERY period, has no shorter one, and
   * satisfies both. Only the published prefix and this caught it.
   *
   * A maximal generator of D stages visits all 2^D - 1 non-zero states, so
   * its period holds exactly 2^(D-1) ones — a property no degenerate
   * sequence has.
   */
  {
    DP_REQUIRE (scratch_len >= period + 64u);
    ccsds_tm_rand_seq_with (r, scratch, period + 64u);
    DP_CHECK_MSG (memcmp (scratch, scratch + period, 64) == 0,
                  "the sequence must repeat with its stated period");

    size_t ones = 0;
    for (size_t i = 0; i < period; i++)
      ones += scratch[i];
    DP_CHECK_MSG (ones == ones_wanted,
                  "a maximal-length period must be exactly balanced");

    /* ...and it must not repeat sooner, or the register is too short. A
       shorter period would divide this one, so the divisors are the only
       offsets worth trying and trying them all is needless. */
    int early = 0;
    for (size_t p = 1; p < period; p++)
      {
        if (period % p == 0 && memcmp (scratch, scratch + p, 40) == 0)
          early = 1;
      }
    DP_CHECK_MSG (!early, "no period shorter than the stated one may appear");
  }

  /* ── 4. its own inverse, over a run that crosses the period boundary ──── */
  {
    const size_t n = period + 91u;
    DP_REQUIRE (scratch_len >= 2u * n);
    uint8_t *data = scratch;
    uint8_t *copy = scratch + n;
    for (size_t i = 0; i < n; i++)
      data[i] = (uint8_t)((i * 7u + 3u) & 1u);
    memcpy (copy, data, n);

    ccsds_tm_randomise_with (r, data, n);
    DP_CHECK_MSG (memcmp (data, copy, n) != 0,
                  "randomising must actually change the data");
    ccsds_tm_randomise_with (r, data, n);
    DP_CHECK_MSG (memcmp (data, copy, n) == 0,
                  "randomise twice must be the identity");
  }

  /* ── 5. each call restarts at the preset (10.4.3) ─────────────────────── */
  {
    uint8_t a[16] = { 0 }, b[16] = { 0 };
    ccsds_tm_randomise_with (r, a, 16);
    ccsds_tm_randomise_with (r, b, 16);
    DP_CHECK_MSG (memcmp (a, b, sizeof a) == 0,
                  "the generator must preset per call, not carry state");
  }

  /* ── 6. stepping it one bit at a time IS the sequence ──────────────────────
   *
   * ccsds_tm_frame_decode packs bits to octets and derandomises in the same
   * pass, so it cannot hand a mutable run to ccsds_tm_randomise and must not
   * hold a sequence the size of the data either -- it steps the generator
   * alongside instead. That is only the same sequence if the state machine
   * and the bulk call agree, so the equivalence is pinned here rather than
   * left as an assumption two files away.
   */
  {
    enum
    {
      N = 600
    };
    uint8_t               bulk[N], stepped[N];
    ccsds_tm_rand_state_t st;
    ccsds_tm_rand_seq_with (r, bulk, N);
    ccsds_tm_rand_init (&st, r);
    for (size_t i = 0; i < N; i++)
      stepped[i] = ccsds_tm_rand_step (&st);
    DP_CHECK_MSG (memcmp (bulk, stepped, N) == 0,
                  "stepping must reproduce the bulk sequence exactly");
  }
  return 0;
}

/* Big enough for the long generator's period checks; static because 256 KB
   does not belong on a stack. */
static uint8_t scratch[2u * (131071u + 91u)];

int
main (void)
{
  if (check_generator (&CCSDS_TM_RAND, "10.4.1 is the 131071-bit sequence",
                       published40_17, 131071u, 65536u, scratch,
                       sizeof scratch)
      != 0)
    return 1;
  if (check_generator (&CCSDS_TM_RAND_LEGACY, "10.4.2 is the 255-bit sequence",
                       published40_8, 255u, 128u, scratch, sizeof scratch)
      != 0)
    return 1;

  /* ── 7. the DEFAULT is 10.4.1's, and that is the point of this change ──────
   *
   * B-6 made the long sequence the `shall` and kept the short one only for
   * legacy systems. A default that silently stayed on the short one would
   * pass every check above -- both generators are individually correct --
   * and would put a waveform on the air that no current receiver
   * derandomises. So the default is asserted directly, and asserted to be
   * DIFFERENT from the legacy one, which is what a fallback would break.
   */
  {
    uint8_t dflt[40] = { 0 }, legacy[40] = { 0 };
    ccsds_tm_randomise (dflt, sizeof dflt);
    ccsds_tm_randomise_with (&CCSDS_TM_RAND_LEGACY, legacy, sizeof legacy);

    DP_CHECK_MSG (memcmp (dflt, published40_17, sizeof dflt) == 0,
                  "10.4.1: the bare call must apply the 131071-bit sequence");
    DP_CHECK_MSG (memcmp (dflt, legacy, sizeof dflt) != 0,
                  "...which is not the legacy one -- a silent fallback would "
                  "pass every other check in this file");
    DP_CHECK_MSG (CCSDS_TM_RAND_PERIOD == 131071,
                  "and the advertised period is the default's");

    uint8_t seq[40];
    ccsds_tm_rand_seq (seq, sizeof seq);
    DP_CHECK_MSG (memcmp (seq, published40_17, sizeof seq) == 0,
                  "the bare sequence accessor must agree with the bare "
                  "randomiser about which generator it is");
  }

  DP_TEST_END ("ccsds_tm_rand");
}
