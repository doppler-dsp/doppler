/**
 * wlan_ppdu_demo.c -- an 802.11a/g PPDU, rendered to samples in one call.
 *
 * The steps a caller takes, in the order they happen:
 *
 *   1. dp_wlan_mpdu()          wrap a body as a PSDU, FCS included
 *   2. dp_wlan_rate_n_sym()    how many Data OFDM symbols that PSDU fills at
 *                              the chosen rate
 *   3. dp_wlan_ppdu_len()      how many samples the whole PPDU occupies: the
 *                              CALLER allocates the buffer
 *   4. dp_wlan_ppdu()          the render: L-STF, L-LTF, SIGNAL and the Data
 *                              field, from the PSDU, the rate, the scrambler
 *                              register state and the standard's tables
 *   5. check what came out     the length, the fields' structure, the power
 *   6. oversample              the same PPDU at 2x the rate, by a larger
 *                              transform, held to the 20 MHz one
 *
 * dp_wlan_ppdu() does everything between a PSDU and its samples: the SERVICE,
 * tail and pad assembly, the scrambler, the punctured BCC, the interleaver,
 * the constellation mapper, the pilots and the OFDM modulator. It returns 0,
 * changing nothing, if any stage refuses: a rate the standard does not have, a
 * scrambler state outside 1..127, a buffer too small for the PPDU.
 *
 * What is checked here, each in the language of the standard rather than of
 * this code: the PPDU is 160 + 160 + 80 + 80 * N_SYM samples; the L-STF is
 * ten repetitions of one 16-sample pattern; the L-LTF's two long symbols are
 * identical, preceded by the double guard interval; and the power of each
 * field is what this library's scale (1/sqrt 52, #3148) makes it: exactly one
 * for the two training fields, and for each SIGNAL or Data symbol the energy
 * of its own grid over 52, which Parseval ties to the points it was built
 * from. The scale is the library's convention (unit mean power over the 52
 * occupied subcarriers, #3148); Annex I's printed samples are the same math
 * with the transform's own 1/64, so they equal ours times sqrt(52)/64.
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/wlan_ppdu_demo
 */
#include "doppler/wlan/wlan_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float _Complex cf;

/* |x|^2 */
static double
pw (cf x)
{
  return (double)crealf (x) * crealf (x) + (double)cimagf (x) * cimagf (x);
}

int
main (void)
{
  printf ("=== doppler 802.11 PPDU demo ===\n\n");
  int ok = 1;

  /* 1. A 24-octet header and a 100-octet body, wrapped as one PSDU. */
  uint8_t header[24], body[100], psdu[128];
  for (int i = 0; i < 24; i++)
    header[i] = (uint8_t)(i * 5 + 1);
  for (int i = 0; i < 100; i++)
    body[i] = (uint8_t)(i * 7 + 3);
  const size_t len
      = dp_wlan_mpdu (WLAN_MAC_DATA, header, 24, body, 100, psdu, sizeof psdu);
  ok &= (len == 128);

  /* 2-3. 16-QAM at code rate 3/4 (36 Mb/s): how many Data symbols, how many
     samples. The buffer is the caller's; the library never allocates it. */
  const dp_wlan_tables_t *tables = dp_wlan_tables_standard ();
  const dp_wlan_rate_t    rate   = { 4, WLAN_CODE_3_4 };
  const size_t            n_sym  = dp_wlan_rate_n_sym (&rate, len);
  const size_t            n      = dp_wlan_ppdu_len (n_sym, 0u, 1u);
  printf (
      "--- 1-3. a %zu-octet PSDU at 16-QAM rate 3/4: %zu Data OFDM symbols, "
      "%zu samples\n",
      len, n_sym, n);
  ok &= (n_sym == 8 && n == 160 + 160 + 80 + 80 * n_sym);
  cf *ppdu = malloc (n * sizeof *ppdu);
  if (!ppdu)
    return 1;

  /* 4. The render. The scrambler register starts at 93 (any of 1..127; the
     standard's example uses 1011101b). Zero window: no transition shaping. */
  const size_t got
      = dp_wlan_ppdu (tables, &rate, psdu, len, 93, 0u, 1u, ppdu, n);
  printf ("--- 4. dp_wlan_ppdu wrote %zu samples\n", got);
  ok &= (got == n);

  /* 5a. The L-STF: ten repetitions of one 16-sample pattern. */
  double stf = 0;
  for (size_t i = 0; i + 16 < 160; i++)
    stf = fmax (stf, cabs ((double _Complex)ppdu[i] - ppdu[i + 16]));
  printf ("--- 5. L-STF periodic with period 16: worst difference %.1e\n",
          stf);
  ok &= (stf < 1e-5);

  /* 5b. The L-LTF: a 32-sample double guard, then two identical 64-sample
     long symbols. */
  double ltf = 0;
  for (size_t i = 0; i < 64; i++)
    ltf = fmax (ltf, cabs ((double _Complex)ppdu[160 + 32 + i]
                           - ppdu[160 + 32 + 64 + i]));
  printf ("    L-LTF's two long symbols alike: worst difference %.1e\n", ltf);
  ok &= (ltf < 1e-5);

  /* 5c. Power, field by field, each against what it must be.
   *
   * The training fields are deterministic, and this library's scale
   * (1/sqrt 52, one unit for the 52 occupied subcarriers) makes their mean
   * power exactly one. A SIGNAL or Data OFDM symbol is not constant: its 64
   * body samples have, by Parseval, a mean power equal to the grid's energy
   * over 52, that is (the sum of |point|^2 over its 48 data points, plus 4
   * for the four unit pilots) / 52, so each is held to THAT, computed from
   * the points dp_wlan_points() returns for the same PSDU, with no tolerance
   * for chance. (The 80-sample symbol's power differs from its body's by the
   * cyclic prefix, which repeats only the body's last 16 samples; and a
   * 16-QAM payload's mean |point|^2 is one only on average over the
   * constellation, 8 symbols of it are not.)
   *
   * What this holds, and what it does not: the Data bodies are compared with
   * the library's OWN points, so that check ties the renderer (the layout,
   * the pilots, the transform and the scale) to the mapper by Parseval; it
   * does not tie the mapper to the standard: no test in this tree does, and
   * the Annex I.1 stage-by-stage comparison was run downstream (#3148). The
   * two training fields reading exactly one holds the library to its own
   * convention, unit power, and nothing more: Annex I's printed samples equal
   * ours times sqrt(52)/64, which the unit tests hold with a table whose three
   * scales are 1/64. */
  double p_stf = 0, p_ltf = 0;
  for (size_t i = 0; i < 160; i++)
    {
      p_stf += pw (ppdu[i]);
      p_ltf += pw (ppdu[160 + i]);
    }
  p_stf /= 160.0;
  p_ltf /= 160.0;
  printf ("    mean power: L-STF %.6f, L-LTF %.6f (both exactly 1)\n", p_stf,
          p_ltf);
  ok &= (fabs (p_stf - 1.0) < 1e-6 && fabs (p_ltf - 1.0) < 1e-6);

  cf *pts = malloc ((48 + 48 * n_sym) * sizeof *pts);
  if (!pts)
    return 1;
  ok &= (dp_wlan_points (tables, &rate, psdu, len, 93, pts, 48 + 48 * n_sym)
         == 48 + 48 * n_sym);
  double worst = 0, data_got = 0, data_want = 0;
  for (size_t s = 0; s < 1 + n_sym; s++) /* SIGNAL, then the Data symbols */
    {
      double grid = 4.0, body = 0;
      for (size_t k = 0; k < 48; k++)
        grid += pw (pts[48 * s + k]);
      for (size_t i = 0; i < 64; i++)
        body += pw (ppdu[320 + 80 * s + 16 + i]);
      body /= 64.0;
      grid /= 52.0;
      worst = fmax (worst, fabs (body - grid));
      if (s == 0)
        printf ("    SIGNAL body %.6f (BPSK and four pilots: exactly 1)\n",
                body);
      else
        {
          data_got += body;
          data_want += grid;
        }
    }
  printf ("    Data bodies: mean power %.4f, from the points %.4f (a "
          "16-QAM payload of %zu symbols; the constellation's own mean is "
          "1)\n",
          data_got / (double)n_sym, data_want / (double)n_sym, n_sym);
  printf ("    every body against its grid's energy: worst difference "
          "%.1e\n",
          worst);
  ok &= (worst < 1e-5);
  free (pts);

  /* A refusal changes nothing and returns 0: a scrambler state of 0 is not
     one the register can hold. */
  cf spare[1] = { 0 };
  ok &= (dp_wlan_ppdu (tables, &rate, psdu, len, 0, 0u, 1u, spare, 1) == 0);
  ok &= (dp_wlan_ppdu (tables, &rate, psdu, len, 93, 0u, 1u, ppdu, n - 1)
         == 0);
  printf ("    a zero scrambler state, and a buffer one sample short, are "
          "refused with 0\n");

  /* 6. The same PPDU at twice the rate, 40 MHz: a transform twice as long,
     the same subcarriers, not a resampler. Every second sample is the 20 MHz
     PPDU's, to float rounding: the relative l2 error is held to 1.5e-3, the
     tolerance and statistic of the unit test (test_wlan_core.c, WLAN_FFT_TOL:
     the FFT's own 5e-4 per component, for the two transforms compared). */
  const size_t n2 = dp_wlan_ppdu_len (n_sym, 0u, 2u);
  cf          *p2 = malloc (n2 * sizeof *p2);
  if (!p2)
    return 1;
  ok &= (dp_wlan_ppdu (tables, &rate, psdu, len, 93, 0u, 2u, p2, n2) == n2);
  double num = 0, den = 0;
  for (size_t i = 0; i < n; i++)
    {
      const double d = cabs ((double _Complex)p2[2 * i] - ppdu[i]);
      num += d * d;
      den += pw (ppdu[i]);
    }
  const double dec = sqrt (num / den);
  printf (
      "--- 6. at 2x oversampling: %zu samples, every second one the 20 MHz "
      "PPDU's, relative l2 difference %.1e\n",
      n2, dec);
  ok &= (n2 == 2 * n && dec < 1.5e-3);
  free (p2);

  free (ppdu);
  printf ("\nwlan_ppdu_demo: %s\n", ok ? "OK" : "FAILED");
  return ok ? 0 : 1;
}
