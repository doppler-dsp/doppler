/**
 * @file dp_crc16.h
 * @brief CRC-16-CCITT over a bit stream — the one CRC shared by every
 * doppler frame producer and consumer.
 *
 * The DSSS burst frame convention is `sync | payload | CRC-16`, with the
 * 16-bit trailer computed over the payload bits only and transmitted
 * MSB-first. `burst_demod` validates it on receive and a frame's CRC stage
 * (`WFM_STAGE_CRC16`, `wfm/wfm_frame.h`) appends it on transmit; both call
 * this one inline so the two ends can never drift.
 *
 * Header-only (like `wfm_synth_mls_poly`) so no component grows a link-line
 * dependency for a 10-line kernel. It is one row of the parametric CRC in
 * `dp_crc.h` (CRC-16/IBM-3740, the catalogue's name for this model), not a
 * second implementation of the arithmetic.
 */
#ifndef DP_CRC16_H
#define DP_CRC16_H

#include "doppler/dp_crc.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC-16-CCITT (poly 0x1021, init 0xFFFF) over a bit stream,
 * MSB-first.
 *
 * Each input byte carries ONE bit (0/1) in its LSB — the natural form for
 * the unpacked bit arrays doppler's frame paths pass around, not a packed
 * byte-stream CRC.
 *
 * @param bits  Array of @p n bytes, each 0 or 1 (only the LSB is used).
 * @param n     Number of bits.
 * @return The 16-bit CRC; transmit it MSB-first (`(crc >> 15) & 1` goes
 *         first on the wire).
 */
static inline uint16_t
dp_crc16_ccitt (const uint8_t *bits, size_t n)
{
  const dp_crc_t model = DP_CRC_16_IBM_3740;
  return (uint16_t)dp_crc_bits (&model, bits, n);
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC16_H */
