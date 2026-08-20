/*
 * conv_core.c — convolutional codes: the description, the encoder, and the
 * Viterbi decoder that reads the same description.
 *
 * Both directions go through conv_outputs(), which is the only place in the
 * tree that says what this family of codes emits. See conv_core.h.
 */
#include "conv/conv_core.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

/* Parity of the tapped stages: the modulo-2 sum an adder in the encoder's
   figure computes. Folds 32 bits, so it covers any k up to CONV_K_MAX. */
static inline unsigned
parity32 (uint32_t v)
{
  v ^= v >> 16;
  v ^= v >> 8;
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return (unsigned)(v & 1u);
}

int
conv_code_valid (const conv_code_t *c)
{
  if (c == NULL || c->k < 2u || c->k > CONV_K_MAX || c->n < 1u
      || c->n > CONV_N_MAX)
    return 0;
  for (unsigned j = 0; j < c->n; j++)
    {
      /* A zero polynomial is an output carrying no information, and a
         polynomial wider than the register is a transcription that lost its
         alignment. Both are typos rather than codes. */
      if (c->poly[j] == 0u || c->poly[j] >> c->k)
        return 0;
    }
  return 1;
}

unsigned
conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit)
{
  const uint32_t reg = ((bit & 1u) << (c->k - 1u)) | state;
  unsigned       w   = 0;
  for (unsigned j = 0; j < c->n; j++)
    w |= parity32 (reg & c->poly[j]) << j;
  /* The inversion is a property of the CODE, applied here so that neither
     the encoder nor the decoder can hold a private opinion about it. */
  return w ^ (unsigned)(c->invert & ((1u << c->n) - 1u));
}

void
conv_enc_init (conv_enc_t *s)
{
  s->reg = 0u;
}

size_t
conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
             size_t n_in, uint8_t *out, size_t max_out)
{
  if (!conv_code_valid (c) || max_out < n_in * (size_t)c->n)
    return 0;

  const uint32_t mask = conv_states (c) - 1u;
  for (size_t i = 0; i < n_in; i++)
    {
      const unsigned b = in[i] & 1u;
      const unsigned w = conv_outputs (c, s->reg & mask, b);
      for (unsigned j = 0; j < c->n; j++)
        out[i * c->n + j] = (uint8_t)((w >> j) & 1u);
      s->reg = conv_next_state (c, s->reg & mask, b);
    }
  return n_in * (size_t)c->n;
}
