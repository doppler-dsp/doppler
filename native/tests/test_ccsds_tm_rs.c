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
 *   - 4.3.9.3 PRINTS the two basis matrices, and both are transcribed here
 *     and checked row by row and across all 256 values. Requiring them
 *     merely to invert each other would catch a single mis-transcribed bit
 *     and nothing more — a wrong pair consistent with itself passes, and
 *     reading the two equations the wrong way round is exactly such a pair.
 *     A second, DERIVED check sits beside the published one: the transform
 *     is solved back into eight field elements and held to the structure a
 *     dual basis has, which would survive even a matrix mis-transcribed the
 *     same way in both the code and this file.
 *   - the code is SYSTEMATIC (4.3.4 note 2), so the information symbols must
 *     survive encoding unchanged.
 */
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

/* Both basis matrices, transcribed as PRINTED — one row per line, one char
 * per bit, in the order the standard sets them out.
 *
 * Read from **131.0-B-6 (April 2026), section 5.3.9.3** — the current issue,
 * checked against the document rather than against a memory of it. B-3 numbers
 * the same equations 4.3.9.3, and the matrices are identical between the two;
 * the SECTION moved and the content did not, which is exactly why
 * `ccsds_tm.h` warns that a section number is not a value to trust across an
 * issue.
 *
 *
 *   [z0, ..., z7] = [u7, ..., u0] T        (row i is selected by u(7-i))
 *   [u7, ..., u0] = [z0, ..., z7] T'       (row i is selected by z_i)
 *
 * Written as bit rows rather than as the bytes rs.c holds, for the reason
 * `asm_published` is spelled out rather than shifted: a transcription of the
 * standard is evidence, and a second copy of the implementation is not. The
 * rows read left to right, so the leftmost printed bit is the FIRST output
 * coordinate and therefore the most significant bit of the packed row.
 */
static const char *const PUB_CONV_TO_DUAL[8] = {
  "10001101", "11101111", "11101100", "10000110",
  "11111010", "10011001", "10101111", "01111011",
};
static const char *const PUB_DUAL_TO_CONV[8] = {
  "11000101", "01000010", "00101110", "11111101",
  "11110000", "01111001", "10101100", "11001100",
};

/* One printed row as the byte the transform XORs, first bit at the top. */
static uint8_t
row_byte (const char *row)
{
  uint8_t v = 0;
  for (unsigned b = 0; b < 8u; b++)
    v = (uint8_t)((v << 1) | (row[b] == '1'));
  return v;
}

/* GF(2^8) multiply and trace, over the field 4.3.3 picks.
 *
 * The tables are rs_core's, built by rs_init from CCSDS_TM_RS -- so the
 * arithmetic below is the code's own field, not a second implementation of
 * it. That matters for what the dual-basis section claims: it derives a
 * property FROM the shipped field, and a private multiply would let the two
 * drift and the derivation prove nothing. */
static const rs_t *
ccsds_field (void)
{
  static rs_t rs;
  static int  ready = 0;
  if (!ready)
    {
      ready = rs_init (&rs, &CCSDS_TM_RS);
    }
  return ready ? &rs : NULL;
}

static uint8_t
gf_mul (const rs_t *rs, uint8_t a, uint8_t b)
{
  if (a == 0 || b == 0)
    return 0;
  return rs->exp[(unsigned)rs->log[a] + (unsigned)rs->log[b]];
}

/* Tr(a) = a + a^2 + a^4 + ... + a^128, which lands in {0, 1}. */
static uint8_t
gf_trace (const rs_t *rs, uint8_t a)
{
  uint8_t t = 0, x = a;
  for (int i = 0; i < 8; i++)
    {
      t ^= x;
      x = gf_mul (rs, x, x);
    }
  return t;
}

int
main (void)
{
  /* ── 1. Annex G: the generator polynomial, coefficient by coefficient ─── */
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

  /* ── 2. 4.3.9.3: the two matrices must be exact inverses ──────────────── */
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

  /* ── 3. 5.3.9.3 (B-6), against the matrices the standard PRINTS ────────────
   *
   * The published oracle, and the one thing the derived check below cannot
   * supply: it proves the transform is a trace-dual basis map, but not that
   * it is CCSDS's. This proves that, and it is the same kind of check the
   * marker and the randomiser already get.
   *
   * Verified against 131.0-B-6 itself: all sixteen rows match what ships.
   *
   * Each matrix row IS the transform of one basis vector, so feeding the
   * eight of them reads the shipped matrix straight back out and names the
   * row that disagrees. The full 256-value map is then checked against the
   * transcription too, which is what covers linearity rather than just the
   * eight rows.
   */
  {
    int rows_ok = 1, map_ok = 1;
    for (int i = 0; i < 8; i++)
      {
        /* Row i is selected by u(7-i), i.e. by bit 7-i of the byte. */
        const uint8_t basis = (uint8_t)(1u << (7 - i));
        if (ccsds_tm_rs_conv_to_dual (basis) != row_byte (PUB_CONV_TO_DUAL[i]))
          rows_ok = 0;
        /* ...and row i of the second equation is selected by z_i. */
        if (ccsds_tm_rs_dual_to_conv (basis) != row_byte (PUB_DUAL_TO_CONV[i]))
          rows_ok = 0;
      }
    DP_CHECK_MSG (rows_ok,
                  "4.3.9.3: every row of BOTH printed matrices must be what "
                  "the transform does to that basis vector");

    for (int v = 0; v < 256; v++)
      {
        uint8_t z = 0, u = 0;
        for (int i = 0; i < 8; i++)
          {
            if ((v >> (7 - i)) & 1)
              {
                z ^= row_byte (PUB_CONV_TO_DUAL[i]);
                u ^= row_byte (PUB_DUAL_TO_CONV[i]);
              }
          }
        if (ccsds_tm_rs_conv_to_dual ((uint8_t)v) != z
            || ccsds_tm_rs_dual_to_conv ((uint8_t)v) != u)
          map_ok = 0;
      }
    DP_CHECK_MSG (map_ok,
                  "...and the whole 256-value map must equal the printed "
                  "matrices applied as the standard applies them");
  }

  /* ── 4. the dual basis is a DUAL BASIS, not merely an invertible pair ──────
   *
   * The section above asks only that the two transforms invert each other.
   * ANY invertible 8x8 GF(2) matrix and its inverse satisfy that, so it is
   * exactly the consistency test docs/dev/contributing/validation.md names as
   * structurally blind: it cannot see a defect the two halves share, and a
   * mis-transcribed matrix PAIR is precisely such a defect.
   *
   * What follows is derived instead of transcribed. Every GF(2)-linear
   * functional on GF(2^8) is `u -> Tr(c*u)` for a unique c, so the eight bits
   * of the transform are eight field elements -- solved for here, from the
   * shipped matrix and the shipped field, with no constant written down.
   *
   * A dual basis is then the claim that those functionals are the powers of
   * ONE element: `c_j = g^j` with `c_0 = 1`. Eight arbitrary independent
   * functionals are not a geometric progression, and a single flipped bit in
   * either matrix destroys it -- which is what makes this a check the mutual
   * inversion above cannot be.
   *
   * Demonstrated rather than argued: reading 4.3.9.3's two equations the
   * WRONG WAY ROUND -- the transcription error a reader is most likely to
   * make -- leaves an exact inverse pair, so the inversion section stays
   * green and every check below goes red.
   *
   * The printed matrices above now settle which basis this is, so this
   * section is no longer the only evidence -- but it is not redundant with
   * them either. A transcription can be wrong the SAME way twice, in rs.c
   * and in this file; a derivation from the shipped field cannot be, because
   * it writes down no matrix at all.
   */
  {
    const rs_t *rs = ccsds_field ();
    DP_REQUIRE_MSG (rs != NULL, "the CCSDS field must initialise");

    /* Solve for the functional behind each output bit. z0 is the MOST
       significant bit of the returned byte (4.3.9.2 fixes it as the first
       bit transmitted), so bit 7-j carries z_j. */
    uint8_t c[8];
    int     solved = 1;
    for (int j = 0; j < 8; j++)
      {
        int found = -1, count = 0;
        for (int cand = 0; cand < 256; cand++)
          {
            int ok = 1;
            for (int v = 0; v < 256 && ok; v++)
              {
                const uint8_t z   = ccsds_tm_rs_conv_to_dual ((uint8_t)v);
                const uint8_t bit = (uint8_t)((z >> (7 - j)) & 1u);
                if (gf_trace (rs, gf_mul (rs, (uint8_t)cand, (uint8_t)v))
                    != bit)
                  ok = 0;
              }
            if (ok)
              {
                found = cand;
                count++;
              }
          }
        /* Uniqueness is part of the claim: a linear functional has exactly
           one such element, so two would mean the map is not linear and none
           would mean it is not a functional at all. */
        if (count != 1)
          solved = 0;
        c[j] = (uint8_t)(found < 0 ? 0 : found);
      }
    DP_REQUIRE_MSG (solved,
                    "each output bit must be Tr(c*u) for exactly one c");

    DP_CHECK_MSG (c[0] == 1,
                  "z0 must be Tr(u): the first dual coordinate is the trace");

    /* The geometric progression -- the property that makes it a DUAL BASIS
       of a polynomial basis rather than eight unrelated functionals. */
    int     geometric = 1;
    uint8_t p         = 1;
    for (int j = 0; j < 8; j++)
      {
        if (c[j] != p)
          geometric = 0;
        p = gf_mul (rs, p, c[1]);
      }
    DP_CHECK_MSG (geometric,
                  "4.3.9.1: the transform must be the trace-dual of a "
                  "polynomial basis, i.e. c_j = c_1^j");

    /* ...and the defining delta property, read through the OTHER matrix, so
       both transcriptions are covered rather than just the forward one. */
    int delta = 1;
    for (int i = 0; i < 8; i++)
      {
        for (int j = 0; j < 8; j++)
          {
            const uint8_t beta
                = ccsds_tm_rs_dual_to_conv ((uint8_t)(1u << (7 - j)));
            const uint8_t want = (uint8_t)(i == j);
            if (gf_trace (rs, gf_mul (rs, c[i], beta)) != want)
              delta = 0;
          }
      }
    DP_CHECK_MSG (delta, "Tr(c_i * beta_j) must be delta_ij -- the definition "
                         "of a dual basis, checked across both matrices");

    /* The generator is not primitive, and saying so is the point: it is a
       fact about the standard's choice that a reader would guess wrong.
       gcd(117, 255) = 3, so a^117 has order 85 -- the eight powers are still
       independent, which is all a basis needs. */
    DP_CHECK_MSG (rs->log[c[1]] == 117,
                  "the dual basis is generated by a^117");
  }

  /* ── 5. the code is systematic, and parity is not trivial ─────────────── */
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

  /* ── 6. an all-zero codeword has zero parity, in EITHER basis ─────────── */
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

  /* ── 7. one changed symbol must change the parity ─────────────────────── */
  {
    uint8_t a[CCSDS_TM_RS_K] = { 0 }, b[CCSDS_TM_RS_K] = { 0 };
    uint8_t pa[CCSDS_TM_RS_2E], pb[CCSDS_TM_RS_2E];
    b[100] = 0x01;
    ccsds_tm_rs_encode (a, pa);
    ccsds_tm_rs_encode (b, pb);
    DP_CHECK_MSG (memcmp (pa, pb, sizeof pa) != 0,
                  "a single changed information symbol must move the parity");
  }

  /* ── 8. the defining property: a produced codeword has zero syndromes ─── */
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

  /* ── 9. 4.3.5.1: depth 1 is the absence of interleaving ───────────────── */
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

  /* ── 10. 4.4.1: every de-interleaved codeword is a codeword ───────────── */
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

  /* ── 11. correction happens in the CONVENTIONAL basis ─────────────────── */
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

  /* ── 12. decode_block undoes exactly the rotation encode_block applied ── */
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

  /* ── 13. what interleaving is FOR, run through the CODE ────────────────────
   *
   * This section used to compute `b % DEPTH` in a loop and assert arithmetic
   * about its own loop -- it called nothing in the library, so it held for
   * any interleaver, including one that did not interleave. A claim nothing
   * runs is prose (docs/dev/contributing/validation.md), and that is as true
   * inside a test file as it is in a report.
   *
   * The property, measured instead: a contiguous burst of B symbols lands as
   * ceil(B / depth) errors in each codeword, so depth buys a `depth`-fold
   * longer correctable burst AT NO COST IN RATE. Asserted at the boundary
   * from BOTH sides, for every depth 4.3.5.1 allows -- which is also what
   * closes depths 2, 3, 4 and 8, exercised nowhere before.
   */
  {
    static const unsigned DEPTHS[] = { 1, 2, 3, 4, 5, 8 };

    for (size_t di = 0; di < sizeof DEPTHS / sizeof DEPTHS[0]; di++)
      {
        const unsigned depth = DEPTHS[di];
        const size_t   ksym  = (size_t)CCSDS_TM_RS_K * depth;
        const size_t   nsym  = (size_t)CCSDS_TM_RS_N * depth;

        static uint8_t info[CCSDS_TM_RS_K * CCSDS_TM_RS_MAX_DEPTH];
        static uint8_t sent[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
        static uint8_t blk[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];

        /* Structured, because R-S of zeros has zero parity: every
           interleaved column is then identical and a rotated de-interleave
           is the identity map. */
        for (size_t i = 0; i < ksym; i++)
          info[i] = (uint8_t)(i * 13u + 7u + depth);

        DP_REQUIRE (ccsds_tm_rs_encode_block (info, depth, sent) == nsym);
        DP_CHECK_MSG (memcmp (sent, info, ksym) == 0,
                      "4.4.1: the information section passes through "
                      "unchanged at every allowed depth");

        /* The boundary: depth * E contiguous symbols is exactly E in each
           codeword, which every one of them can repair. */
        const unsigned         edge = depth * CCSDS_TM_RS_E;
        ccsds_tm_rs_block_rx_t rx;

        memcpy (blk, sent, nsym);
        for (unsigned b = 0; b < edge; b++)
          blk[b] ^= (uint8_t)(0x53u + b);
        DP_REQUIRE (ccsds_tm_rs_decode_block (blk, depth, &rx) == ksym);
        DP_CHECK_MSG (rx.codewords == depth && rx.uncorrectable == 0u
                          && rx.symbols == edge,
                      "a burst of depth*E must be repaired in full");
        DP_CHECK_MSG (memcmp (blk, sent, nsym) == 0,
                      "...restoring the block exactly, check symbols "
                      "included");

        /* And one symbol more is past the radius in exactly one codeword.
           Both sides, because a decoder that refused everything would
           satisfy this half alone and one that accepted everything would
           satisfy the half above. */
        memcpy (blk, sent, nsym);
        for (unsigned b = 0; b < edge + 1u; b++)
          blk[b] ^= (uint8_t)(0x53u + b);
        DP_REQUIRE (ccsds_tm_rs_decode_block (blk, depth, &rx) == ksym);
        DP_CHECK_MSG (rx.uncorrectable == 1u,
                      "one symbol past depth*E must cost exactly one "
                      "codeword");
      }

    /* The differential the old section was reaching for, now measured: the
       SAME burst that depth 5 carries is fatal at depth 1. Without this the
       loop above is satisfied by an interleaver that simply copies -- each
       depth would meet its own boundary while buying nothing. */
    {
      const unsigned         burst = 5u * CCSDS_TM_RS_E; /* 80 symbols */
      static uint8_t         info[CCSDS_TM_RS_K * 5];
      static uint8_t         blk[CCSDS_TM_RS_N * 5];
      ccsds_tm_rs_block_rx_t rx;

      for (size_t i = 0; i < sizeof info; i++)
        info[i] = (uint8_t)(i * 17u + 3u);

      ccsds_tm_rs_encode_block (info, 5, blk);
      for (unsigned b = 0; b < burst; b++)
        blk[b] ^= (uint8_t)(0x91u + b);
      DP_REQUIRE (ccsds_tm_rs_decode_block (blk, 5, &rx) != 0);
      DP_CHECK_MSG (rx.uncorrectable == 0u,
                    "at depth 5 an 80-symbol burst is 16 per codeword and "
                    "survives");

      ccsds_tm_rs_encode_block (info, 1, blk);
      for (unsigned b = 0; b < burst; b++)
        blk[b] ^= (uint8_t)(0x91u + b);
      DP_REQUIRE (ccsds_tm_rs_decode_block (blk, 1, &rx) != 0);
      DP_CHECK_MSG (rx.uncorrectable == 1u,
                    "...and at depth 1 the SAME burst is 80 in one codeword "
                    "and is refused -- the rate is identical either way");
    }

    /* 4.3.5.1 allows six depths and no others, in both directions. */
    static const unsigned BAD[] = { 0, 6, 7, 9, CCSDS_TM_RS_MAX_DEPTH + 1u };
    static uint8_t        scratch[CCSDS_TM_RS_N * CCSDS_TM_RS_MAX_DEPTH];
    int                   refused = 1;
    for (size_t i = 0; i < sizeof BAD / sizeof BAD[0]; i++)
      {
        if (ccsds_tm_rs_encode_block (scratch, BAD[i], scratch) != 0
            || ccsds_tm_rs_decode_block (scratch, BAD[i], NULL) != 0)
          refused = 0;
      }
    DP_CHECK_MSG (refused,
                  "4.3.5.1 allows 1, 2, 3, 4, 5 and 8 — every other depth "
                  "must be refused by BOTH directions");
  }
  DP_TEST_END ("ccsds_tm_rs");
}
