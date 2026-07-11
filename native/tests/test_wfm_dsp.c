/*
 * test_wfm_dsp.c — DSSS spreading + RRC taps (Phase B) + the two-code DSSS
 * burst frame builder.
 */
#include "wfm/wfm_dsp.h"

#include "dp_crc16.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c, m)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", m);                                  \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

static int
check_rrc (double beta, int sps, int span)
{
  size_t       n = wfm_rrc_ntaps (sps, span);
  static float taps[4096];
  CHECK (n <= 4096, "ntaps fits");
  wfm_rrc_taps (beta, sps, span, taps);
  double sumsq = 0.0;
  size_t mid   = (size_t)(span * sps);
  for (size_t i = 0; i < n; i++)
    {
      CHECK (isfinite (taps[i]), "tap finite (no singularity NaN)");
      sumsq += (double)taps[i] * taps[i];
      /* symmetric about the centre */
      CHECK (fabsf (taps[i] - taps[n - 1 - i]) < 1e-5f, "rrc symmetric");
      /* centre tap is the peak */
      CHECK (fabsf (taps[i]) <= fabsf (taps[mid]) + 1e-6f, "centre is peak");
    }
  CHECK (fabs (sumsq - 1.0) < 1e-4, "rrc unit energy");
  return 0;
}

int
main (void)
{
  /* RRC: plain (β=0), typical (β=0.35), and βs whose 1/(4β) lands exactly on
   * a sample so the singularity branch is exercised (β=0.25→1 sym, sps=4). */
  if (check_rrc (0.0, 4, 6))
    return 1;
  if (check_rrc (0.35, 8, 8))
    return 1;
  if (check_rrc (0.25, 4, 6))
    return 1;
  if (check_rrc (0.5, 4, 6))
    return 1;

  /* DSSS: spread two symbols by a 4-chip code, check values + despread. */
  float _Complex syms[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
  uint8_t code[4]        = { 0, 1, 1, 0 }; /* signs: +,-,-,+ */
  float _Complex chips[8];
  wfm_dsss_spread (syms, 2, code, 4, chips);
  const float sgn[4] = { 1, -1, -1, 1 };
  for (size_t i = 0; i < 2; i++)
    for (size_t j = 0; j < 4; j++)
      CHECK (chips[i * 4 + j] == syms[i] * sgn[j], "spread value");

  /* despread (correlate with the code) recovers sym * sf */
  for (size_t i = 0; i < 2; i++)
    {
      float _Complex acc = 0;
      for (size_t j = 0; j < 4; j++)
        acc += chips[i * 4 + j] * sgn[j];
      CHECK (cabsf (acc / 4.0f - syms[i]) < 1e-6f, "despread recovers symbol");
    }

  /* ── CRC-16-CCITT vector: the standard check input "123456789" (as bits,
   * MSB-first per byte) must give 0x29B1 — pins dp_crc16 to the same
   * convention burst_demod validates on receive. */
  {
    const char *ascii = "123456789";
    uint8_t     bits[72];
    for (size_t i = 0; i < 9; i++)
      for (int b = 0; b < 8; b++)
        bits[i * 8 + b] = (uint8_t)((ascii[i] >> (7 - b)) & 1);
    CHECK (dp_crc16_ccitt (bits, 72) == 0x29B1u, "crc16-ccitt check vector");
  }

  /* ── Frame builder: preamble tile + XOR spread + MSB-first CRC trailer,
   * against a fully hand-computed vector. */
  {
    uint8_t acq[3]   = { 1, 0, 1 };
    uint8_t dcode[2] = { 0, 1 };
    uint8_t sync[2]  = { 1, 0 };
    uint8_t pay[3]   = { 1, 1, 0 };

    /* sizing: 3*2 preamble + (2 sync + 3 payload + 16 crc) * 2 chips */
    size_t n = wfm_frame_dsss_nchips (3, 2, 2, 2, 3, 1);
    CHECK (n == 6 + 21 * 2, "nchips counts preamble + spread frame + crc");
    /* crc off / no payload: trailer only with payload bits to protect */
    CHECK (wfm_frame_dsss_nchips (3, 2, 2, 2, 3, 0) == 6 + 5 * 2,
           "nchips without crc");
    CHECK (wfm_frame_dsss_nchips (3, 2, 2, 2, 0, 1) == 6 + 2 * 2,
           "crc over empty payload is dropped");
    CHECK (wfm_frame_dsss_nchips (0, 0, 2, 0, 3, 0) == 3 * 2,
           "preamble-less frame");
    CHECK (wfm_frame_dsss_nchips (3, 2, 0, 0, 0, 0) == 6, "preamble only");
    CHECK (wfm_frame_dsss_nchips (0, 0, 0, 2, 3, 1) == 0,
           "frame bits with no data code is invalid");
    CHECK (wfm_frame_dsss_nchips (0, 0, 2, 0, 0, 0) == 0, "empty burst");

    static uint8_t out[64];
    /* the chips builder mirrors the sizing guard: invalid geometry (frame
     * bits with no data code) writes nothing and returns 0 */
    CHECK (wfm_frame_dsss_chips (NULL, 0, 0, NULL, 0, sync, 2, pay, 3, 1, out)
               == 0,
           "chips builder rejects invalid geometry");
    CHECK (wfm_frame_dsss_chips (acq, 3, 2, dcode, 2, sync, 2, pay, 3, 1, out)
               == n,
           "chips written == nchips");
    /* preamble: acq tiled twice, unmodulated */
    const uint8_t pre[6] = { 1, 0, 1, 1, 0, 1 };
    CHECK (memcmp (out, pre, 6) == 0, "preamble is the tiled code");
    /* frame: each bit XOR the code — sync 1,0 then payload 1,1,0 */
    const uint8_t head[10] = { 1, 0, 0, 1, /* sync 1,0 */
                               1, 0, 1, 0, 0, 1 /* payload 1,1,0 */ };
    CHECK (memcmp (out + 6, head, 10) == 0, "frame bits XOR-spread");
    /* crc trailer: crc16(payload) spread MSB-first */
    uint16_t c = dp_crc16_ccitt (pay, 3);
    for (size_t i = 0; i < 16; i++)
      {
        uint8_t b = (uint8_t)((c >> (15 - i)) & 1u);
        CHECK (out[16 + 2 * i] == (b ^ 0u) && out[16 + 2 * i + 1] == (b ^ 1u),
               "crc trailer spread MSB-first");
      }
  }

  printf ("test_wfm_dsp: OK (rrc unit-energy/symmetric, dsss "
          "spread/despread, crc16 vector, burst frame builder)\n");
  return 0;
}
