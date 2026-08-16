/*
 * ccsds_conv.c — the CCSDS basic convolutional code (131.0-B-3, section 3.3).
 *
 * Rate 1/2, K=7, non-systematic, G1 = 1111001 (171 octal), G2 = 1011011 (133
 * octal), with symbol inversion on the G2 output path. Continuous: there is
 * no per-frame tail in the basic code, so the register carries across calls.
 */
#include "fec/fec_ccsds.h"

/* The connection vectors as the standard writes them, left-to-right, with the
 * newest input at the left. Holding the register the same way — newest in bit
 * 6, oldest in bit 0 — makes the mask a direct transcription of 3.3.1(4)
 * rather than a reversal that has to be reasoned about. */
#define G1 0x79u /* 1111001b = 171 octal */
#define G2 0x5Bu /* 1011011b = 133 octal */

/* Parity of the tapped stages: the modulo-2 sum an adder in figure 3-1
 * computes. */
static inline uint8_t
parity (uint8_t v)
{
  v ^= (uint8_t)(v >> 4);
  v ^= (uint8_t)(v >> 2);
  v ^= (uint8_t)(v >> 1);
  return (uint8_t)(v & 1u);
}

void
fec_conv_init (fec_conv_t *s)
{
  s->reg = 0u;
}

size_t
fec_conv_encode (fec_conv_t *s, const uint8_t *in, size_t n, uint8_t *out)
{
  for (size_t i = 0; i < n; i++)
    {
      /* Shift the new bit into the high stage; stage 0 falls off the end. */
      s->reg = (uint8_t)(((s->reg >> 1) | ((in[i] & 1u) << 6)) & 0x7Fu);

      /* 3.3.2: C1 first, then C2, per input bit.
       *
       * The `^ 1u` on C2 is 3.3.1(5), and it is the whole reason this file
       * has a vector test: omit it and the code still decodes its own output
       * perfectly, because a matched Viterbi inverts whatever it was given.
       * It simply interoperates with nothing. */
      out[2 * i]     = parity ((uint8_t)(s->reg & G1));
      out[2 * i + 1] = (uint8_t)(parity ((uint8_t)(s->reg & G2)) ^ 1u);
    }
  return 2u * n;
}
