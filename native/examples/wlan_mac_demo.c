/**
 * wlan_mac_demo.c -- the 802.11 minimal MAC: a frame body to a PSDU.
 *
 * The generator wraps the caller's octets as one MPDU and does nothing more
 * (docs/design/wlan.md, "The minimal MAC"): a header and a body in, the
 * header, the body and the frame check sequence out. The FCS is CRC-32 over
 * every octet before it, appended least significant octet first, so the CRC
 * over a whole frame is the same constant whatever the frame holds.
 *
 * Shows:
 *   1. A 24-octet header and a body wrapped into a PSDU
 *   2. The FCS against the CRC-32 catalogue check value
 *   3. The property the wire order gives: any frame with its FCS appended
 *      has one CRC, a receiver's test for "frame intact"
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/wlan_mac_demo
 */
#include "doppler/wlan/wlan_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{
  printf ("=== doppler 802.11 minimal MAC demo ===\n\n");
  int ok = 1;

  /* 1. A header and a body, wrapped. The header is raw octets the caller
   *    gives: the generator does not build one. */
  uint8_t header[24], body[100], psdu[128];
  for (int i = 0; i < 24; i++)
    header[i] = (uint8_t)(i * 5 + 1);
  for (int i = 0; i < 100; i++)
    body[i] = (uint8_t)(i * 7 + 3);
  const size_t n
      = dp_wlan_mpdu (WLAN_MAC_DATA, header, 24, body, 100, psdu, sizeof psdu);
  printf ("--- 1. %zu octets in, a %zu-octet PSDU out ---\n", (size_t)124, n);
  ok &= (n == 128);
  ok &= (memcmp (psdu, header, 24) == 0);
  ok &= (memcmp (psdu + 24, body, 100) == 0);

  /* how many OFDM symbols it fills at 36 Mb/s: 16-QAM, rate 3/4 */
  dp_wlan_state_t *w = dp_wlan_create (2, 2, 0u, 0, 1);
  if (!w)
    return 1;
  printf (
      "    at 16-QAM rate 3/4 (%u data bits per symbol): %zu OFDM symbols\n\n",
      (unsigned)w->n_dbps, dp_wlan_n_sym (w, n));
  ok &= (dp_wlan_n_sym (w, n) == 8);
  dp_wlan_destroy (w);

  /* 2. The FCS, against the catalogue check value of CRC-32/ISO-HDLC over
   *    the nine ASCII digits: CBF43926, appended least significant first. */
  const uint8_t digits[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  uint8_t       check[16];
  const size_t  nc
      = dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, digits, 9, check, sizeof check);
  printf ("--- 2. The FCS ---\n    123456789 -> %02X %02X %02X %02X "
          "(the check value CBF43926, least significant octet first)\n\n",
          check[9], check[10], check[11], check[12]);
  ok &= (nc == 13 && check[9] == 0x26 && check[10] == 0x39 && check[11] == 0xF4
         && check[12] == 0xCB);

  /* 3. Any frame with its FCS appended has the same CRC. */
  const dp_crc_t model = DP_CRC_32_ISO_HDLC;
  const uint64_t a     = dp_crc_bytes (&model, psdu, n);
  uint8_t        other[128];
  for (int i = 0; i < 100; i++)
    body[i] = (uint8_t)(i * 11 + 9);
  dp_wlan_mpdu (WLAN_MAC_DATA, header, 24, body, 100, other, sizeof other);
  const uint64_t b = dp_crc_bytes (&model, other, 128);
  printf ("--- 3. The CRC over a whole frame ---\n    %08llX and %08llX: ",
          (unsigned long long)a, (unsigned long long)b);
  printf ("%s\n\n", a == b ? "equal, whatever the frame" : "DIFFERENT");
  ok &= (a == b);

  if (!ok)
    {
      fprintf (stderr, "FAILED\n");
      return 1;
    }
  printf ("OK\n");
  return 0;
}
