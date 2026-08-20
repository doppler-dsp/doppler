/* test_conv_enc_core.c — the encoder object, at its own component.
 *
 * `conv_enc` is an OBJECT over `conv`, not a second encoder, so this file
 * does not re-derive what `test_conv_core.c` already pins: the impulse
 * response, the inversion mask, the output order. Those are the CODE's
 * claims, they are checked there against the polynomials themselves, and
 * checking them again here would be two files agreeing about one kernel.
 *
 * What only this object can be wrong about is the BINDING of a code to a
 * register: that create() copies the code, that the register survives
 * between calls and nothing else does, that a reset returns to the state a
 * fresh encoder is in, and that a serialized register is refused by an
 * encoder built for a different code. Each of those is a way to produce a
 * stream that is self-consistent and matches no decoder.
 *
 * jm's scaffold opened by passing `NULL` for the required `poly` array,
 * which this component correctly REFUSES, so it failed on its own first
 * assertion. That refusal is pinned in section 1 rather than worked around.
 */
#include "conv/conv_core.h"
#include "conv_enc/conv_enc_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <stdlib.h>

#include <string.h>

/* CCSDS 131.0-B-3 section 3.3, as the object takes it. */
static const uint32_t POLY[2] = { 0171u, 0133u };

int
main (void)
{
  /* ── 1. the declared constructor, and what it refuses ──────────────────*/
  {
    DP_CHECK_MSG (conv_enc_create (NULL, 0, 7u, 0u) == NULL,
                  "a NULL polynomial array is not a code");
    DP_CHECK_MSG (conv_enc_create (POLY, 0, 7u, 0u) == NULL,
                  "zero polynomials is not a code");
    DP_CHECK_MSG (conv_enc_create (POLY, CONV_N_MAX + 1u, 7u, 0u) == NULL,
                  "more polynomials than the code family admits");
    DP_CHECK_MSG (conv_enc_create (POLY, 2, 1u, 0u) == NULL,
                  "k below the smallest register");
    DP_CHECK_MSG (conv_enc_create (POLY, 2, CONV_K_MAX + 1u, 0u) == NULL,
                  "k past the largest the family admits");
    {
      const uint32_t zero[2] = { 0171u, 0u };
      DP_CHECK_MSG (conv_enc_create (zero, 2, 7u, 0u) == NULL,
                    "a zero polynomial is an output carrying nothing");
      const uint32_t wide[2] = { 0171u, 0400u };
      DP_CHECK_MSG (conv_enc_create (wide, 2, 7u, 0u) == NULL,
                    "a polynomial wider than the register");
    }

    conv_enc_state_t *e = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE_MSG (e != NULL, "the CCSDS inner code is constructible");
    DP_CHECK_MSG (conv_enc_code (e)->k == 7u, "k survives the constructor");
    DP_CHECK_MSG (conv_enc_code (e)->n == 2u, "n comes from the array length");
    DP_CHECK (conv_enc_code (e)->poly[0] == POLY[0]);
    DP_CHECK (conv_enc_code (e)->poly[1] == POLY[1]);
    DP_CHECK_MSG (conv_enc_code (e)->invert == 0x2u,
                  "the inversion mask is the caller's, not a default");
    conv_enc_destroy (e);
    conv_enc_destroy (NULL); /* a no-op, not a crash */
  }

  /* ── 2. the object encodes what the kernel encodes ─────────────────────
   *
   * Against `conv_encode` driven by hand, not against a stored vector: the
   * claim is that this object is a BINDING of a code to a register, so the
   * thing to prove is that it adds nothing. A golden array here would pin
   * the kernel a second time and say nothing about the binding.
   */
  {
    enum
    {
      N = 300
    };
    uint8_t  in[N], want[N * 2], got[N * 2];
    uint32_t st = 20260820u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    const conv_code_t c = { 7u, 2u, { 0171u, 0133u }, 0x2u };
    conv_enc_t        raw;
    conv_enc_init (&raw);
    DP_REQUIRE (conv_encode (&raw, &c, in, N, want, sizeof want)
                == (size_t)N * 2u);

    conv_enc_state_t *e = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (e != NULL);
    const size_t n = conv_enc_encode (e, in, N, got, sizeof got);
    DP_CHECK_MSG (n == conv_enc_encode_max_out (e, N),
                  "encode must write exactly what max_out predicted");
    DP_CHECK_MSG (n == (size_t)N * 2u, "and that is n_in * n, with no fill");
    DP_CHECK_MSG (memcmp (want, got, n) == 0,
                  "the object adds nothing to the kernel it calls");
    conv_enc_destroy (e);
  }

  /* ── 3. the register carries, which is the whole reason for the object ─
   *
   * A stream split anywhere must equal one call. An encoder that restarted
   * its register per block would differ in exactly `k-1` bits at every
   * boundary — decodable by a receiver of one's own construction, and not
   * what any standard specifies.
   */
  {
    enum
    {
      N   = 400,
      CUT = 137
    };
    uint8_t  whole[N * 2], split[N * 2], in[N];
    uint32_t st = 31337u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_state_t *a = conv_enc_create (POLY, 2, 7u, 0x2u);
    conv_enc_state_t *b = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (a != NULL && b != NULL);

    conv_enc_encode (a, in, N, whole, sizeof whole);
    conv_enc_encode (b, in, CUT, split, 2u * CUT);
    conv_enc_encode (b, in + CUT, N - CUT, split + 2u * CUT,
                     sizeof split - 2u * CUT);
    DP_CHECK_MSG (memcmp (whole, split, 2u * N) == 0,
                  "a chunked encode must equal one call");

    /* ...and the check is not vacuous: a RESET encoder at the same cut point
       must differ, or the register was never carrying anything. */
    conv_enc_state_t *d = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (d != NULL);
    uint8_t restarted[N * 2];
    conv_enc_encode (d, in, CUT, restarted, 2u * CUT);
    conv_enc_reset (d);
    conv_enc_encode (d, in + CUT, N - CUT, restarted + 2u * CUT,
                     sizeof restarted - 2u * CUT);
    DP_CHECK_MSG (memcmp (whole, restarted, 2u * N) != 0,
                  "restarting the register MUST change the stream, or this "
                  "test proves nothing");
    conv_enc_destroy (a);
    conv_enc_destroy (b);
    conv_enc_destroy (d);
  }

  /* ── 4. reset returns to the state a fresh encoder is in ───────────────*/
  {
    enum
    {
      N = 64
    };
    uint8_t  in[N], first[N * 2], again[N * 2];
    uint32_t st = 5150u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_state_t *e = conv_enc_create (POLY, 2, 7u, 0u);
    DP_REQUIRE (e != NULL);
    conv_enc_encode (e, in, N, first, sizeof first);
    conv_enc_encode (e, in, N, again, sizeof again); /* dirty the register */
    DP_CHECK_MSG (memcmp (first, again, sizeof first) != 0,
                  "the second block must differ, or the register is dead");
    conv_enc_reset (e);
    conv_enc_encode (e, in, N, again, sizeof again);
    DP_CHECK_MSG (memcmp (first, again, sizeof first) == 0,
                  "after a reset the same input must give the same stream");
    conv_enc_destroy (e);
  }

  /* ── 5. the refusals, verified by a poisoned buffer ────────────────────*/
  {
    uint8_t           in[8] = { 0 }, out[16];
    conv_enc_state_t *e     = conv_enc_create (POLY, 2, 7u, 0u);
    DP_REQUIRE (e != NULL);
    memset (out, 0xAA, sizeof out);
    DP_CHECK_MSG (conv_enc_encode (e, in, 8, out, 15) == 0,
                  "one symbol short of the output must refuse");
    for (size_t i = 0; i < sizeof out; i++)
      DP_CHECK_MSG (out[i] == 0xAAu, "...leaving the buffer untouched");
    conv_enc_destroy (e);
  }

  /* ── 6. the state bytes interface: resume, and refuse a foreign blob ───
   *
   * The register is the running state and the code is config, so a blob
   * carries the code to be CHECKED rather than restored. A blob from a
   * different code describes a register that means something else, and every
   * size check in the envelope passes — which is why the comparison is field
   * by field.
   */
  {
    enum
    {
      N   = 200,
      CUT = 71
    };
    uint8_t  in[N], whole[N * 2], resumed[N * 2];
    uint32_t st = 90210u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_state_t *a = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (a != NULL);
    conv_enc_encode (a, in, N, whole, sizeof whole);

    conv_enc_state_t *b = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (b != NULL);
    conv_enc_encode (b, in, CUT, resumed, 2u * CUT);

    void *blob = malloc (conv_enc_state_bytes (b));
    DP_REQUIRE (blob != NULL);
    conv_enc_get_state (b, blob);

    conv_enc_state_t *r = conv_enc_create (POLY, 2, 7u, 0x2u);
    DP_REQUIRE (r != NULL);
    DP_CHECK (conv_enc_set_state (r, blob) == DP_OK);
    conv_enc_encode (r, in + CUT, N - CUT, resumed + 2u * CUT,
                     sizeof resumed - 2u * CUT);
    DP_CHECK_MSG (memcmp (whole, resumed, 2u * N) == 0,
                  "a resumed encoder must continue bit-for-bit");

    /* The envelope: a clobbered magic is refused rather than reinterpreted. */
    {
      uint8_t *poison = malloc (conv_enc_state_bytes (b));
      DP_REQUIRE (poison != NULL);
      memcpy (poison, blob, conv_enc_state_bytes (b));
      poison[0] ^= 0xFFu;
      DP_CHECK_MSG (conv_enc_set_state (r, poison) == DP_ERR_INVALID,
                    "a clobbered envelope must be refused");
      free (poison);
    }

    /* A different code, same k and n, so the blob is the same LENGTH. Only a
       field-by-field comparison can see this one. */
    {
      const uint32_t    other[2] = { 0133u, 0171u }; /* swapped */
      conv_enc_state_t *o        = conv_enc_create (other, 2, 7u, 0x2u);
      DP_REQUIRE (o != NULL);
      DP_CHECK_MSG (conv_enc_state_bytes (o) == conv_enc_state_bytes (r),
                    "the two blobs are the same size, so length cannot tell "
                    "them apart");
      DP_CHECK_MSG (conv_enc_set_state (o, blob) == DP_ERR_INVALID,
                    "a blob from another code must be refused");
      conv_enc_destroy (o);
    }

    /* ...and the guard is not simply refusing everything. */
    DP_CHECK (conv_enc_set_state (r, blob) == DP_OK);

    free (blob);
    conv_enc_destroy (a);
    conv_enc_destroy (b);
    conv_enc_destroy (r);
  }

  DP_TEST_END ("test_conv_enc_core");
}
