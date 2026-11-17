/*
 * test_conv_core.c — the convolutional code description and its encoder.
 *
 * The external truth is the IMPULSE RESPONSE, which is what a generator
 * polynomial means: drive a 1 followed by zeros and output j must trace
 * poly[j] bit for bit, inverted where the code says so. It is checkable for
 * every configuration rather than only the one this was developed against,
 * and it is not a round trip -- a decoder matched to a wrong encoder decodes
 * perfectly and interoperates with nothing, which is why the encoder is
 * pinned against the polynomial here and never against the decoder.
 *
 * The DECODER is test_viterbi_core.c's: `viterbi` is its own declared
 * component (doppler#893), and everything it claims -- the sweep over codes,
 * scale invariance, error correction, node synchronization, the free
 * distance and the state blob -- is asserted there.
 *
 * Nothing here includes `ccsds_tm`. The CCSDS configuration is one row in
 * the table below, and holding it to the STANDARD's printed values is
 * test_ccsds_tm_conv.c's job.
 *
 * Generality is asserted rather than assumed: the sections below run k from 3
 * to 9 and n from 1 to 3, because an encoder that only works at the one
 * configuration it was developed against is a CCSDS encoder wearing a
 * parameter list.
 *
 * The design is docs/design/viterbi.md.
 */
#include "dp_rng_test.h"
#include "dp_test.h"

#include "doppler/conv/conv_core.h"

#include <string.h>

/* CCSDS 131.0-B-3 section 3.3, as a conv_code_t: G1 = 171, G2 = 133 octal,
   with the inversion on the SECOND output and nothing else. */
static const conv_code_t CCSDS
    = { .k = 7u, .n = 2u, .poly = { 0171u, 0133u }, .invert = 0x2u };

int
main (void)
{
  /* ── 1. validation, and what it refuses ────────────────────────────────
   *
   * A zero polynomial is an output carrying no information and a polynomial
   * wider than the register is a transcription that lost its alignment. Both
   * are typos, and a codec that accepts them produces a code nobody else has.
   */
  {
    DP_CHECK (dp_conv_code_valid (&CCSDS));
    conv_code_t bad = CCSDS;
    bad.k           = 1u;
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "k < 2 is not a code");
    bad   = CCSDS;
    bad.k = CONV_K_MAX + 1u;
    DP_CHECK (!dp_conv_code_valid (&bad));
    bad   = CCSDS;
    bad.n = 0u;
    DP_CHECK (!dp_conv_code_valid (&bad));
    bad   = CCSDS;
    bad.n = CONV_N_MAX + 1u;
    DP_CHECK (!dp_conv_code_valid (&bad));
    bad         = CCSDS;
    bad.poly[1] = 0u;
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "a zero polynomial is a typo");
    bad         = CCSDS;
    bad.poly[0] = 1u << CCSDS.k; /* one bit too wide */
    DP_CHECK_MSG (!dp_conv_code_valid (&bad),
                  "a polynomial wider than k lost its alignment");
    DP_CHECK (!dp_conv_code_valid (NULL));
    DP_CHECK (conv_states (&CCSDS) == 64u);
  }

  /* ── 2. the impulse response IS the generator polynomial ───────────────
   *
   * What a generator polynomial MEANS: a 1 followed by zeros walks a single
   * bit through the register, so output j reads off poly[j] one tap at a
   * time -- inverted wherever the code says so. That makes the polynomial as
   * written the external truth, for every code and not just the familiar one,
   * and it is what catches a state convention derived backwards (perfectly
   * self-consistent, decodes nothing a conforming encoder produced) or an
   * inversion applied to the wrong output.
   */
  {
    static const conv_code_t IMP[] = {
      { .k = 3u, .n = 2u, .poly = { 07u, 05u }, .invert = 0u },
      { .k      = 7u,
        .n      = 2u,
        .poly   = { 0171u, 0133u },
        .invert = 0x2u }, /* CCSDS */
      { .k = 7u, .n = 3u, .poly = { 0171u, 0165u, 0133u }, .invert = 0x5u },
      { .k = 9u, .n = 2u, .poly = { 0753u, 0561u }, .invert = 0u },
    };
    for (size_t ci = 0; ci < sizeof IMP / sizeof IMP[0]; ci++)
      {
        const conv_code_t *c              = &IMP[ci];
        uint8_t            in[CONV_K_MAX] = { 1u };
        uint8_t            out[CONV_K_MAX * CONV_N_MAX];
        conv_enc_t         e;
        dp_conv_enc_init (&e);
        const size_t got = dp_conv_encode (&e, c, in, c->k, out, sizeof out);
        DP_REQUIRE (got == (size_t)c->k * c->n);

        for (unsigned j = 0; j < c->n; j++)
          {
            const unsigned inv = (c->invert >> j) & 1u;
            for (unsigned t = 0; t < c->k; t++)
              {
                /* At step t the 1 sits t stages down from the top, so the tap
                   it meets is bit (k-1-t) of the polynomial. */
                const unsigned tap
                    = (unsigned)((c->poly[j] >> (c->k - 1u - t)) & 1u);
                DP_CHECK_MSG (out[t * c->n + j] == (tap ^ inv),
                              "output j must trace poly[j], inverted where "
                              "the code says so");
              }
          }
      }
  }

  /* ── 2b. dp_conv_outputs and conv_next_state, the two the trellis is built
   *      from — and which nothing here had ever called ─────────────────────
   *
   * The file docstring calls `dp_conv_outputs` "the only place that says what
   * this family of codes emits", and the register convention "load-bearing"
   * — a trellis derived the other way round is self-consistent and decodes
   * nothing a conforming encoder produced. Both were exercised only THROUGH
   * `dp_conv_encode`, which is the caller that happens to agree with them; a
   * user building a trellis (which is what `dp_viterbi_create` does, and what
   * anyone extending this must do) reads them directly.
   *
   * So: run the description BY HAND — `dp_conv_outputs` for the symbols,
   * `conv_next_state` for the state — and require the result to equal what
   * `dp_conv_encode` produced for the same bits. Two independent expressions
   * of the same code, which is what the docstring claims this file pins.
   */
  {
    enum
    {
      N = 200
    };
    uint8_t  in[N], sym[3 * N];
    uint32_t st = 90210u;

    const conv_code_t codes[3] = {
      CCSDS,
      { .k = 3u, .n = 2u, .poly = { 07u, 05u }, .invert = 0u },
      { .k = 5u, .n = 3u, .poly = { 025u, 033u, 037u }, .invert = 0x5u }
    };

    for (size_t ci = 0; ci < 3u; ci++)
      {
        const conv_code_t *c = &codes[ci];
        for (int i = 0; i < N; i++)
          in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

        conv_enc_t e;
        dp_conv_enc_init (&e);
        const size_t ns = dp_conv_encode (&e, c, in, N, sym, sizeof sym);
        DP_REQUIRE (ns == (size_t)N * c->n);

        /* The hand-run trellis. It starts where dp_conv_enc_init does — the
           all-zero state — which is the only thing this borrows from the
           encoder. */
        uint32_t state = 0u;
        int      same  = 1;
        for (int i = 0; i < N; i++)
          {
            const unsigned w = dp_conv_outputs (c, state, in[i]);
            for (unsigned j = 0; j < c->n; j++)
              {
                /* Output j is BIT j of the word and the j-th symbol emitted
                   — the ordering claim, which is the half a round trip
                   cannot see because both ends would move together. */
                if (((w >> j) & 1u) != sym[(size_t)i * c->n + j])
                  same = 0;
              }
            state = conv_next_state (c, state, in[i]);
          }
        DP_CHECK_MSG (same,
                      "the trellis run by hand from dp_conv_outputs and "
                      "conv_next_state must reproduce dp_conv_encode exactly");

        /* And the state is genuinely the k-1 previous inputs, newest in the
           high stage — read back rather than inferred, because this is the
           convention the docstring says a reader gets backwards. */
        uint32_t expect = 0u;
        for (unsigned b = 0; b < c->k - 1u; b++)
          expect |= (uint32_t)(in[N - 1 - b] & 1u) << (c->k - 2u - b);
        DP_CHECK_MSG (state == expect,
                      "a state must BE the k-1 previous inputs, newest in "
                      "the high stage");
      }
  }

  /* ── 3. continuity: the register carries across calls ──────────────────
   *
   * 3.3.2 fixes the output as one uninterrupted sequence. A caller encoding
   * in chunks must get the same stream as one call, or every chunk boundary
   * is a discontinuity no decoder expects.
   */
  {
    enum
    {
      N = 200
    };
    uint8_t  in[N], whole[2 * N], split[2 * N];
    uint32_t st = 77777u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_t s;
    dp_conv_enc_init (&s);
    dp_conv_encode (&s, &CCSDS, in, N, whole, sizeof whole);

    dp_conv_enc_init (&s);
    dp_conv_encode (&s, &CCSDS, in, 37, split, 74);
    dp_conv_encode (&s, &CCSDS, in + 37, N - 37, split + 74,
                    sizeof split - 74);
    DP_CHECK_MSG (memcmp (whole, split, 2u * N) == 0,
                  "chunked encoding must equal one call");
  }

  /* ── 4. the refusals, each verified by a poisoned buffer ───────────────*/
  {
    uint8_t     in[8] = { 0 }, out[16];
    conv_enc_t  e;
    conv_code_t bad = CCSDS;
    bad.poly[0]     = 0u;

    dp_conv_enc_init (&e);
    memset (out, 0xAA, sizeof out);
    DP_CHECK_MSG (dp_conv_encode (&e, &bad, in, 8, out, sizeof out) == 0,
                  "an invalid code must refuse");
    DP_CHECK_MSG (dp_conv_encode (&e, &CCSDS, in, 8, out, 15) == 0,
                  "one symbol short of the output must refuse");
    for (size_t i = 0; i < sizeof out; i++)
      DP_CHECK (out[i] == 0xAAu);
  }

  /* ── 5. puncturing: the pattern, and the arithmetic over it ────────────
   *
   * The oracle is the DEFINITION, written out literally: the mother code's
   * symbols with the deleted positions struck, read from a list of indices
   * rather than derived from the mask by the arithmetic under test. Rate 2/3
   * and 3/4 patterns are used as shapes; nothing here claims they are any
   * standard's.
   */
  {
    /* P = 3, n = 2: bit i*2+j. Keep indices 0 1 2 5 of each 6. */
    conv_code_t p34  = CCSDS;
    p34.punct_period = 3u;
    p34.punct_mask   = 0x27u; /* 0b100111 */
    DP_CHECK_MSG (dp_conv_code_valid (&p34), "a pattern that fits is a code");
    DP_CHECK (dp_conv_punctured (&p34));

    /* both-zero is none; one without the other is a typo */
    conv_code_t bad = CCSDS;
    bad.punct_mask  = 0x3u;
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "a mask with no period");
    bad              = CCSDS;
    bad.punct_period = 3u;
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "a period with no mask");
    bad.punct_mask = 0x47u; /* bit 6 is beyond 3*2 */
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "mask bits beyond the period");
    bad.punct_mask = 0x33u; /* input 1 keeps nothing */
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "an input that keeps nothing");
    bad.punct_period = 17u; /* 34 bits > 32 */
    bad.punct_mask   = 0xFFFFFFFFu;
    DP_CHECK_MSG (!dp_conv_code_valid (&bad), "period * n over 32");
    bad.punct_period = 16u; /* exactly 32: legal width */
    DP_CHECK_MSG (dp_conv_code_valid (&bad), "period * n == 32 is the limit");
    DP_CHECK_MSG (!dp_conv_punctured (&bad), "an all-ones mask punctures "
                                             "nothing");

    /* the literal oracle */
    enum
    {
      N = 12
    };
    static const uint8_t in[N] = { 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0 };
    uint8_t              mother[2 * N], got[2 * N];
    conv_enc_t           e;
    dp_conv_enc_init (&e);
    dp_conv_encode (&e, &CCSDS, in, N, mother, sizeof mother);

    /* N = 12 inputs = 4 periods; indices 0 1 2 5 of each 6 survive */
    uint8_t want_all[32];
    for (size_t per = 0; per < 4; per++)
      for (size_t i = 0; i < 4; i++)
        want_all[per * 4 + i] = mother[per * 6 + (size_t[]){ 0, 1, 2, 5 }[i]];

    dp_conv_enc_init (&e);
    const size_t n = dp_conv_encode (&e, &p34, in, N, got, sizeof got);
    DP_CHECK_MSG (n == 16u, "12 inputs at 3/4 punctured is 16 symbols");
    DP_CHECK_MSG (memcmp (got, want_all, 16u) == 0,
                  "punctured stream is the mother stream with the listed "
                  "positions struck");
    DP_CHECK (e.phase == 0u); /* 12 is a whole number of periods */

    /* chunk invariance: the phase carries across every split point */
    for (size_t cut = 1; cut < N; cut++)
      {
        uint8_t a[32] = { 0 };
        dp_conv_enc_init (&e);
        const size_t n1 = dp_conv_encode (&e, &p34, in, cut, a, sizeof a);
        const size_t n2 = dp_conv_encode (&e, &p34, in + cut, N - cut, a + n1,
                                          sizeof a - n1);
        DP_CHECK_MSG (n1 + n2 == 16u && memcmp (a, want_all, 16u) == 0,
                      "chunked punctured encoding must equal one call");
        DP_CHECK (e.phase == 0u);
      }

    /* one bit at a time, ending mid-pattern */
    uint8_t one[32];
    size_t  w = 0;
    dp_conv_enc_init (&e);
    for (size_t i = 0; i < 8; i++)
      w += dp_conv_encode (&e, &p34, in + i, 1, one + w, sizeof one - w);
    DP_CHECK_MSG (memcmp (one, want_all, w) == 0 && w == 11u,
                  "8 single-bit calls: two periods (8) + two inputs (3)");
    DP_CHECK_MSG (e.phase == 2u, "8 mod 3 inputs leaves the pattern at 2");
  }

  /* ── 5b. the length arithmetic agrees with a count, and inverts ────────*/
  {
    static const struct
    {
      unsigned period;
      uint32_t mask;
    } pat[] = { { 3u, 0x27u }, { 2u, 0x7u }, { 5u, 0x3AFu }, { 7u, 0x2FFFu } };
    for (size_t q = 0; q < sizeof pat / sizeof pat[0]; q++)
      {
        conv_code_t c  = CCSDS;
        c.punct_period = pat[q].period;
        c.punct_mask   = pat[q].mask;
        DP_REQUIRE (dp_conv_code_valid (&c));
        for (unsigned ph = 0; ph < c.punct_period; ph++)
          for (size_t n_in = 0; n_in < 40u; n_in++)
            {
              size_t sum = 0;
              for (size_t i = 0; i < n_in; i++)
                sum += (size_t)__builtin_popcount (
                    (pat[q].mask >> (((ph + i) % c.punct_period) * 2u)) & 3u);
              DP_CHECK_MSG (dp_conv_coded_len (&c, ph, n_in) == sum,
                            "coded length is the sum of kept symbols");
              size_t steps = 999;
              DP_CHECK_MSG (dp_conv_steps_for (&c, ph, sum, &steps)
                                && steps == n_in,
                            "steps_for inverts coded_len");
              DP_CHECK (dp_conv_phase_after (&c, ph, n_in)
                        == (ph + n_in) % c.punct_period);
              /* one symbol more or less than a whole number of inputs
                 ends mid-bit unless the next/previous input keeps one */
              if (sum > 0u)
                {
                  const size_t last = (size_t)__builtin_popcount (
                      (pat[q].mask
                       >> (((ph + n_in - 1u) % c.punct_period) * 2u))
                      & 3u);
                  if (last == 2u)
                    DP_CHECK_MSG (
                        !dp_conv_steps_for (&c, ph, sum - 1u, &steps),
                        "a length ending inside an input's two "
                        "symbols is not whole");
                }
            }
      }
  }

  /* ── 5c. refusals, by a poisoned buffer ────────────────────────────────*/
  {
    conv_code_t p    = CCSDS;
    p.punct_period   = 3u;
    p.punct_mask     = 0x27u;
    uint8_t    in[6] = { 1, 1, 0, 1, 0, 1 }, out[8];
    conv_enc_t e;
    dp_conv_enc_init (&e);
    memset (out, 0xAA, sizeof out);
    DP_CHECK_MSG (dp_conv_encode (&e, &p, in, 6, out, 7) == 0,
                  "one symbol short of the PUNCTURED length must refuse");
    for (size_t i = 0; i < sizeof out; i++)
      DP_CHECK (out[i] == 0xAAu);
    DP_CHECK_MSG (e.phase == 0u && e.reg == 0u,
                  "a refusal leaves the encoder untouched");
    DP_CHECK (dp_conv_encode (&e, &p, in, 6, out, 8) == 8u);
  }

  DP_TEST_END ("test_conv_core");
}
