/*
 * test_wfm_dsp.c — DSSS spreading + RRC taps (Phase B).
 */
#include "wfmgen/wfm_dsp.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>

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

  printf (
      "test_wfm_dsp: OK (rrc unit-energy/symmetric, dsss spread/despread)\n");
  return 0;
}
