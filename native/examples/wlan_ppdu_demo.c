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
 * identical, preceded by the double guard interval; and the mean power over
 * the whole PPDU is one, because the standard's scale gives every field unit
 * average power.
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
  const size_t            n      = dp_wlan_ppdu_len (n_sym, 0u);
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
  const size_t got = dp_wlan_ppdu (tables, &rate, psdu, len, 93, 0u, ppdu, n);
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

  /* 5c. Mean power over the whole PPDU: one. */
  double power = 0;
  for (size_t i = 0; i < n; i++)
    power += (double)crealf (ppdu[i]) * crealf (ppdu[i])
             + (double)cimagf (ppdu[i]) * cimagf (ppdu[i]);
  power /= (double)n;
  printf ("    mean power %.3f (the standard's scale gives one)\n", power);
  ok &= (fabs (power - 1.0) < 0.1);

  /* A refusal changes nothing and returns 0: a scrambler state of 0 is not
     one the register can hold. */
  cf spare[1] = { 0 };
  ok &= (dp_wlan_ppdu (tables, &rate, psdu, len, 0, 0u, spare, 1) == 0);
  ok &= (dp_wlan_ppdu (tables, &rate, psdu, len, 93, 0u, ppdu, n - 1) == 0);
  printf ("    a zero scrambler state, and a buffer one sample short, are "
          "refused with 0\n");

  free (ppdu);
  printf ("\nwlan_ppdu_demo: %s\n", ok ? "OK" : "FAILED");
  return ok ? 0 : 1;
}
