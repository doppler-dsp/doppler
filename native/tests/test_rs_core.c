/*
 * test_rs_core.c — general Reed-Solomon: the code's own distance, held
 * against every configuration rather than against a matching decoder.
 *
 * Reed-Solomon is the worst offender in this slice for tests that cannot
 * fail. Encode-then-decode inverts correctly over ANY field polynomial, ANY
 * root set and ANY basis so long as both ends agree, and the whole point of a
 * standard is that both ends are built by people who never compare notes.
 *
 * So the claims here are the ones a decoder cannot satisfy by agreeing with
 * itself:
 *
 *   - the generator vanishes at every root it was built from — the DEFINITION
 *     of g(x), checked by evaluation and not by construction;
 *   - E symbol errors are corrected EXACTLY, at every configuration;
 *   - E+1 errors never RECOVER the transmitted word, which is impossible for
 *     a bounded-distance decoder and would betray a decoder measuring itself;
 *   - a decode either refuses or returns a codeword — never a third thing;
 *   - a refusal leaves the caller's buffer untouched.
 *
 * The two configurations with a non-textbook first root or stride are what
 * make the exact-recovery checks bite: a decoder that assumed `first_root ==
 * 1` finds the right error POSITIONS and the wrong magnitudes, so it still
 * "decodes" and the word it returns is not the one that was sent. See
 * docs/design/reed-solomon.md §4.
 */
#define _GNU_SOURCE
#include "dp_rng_test.h"
#include "dp_test.h"

#include "rs/rs_core.h"

#include <string.h>

/* The three configurations every property below is checked at. CCSDS's row
 * is here as a CONFIGURATION -- holding its generator to Annex G is
 * test_ccsds_tm_rs.c's job, with the standard it belongs to. */
static const struct
{
  const char *name;
  rs_code_t   code;
  /* Refusals demanded out of TRIALS at E+1 errors. A bounded-distance
     decoder miscorrects when the received word lands inside another
     codeword's sphere, with probability ~1/E! for random errors -- so at
     E = 16 refusing is the only outcome ever seen (1/16! ~ 5e-14), while at
     E = 2 it is a coin toss and demanding more would be pinning the seed
     rather than the code. Measured here: 64/64, 64/64, 46/64. */
  unsigned min_refusals;
} CODES[] = {
  { "textbook RS(255,223)",
    { .symbol_bits = 8,
      .field_poly  = 0x1Du,
      .nroots      = 32,
      .first_root  = 1,
      .root_stride = 1 },
    64 },
  { "CCSDS-shaped RS(255,223)",
    { .symbol_bits = 8,
      .field_poly  = 0x87u,
      .nroots      = 32,
      .first_root  = 112,
      .root_stride = 11 },
    64 },
  { "RS(15,11)",
    { .symbol_bits = 4,
      .field_poly  = 0x03u,
      .nroots      = 4,
      .first_root  = 1,
      .root_stride = 1 },
    32 },
};

/* Fill a codeword with structured, non-constant data and encode it.
 *
 * NOT zeros: an all-zero information block has all-zero parity, so every
 * symbol of the codeword is identical and a decoder that repaired the wrong
 * POSITION would still return the transmitted word. That trap has hidden two
 * separate defects in this slice already. */
static void
make_codeword (const rs_t *rs, uint8_t *word, uint32_t *seed)
{
  const uint8_t mask = (uint8_t)((1u << rs->code.symbol_bits) - 1u);
  for (unsigned i = 0; i < rs->k; i++)
    word[i] = (uint8_t)(dp_xs32 (seed) & mask);
  rs_encode (rs, word, word + rs->k);
}

/* Corrupt `count` distinct positions with nonzero deltas. */
static void
inject (const rs_t *rs, uint8_t *word, unsigned count, uint32_t *seed)
{
  const uint8_t mask          = (uint8_t)((1u << rs->code.symbol_bits) - 1u);
  uint8_t       hit[RS_N_MAX] = { 0 };

  for (unsigned c = 0; c < count; c++)
    {
      unsigned p;
      do
        p = dp_xs32 (seed) % rs->n;
      while (hit[p]);
      hit[p] = 1u;

      uint8_t delta;
      do
        delta = (uint8_t)(dp_xs32 (seed) & mask);
      while (delta == 0);

      word[p] ^= delta;
    }
}

int
main (void)
{
  /* ── the description is validated, not trusted ──────────────────────── */
  {
    rs_t      rs;
    rs_code_t c = { .symbol_bits = 4,
                    .field_poly  = 0x03u,
                    .nroots      = 4,
                    .first_root  = 1,
                    .root_stride = 1 };

    DP_CHECK_MSG (rs_init (&rs, &c), "RS(15,11) must be a usable code");

    /* x^4+x^3+x^2+x+1 is irreducible over GF(2) and NOT primitive: a = x has
       order 5, so it generates a subgroup of five elements and arithmetic
       over it is entirely self-consistent. Nothing but a coverage check
       catches this. */
    c.field_poly = 0x0Fu;
    DP_CHECK_MSG (!rs_init (&rs, &c),
                  "a non-primitive field polynomial must be refused");
    c.field_poly = 0x03u;

    /* gcd(3, 15) = 3, so a^3 has order 5 and the four "roots" are not four
       distinct roots. The code still encodes and still checks. */
    c.root_stride = 3;
    DP_CHECK_MSG (!rs_code_valid (&c),
                  "a root stride sharing a factor with n must be refused");
    DP_CHECK_MSG (!rs_init (&rs, &c), "...and rs_init must refuse it too");
    c.root_stride = 1;

    c.nroots = 5;
    DP_CHECK_MSG (!rs_code_valid (&c), "an odd parity count is not 2E");
    c.nroots = 16;
    DP_CHECK_MSG (!rs_code_valid (&c),
                  "parity must leave room for information (nroots < n)");
    c.nroots = 4;

    c.symbol_bits = 9;
    DP_CHECK_MSG (!rs_code_valid (&c), "symbols wider than a byte are out");
    c.symbol_bits = 4;

    DP_CHECK_MSG (rs_code_valid (&c), "...and the code is valid again");
  }

  for (size_t ci = 0; ci < sizeof CODES / sizeof CODES[0]; ci++)
    {
      const rs_code_t *code = &CODES[ci].code;
      rs_t             rs;
      DP_REQUIRE_MSG (rs_init (&rs, code), CODES[ci].name);

      /* ── g(x) vanishes at every root it claims ───────────────────────── */
      {
        const uint8_t *g   = rs_generator (&rs);
        int            all = 1;
        for (unsigned m = 0; m < code->nroots; m++)
          {
            /* Horner over g at a^(s*(j0+m)), using the field the code
               built. A generator assembled from the wrong roots -- a
               dropped stride, an off-by-one first root -- fails here, and
               this is the definition rather than a second construction. */
            const unsigned je
                = (code->root_stride * (code->first_root + m)) % rs.n;
            const uint8_t x   = rs.exp[je];
            uint8_t       acc = 0;
            for (unsigned i = code->nroots + 1u; i-- > 0;)
              {
                uint8_t t = 0;
                if (acc && x)
                  t = rs.exp[rs.log[acc] + rs.log[x]];
                acc = (uint8_t)(t ^ g[i]);
              }
            if (acc != 0)
              all = 0;
          }
        DP_CHECK_MSG (all, "g(x) must vanish at each of its own roots");
        DP_CHECK_MSG (g[code->nroots] == 1u, "g(x) must be monic");
        DP_CHECK_MSG (g[0] != 0u, "g(x) must have degree exactly 2E");
      }

      /* ── systematic, and the syndromes say so ────────────────────────── */
      {
        uint8_t  word[RS_N_MAX], info[RS_N_MAX];
        uint32_t seed = 12345u + (uint32_t)ci;
        make_codeword (&rs, word, &seed);
        memcpy (info, word, rs.k);

        DP_CHECK_MSG (rs_codeword_ok (&rs, word),
                      "an encoded word must have every syndrome zero");

        uint8_t parity[RS_NROOTS_MAX];
        rs_encode (&rs, info, parity);
        DP_CHECK_MSG (memcmp (info, word, rs.k) == 0,
                      "encoding must not disturb the information symbols");
        DP_CHECK_MSG (memcmp (parity, word + rs.k, code->nroots) == 0,
                      "encoding must be a function of the information alone");

        word[3] ^= 0x0Bu;
        DP_CHECK_MSG (!rs_codeword_ok (&rs, word),
                      "a corrupted word must NOT pass the syndrome check");
      }

      /* ── E errors are corrected, exactly, and E+1 never recovers ─────── */
      {
        enum
        {
          TRIALS = 64
        };
        uint32_t seed        = 777u + (uint32_t)ci * 31u;
        int      all_fixed   = 1;
        int      all_counted = 1;
        int      ever_wrong  = 0;
        unsigned refusals    = 0;

        for (unsigned t = 0; t < TRIALS; t++)
          {
            uint8_t sent[RS_N_MAX], rx[RS_N_MAX];
            make_codeword (&rs, sent, &seed);

            /* exactly E: the code's distance says this is always
               correctable, for every error pattern, at every position. */
            memcpy (rx, sent, rs.n);
            inject (&rs, rx, rs.e, &seed);
            const int fixed = rs_decode (&rs, rx);
            if (fixed != (int)rs.e)
              all_counted = 0;
            if (memcmp (rx, sent, rs.n) != 0)
              all_fixed = 0;

            /* E+1: beyond the guaranteed radius. A bounded-distance decoder
               cannot recover the sent word -- it refuses, or it lands on a
               DIFFERENT codeword. Either is fine; returning the sent word
               would mean the decoder is not bounded-distance and the test
               above proved nothing. */
            memcpy (rx, sent, rs.n);
            inject (&rs, rx, rs.e + 1u, &seed);
            const int over = rs_decode (&rs, rx);
            if (over < 0)
              refusals++;
            else if (memcmp (rx, sent, rs.n) == 0)
              ever_wrong = 1;
          }

        DP_CHECK_MSG (all_counted, "E errors must be reported as E repairs");
        DP_CHECK_MSG (all_fixed, "E errors must be corrected exactly");
        DP_CHECK_MSG (!ever_wrong,
                      "E+1 errors must never recover the sent word");
        DP_CHECK_MSG (refusals >= CODES[ci].min_refusals,
                      "E+1 errors must be REFUSED as often as ~1/E! says");
      }

      /* ── refuse or return a codeword; never a third thing ────────────── */
      {
        uint32_t seed      = 4242u + (uint32_t)ci;
        int      ok        = 1;
        int      untouched = 1;
        for (unsigned errs = 0; errs <= rs.n; errs += (rs.n / 16u) + 1u)
          {
            uint8_t sent[RS_N_MAX], rx[RS_N_MAX], before[RS_N_MAX];
            make_codeword (&rs, sent, &seed);
            memcpy (rx, sent, rs.n);
            inject (&rs, rx, errs > rs.n ? rs.n : errs, &seed);
            memcpy (before, rx, rs.n);

            const int r = rs_decode (&rs, rx);
            if (r < 0)
              {
                /* A refusal must not have half-corrected anything. */
                if (memcmp (rx, before, rs.n) != 0)
                  untouched = 0;
              }
            else if (!rs_codeword_ok (&rs, rx))
              ok = 0;
          }
        DP_CHECK_MSG (ok, "a successful decode must return a codeword");
        DP_CHECK_MSG (untouched, "a refusal must leave the buffer untouched");
      }

      /* ── a clean word is left alone, and costs nothing ───────────────── */
      {
        uint8_t  word[RS_N_MAX], before[RS_N_MAX];
        uint32_t seed = 99u + (uint32_t)ci;
        make_codeword (&rs, word, &seed);
        memcpy (before, word, rs.n);
        DP_CHECK_MSG (rs_decode (&rs, word) == 0,
                      "a valid codeword must report zero repairs");
        DP_CHECK_MSG (memcmp (word, before, rs.n) == 0,
                      "...and must be returned unchanged");
      }
    }

  /* ── RS(15,11): small enough to sweep every single error there is ────── */
  {
    rs_t rs;
    DP_REQUIRE (rs_init (&rs, &CODES[2].code));

    uint8_t  sent[RS_N_MAX];
    uint32_t seed = 5u;
    make_codeword (&rs, sent, &seed);

    int all = 1;
    for (unsigned p = 0; p < rs.n; p++)
      {
        for (uint8_t d = 1; d < 16u; d++)
          {
            uint8_t rx[RS_N_MAX];
            memcpy (rx, sent, rs.n);
            rx[p] ^= d;
            if (rs_decode (&rs, rx) != 1 || memcmp (rx, sent, rs.n) != 0)
              all = 0;
          }
      }
    DP_CHECK_MSG (all,
                  "every single-symbol error, at every position and value, "
                  "must be repaired exactly");
  }

  DP_TEST_END ("rs_core");
}
