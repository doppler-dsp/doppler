/*
 * ccsds_rand.c — the CCSDS pseudo-randomiser (CCSDS 131.0-B, section 8).
 *
 * Eight stages over h(x) = x^8 + x^7 + x^5 + x^3 + 1, preset to all ones.
 * The sequence is fixed and published, so the tap arrangement is not a design
 * choice here — it is whatever reproduces `FF 48 0E C0 9A ...`, and
 * test_fec_ccsds_rand is what holds it to that. An LFSR is easy to write in
 * several self-consistent ways that produce different sequences; only the
 * published prefix separates them.
 */
#include "fec/fec_ccsds.h"

/* One step of the generator: emit the oldest stage, then shift in feedback.
 *
 * h(x) = x^8 + x^7 + x^5 + x^3 + 1 is the characteristic polynomial, so the
 * recurrence it stands for is
 *
 *     s[n+8] = s[n+7] ^ s[n+5] ^ s[n+3] ^ s[n]
 *
 * and THAT is what has to be transcribed, not the exponents. Holding s[n] in
 * bit 7 (oldest, emitted next) through s[n+7] in bit 0, the four terms land
 * on bits 0, 2, 4 and 7.
 *
 * Writing the taps as the exponents instead — bits 7, 6, 4, 2 — is a
 * plausible-looking transcription that makes the feedback of the all-ones
 * preset zero, so the register walks 0xFF, 0xFE ... 0x00 and sticks at the
 * all-zero fixed point. It survived a round trip and both period checks (a
 * dead sequence repeats with every period and matches no earlier one) and was
 * caught only by the published prefix. */
static inline uint8_t
step (uint8_t *reg)
{
  const uint8_t out = (uint8_t)((*reg >> 7) & 1u);
  const uint8_t fb
      = (uint8_t)(((*reg >> 7) ^ (*reg >> 4) ^ (*reg >> 2) ^ *reg) & 1u);
  *reg = (uint8_t)((*reg << 1) | fb);
  return out;
}

void
fec_ccsds_rand_seq (uint8_t *out, size_t n)
{
  uint8_t reg = 0xFFu;
  for (size_t i = 0; i < n; i++)
    out[i] = step (&reg);
}

void
fec_ccsds_randomise (uint8_t *bits, size_t n)
{
  uint8_t reg = 0xFFu;
  for (size_t i = 0; i < n; i++)
    bits[i] = (uint8_t)((bits[i] ^ step (&reg)) & 1u);
}
