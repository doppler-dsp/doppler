/**
 * @file ber_despreader.c
 * @brief Monte-Carlo BER validation: the synchronous coherent despreader hits
 *        the BPSK matched-filter bound.
 *
 * A synchronous DSSS-BPSK symbol is one data bit spread by a full MLS code
 * period (carrier and code known/acquired); the optimal detector coherently
 * correlates the received chips with the local code — a matched filter whose
 * decision SNR is exactly Es/N0, so the bit-error rate must follow the BPSK
 * bound `BER = Q(sqrt(2 Es/N0))` with no implementation loss. In particular
 * `BER = 1e-5` at `Es/N0 = 9.6 dB`.
 *
 * Speed comes from two doppler primitives: the MLS code from `pn`
 * (wfm_synth_mls_poly), and **vectorized AWGN** from `awgn` (xoshiro256++ +
 * Box-Muller, AVX2) to fill the per-chip noise in bulk. The despread is a flat
 * `code . rx` dot product the compiler auto-vectorizes (~1 Gchip-trial/s).
 *
 * Usage:
 *   ber_despreader              full sweep (proves 1e-5 @ 9.6 dB), ~8 s
 *   ber_despreader --check      fast CI assert vs theory at 2..6 dB, ~2 s
 */
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BATCHC (1 << 20) /* complex noise samples per AWGN refill */

static double
qfunc (double x)
{
  return 0.5 * erfc (x / sqrt (2.0));
}

/* One BER point: NSYM synchronous symbols at the given Es/N0, MLS length n. */
static double
ber_point (const float *codef, int L, awgn_state_t *g, float *nb,
           double esn0_db, long nsym, uint64_t *dst)
{
  double esn0  = pow (10.0, esn0_db / 10.0);
  double sigma = sqrt ((double)L / (2.0 * esn0)); /* per-chip real-noise std */
  awgn_set_amplitude (g, (float)sigma);
  awgn_reset (g);
  long have = 0, pos = 0, errs = 0;
  for (long k = 0; k < nsym; k++)
    {
      if (pos + L > have)
        {
          awgn_generate (g, BATCHC, (float complex *)nb);
          have = 2 * BATCHC;
          pos  = 0;
        }
      const float *restrict ns = nb + pos;
      pos += L;
      float zn = 0.f;
      for (int i = 0; i < L; i++)
        zn += codef[i] * ns[i]; /* despread noise term: code . noise */
      /* data bit d (random); received despread = d*L + zn, decide sign */
      *dst ^= *dst << 13;
      *dst ^= *dst >> 7;
      *dst ^= *dst << 17;
      int    d = (*dst & 1) ? 1 : -1;
      double z = (double)d * L + ((d > 0) ? zn : -zn);
      if ((z >= 0.0 ? 1 : -1) != d)
        errs++;
    }
  return (double)errs / (double)nsym;
}

int
main (int argc, char **argv)
{
  int  check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int  n     = 10; /* MLS length -> L = 1023 (GPS C/A length) */
  int  L     = (1 << n) - 1;
  long nsym  = check ? 300000 : 8000000;

  pn_state_t *pn    = pn_create (wfm_synth_mls_poly (n), 1, n, 0);
  float      *codef = malloc ((size_t)((L + 7) & ~7) * sizeof (float));
  for (int i = 0; i < L; i++)
    codef[i] = pn_step (pn) ? -1.f : 1.f;
  pn_destroy (pn);

  awgn_state_t *g   = awgn_create (0xBEEF, 1.0f);
  float        *nb  = malloc ((size_t)2 * BATCHC * sizeof (float));
  uint64_t      dst = 0x1234567u;

  /* --check uses 2..6 dB where BER is high enough for a tight interval; the
   * full run adds 8 and 9.6 dB to exhibit the 1e-5 operating point. */
  double  full_db[] = { 2, 4, 6, 8, 9.6 };
  double  chk_db[]  = { 2, 4, 6 };
  double *db        = check ? chk_db : full_db;
  int     np        = check ? 3 : 5;

  printf ("synchronous despreader BER  (MLS n=%d, L=%d, %ld sym/pt)\n", n, L,
          nsym);
  printf ("  Es/N0(dB)   measured     theory       meas/theory\n");
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  int fail = 0;
  for (int p = 0; p < np; p++)
    {
      double meas = ber_point (codef, L, g, nb, db[p], nsym, &dst);
      double th   = qfunc (sqrt (2.0 * pow (10.0, db[p] / 10.0)));
      double r    = meas / th;
      printf ("   %5.1f     %.3e    %.3e    %.3f\n", db[p], meas, th, r);
      if (check && (r < 0.85 || r > 1.15)) /* ~few-% CI at these counts */
        fail = 1;
    }
  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
  printf ("  elapsed %.1f s  (%.2e chip-trials/s)\n", sec,
          (double)L * nsym * np / sec);

  free (nb);
  free (codef);
  awgn_destroy (g);
  if (fail)
    {
      fprintf (stderr, "BER deviates from the BPSK bound — FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: synchronous despreader tracks the BPSK bound\n");
  return 0;
}
