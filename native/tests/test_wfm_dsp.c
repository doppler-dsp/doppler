/*
 * test_wfm_dsp.c — DSSS spreading + RRC taps (Phase B) + the two-code DSSS
 * burst frame builder.
 */
#include "doppler/wfm/wfm_dsp.h"
#include "dp_test.h"

#include "doppler/dp_crc16.h"

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int
check_rrc (double beta, int sps, int span)
{
  size_t       n = wfm_rrc_ntaps (sps, span);
  static float taps[4096];
  DP_REQUIRE_MSG (n <= 4096, "ntaps fits");
  dp_wfm_rrc_taps (beta, sps, span, taps);
  double sumsq = 0.0;
  size_t mid   = (size_t)(span * sps);
  for (size_t i = 0; i < n; i++)
    {
      DP_REQUIRE_MSG (isfinite (taps[i]), "tap finite (no singularity NaN)");
      sumsq += (double)taps[i] * taps[i];
      /* symmetric about the centre */
      DP_REQUIRE_MSG (fabsf (taps[i] - taps[n - 1 - i]) < 1e-5f,
                      "rrc symmetric");
      /* centre tap is the peak */
      DP_REQUIRE_MSG (fabsf (taps[i]) <= fabsf (taps[mid]) + 1e-6f,
                      "centre is peak");
    }
  DP_REQUIRE_MSG (fabs (sumsq - 1.0) < 1e-4, "rrc unit energy");
  return 0;
}

/* The polyphase bank must be the sqrt(sps)-scaled prototype, dealt into
 * `sps` phases of `2*span+1` taps by `bank[p*num_taps+t] = proto[t*sps+p]`
 * (zero past the prototype). This pins the exact decomposition the resamp
 * interp path consumes — the crux the Python twin validated end to end. */
static int
check_rrc_polyphase (double beta, int sps, int span)
{
  size_t       proto_len = wfm_rrc_ntaps (sps, span);
  size_t       num_taps  = wfm_rrc_bank_ntaps (span);
  static float proto[4096];
  static float bank[4096];
  DP_REQUIRE_MSG (proto_len <= 4096 && (size_t)sps * num_taps <= 4096,
                  "sizes fit");

  dp_wfm_rrc_taps (beta, sps, span, proto);
  float scale = (float)sqrt ((double)sps);
  dp_wfm_rrc_polyphase_bank (beta, sps, span, bank);

  double bank_sumsq = 0.0;
  for (int p = 0; p < sps; p++)
    for (size_t t = 0; t < num_taps; t++)
      {
        size_t idx  = t * (size_t)sps + (size_t)p;
        float  want = (idx < proto_len) ? proto[idx] * scale : 0.0f;
        float  got  = bank[(size_t)p * num_taps + t];
        DP_REQUIRE_MSG (fabsf (got - want) < 1e-6f,
                        "bank == scaled decomposed proto");
        bank_sumsq += (double)got * got;
      }
  /* Every prototype tap lands in exactly one phase, so the bank's total
   * energy is the prototype's (== 1 unit-energy) times the sqrt(sps)^2 = sps
   * transmit-power scale. */
  DP_REQUIRE_MSG (fabs (bank_sumsq - (double)sps) < 1e-3,
                  "bank energy == sps");
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

  /* Polyphase bank decomposition (pow-2 sps, the shaper fast-path cases). */
  if (check_rrc_polyphase (0.35, 8, 8))
    return 1;
  if (check_rrc_polyphase (0.5, 4, 6))
    return 1;
  if (check_rrc_polyphase (0.25, 2, 10))
    return 1;

  /* DSSS: spread two symbols by a 4-chip code, check values + despread. */
  float _Complex syms[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
  uint8_t code[4]        = { 0, 1, 1, 0 }; /* signs: +,-,-,+ */
  float _Complex chips[8];
  dp_wfm_dsss_spread (syms, 2, code, 4, chips);
  const float sgn[4] = { 1, -1, -1, 1 };
  for (size_t i = 0; i < 2; i++)
    for (size_t j = 0; j < 4; j++)
      DP_REQUIRE_MSG (chips[i * 4 + j] == syms[i] * sgn[j], "spread value");

  /* despread (correlate with the code) recovers sym * sf */
  for (size_t i = 0; i < 2; i++)
    {
      float _Complex acc = 0;
      for (size_t j = 0; j < 4; j++)
        acc += chips[i * 4 + j] * sgn[j];
      DP_REQUIRE_MSG (cabsf (acc / 4.0f - syms[i]) < 1e-6f,
                      "despread recovers symbol");
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
    DP_REQUIRE_MSG (dp_crc16_ccitt (bits, 72) == 0x29B1u,
                    "crc16-ccitt check vector");
  }

  printf ("test_wfm_dsp: OK (rrc unit-energy/symmetric, dsss "
          "spread/despread, crc16 vector)\n");
  return 0;
}
