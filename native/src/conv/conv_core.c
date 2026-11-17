/*
 * conv_core.c — convolutional codes: the description, the encoder, and the
 * Viterbi decoder that reads the same description.
 *
 * Both directions go through dp_conv_outputs(), which is the only place in the
 * tree that says what this family of codes emits. See conv_core.h.
 */
#include "doppler/conv/conv_core.h"

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

/* The mask bits the pattern may use: one per symbol of a period. */
static inline uint32_t
punct_full (const conv_code_t *c)
{
  const unsigned bits = c->punct_period * c->n;
  return bits >= 32u ? 0xFFFFFFFFu : (1u << bits) - 1u;
}

int
dp_conv_code_valid (const conv_code_t *c)
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

  /* A pattern is both fields zero, or a period and a mask that fit and keep
     something for every input. A mask with no period would silently mean
     nothing, which is how a transcription error survives. */
  if (c->punct_period == 0u)
    return c->punct_mask == 0u;
  if (c->punct_period * c->n > 32u || (c->punct_mask & ~punct_full (c)))
    return 0;
  for (unsigned i = 0; i < c->punct_period; i++)
    if (((c->punct_mask >> (i * c->n)) & ((1u << c->n) - 1u)) == 0u)
      return 0;
  return 1;
}

int
dp_conv_punctured (const conv_code_t *c)
{
  return c->punct_period != 0u && c->punct_mask != punct_full (c);
}

unsigned
dp_conv_kept (const conv_code_t *c, unsigned phase)
{
  if (!dp_conv_punctured (c))
    return c->n;
  return (unsigned)__builtin_popcount ((c->punct_mask >> (phase * c->n))
                                       & ((1u << c->n) - 1u));
}

size_t
dp_conv_coded_len (const conv_code_t *c, unsigned phase, size_t n_in)
{
  if (!dp_conv_punctured (c))
    return n_in * (size_t)c->n;

  const unsigned P     = c->punct_period;
  const size_t   K     = (size_t)__builtin_popcount (c->punct_mask);
  size_t         total = 0;
  unsigned       ph    = phase % P;

  /* The head to the next period boundary, whole periods by arithmetic, then
     the tail: a block's cost is its length, not its position. */
  size_t head = (P - ph) % P;
  if (head > n_in)
    head = n_in;
  for (size_t i = 0; i < head; i++)
    total += dp_conv_kept (c, ph + (unsigned)i);
  const size_t rem = n_in - head;
  total += (rem / P) * K;
  for (size_t i = 0; i < rem % P; i++)
    total += dp_conv_kept (c, (unsigned)i);
  return total;
}

int
dp_conv_steps_for (const conv_code_t *c, unsigned phase, size_t n_syms,
                   size_t *steps)
{
  size_t got = 0;
  if (!dp_conv_punctured (c))
    {
      *steps = n_syms / c->n;
      return n_syms % c->n == 0u;
    }

  const unsigned P  = c->punct_period;
  const size_t   K  = (size_t)__builtin_popcount (c->punct_mask);
  unsigned       ph = phase % P;

  /* Same walk as dp_conv_coded_len, run backwards. Unique because every
     input keeps at least one symbol, so no step consumes nothing. */
  while (ph != 0u)
    {
      const unsigned k = dp_conv_kept (c, ph);
      if (n_syms < k)
        {
          *steps = got;
          return n_syms == 0u;
        }
      n_syms -= k;
      got++;
      ph = (ph + 1u) % P;
    }
  const size_t q = n_syms / K;
  got += q * P;
  n_syms -= q * K;
  for (unsigned i = 0; i < P; i++)
    {
      const unsigned k = dp_conv_kept (c, i);
      if (n_syms < k)
        break;
      n_syms -= k;
      got++;
    }
  *steps = got;
  return n_syms == 0u;
}

unsigned
dp_conv_phase_after (const conv_code_t *c, unsigned phase, size_t n_in)
{
  if (!dp_conv_punctured (c))
    return 0u;
  return (unsigned)((phase + n_in) % c->punct_period);
}

unsigned
dp_conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit)
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
dp_conv_enc_init (conv_enc_t *s)
{
  s->reg   = 0u;
  s->phase = 0u;
}

size_t
dp_conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
                size_t n_in, uint8_t *out, size_t max_out)
{
  if (!dp_conv_code_valid (c))
    return 0;

  const uint32_t mask = conv_states (c) - 1u;
  if (!dp_conv_punctured (c))
    {
      if (max_out < n_in * (size_t)c->n)
        return 0;
      for (size_t i = 0; i < n_in; i++)
        {
          const unsigned b = in[i] & 1u;
          const unsigned w = dp_conv_outputs (c, s->reg & mask, b);
          for (unsigned j = 0; j < c->n; j++)
            out[i * c->n + j] = (uint8_t)((w >> j) & 1u);
          s->reg = conv_next_state (c, s->reg & mask, b);
        }
      return n_in * (size_t)c->n;
    }

  /* Punctured: the same outputs from the same expression, with the pattern
     deciding which are written. The encoder runs the mother code in full, so
     the register never knows it was punctured. */
  const size_t need = dp_conv_coded_len (c, s->phase, n_in);
  if (max_out < need)
    return 0;
  size_t   w_out = 0;
  unsigned ph    = s->phase % c->punct_period;
  for (size_t i = 0; i < n_in; i++)
    {
      const unsigned b = in[i] & 1u;
      const unsigned w = dp_conv_outputs (c, s->reg & mask, b);
      for (unsigned j = 0; j < c->n; j++)
        if ((c->punct_mask >> (ph * c->n + j)) & 1u)
          out[w_out++] = (uint8_t)((w >> j) & 1u);
      s->reg = conv_next_state (c, s->reg & mask, b);
      ph     = (ph + 1u == c->punct_period) ? 0u : ph + 1u;
    }
  s->phase = ph;
  return w_out;
}
