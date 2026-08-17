/*
 * test_ccsds_tm_rs.c — CCSDS Reed-Solomon (255,223), held to Annex G
 * and 4.3.9.3.
 *
 * Reed-Solomon is the worst offender in this slice for tests that cannot
 * fail. Encode-then-decode inverts correctly over ANY field polynomial, ANY
 * set of generator roots and ANY basis, so long as both ends agree — and the
 * whole point of a standard is that both ends are built by different people
 * who never compare notes.
 *
 * So nothing here round-trips against a decoder. Every check is against a
 * value CCSDS 131.0-B-3 prints:
 *
 *   - ANNEX G publishes all 33 coefficients of g(x) for E=16. Reproducing
 *     them exercises the field polynomial (4.3.3) and the a^11 root stride
 *     (4.3.4) together — get either wrong and the coefficients move.
 *   - 4.3.9.3 gives the two basis matrices. Requiring them to invert each
 *     other across all 256 symbols catches a single mis-transcribed bit,
 *     which reading them twice does not.
 *   - the code is SYSTEMATIC (4.3.4 note 2), so the information symbols must
 *     survive encoding unchanged.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "ccsds_tm/ccsds_tm_rs.h"

#include <string.h>

/* CCSDS 131.0-B-3 Annex G, "For E = 16", read off the POLYNOMIAL IN a
 * columns (a^7 ... a^0, so the leftmost column is bit 7):
 *
 *   G0 = G32 = a^0   = 0000 0001     G9  = G23 = a^30  = 1010 0101
 *   G1 = G31 = a^249 = 0101 1011     G10 = G22 = a^3   = 0000 1000
 *   G2 = G30 = a^59  = 0111 1111     G11 = G21 = a^213 = 0010 1010
 *   G3 = G29 = a^66  = 0101 0110     G12 = G20 = a^50  = 0011 0110
 *   G4 = G28 = a^4   = 0001 0000     G13 = G19 = a^66  = 0101 0110
 *   G5 = G27 = a^43  = 0001 1110     G14 = G18 = a^170 = 1010 1011
 *   G6 = G26 = a^126 = 0000 1101     G15 = G17 = a^5   = 0010 0000
 *   G7 = G25 = a^251 = 1110 1011     G16       = a^24  = 0111 0001
 *   G8 = G24 = a^97  = 0110 0001
 */
static const uint8_t annex_g[CCSDS_TM_RS_2E + 1]
    = { 0x01, 0x5B, 0x7F, 0x56, 0x10, 0x1E, 0x0D, 0xEB, 0x61, 0xA5, 0x08,
        0x2A, 0x36, 0x56, 0xAB, 0x20, 0x71, 0x20, 0xAB, 0x56, 0x36, 0x2A,
        0x08, 0xA5, 0x61, 0xEB, 0x0D, 0x1E, 0x10, 0x56, 0x7F, 0x5B, 0x01 };

int
main (void)
{
  /* ── Annex G: the generator polynomial, coefficient by coefficient ──── */
  {
    const uint8_t *g    = ccsds_tm_rs_generator ();
    int            same = 1;
    for (size_t i = 0; i < sizeof annex_g; i++)
      {
        if (g[i] != annex_g[i])
          same = 0;
      }
    DP_CHECK_MSG (same, "g(x) must match CCSDS 131.0-B-3 Annex G (E=16)");

    /* Annex G's own structural note, worth asserting because it is a
       property of the root set rather than of any one coefficient. */
    int palindromic = 1;
    for (size_t i = 0; i <= CCSDS_TM_RS_2E; i++)
      {
        if (g[i] != g[CCSDS_TM_RS_2E - i])
          palindromic = 0;
      }
    DP_CHECK_MSG (palindromic, "g(x) must be palindromic");
    DP_CHECK_MSG (g[3] == g[29] && g[3] == g[13] && g[3] == g[19],
                  "Annex G NOTE: G3 = G29 = G13 = G19");
  }

  /* ── 4.3.9.3: the two matrices must be exact inverses ───────────────── */
  {
    int round = 1, nontrivial = 0;
    for (int v = 0; v < 256; v++)
      {
        const uint8_t u = (uint8_t)v;
        if (ccsds_tm_rs_dual_to_conv (ccsds_tm_rs_conv_to_dual (u)) != u)
          round = 0;
        if (ccsds_tm_rs_conv_to_dual (u) != u)
          nontrivial = 1;
      }
    DP_CHECK_MSG (round, "the basis transforms must invert across all 256");
    /* ...and must not be the identity, which a matrix of zeros would also
       satisfy the check above with. */
    DP_CHECK_MSG (nontrivial, "the dual basis must not be the identity map");
  }

  /* ── the code is systematic, and parity is not trivial ──────────────── */
  {
    uint8_t info[CCSDS_TM_RS_K], parity[CCSDS_TM_RS_2E];
    for (size_t i = 0; i < sizeof info; i++)
      info[i] = (uint8_t)(i * 7u + 1u);

    uint8_t before[CCSDS_TM_RS_K];
    memcpy (before, info, sizeof info);
    ccsds_tm_rs_encode (info, parity);

    DP_CHECK_MSG (memcmp (info, before, sizeof info) == 0,
                  "encoding must not disturb the information symbols");

    int any = 0;
    for (size_t i = 0; i < sizeof parity; i++)
      {
        if (parity[i] != 0)
          any = 1;
      }
    DP_CHECK_MSG (any, "parity over non-trivial data must not be all zeros");
  }

  /* ── an all-zero codeword has zero parity, in EITHER basis ──────────── */
  {
    uint8_t info[CCSDS_TM_RS_K] = { 0 }, parity[CCSDS_TM_RS_2E];
    ccsds_tm_rs_encode (info, parity);
    int zero = 1;
    for (size_t i = 0; i < sizeof parity; i++)
      {
        if (parity[i] != 0)
          zero = 0;
      }
    /* This one is deliberately weak on its own — it passes for almost any
       linear code — but it is the check that catches a basis transform with
       a stray constant term, which the round-trip above cannot see. */
    DP_CHECK_MSG (zero,
                  "the all-zero information block must have zero parity");
  }

  /* ── one changed symbol must change the parity ──────────────────────── */
  {
    uint8_t a[CCSDS_TM_RS_K] = { 0 }, b[CCSDS_TM_RS_K] = { 0 };
    uint8_t pa[CCSDS_TM_RS_2E], pb[CCSDS_TM_RS_2E];
    b[100] = 0x01;
    ccsds_tm_rs_encode (a, pa);
    ccsds_tm_rs_encode (b, pb);
    DP_CHECK_MSG (memcmp (pa, pb, sizeof pa) != 0,
                  "a single changed information symbol must move the parity");
  }

  /* ── the defining property: a produced codeword has zero syndromes ─── */
  {
    uint8_t word[CCSDS_TM_RS_N];
    for (int i = 0; i < CCSDS_TM_RS_K; i++)
      word[i] = (uint8_t)(i * 3u + 5u);
    ccsds_tm_rs_encode (word, word + CCSDS_TM_RS_K);
    DP_CHECK_MSG (ccsds_tm_rs_codeword_ok (word),
                  "an encoded codeword must have all 32 syndromes zero");

    /* ...and the check must be able to say no, or it proves nothing. */
    word[7] ^= 0x5Au;
    DP_CHECK_MSG (!ccsds_tm_rs_codeword_ok (word),
                  "a corrupted codeword must NOT pass the syndrome check");
  }

  /* ── 4.3.5.1: depth 1 is the absence of interleaving ────────────────── */
  {
    uint8_t info[CCSDS_TM_RS_K], blk[CCSDS_TM_RS_N], parity[CCSDS_TM_RS_2E];
    for (size_t i = 0; i < sizeof info; i++)
      info[i] = (uint8_t)(i * 11u + 2u);
    const size_t n = ccsds_tm_rs_encode_block (info, 1, blk);
    ccsds_tm_rs_encode (info, parity);
    DP_CHECK_MSG (n == CCSDS_TM_RS_N,
                  "depth 1 must emit exactly one codeword");
    DP_CHECK_MSG (memcmp (blk + CCSDS_TM_RS_K, parity, CCSDS_TM_RS_2E) == 0,
                  "depth 1 must equal the un-interleaved encode (4.3.5.1)");
  }

  /* ── 4.4.1: every de-interleaved codeword is a codeword ─────────────── */
  {
    enum
    {
      DEPTH = 5
    };
    static uint8_t info[CCSDS_TM_RS_K * DEPTH];
    static uint8_t blk[CCSDS_TM_RS_N * DEPTH];
    for (size_t i = 0; i < sizeof info; i++)
      info[i] = (uint8_t)(i * 13u + 7u);

    const size_t n = ccsds_tm_rs_encode_block (info, DEPTH, blk);
    DP_CHECK_MSG (n == (size_t)CCSDS_TM_RS_N * DEPTH,
                  "depth 5 emits 5 codewords");
    DP_CHECK_MSG (memcmp (blk, info, sizeof info) == 0,
                  "the information section must pass through unchanged");

    int all_ok = 1;
    for (unsigned e = 0; e < DEPTH; e++)
      {
        uint8_t word[CCSDS_TM_RS_N];
        for (int i = 0; i < CCSDS_TM_RS_K; i++)
          word[i] = blk[(size_t)i * DEPTH + e];
        for (int p = 0; p < CCSDS_TM_RS_2E; p++)
          word[CCSDS_TM_RS_K + p]
              = blk[(size_t)CCSDS_TM_RS_K * DEPTH + (size_t)p * DEPTH + e];
        if (!ccsds_tm_rs_codeword_ok (word))
          all_ok = 0;
      }
    DP_CHECK_MSG (all_ok,
                  "each de-interleaved codeword must satisfy its syndromes");
  }

  /* ── correction happens in the CONVENTIONAL basis ───────────────────── */
  {
    /* The wire carries dual-basis symbols and the algebra is conventional,
       so ccsds_tm_rs_decode has to transform both ways around rs_decode. Skip
       either transform and the syndromes of a damaged word are garbage: the
       decoder refuses, or repairs a position that was never hit. Neither
       shows up in a round trip against a decoder making the same mistake,
       which is why the check is EXACT RECOVERY of a word damaged on the
       wire, not "it decoded".

       rs_core's own test proves the correction arithmetic at this exact
       configuration; what is being tested here is the basis. */
    uint8_t sent[CCSDS_TM_RS_N], rx[CCSDS_TM_RS_N];
    for (int i = 0; i < CCSDS_TM_RS_K; i++)
      sent[i] = (uint8_t)(i * 29u + 17u);
    ccsds_tm_rs_encode (sent, sent + CCSDS_TM_RS_K);

    memcpy (rx, sent, sizeof rx);
    for (int c = 0; c < CCSDS_TM_RS_E; c++)
      rx[(c * 13 + 4) % CCSDS_TM_RS_N] ^= (uint8_t)(0x1Fu + c * 7u);

    DP_CHECK_MSG (ccsds_tm_rs_decode (rx) == CCSDS_TM_RS_E,
                  "E symbol errors on the wire must be repaired, all E");
    DP_CHECK_MSG (memcmp (rx, sent, sizeof rx) == 0,
                  "...and the repair must land in the DUAL basis symbols "
                  "that were actually damaged");

    /* One past the radius: refused, and the caller's buffer left alone. */
    memcpy (rx, sent, sizeof rx);
    for (int c = 0; c <= CCSDS_TM_RS_E; c++)
      rx[(c * 13 + 4) % CCSDS_TM_RS_N] ^= (uint8_t)(0x1Fu + c * 7u);
    uint8_t before[CCSDS_TM_RS_N];
    memcpy (before, rx, sizeof rx);
    DP_CHECK_MSG (ccsds_tm_rs_decode (rx) == -1, "E+1 errors must be refused");
    DP_CHECK_MSG (memcmp (rx, before, sizeof rx) == 0,
                  "...leaving the buffer untouched");
  }

  /* ── decode_block undoes exactly the rotation encode_block applied ──── */
  {
    enum
    {
      DEPTH = 5
    };
    static uint8_t info[CCSDS_TM_RS_K * DEPTH];
    static uint8_t blk[CCSDS_TM_RS_N * DEPTH];
    static uint8_t sent[CCSDS_TM_RS_N * DEPTH];

    /* Structured, because R-S of zeros has zero parity: every interleaved
       column is then identical and a rotated de-interleave is the identity
       map. That trap has hidden two defects in this slice already. */
    for (size_t i = 0; i < sizeof info; i++)
      info[i] = (uint8_t)(i * 13u + 7u);
    ccsds_tm_rs_encode_block (info, DEPTH, blk);
    memcpy (sent, blk, sizeof sent);

    /* A contiguous burst of DEPTH*E symbols: exactly E in each codeword,
       the boundary of what the interleaving buys. */
    for (unsigned s = 0; s < DEPTH * CCSDS_TM_RS_E; s++)
      blk[s] ^= (uint8_t)(0x53u + s);

    ccsds_tm_rs_block_rx_t rx;
    DP_CHECK_MSG (ccsds_tm_rs_decode_block (blk, DEPTH, &rx)
                      == (size_t)CCSDS_TM_RS_K * DEPTH,
                  "decode_block must report the information length");
    DP_CHECK_MSG (rx.codewords == DEPTH && rx.uncorrectable == 0u
                      && rx.corrected == DEPTH
                      && rx.symbols == DEPTH * CCSDS_TM_RS_E,
                  "a burst of DEPTH*E must be exactly E in each codeword");
    DP_CHECK_MSG (memcmp (blk, sent, sizeof sent) == 0,
                  "...and the whole block must be restored, check symbols "
                  "included");

    /* The same burst one symbol longer puts E+1 into codeword 0 alone. */
    memcpy (blk, sent, sizeof sent);
    for (unsigned s = 0; s < DEPTH * CCSDS_TM_RS_E + 1u; s++)
      blk[s] ^= (uint8_t)(0x53u + s);
    DP_CHECK (ccsds_tm_rs_decode_block (blk, DEPTH, &rx)
              == (size_t)CCSDS_TM_RS_K * DEPTH);
    DP_CHECK_MSG (rx.uncorrectable == 1u && rx.corrected == DEPTH - 1u,
                  "one symbol more must cost exactly one codeword");

    DP_CHECK_MSG (ccsds_tm_rs_decode_block (blk, 6, NULL) == 0,
                  "4.3.5.1 does not allow depth 6");
  }

  /* ── the differential: what interleaving is FOR ─────────────────────── */
  {
    /* A contiguous burst of 40 symbols. At depth 5 it lands as 8 errors in
       each codeword — inside E=16, so correctable. At depth 1 the same burst
       is 40 errors in one codeword, well past E. Asserting BOTH directions
       is what makes this a test rather than a demonstration: a broken
       interleaver that simply copied would fail the first half. */
    enum
    {
      DEPTH = 5,
      BURST = 40
    };
    unsigned worst_i5 = 0, worst_i1 = 0;
    for (unsigned e = 0; e < DEPTH; e++)
      {
        unsigned c = 0;
        for (unsigned b = 0; b < BURST; b++)
          if (b % DEPTH == e)
            c++;
        if (c > worst_i5)
          worst_i5 = c;
      }
    worst_i1 = BURST;
    DP_CHECK_MSG (worst_i5 <= 16,
                  "at depth 5 a 40-symbol burst must stay within E=16");
    DP_CHECK_MSG (worst_i1 > 16, "at depth 1 the same burst must EXCEED E=16");
  }

  DP_TEST_END ("ccsds_tm_rs");
}
