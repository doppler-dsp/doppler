/*
 * conv_enc_core.c — the convolutional encoder as an object over `conv`.
 *
 * Every line here is lifecycle and bookkeeping. The encoding itself is
 * `conv_encode`'s, one call in `conv_enc_encode`, because a second
 * implementation of a code family is how an inversion or an output order
 * comes to differ between the two and interoperate with neither.
 */
#include "conv_enc/conv_enc_core.h"

#include <stdlib.h>
#include <string.h>

conv_enc_state_t *
conv_enc_create_code (const conv_code_t *c)
{
  if (!conv_code_valid (c))
    return NULL;

  conv_enc_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;
  s->code = *c;
  conv_enc_init (&s->enc);
  return s;
}

conv_enc_state_t *
conv_enc_create (const uint32_t *poly, size_t poly_len, uint32_t k,
                 uint32_t invert)
{
  /* Assembled here rather than by the caller: the manifest cannot express a
     conv_code_t, so the declared constructor takes the polynomials and the
     array's length IS n. conv_code_valid then refuses anything unusable,
     which is what turns a bad argument into a NULL rather than a decoder
     that encodes to nothing. */
  conv_code_t c = { 0 };
  if (poly == NULL || poly_len == 0u || poly_len > CONV_N_MAX)
    return NULL;
  c.k      = k;
  c.n      = (unsigned)poly_len;
  c.invert = invert;
  for (size_t i = 0; i < poly_len; i++)
    c.poly[i] = poly[i];
  return conv_enc_create_code (&c);
}

void
conv_enc_destroy (conv_enc_state_t *state)
{
  free (state);
}

void
conv_enc_reset (conv_enc_state_t *state)
{
  conv_enc_init (&state->enc);
}

const conv_code_t *
conv_enc_code (const conv_enc_state_t *s)
{
  return &s->code;
}

size_t
conv_enc_encode_max_out (const conv_enc_state_t *state, size_t n_in)
{
  return n_in * (size_t)state->code.n;
}

size_t
conv_enc_encode (conv_enc_state_t *state, const uint8_t *in, size_t n_in,
                 uint8_t *out, size_t max_out)
{
  return conv_encode (&state->enc, &state->code, in, n_in, out, max_out);
}

/* ── the state bytes interface ───────────────────────────────────────────
 *
 * Running state is the register and nothing else. The code is CONFIG —
 * create() fixed it — so it travels in the blob to be checked rather than
 * restored: a blob from a different code describes a register that means
 * something else, and a register restored under the wrong code produces a
 * stream no decoder matches while every size check passes.
 */
typedef struct
{
  uint32_t reg;
  uint32_t k;
  uint32_t n;
  uint32_t invert;
  uint32_t poly[CONV_N_MAX];
} conv_enc_extra_t;

size_t
conv_enc_state_bytes (const conv_enc_state_t *s)
{
  (void)s;
  return sizeof (dp_state_hdr_t) + sizeof (conv_enc_extra_t);
}

void
conv_enc_get_state (const conv_enc_state_t *s, void *blob)
{
  DP_GET_OPEN (CONV_ENC_STATE_MAGIC, CONV_ENC_STATE_VERSION,
               conv_enc_state_bytes (s));

  /* memset rather than a designated initializer: the padding one leaves
     unspecified (C11 6.7.9p10) would be written to the blob whole, which
     reads green on one compiler and red on another. It also zeroes
     poly[n..CONV_N_MAX-1], which conv_code_t does not require a caller to
     fill — serializing those would make the blob depend on the caller's
     stack. */
  conv_enc_extra_t extra;
  memset (&extra, 0, sizeof extra);
  extra.reg    = s->enc.reg;
  extra.k      = s->code.k;
  extra.n      = s->code.n;
  extra.invert = s->code.invert & ((1u << s->code.n) - 1u);
  for (unsigned j = 0; j < s->code.n; j++)
    extra.poly[j] = s->code.poly[j];

  dp_w_bytes (&_w, &extra, sizeof extra);
}

int
conv_enc_set_state (conv_enc_state_t *s, const void *blob)
{
  DP_SET_OPEN (CONV_ENC_STATE_MAGIC, CONV_ENC_STATE_VERSION,
               conv_enc_state_bytes (s));

  conv_enc_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);

  /* The envelope's size check cannot see a configuration difference — every
     code of this family serializes to the same length — so the code is
     compared field by field. */
  if (extra.k != s->code.k || extra.n != s->code.n
      || extra.invert != (s->code.invert & ((1u << s->code.n) - 1u)))
    return DP_ERR_INVALID;
  for (unsigned j = 0; j < s->code.n; j++)
    if (extra.poly[j] != s->code.poly[j])
      return DP_ERR_INVALID;

  /* The register holds k-1 bits; anything above that is a blob describing a
     wider encoder than this one, which would shift bits in that the trellis
     never sees. */
  if (extra.reg >> (s->code.k - 1u))
    return DP_ERR_INVALID;

  s->enc.reg = extra.reg;
  return DP_OK;
}
