/*
 * rs_codec_core.c — the Reed-Solomon codec object.
 *
 * Every line of arithmetic here is rs_core.c's. What this file owns is the
 * BINDING of a code to its tables (so a caller cannot pair the wrong two)
 * and the placement of a systematic codeword (so `encode` answers in the
 * same unit every other method takes).
 */
#include "rs_codec/rs_codec_core.h"

#include <stdlib.h>
#include <string.h>

rs_codec_state_t *
rs_codec_create (uint32_t nroots, uint32_t symbol_bits, uint32_t field_poly,
                 uint32_t first_root, uint32_t root_stride)
{
  rs_codec_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  const rs_code_t code = { .symbol_bits = (unsigned)symbol_bits,
                           .field_poly  = (uint16_t)field_poly,
                           .nroots      = (unsigned)nroots,
                           .first_root  = (unsigned)first_root,
                           .root_stride = (unsigned)root_stride };

  /* rs_init is where a non-primitive field polynomial and a root stride
     sharing a factor with n are caught. Both produce arithmetic that is
     entirely self-consistent, so this is the ONLY place they can be caught
     -- a round trip against a matching encoder never will be. */
  if (!rs_init (&obj->rs, &code))
    {
      free (obj);
      return NULL;
    }
  return obj;
}

void
rs_codec_destroy (rs_codec_state_t *state)
{
  free (state);
}

size_t
rs_codec_encode_max_out (rs_codec_state_t *state, size_t n_in)
{
  (void)n_in; /* a codeword is n symbols whatever it was built from */
  return state->rs.n;
}

size_t
rs_codec_encode (rs_codec_state_t *state, const uint8_t *in, size_t n_in,
                 uint8_t *out, size_t max_out)
{
  const size_t n = state->rs.n;
  if (n_in != state->rs.k || max_out < n)
    return 0;

  /* Systematic: the information travels untouched and rs_encode appends the
     remainder. Placing it here rather than in the kernel is what lets
     rs_encode stay the parity-only primitive a frame assembler wants, which
     already has the information in place and must not copy it again.

     The guard is not defensive: `out == in` is the in-place call a frame
     assembler makes, and memcpy with identical pointers is undefined
     behaviour rather than a no-op. */
  if (out != in)
    memcpy (out, in, state->rs.k);
  rs_encode (&state->rs, in, out + state->rs.k);
  return n;
}

int
rs_codec_decode (rs_codec_state_t *state, uint8_t *codeword,
                 size_t codeword_len)
{
  if (codeword_len != state->rs.n)
    return -2;
  return rs_decode (&state->rs, codeword);
}

size_t
rs_codec_syndromes_max_out (rs_codec_state_t *state, size_t n_in)
{
  (void)n_in;
  return state->rs.code.nroots;
}

size_t
rs_codec_syndromes (rs_codec_state_t *state, const uint8_t *in, size_t n_in,
                    uint8_t *out, size_t max_out)
{
  const size_t nroots = state->rs.code.nroots;
  if (n_in != state->rs.n || max_out < nroots)
    return 0;
  rs_syndromes (&state->rs, in, out);
  return nroots;
}

int
rs_codec_codeword_ok (rs_codec_state_t *state, const uint8_t *codeword,
                      size_t codeword_len)
{
  if (codeword_len != state->rs.n)
    return 0;
  return rs_codeword_ok (&state->rs, codeword) ? 1 : 0;
}

size_t
rs_codec_generator (rs_codec_state_t *state, uint8_t *out, size_t out_len)
{
  const size_t len = (size_t)state->rs.code.nroots + 1u;
  if (out_len < len)
    return 0;
  memcpy (out, rs_generator (&state->rs), len);
  return len;
}

size_t
rs_codec_get_n (const rs_codec_state_t *state)
{
  return state->rs.n;
}

size_t
rs_codec_get_k (const rs_codec_state_t *state)
{
  return state->rs.k;
}

size_t
rs_codec_get_e (const rs_codec_state_t *state)
{
  return state->rs.e;
}

size_t
rs_codec_get_nroots (const rs_codec_state_t *state)
{
  return state->rs.code.nroots;
}

size_t
rs_codec_get_symbol_bits (const rs_codec_state_t *state)
{
  return state->rs.code.symbol_bits;
}
