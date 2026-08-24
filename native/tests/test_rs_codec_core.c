/*
 * test_rs_codec_core.c — the codec object, at its own component.
 *
 * `rs_codec` is an OBJECT over `rs`, not a second Reed-Solomon, so this file
 * does not re-derive what `test_rs_core.c` already pins: the field
 * arithmetic, the generator, the syndromes, the decoder's radius. Those are
 * the CODE's claims, they are checked there against the code's own distance,
 * and checking them again here would be two files agreeing about one kernel.
 *
 * What only this object can be wrong about:
 *
 * 1. **The BINDING of a code to its tables.** create() takes five loose
 *    integers and must assemble the same `rs_code_t` the caller named — and
 *    must REFUSE the two that produce self-consistent nonsense, since a
 *    round trip cannot.
 * 2. **The placement of a systematic codeword.** `rs_encode` returns parity
 *    alone; this object answers in whole codewords, and getting that
 *    placement wrong produces a word that decodes against itself and matches
 *    no other implementation.
 * 3. **The length contract.** Every method here takes a length the kernels
 *    do not, because Python hands them arrays rather than pointers, and a
 *    wrong length must be refused rather than read past.
 */
#include "dp_rng_test.h"
#include "dp_test.h"

#include "rs_codec/rs_codec_core.h"

#include <stdlib.h>
#include <string.h>

/* The default GF(256): x^8 + x^4 + x^3 + x^2 + 1, textbook root set. */
#define POLY_DEFAULT 0x1Du

int
main (void)
{
  /* ── 1. the five arguments become the code the caller named ───────────────
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    DP_CHECK_MSG (rs_codec_get_n (rs) == 255u, "n = 2^J - 1");
    DP_CHECK_MSG (rs_codec_get_k (rs) == 223u, "k = n - nroots");
    DP_CHECK_MSG (rs_codec_get_e (rs) == 16u, "E = nroots / 2");
    DP_CHECK (rs_codec_get_nroots (rs) == 32u);
    DP_CHECK (rs_codec_get_symbol_bits (rs) == 8u);
    rs_codec_destroy (rs);

    /* A different field entirely: J = 4 is GF(16), n = 15. If create() had
       hardcoded the byte-wide case anywhere -- in the table build, in a
       length, in a mask -- every assertion above still passes. */
    rs_codec_state_t *s = rs_codec_create (4u, 4u, 0x3u, 1u, 1u);
    DP_REQUIRE (s != NULL);
    DP_CHECK_MSG (rs_codec_get_n (s) == 15u && rs_codec_get_k (s) == 11u,
                  "GF(16) gives RS(15,11)");
    DP_CHECK (rs_codec_get_symbol_bits (s) == 4u);
    rs_codec_destroy (s);
  }

  /* ── 2. the two arguments that must be VALIDATED, not trusted ─────────────
   *
   * Both produce arithmetic that is entirely self-consistent, so an
   * encode/decode round trip against a matching decoder passes with either
   * of them wrong. The constructor is the only place they can be caught,
   * which is exactly why it is a `create_error` rather than a MemoryError.
   */
  {
    /* 0x1C is x^8 + x^4 + x^3 + x^2, not primitive -- it generates a
       subgroup rather than the field. */
    DP_CHECK_MSG (rs_codec_create (32u, 8u, 0x1Cu, 1u, 1u) == NULL,
                  "a non-primitive field polynomial must be refused");

    /* gcd(5, 255) = 5, so the 32 'roots' are not distinct and the code
       corrects fewer errors than its parity count claims. */
    DP_CHECK_MSG (rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 5u) == NULL,
                  "a root stride sharing a factor with n must be refused");

    /* And the plain range refusals, so a caller gets an exception rather
       than a codec that cannot work. */
    DP_CHECK_MSG (rs_codec_create (31u, 8u, POLY_DEFAULT, 1u, 1u) == NULL,
                  "an odd nroots is not 2E");
    DP_CHECK_MSG (rs_codec_create (0u, 8u, POLY_DEFAULT, 1u, 1u) == NULL,
                  "a code with no parity is not a code");
    DP_CHECK_MSG (rs_codec_create (32u, 4u, 0x3u, 1u, 1u) == NULL,
                  "nroots must leave at least one information symbol");
  }

  /* ── 3. encode places a SYSTEMATIC codeword ────────────────────────────────
   *
   * The object's own claim, and the one `rs_encode` cannot make: information
   * first, untouched, then the parity the kernel computed. A codeword built
   * the other way round decodes perfectly against an equally-reversed
   * decoder and matches no other implementation of this code.
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs);

    uint8_t *info = malloc (k), *word = malloc (n);
    DP_REQUIRE (info && word);
    uint32_t st = 12345u;
    for (size_t i = 0; i < k; i++)
      info[i] = (uint8_t)(dp_xs32 (&st) & 0xFFu);

    DP_REQUIRE (rs_codec_encode (rs, info, k, word, n) == n);
    DP_CHECK_MSG (memcmp (word, info, k) == 0,
                  "systematic: the information is carried through untouched");

    /* The parity is the kernel's, at the kernel's own offset -- checked
       against rs_encode directly rather than against a second computation
       here, which would only prove this file agrees with itself. */
    uint8_t parity[32];
    rs_encode (&rs->rs, info, parity);
    DP_CHECK_MSG (memcmp (word + k, parity, 32u) == 0,
                  "the parity is rs_encode's, placed after the information");

    DP_CHECK_MSG (rs_codec_codeword_ok (rs, word, n),
                  "...and the whole thing is therefore a codeword");

    free (info);
    free (word);
    rs_codec_destroy (rs);
  }

  /* ── 4. decode corrects IN PLACE, and reports the count ────────────────────
   *
   * In place is the contract the binding relies on: it hands the kernel the
   * caller's own numpy buffer, so a decode that worked on a copy would
   * return the right count and leave the caller's data wrong.
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs),
                 e = rs_codec_get_e (rs);

    uint8_t *info = malloc (k), *word = malloc (n), *clean = malloc (n);
    DP_REQUIRE (info && word && clean);
    uint32_t st = 777u;
    for (size_t i = 0; i < k; i++)
      info[i] = (uint8_t)(dp_xs32 (&st) & 0xFFu);
    DP_REQUIRE (rs_codec_encode (rs, info, k, clean, n) == n);

    /* Exactly E errors: the decoder's radius, so this must succeed and must
       report every one of them. */
    memcpy (word, clean, n);
    for (size_t i = 0; i < e; i++)
      word[i * 7u] ^= 0xA5u;
    DP_CHECK_MSG (rs_codec_decode (rs, word, n) == (int)e,
                  "E errors are corrected, and counted");
    DP_CHECK_MSG (memcmp (word, clean, n) == 0,
                  "...in the CALLER's buffer, which now holds the codeword");

    /* A clean codeword: zero corrected, not a refusal and not a repair. */
    memcpy (word, clean, n);
    DP_CHECK_MSG (rs_codec_decode (rs, word, n) == 0,
                  "an already-valid codeword costs no corrections");

    free (info);
    free (word);
    free (clean);
    rs_codec_destroy (rs);
  }

  /* ── 5. the length contract ────────────────────────────────────────────────
   *
   * Every method takes a length the kernels do not, because the binding
   * hands it whatever array the caller passed. A wrong length must be
   * refused rather than read past -- and the two negative decode codes are
   * different KINDS of fact, so they are distinguishable: -1 is the
   * channel's answer, -2 is the caller's mistake.
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs);

    static uint8_t buf[255], out[255];
    memset (buf, 0, sizeof buf);

    DP_CHECK_MSG (rs_codec_encode (rs, buf, k - 1u, out, n) == 0,
                  "fewer than k information symbols is not a message");
    DP_CHECK_MSG (rs_codec_encode (rs, buf, k + 1u, out, n) == 0,
                  "...and neither is more");
    DP_CHECK_MSG (rs_codec_encode (rs, buf, k, out, n - 1u) == 0,
                  "a buffer too small is refused, never truncated");

    DP_CHECK_MSG (rs_codec_decode (rs, buf, n - 1u) == -2,
                  "a word of the wrong length is the CALLER's mistake (-2)");
    DP_CHECK_MSG (rs_codec_syndromes (rs, buf, n - 1u, out, 32u) == 0,
                  "syndromes of a wrong-length word are not syndromes");
    DP_CHECK_MSG (!rs_codec_codeword_ok (rs, buf, n - 1u),
                  "a word of the wrong length is not a codeword of this code");

    /* All-zero IS a codeword of every linear code, so the length refusal
       above is doing real work: without it this call would say yes. */
    DP_CHECK_MSG (rs_codec_codeword_ok (rs, buf, n),
                  "...while the all-zero word at the RIGHT length is one");

    rs_codec_destroy (rs);
  }

  /* ── 6. syndromes and the generator are the kernel's, in full ──────────────
   *
   * Both exist so a caller can check an implementation against a published
   * document rather than against itself, so what this pins is that the
   * object passes the whole thing through -- the right count, the right
   * bytes, from the right code.
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs);

    static uint8_t word[255], syn[64], gen[65];
    static uint8_t info[223];
    for (size_t i = 0; i < k; i++)
      info[i] = (uint8_t)(i * 13u + 7u);
    DP_REQUIRE (rs_codec_encode (rs, info, k, word, n) == n);

    /* The IN-PLACE call, which is the one a frame assembler makes: the same
       buffer as source and destination, so the parity is appended to
       information already in position. It must agree with the copy above --
       and memcpy with identical pointers is undefined behaviour, not a
       no-op, so this is a real path and not a courtesy. */
    static uint8_t inplace[255];
    memcpy (inplace, info, k);
    DP_REQUIRE (rs_codec_encode (rs, inplace, k, inplace, n) == n);
    DP_CHECK_MSG (memcmp (inplace, word, n) == 0,
                  "encoding in place gives the same codeword");

    DP_REQUIRE (rs_codec_syndromes (rs, word, n, syn, sizeof syn) == 32u);
    unsigned nz = 0;
    for (unsigned i = 0; i < 32u; i++)
      nz += (syn[i] != 0u);
    DP_CHECK_MSG (nz == 0u, "every syndrome of a codeword is zero");

    word[9] ^= 0x40u;
    DP_REQUIRE (rs_codec_syndromes (rs, word, n, syn, sizeof syn) == 32u);
    nz = 0;
    for (unsigned i = 0; i < 32u; i++)
      nz += (syn[i] != 0u);
    DP_CHECK_MSG (nz > 0u, "...and an error shows in them");

    DP_REQUIRE (rs_codec_generator (rs, gen, sizeof gen) == 33u);
    DP_CHECK_MSG (memcmp (gen, rs_generator (&rs->rs), 33u) == 0,
                  "the generator is the kernel's, whole");
    DP_CHECK_MSG (gen[32] == 1u, "g(x) is monic in its highest coefficient");

    /* A buffer too small writes NOTHING rather than a prefix: a truncated
       generator would compare unequal to a published one for a reason that
       looks like a wrong code. */
    memset (gen, 0xEEu, sizeof gen);
    DP_CHECK_MSG (rs_codec_generator (rs, gen, 32u) == 0u,
                  "a buffer shorter than nroots + 1 is refused");
    DP_CHECK_MSG (gen[0] == 0xEEu, "...and left untouched");

    rs_codec_destroy (rs);
  }

  /* ── 7. every one of the five reaches the arithmetic ──────────────────────
   *
   * Added because the sabotage found the gap: an object that IGNORED
   * `first_root` and always used 1 passed every section above. That is not a
   * hypothetical slip -- CCSDS 4.3.4 uses `j0 = 128 - E` and a stride of 11,
   * and a codec that quietly substituted the textbook root set would produce
   * a perfectly good (255,223) code that no CCSDS receiver can decode, while
   * round-tripping against itself forever.
   *
   * So the check is INTEROPERABILITY, stated directly: a word built under
   * one code must not be a codeword under a code differing in exactly one
   * parameter. Nothing weaker catches a dropped argument, because every
   * self-consistent code passes its own tests.
   */
  {
    static const struct
    {
      const char *what;
      uint32_t    nroots, symbol_bits, field_poly, first_root, root_stride;
      int         interops; /* is the base word still a codeword here? */
    } OTHER[] = {
      { "first_root", 32u, 8u, POLY_DEFAULT, 112u, 1u, 0 },
      { "root_stride", 32u, 8u, POLY_DEFAULT, 1u, 11u, 0 },
      { "field_poly", 32u, 8u, 0x87u, 1u, 1u, 0 },
      /* NESTED, and it is the algebra rather than a leak: with the same
         field and root stride, RS(255,239)'s roots a^1..a^16 are a SUBSET of
         RS(255,223)'s a^1..a^32, so every codeword of the stronger code is
         one of the weaker. A test that asserted non-interoperability here
         would be asserting something false about Reed-Solomon. `nroots`
         still has to reach the arithmetic, and the generator check below is
         what says so. */
      { "nroots (subset roots)", 16u, 8u, POLY_DEFAULT, 1u, 1u, 1 },
      /* ...and the containment is one-way: widen the root set and the word
         stops qualifying. */
      { "nroots (superset roots)", 64u, 8u, POLY_DEFAULT, 1u, 1u, 0 },
    };

    rs_codec_state_t *base = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (base != NULL);
    const size_t n = rs_codec_get_n (base), k = rs_codec_get_k (base);

    static uint8_t info[223], word[255], gbase[65], gother[65];
    uint32_t       st = 31337u;
    for (size_t i = 0; i < k; i++)
      info[i] = (uint8_t)(dp_xs32 (&st) & 0xFFu);
    DP_REQUIRE (rs_codec_encode (base, info, k, word, n) == n);
    DP_REQUIRE (rs_codec_generator (base, gbase, sizeof gbase) == 33u);

    for (size_t c = 0; c < sizeof OTHER / sizeof OTHER[0]; c++)
      {
        rs_codec_state_t *alt = rs_codec_create (
            OTHER[c].nroots, OTHER[c].symbol_bits, OTHER[c].field_poly,
            OTHER[c].first_root, OTHER[c].root_stride);
        DP_REQUIRE_MSG (alt != NULL, OTHER[c].what);

        const size_t gn = rs_codec_get_nroots (alt) + 1u;
        DP_REQUIRE (rs_codec_generator (alt, gother, sizeof gother) == gn);
        DP_CHECK_MSG (gn != 33u || memcmp (gbase, gother, 33u) != 0,
                      "changing one parameter must change the code");

        /* The claim that matters to a caller: whether the two interoperate,
           each case carrying the answer its own algebra gives. */
        DP_CHECK_MSG (rs_codec_codeword_ok (alt, word, n) == OTHER[c].interops,
                      OTHER[c].what);

        rs_codec_destroy (alt);
      }
    rs_codec_destroy (base);
  }

  /* ── 8. the *_max_out functions ARE the allocation contract ───────────────
   *
   * Added because the sabotage found the gap: an off-by-one in
   * `rs_codec_generator_max_out` passed every section above, and it is not a
   * cosmetic error. The binding allocates from these -- `generator` is
   * declared `exact_max_out`, so Python would allocate 32 bytes and the
   * kernel would write 33 into them. A heap overflow, from a function
   * nothing was checking.
   *
   * So each bound is checked against what its method actually WRITES, which
   * is the only comparison that means anything. Checking it against a
   * literal would just be the same arithmetic typed twice.
   */
  {
    rs_codec_state_t *rs = rs_codec_create (32u, 8u, POLY_DEFAULT, 1u, 1u);
    DP_REQUIRE (rs != NULL);
    const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs);

    static uint8_t info[223], word[255], big[512];
    memset (info, 0x5Au, sizeof info);

    DP_CHECK_MSG (rs_codec_encode_max_out (rs, k)
                      == rs_codec_encode (rs, info, k, word, n),
                  "encode_max_out must equal what encode writes");
    DP_CHECK_MSG (rs_codec_syndromes_max_out (rs, n)
                      == rs_codec_syndromes (rs, word, n, big, sizeof big),
                  "syndromes_max_out must equal what syndromes writes");
    DP_CHECK_MSG (rs_codec_generator (rs, big, sizeof big)
                      == rs_codec_get_nroots (rs) + 1u,
                  "generator writes nroots + 1 coefficients");

    /* And they must not depend on the caller's count, because the binding
       may not pass one: a bound that shrank with `n_in` would under-allocate
       the moment somebody called it the other way. */
    DP_CHECK (rs_codec_encode_max_out (rs, 0u) == n);
    DP_CHECK (rs_codec_syndromes_max_out (rs, 0u) == 32u);

    /* At a second, smaller code too -- a bound that had been hardcoded to
       the 255-symbol case would pass everything above. */
    rs_codec_state_t *s = rs_codec_create (4u, 4u, 0x3u, 1u, 1u);
    DP_REQUIRE (s != NULL);
    DP_CHECK (rs_codec_encode_max_out (s, 0u) == 15u);
    DP_CHECK (rs_codec_syndromes_max_out (s, 0u) == 4u);
    rs_codec_destroy (s);

    rs_codec_destroy (rs);
  }

  DP_TEST_END ("rs_codec_core");
}
