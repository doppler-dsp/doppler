/* test_viterbi_core.c — the decoder, at its own object.
 *
 * `viterbi` became a declared jm component in doppler#893, so this file, its
 * CMake target and the Python face all generate. What jm scaffolded was a
 * create/reset/destroy smoke test that passed `NULL` for the required `poly`
 * array — which this component correctly REFUSES, so the scaffold failed on
 * its own first assertion. That refusal is pinned below rather than worked
 * around.
 *
 * The exhaustive sweep over codes lives in test_conv_core.c sections 4-8 and
 * moves here next (doppler#893); this pins the OBJECT's own contract: the
 * declared constructor, what it rejects, and that a noiseless round trip
 * comes back bit-for-bit through it.
 */
#include "conv/conv_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "viterbi/viterbi_core.h"

/* CCSDS 131.0-B-3 section 3's inner code, as the object takes it. */
static const uint32_t POLY[2] = { 0171u, 0133u };

int
main (void)
{
  /* ── 1. the declared constructor, and what it refuses ──────────────────*/
  {
    DP_CHECK_MSG (viterbi_create (NULL, 0, 7u, 0u, 35u) == NULL,
                  "a NULL polynomial array is not a code");
    DP_CHECK_MSG (viterbi_create (POLY, 0, 7u, 0u, 35u) == NULL,
                  "zero polynomials is not a code");
    DP_CHECK_MSG (viterbi_create (POLY, CONV_N_MAX + 1u, 7u, 0u, 35u) == NULL,
                  "more polynomials than the code family admits");

    viterbi_state_t *v = viterbi_create (POLY, 2, 7u, 0u, 35u);
    DP_REQUIRE_MSG (v != NULL, "the CCSDS inner code is constructible");
    DP_CHECK (viterbi_depth (v) == 35u);
    DP_CHECK_MSG (viterbi_code (v)->k == 7u, "k survives the constructor");
    DP_CHECK_MSG (viterbi_code (v)->n == 2u, "n comes from the array length");
    DP_CHECK (viterbi_code (v)->poly[0] == POLY[0]);
    DP_CHECK (viterbi_code (v)->poly[1] == POLY[1]);
    viterbi_destroy (v);
  }

  /* ── 2. the declared constructor agrees with the struct one ────────────
   *
   * Two ways in, one decoder: the manifest cannot express a conv_code_t, so
   * the object takes the polynomials and assembles it. If the two disagreed,
   * the Python face would be decoding with a different code than the C
   * callers, which is the kind of divergence nobody would look for.
   */
  {
    const conv_code_t c = { 7u, 2u, { 0171u, 0133u }, 0u };
    viterbi_state_t  *a = viterbi_create (POLY, 2, 7u, 0u, 60u);
    viterbi_state_t  *b = viterbi_create_code (&c, 60u);
    DP_REQUIRE (a != NULL && b != NULL);
    DP_CHECK (viterbi_depth (a) == viterbi_depth (b));
    DP_CHECK (viterbi_code (a)->k == viterbi_code (b)->k);
    DP_CHECK (viterbi_code (a)->n == viterbi_code (b)->n);
    DP_CHECK (viterbi_code (a)->poly[0] == viterbi_code (b)->poly[0]);
    DP_CHECK (viterbi_code (a)->poly[1] == viterbi_code (b)->poly[1]);
    DP_CHECK (viterbi_state_bytes (a) == viterbi_state_bytes (b));
    viterbi_destroy (a);
    viterbi_destroy (b);
  }

  /* ── 3. noiseless round trip, through the DECLARED constructor ─────────
   *
   * Noiseless, so any failure is the trellis rather than the channel.
   */
  {
    enum
    {
      N = 400
    };
    uint8_t  in[N], sym[N * 2], dec[N + 64];
    float    llr[N * 2];
    uint32_t st = 20260820u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_t e;
    conv_enc_init (&e);
    const conv_code_t c  = { 7u, 2u, { 0171u, 0133u }, 0u };
    const size_t      ns = conv_encode (&e, &c, in, N, sym, sizeof sym);
    DP_REQUIRE (ns == (size_t)N * 2u);
    for (size_t i = 0; i < ns; i++)
      llr[i] = sym[i] ? -8.0f : 8.0f;

    viterbi_state_t *v = viterbi_create (POLY, 2, 7u, 0u, 60u);
    DP_REQUIRE (v != NULL);
    const size_t cap = viterbi_decode_max_out (v, ns);
    DP_REQUIRE (cap <= sizeof dec);
    const size_t nb = viterbi_decode (v, llr, ns, dec, cap);
    DP_REQUIRE_MSG (nb > 0, "the decoder emitted nothing");

    size_t errs = 0;
    for (size_t i = 0; i < nb; i++)
      errs += (dec[i] != in[i]);
    DP_CHECK_MSG (errs == 0, "a noiseless round trip is bit-for-bit");
    viterbi_destroy (v);
  }

  DP_TEST_END ("test_viterbi_core");
}
