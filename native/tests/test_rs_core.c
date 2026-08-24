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
 *   - the parity IS the remainder modulo g(x), by long division (§2b);
 *   - a single error's syndromes are the closed form the header prints, which
 *     fixes the wire order absolutely rather than against another function in
 *     the same file (§3b);
 *   - E symbol errors are corrected EXACTLY, at every configuration, and so
 *     is every count below E (§4b);
 *   - E+1 errors never RECOVER the transmitted word, which is impossible for
 *     a bounded-distance decoder and would betray a decoder measuring itself;
 *   - a decode either refuses or returns a codeword — never a third thing;
 *   - a refusal leaves the caller's buffer untouched;
 *   - the description carries no running state (§8).
 *
 * The two configurations with a non-textbook first root or stride are what
 * make the exact-recovery checks bite: a decoder that assumed `first_root ==
 * 1` finds the right error POSITIONS and the wrong magnitudes, so it still
 * "decodes" and the word it returns is not the one that was sent. See
 * docs/design/reed-solomon.md §4.
 */
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
     codeword's sphere, and since the spheres are disjoint that probability
     is the fraction of the space they fill:

         P = V(E) / q^(n-k),   V(E) = sum_{i<=E} C(n,i) (q-1)^i

     At E = 16 that is 2.6e-14, so refusing is the only outcome ever seen.
     At RS(15,11) it is 0.364, so a third of these miscorrect and demanding
     many more refusals would be pinning the seed rather than the code.

     Note what the textbook `~1/E!` would have said: 0.5 at RS(15,11),
     which is 37 % high, and 4.8e-14 at E = 16, which is 83 % high. It is
     what is left of the expression above after two approximations --
     C(n,E) ~ n^E/E! and n ~ q -- and BOTH cost something, the first growing
     with E and the second with a small field. The certification measured
     all of it: src/doppler/tests/validation/rs/results.md §2.2.

     Measured here: 64/64, 64/64, 46/64. */
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

/* Multiply in the field the code built.
 *
 * rs_core.c's own gf_mul is static, and reaching into it would check the file
 * against itself. This is the definition of the tables rs_t publishes: they
 * are transparent members precisely so a caller — or a test — can evaluate a
 * polynomial without the implementation's help. */
static uint8_t
gf (const rs_t *rs, uint8_t a, uint8_t b)
{
  if (a == 0 || b == 0)
    return 0;
  return rs->exp[rs->log[a] + rs->log[b]];
}

/* `a^e` for any integer exponent, reduced into [0, n). */
static uint8_t
gf_pow_a (const rs_t *rs, long e)
{
  long m = e % (long)rs->n;
  if (m < 0)
    m += (long)rs->n;
  return rs->exp[m];
}

/* Evaluate a polynomial held the way rs_generator() holds one: `c[i]` is the
   coefficient of `x^i`. */
static uint8_t
poly_eval (const rs_t *rs, const uint8_t *c, unsigned len, uint8_t x)
{
  uint8_t acc = 0;
  for (unsigned i = len; i-- > 0;)
    acc = (uint8_t)(gf (rs, acc, x) ^ c[i]);
  return acc;
}

/* Evaluate a polynomial held the way the WIRE holds one: index `i` carries
   `x^(n-1-i)`, so the first symbol transmitted is the highest-order
   coefficient. The header states this convention and everything below that
   depends on it says so. */
static uint8_t
word_eval (const rs_t *rs, const uint8_t *w, unsigned len, uint8_t x)
{
  uint8_t acc = 0;
  for (unsigned i = 0; i < len; i++)
    acc = (uint8_t)(gf (rs, acc, x) ^ w[i]);
  return acc;
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
  /* ── 1. the description is validated, not trusted ───────────────────── */
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

  /* ── 1b. the sizes it derives, the range it declares, and the one thing
   *        rs_code_valid deliberately does NOT check ────────────────────
   *
   * §1 walks the refusals. This walks the other side of the same door: the
   * three derived sizes every caller allocates against, the floor and the
   * ceiling of the declared range (a bound nothing constructs is a bound
   * nobody has tried), and the primitivity check the doc comment promises
   * rs_code_valid does not make — asserted as an ACCEPT, because "does not
   * check" is only visible from the accepting side.
   */
  {
    for (size_t ci = 0; ci < sizeof CODES / sizeof CODES[0]; ci++)
      {
        const rs_code_t *code = &CODES[ci].code;
        rs_t             rs;
        DP_REQUIRE_MSG (rs_init (&rs, code), CODES[ci].name);

        const unsigned n = (1u << code->symbol_bits) - 1u;
        DP_CHECK_MSG (rs.n == n, "n must be 2^J - 1");
        DP_CHECK_MSG (rs.k == n - code->nroots, "k must be n - nroots");
        DP_CHECK_MSG (rs.e == code->nroots / 2u, "E must be nroots / 2");
        DP_CHECK_MSG (rs_generator (&rs) == rs.gen,
                      "rs_generator must point into the code it was given");
      }

    /* The declared FLOOR. J = 2 is in range, so RS(3,1) over GF(4) is a
       code this file admits, and the smallest one there is. */
    {
      rs_t            rs;
      const rs_code_t tiny = { .symbol_bits = 2,
                               .field_poly  = 0x3u,
                               .nroots      = 2,
                               .first_root  = 1,
                               .root_stride = 1 };
      DP_CHECK_MSG (rs_init (&rs, &tiny),
                    "the declared floor J = 2 must be a usable code");
      DP_CHECK_MSG (rs.n == 3u && rs.k == 1u && rs.e == 1u,
                    "...which is RS(3,1), correcting one symbol");

      uint8_t word[3] = { 2u, 0u, 0u }, before[3];
      rs_encode (&rs, word, word + 1);
      memcpy (before, word, sizeof word);
      DP_CHECK_MSG (rs_codeword_ok (&rs, word),
                    "...and must encode a word its own syndromes accept");
      word[2] ^= 1u;
      DP_CHECK_MSG (rs_decode (&rs, word) == 1
                        && memcmp (word, before, sizeof word) == 0,
                    "...and must correct the one error it claims to");
    }

    /* The declared CEILING, and one step past it. nroots + 2 rather than
       + 1 so the parity check is not what refuses it: this is the bound
       being tested, not the evenness rule §1 already covers. */
    {
      rs_t      rs;
      rs_code_t c = { .symbol_bits = 8,
                      .field_poly  = 0x1Du,
                      .nroots      = RS_NROOTS_MAX,
                      .first_root  = 1,
                      .root_stride = 1 };
      DP_CHECK_MSG (rs_init (&rs, &c),
                    "nroots = RS_NROOTS_MAX must be a usable code");
      DP_CHECK_MSG (rs.e == RS_NROOTS_MAX / 2u, "...correcting E = 32");
      c.nroots = RS_NROOTS_MAX + 2u;
      DP_CHECK_MSG (!rs_code_valid (&c), "...and one step past it must not");
    }

    /* "Leaves room for at least one information symbol" — the edge is
       n-1 parity, which leaves exactly one. §1 pins only the far side of
       it (nroots >= n), which a check off by one still passes. */
    {
      rs_t      rs;
      rs_code_t c = { .symbol_bits = 4,
                      .field_poly  = 0x03u,
                      .nroots      = 14,
                      .first_root  = 1,
                      .root_stride = 1 };
      DP_CHECK_MSG (rs_code_valid (&c),
                    "n-1 parity symbols leave exactly one information "
                    "symbol, which is room enough");
      DP_REQUIRE (rs_init (&rs, &c));
      DP_CHECK_MSG (rs.k == 1u, "...and that is what k = 1 means");
    }

    /* field_poly is bounded on both sides, and symbol_bits below as well
       as above. */
    {
      rs_t      rs;
      rs_code_t c = { .symbol_bits = 4,
                      .field_poly  = 0x03u,
                      .nroots      = 4,
                      .first_root  = 1,
                      .root_stride = 1 };
      DP_REQUIRE (rs_code_valid (&c));

      c.field_poly = 0u;
      DP_CHECK_MSG (!rs_code_valid (&c),
                    "an empty field polynomial is not a polynomial");
      c.field_poly = 0x02u;
      DP_CHECK_MSG (!rs_code_valid (&c),
                    "F(x) with no constant term is divisible by x, so it is "
                    "not even irreducible");
      /* 0x11 and not the boundary 1<<J: 2^J is EVEN, so the constant-term
         rule above refuses it and the width check is never what fires — a
         reject passing for the wrong reason. An odd too-wide value leaves
         exactly one rule that can refuse it. */
      c.field_poly = 0x11u;
      DP_CHECK_MSG (!rs_code_valid (&c),
                    "F(x) is held without its x^J term, so it must fit in J "
                    "bits");

      /* The doc comment's own carve-out, from the side that can see it. */
      c.field_poly = 0x0Fu;
      DP_CHECK_MSG (rs_code_valid (&c),
                    "rs_code_valid does NOT check primitivity — it says so, "
                    "because that costs the table build");
      DP_CHECK_MSG (!rs_init (&rs, &c),
                    "...and rs_init, which pays for the table, is where a "
                    "non-primitive polynomial is caught");
      c.field_poly = 0x03u;

      c.symbol_bits = 1;
      DP_CHECK_MSG (!rs_code_valid (&c),
                    "a one-bit symbol is below the declared floor");
    }
  }

  /* ── 1c. WHY a stride sharing a factor with n is refused ──────────────
   *
   * §1 asserts the refusal. The header states the REASON — such a code
   * "corrects fewer errors than its parity count claims" — and a refusal
   * cannot show that. Nothing else can either: the encoder and the decoder
   * would agree with each other perfectly, which is the trap this whole
   * file is arranged around.
   *
   * At RS(15,11) with s = 3 the four roots are a^3, a^6, a^9, a^12 — the
   * non-unit fifth roots of unity. Their product is (x^5 - 1)/(x - 1), so
   * x^5 - 1 = (x - 1) g(x) is a multiple of g of weight TWO. A code with a
   * weight-2 codeword has distance 2 and corrects NOTHING, while its parity
   * count claims E = 2.
   *
   * Evaluated in the field the accepted code built: the field is a property
   * of symbol_bits and field_poly, and the stride does not touch it.
   */
  {
    rs_t rs;
    DP_REQUIRE (rs_init (&rs, &CODES[2].code)); /* RS(15,11), s = 1 */

    uint8_t word[RS_N_MAX] = { 0 };
    word[rs.n - 1u - 5u]   = 1u; /* x^5 */
    word[rs.n - 1u]        = 1u; /* x^0 */

    int vanishes = 1;
    for (unsigned m = 0; m < 4u; m++)
      if (word_eval (&rs, word, rs.n, gf_pow_a (&rs, 3L * (long)(1u + m)))
          != 0u)
        vanishes = 0;

    DP_CHECK_MSG (vanishes,
                  "with gcd(s, n) = 3 a weight-2 word satisfies every root, "
                  "so the distance is 2 and not nroots + 1");
    DP_CHECK_MSG (!rs_codeword_ok (&rs, word),
                  "...and that same word is NOT a codeword of the accepted "
                  "stride, which is the difference the check protects");
  }

  for (size_t ci = 0; ci < sizeof CODES / sizeof CODES[0]; ci++)
    {
      const rs_code_t *code = &CODES[ci].code;
      rs_t             rs;
      DP_REQUIRE_MSG (rs_init (&rs, code), CODES[ci].name);

      /* ── 2. g(x) vanishes at every root it claims ────────────────────── */
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
            if (poly_eval (&rs, g, code->nroots + 1u, rs.exp[je]) != 0)
              all = 0;
          }
        DP_CHECK_MSG (all, "g(x) must vanish at each of its own roots");
        DP_CHECK_MSG (g[code->nroots] == 1u, "g(x) must be monic");
        DP_CHECK_MSG (g[0] != 0u, "g(x) must have degree exactly 2E");
      }

      /* ── 2b. the parity IS the remainder modulo g(x) ─────────────────
       *
       * The header says exactly what rs_encode computes: "the remainder of
       * info(x) * x^nroots modulo g(x), highest-order coefficient first,
       * which is the order it is transmitted in". §3 pins only that the
       * syndromes of the result vanish — and rs_encode and rs_syndromes
       * could satisfy that together while both read the wire backwards.
       *
       * So divide, by hand, in the order the header names: coefficient of
       * x^(n-1-i) in slot i, reduced modulo the generator rs_generator()
       * publishes. Long division is the DEFINITION of the remainder, not a
       * second copy of the LFSR — and it fixes the wire order absolutely
       * rather than against another function in this same file.
       */
      {
        uint8_t        info[RS_N_MAX], parity[RS_NROOTS_MAX];
        uint8_t        work[RS_N_MAX] = { 0 };
        uint32_t       seed           = 606u + (uint32_t)ci;
        const uint8_t  mask = (uint8_t)((1u << code->symbol_bits) - 1u);
        const uint8_t *g    = rs_generator (&rs);

        for (unsigned i = 0; i < rs.k; i++)
          info[i] = (uint8_t)(dp_xs32 (&seed) & mask);
        rs_encode (&rs, info, parity);

        /* info(x) * x^nroots: the information in the high k slots, the
           parity positions zero. */
        memcpy (work, info, rs.k);

        /* g is monic, so the quotient coefficient at each step IS the
           leading coefficient being cleared. */
        for (unsigned i = 0; i < rs.k; i++)
          {
            const uint8_t q = work[i];
            if (q == 0)
              continue;
            for (unsigned j = 1; j <= code->nroots; j++)
              work[i + j] ^= gf (&rs, q, g[code->nroots - j]);
            work[i] = 0;
          }

        DP_CHECK_MSG (memcmp (work + rs.k, parity, code->nroots) == 0,
                      "the parity must BE info(x)*x^nroots mod g(x), "
                      "highest-order coefficient first");
      }

      /* ── 3. systematic, and the syndromes say so ─────────────────────── */
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

      /* ── 3b. the syndromes are the closed form the header prints ──────
       *
       * "S_m = C(a^(s*(j0+m))), evaluated over the codeword as a polynomial
       * with index i carrying x^(n-1-i)."
       *
       * §3 pins that a codeword's syndromes are zero and a corrupted word's
       * are not, which an implementation with the WRONG root set satisfies
       * so long as the encoder shares its mistake. Evaluation is linear, so
       * a single error of value v at index i has, in closed form,
       *
       *     S_m = v * (a^(s*(j0+m)))^(n-1-i)
       *
       * and that expression contains everything a caller has to agree with
       * a stranger about: the field, the stride, the first root and which
       * end of the buffer is the high-order coefficient.
       */
      {
        uint8_t       sent[RS_N_MAX], rx[RS_N_MAX], syn[RS_NROOTS_MAX];
        uint32_t      seed = 313u + (uint32_t)ci;
        const uint8_t mask = (uint8_t)((1u << code->symbol_bits) - 1u);
        int           all  = 1;

        make_codeword (&rs, sent, &seed);

        /* Both ends and a middle: the position enters as an exponent, so an
           off-by-one in either direction survives a single sample. */
        const unsigned where[] = { 0u, 1u, rs.n / 3u, rs.k, rs.n - 1u };
        for (size_t w = 0; w < sizeof where / sizeof where[0]; w++)
          {
            const unsigned i = where[w];
            uint8_t        v;
            do
              v = (uint8_t)(dp_xs32 (&seed) & mask);
            while (v == 0);

            memcpy (rx, sent, rs.n);
            rx[i] ^= v;
            rs_syndromes (&rs, rx, syn);

            for (unsigned m = 0; m < code->nroots; m++)
              {
                const long root_e
                    = (long)((code->root_stride * (code->first_root + m))
                             % rs.n);
                const uint8_t want = gf (
                    &rs, v, gf_pow_a (&rs, root_e * (long)(rs.n - 1u - i)));
                if (syn[m] != want)
                  all = 0;
              }
          }
        DP_CHECK_MSG (all, "a single error's syndromes must be v * X^(n-1-i), "
                           "the closed form the header prints");
      }

      /* ── 3c. symbols are packed, one per byte ─────────────────────────
       *
       * "A Reed-Solomon symbol IS a byte at J = 8, and at J < 8 it is a byte
       * with the top bits clear." The information symbols are the caller's
       * and trivially satisfy this; the claim is about what the CODEC
       * produces — parity out of rs_encode, and repaired symbols out of
       * rs_decode. This is vacuous at J = 8 and has teeth at J = 4 and
       * J = 2, which is why it runs at every configuration rather than at
       * the small one alone.
       */
      {
        uint8_t  word[RS_N_MAX];
        uint32_t seed  = 8080u + (uint32_t)ci;
        int      clean = 1;

        make_codeword (&rs, word, &seed);
        for (unsigned i = rs.k; i < rs.n; i++)
          if ((word[i] >> code->symbol_bits) != 0)
            clean = 0;
        DP_CHECK_MSG (clean, "every parity symbol must fit in J bits");

        inject (&rs, word, rs.e, &seed);
        DP_CHECK_MSG (rs_decode (&rs, word) == (int)rs.e,
                      "the E injected errors must be repaired, so the scan "
                      "below sees symbols the decoder wrote");
        for (unsigned i = 0; i < rs.n; i++)
          if ((word[i] >> code->symbol_bits) != 0)
            clean = 0;
        DP_CHECK_MSG (clean, "and so must every repaired symbol");
      }

      /* ── 4. E errors are corrected, exactly, and E+1 never recovers ─── */
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
                      "E+1 errors must be REFUSED as often as the sphere "
                      "model says");
      }

      /* ── 4b. "up to E", meaning every count below it too ──────────────
       *
       * §4 injects exactly E, and §7 sweeps exactly one at RS(15,11). The
       * header claims the whole range, and the middle of it was pinned
       * nowhere: a decoder that reported E whatever it repaired, or one
       * whose locator degree and repair count parted company, passes §4 and
       * §7 and is wrong at every count between them.
       */
      {
        enum
        {
          PER_COUNT = 8
        };
        uint32_t seed        = 1717u + (uint32_t)ci;
        int      all_fixed   = 1;
        int      all_counted = 1;

        for (unsigned errs = 1; errs <= rs.e; errs++)
          {
            for (unsigned t = 0; t < PER_COUNT; t++)
              {
                uint8_t sent[RS_N_MAX], rx[RS_N_MAX];
                make_codeword (&rs, sent, &seed);
                memcpy (rx, sent, rs.n);
                inject (&rs, rx, errs, &seed);

                if (rs_decode (&rs, rx) != (int)errs)
                  all_counted = 0;
                if (memcmp (rx, sent, rs.n) != 0)
                  all_fixed = 0;
              }
          }

        DP_CHECK_MSG (all_counted,
                      "j errors must be reported as j repairs, for every j "
                      "from 1 to E");
        DP_CHECK_MSG (all_fixed, "...and every one of them corrected");
      }

      /* ── 5. refuse or return a codeword; never a third thing ─────────── */
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

      /* ── 6. a clean word is left alone, and costs nothing ────────────── */
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

  /* ── 7. RS(15,11): small enough to sweep every single error there is ─── */
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

  /* ── 8. the description carries no running state ──────────────────────
   *
   * "Build it with rs_init and then treat it as read-only: it carries no
   * running state, and every function below takes it as const."
   *
   * `const` is a promise about a POINTER, and a cast inside the file — or a
   * scratch member reused across calls — satisfies the compiler while making
   * the second codeword depend on the first. That defect is invisible to
   * every other section here, because they all run one workload.
   *
   * So: two independent builds of one code must be byte-identical, which is
   * what makes a caller's stack copy interchangeable with anyone else's; and
   * a full workload — encode, syndromes, a decode that corrects and a decode
   * that refuses — must leave the description bit-for-bit as rs_init wrote
   * it. rs_init memsets before filling, so the padding is deterministic and
   * a whole-struct memcmp is a fair comparison.
   */
  {
    rs_t a, b;
    DP_REQUIRE (rs_init (&a, &CODES[1].code)); /* the CCSDS-shaped one */
    DP_REQUIRE (rs_init (&b, &CODES[1].code));
    DP_CHECK_MSG (memcmp (&a, &b, sizeof a) == 0,
                  "two builds of one code must be byte-identical");

    const rs_t before = a;

    uint8_t  word[RS_N_MAX], syn[RS_NROOTS_MAX];
    uint32_t seed = 31415u;
    make_codeword (&a, word, &seed);
    rs_syndromes (&a, word, syn);
    (void)rs_codeword_ok (&a, word);

    inject (&a, word, a.e, &seed);
    DP_CHECK_MSG (rs_decode (&a, word) == (int)a.e, "the correcting decode");

    inject (&a, word, a.n / 2u, &seed);
    DP_CHECK_MSG (rs_decode (&a, word) < 0, "and the refusing one");

    DP_CHECK_MSG (memcmp (&a, &before, sizeof a) == 0,
                  "a full workload must leave the description untouched");
  }

  DP_TEST_END ("rs_core");
}
