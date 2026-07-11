/*
 * crc16.c — wfmgen module-level function.
 */
#include "wfm/wfm_core.h"

#include "dp_crc16.h"

/* CRC-16-CCITT over an unpacked bit array — thin public alias over the
 * shared TX/RX frame kernel (dp_crc16.h), so Python frame builders and
 * validators use the exact bits burst_demod checks. */
uint16_t
crc16 (const uint8_t *bits, size_t bits_len)
{
  return dp_crc16_ccitt (bits, bits_len);
}
