/*
 * rand.c — the CCSDS pseudo-randomisers (131.0-B-6, section 10).
 *
 * B-6 specifies TWO, and which one a mission uses is a choice rather than a
 * property of the coding — so they are configurations over one generator,
 * exactly as the inner and outer codes are configurations of `conv` and `rs`.
 *
 *   10.4.1  h(x) = x^17 + x^14 + 1              131071 bits   the `shall`
 *   10.4.2  h(x) = x^8 + x^7 + x^5 + x^3 + 1       255 bits   legacy only
 *
 * Both sequences are published, so neither tap arrangement is a design choice
 * here: it is whatever reproduces the printed prefix, and
 * test_ccsds_tm_rand.c is what holds each one to its own. An LFSR is easy to
 * write in several self-consistent ways that produce different sequences;
 * only the published prefix separates them.
 */
#include "ccsds_tm/ccsds_tm.h"

/* The feedback mask is DERIVED from the recurrence, not transcribed from the
 * polynomial's exponents, and that distinction has already cost this file
 * once.
 *
 * A characteristic polynomial of degree D stands for
 *
 *     s[n+D] = sum of s[n+k] over the terms x^k it names, plus s[n]
 *
 * and holding s[n] in the HIGHEST stage (oldest, emitted next) through
 * s[n+D-1] in bit 0, the term x^k lands on bit D-1-k.
 *
 *   degree 8, x^8 + x^7 + x^5 + x^3 + 1  ->  bits 0, 2, 4, 7   = 0x95
 *   degree 17, x^17 + x^14 + 1           ->  bits 2, 16        = 0x10004
 *
 * Writing the taps as the exponents instead — bits 7, 6, 4, 2 for the degree
 * 8 case — is a plausible-looking transcription that makes the feedback of
 * the all-ones preset zero, so the register walks 0xFF, 0xFE ... 0x00 and
 * sticks at the all-zero fixed point. It survived a round trip and both
 * period checks (a dead sequence repeats with every period and matches no
 * earlier one) and was caught only by the published prefix.
 */
const ccsds_tm_rand_t CCSDS_TM_RAND = {
  /* .taps   */ (1u << 16) | (1u << 2),
  /* 10.4.3: '11000111000111000', loaded so its LAST printed bit leaves
     FIRST — the string reads along figure 10-2's register and the stage that
     leaves first is the far end. The legacy preset is all ones and so reads
     the same either way, which is why nothing forced the question before;
     the published prefix is what settles it. */
  /* .seed   */ 0x038E3u, /* 0 0011 1000 1110 0011 */
  /* .stages */ 17u,
  /* .period */ 131071u
};

const ccsds_tm_rand_t CCSDS_TM_RAND_LEGACY = {
  /* .taps   */ (1u << 7) | (1u << 4) | (1u << 2) | (1u << 0),
  /* .seed   */ 0xFFu,
  /* .stages */ 8u,
  /* .period */ 255u
};

void
ccsds_tm_rand_init (ccsds_tm_rand_state_t *s, const ccsds_tm_rand_t *r)
{
  if (r == NULL)
    r = &CCSDS_TM_RAND;
  s->reg    = r->seed & (uint32_t)((1u << r->stages) - 1u);
  s->taps   = r->taps;
  s->stages = r->stages;
}

uint8_t
ccsds_tm_rand_step (ccsds_tm_rand_state_t *s)
{
  const uint32_t mask = (uint32_t)((1u << s->stages) - 1u);
  const uint8_t  out  = (uint8_t)((s->reg >> (s->stages - 1u)) & 1u);

  /* Parity of the tapped stages. */
  uint32_t v = s->reg & s->taps;
  v ^= v >> 16;
  v ^= v >> 8;
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;

  s->reg = ((s->reg << 1) | (v & 1u)) & mask;
  return out;
}

void
ccsds_tm_rand_seq_with (const ccsds_tm_rand_t *r, uint8_t *out, size_t n)
{
  ccsds_tm_rand_state_t s;
  ccsds_tm_rand_init (&s, r);
  for (size_t i = 0; i < n; i++)
    out[i] = ccsds_tm_rand_step (&s);
}

void
ccsds_tm_rand_seq (uint8_t *out, size_t n)
{
  ccsds_tm_rand_seq_with (NULL, out, n);
}

void
ccsds_tm_randomise_with (const ccsds_tm_rand_t *r, uint8_t *bits, size_t n)
{
  ccsds_tm_rand_state_t s;
  ccsds_tm_rand_init (&s, r);
  for (size_t i = 0; i < n; i++)
    bits[i] = (uint8_t)((bits[i] ^ ccsds_tm_rand_step (&s)) & 1u);
}

void
ccsds_tm_randomise (uint8_t *bits, size_t n)
{
  ccsds_tm_randomise_with (NULL, bits, n);
}
