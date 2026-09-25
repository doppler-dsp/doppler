/**
 * @file dp_crc16.h
 * @brief CRC-16-CCITT over a bit stream — the one CRC shared by every
 * doppler frame producer and consumer.
 *
 * The DSSS burst frame convention is `sync | payload | CRC-16`, with the
 * 16-bit trailer computed over the payload bits only and transmitted
 * MSB-first. `burst_demod` validates it on receive and the wfmgen DSSS
 * frame builder (`wfm_frame_dsss_chips`) appends it on transmit; both call
 * this one inline so the two ends can never drift.
 *
 * Header-only (like `wfm_synth_mls_poly`) so no component grows a link-line
 * dependency for a 10-line kernel.
 */
#ifndef DP_CRC16_H
#define DP_CRC16_H

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
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      crc ^= (uint16_t)((bits[i] & 1u) << 15);
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
    }
  return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC16_H */
