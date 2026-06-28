/**
 * @file mpsk_receiver_ber.c
 * @brief Symbol-error-rate validation for the pulse-shaped M-PSK receiver.
 *
 * Drives MpskReceiver with a rectangular (I&D-matched) BPSK/QPSK/8PSK signal
 * at a carrier offset, over a sweep of matched-filter-output Es/N0, and
 * compares the genie-aligned symbol error rate to the theoretical coherent
 * M-PSK bound. The receiver acquires the carrier non-data-aided (M-th power)
 * and recovers symbol timing (Gardner); the only genie is resolving the
 * inherent M-fold phase ambiguity and the fixed loop/filter lag when scoring.
 *
 *   theory:  BPSK  SER = Q(sqrt(2*Es/N0))
 *            QPSK  SER ~ 2*Q(sqrt(Es/N0))
 *            8PSK  SER ~ 2*Q(sqrt(2*Es/N0)*sin(pi/8))
 *
 * Build sets the matched-filter-output Es/N0 directly: a rectangular symbol of
 * unit amplitude through the length-`sps` boxcar matched filter has output
 * noise power N0_out = sigma_w^2 / sps, so sigma_quad^2 = sps / (2 *
 * Es/N0_lin).
 *
 * Usage:
 *   mpsk_receiver_ber           # print the SER vs Es/N0 sweep per M
 *   mpsk_receiver_ber --check   # assert implementation loss <= LOSS_DB, exit
 * 1 on fail
 */
#include "mpsk_receiver/mpsk_receiver_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSYM 40000
#define SPS 8
#define LOSS_DB 2.0 /* max tolerated implementation loss vs the bound */

static uint32_t
xorshift (uint32_t *s)
{
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}

static double
uni (uint32_t *s)
{
  return ((double)xorshift (s) + 1.0) / 4294967297.0;
}

static double
gauss (uint32_t *s)
{
  return sqrt (-2.0 * log (uni (s))) * cos (2.0 * M_PI * uni (s));
}

static double
qfunc (double x)
{
  return 0.5 * erfc (x / sqrt (2.0));
}

static double
phi0_for (int m)
{
  return (m == 4) ? (M_PI / 4.0) : 0.0;
}

/* Theoretical coherent M-PSK symbol error rate at matched-filter Es/N0 (lin).
 */
static double
theory_ser (int m, double esn0)
{
  if (m == 2)
    return qfunc (sqrt (2.0 * esn0));
  if (m == 4)
    return 2.0 * qfunc (sqrt (esn0));
  return 2.0 * qfunc (sqrt (2.0 * esn0) * sin (M_PI / 8.0)); /* 8PSK */
}

/* Run one (m, Es/N0) point; return the genie-aligned tail SER. */
static double
measure_ser (int m, double esn0_db, double foff, uint32_t seed)
{
  double   esn0  = pow (10.0, esn0_db / 10.0);
  double   sigma = sqrt ((double)SPS / (2.0 * esn0)); /* per-quadrature */
  double   phi0  = phi0_for (m);
  uint32_t st    = seed;

  float complex *tx  = malloc (NSYM * SPS * sizeof (*tx));
  int           *idx = malloc (NSYM * sizeof (int));
  float complex *out = malloc (NSYM * sizeof (*out));
  for (size_t k = 0; k < NSYM; k++)
    {
      int ki           = (int)(xorshift (&st) % (uint32_t)m);
      idx[k]           = ki;
      double        th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      float complex s  = (float)cos (th) + (float)sin (th) * I;
      for (size_t j = 0; j < SPS; j++)
        {
          size_t        n  = k * SPS + j;
          double        ph = 2.0 * M_PI * foff * (double)n;
          float complex c  = (float)cos (ph) + (float)sin (ph) * I;
          float complex w  = (float)(sigma * gauss (&st))
                             + (float)(sigma * gauss (&st)) * I;
          tx[n]            = s * c + w;
        }
    }
  /* Acquire non-data-aided, then hand over to low-jitter decision-directed
   * tracking — essential for 8PSK, whose M-th-power phase noise would
   * otherwise cross the +-pi/8 decision margins. lock_thresh below the per-M
   * lock ceiling (BPSK ~1, QPSK ~0.62, 8PSK ~0.41). */
  mpsk_receiver_state_t *rx
      = mpsk_receiver_create (m, SPS, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.005,
                              0.707, 0.005, 1, 0.3, foff, 300, 0);
  size_t nout = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
  mpsk_receiver_destroy (rx);

  /* genie: best over the M-fold rotation and a small lag, on the locked tail
   */
  size_t lo = nout / 4, hi = nout - nout / 8;
  double best = 1.0;
  for (int lag = -30; lag <= 30; lag++)
    for (int rot = 0; rot < m; rot++)
      {
        long err = 0, cnt = 0;
        for (size_t i = lo; i < hi; i++)
          {
            long j = (long)i + lag;
            if (j < 0 || j >= (long)NSYM)
              continue;
            double th
                = atan2 ((double)cimagf (out[i]), (double)crealf (out[i]))
                  - phi0;
            int d
                = (int)((lround (th * (double)m / (2.0 * M_PI)) % m + m) % m);
            if (((d - idx[j] - rot) % m + m) % m != 0)
              err++;
            cnt++;
          }
        if (cnt > 1000)
          {
            double s = (double)err / (double)cnt;
            if (s < best)
              best = s;
          }
      }
  free (tx);
  free (idx);
  free (out);
  return best;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int ms[3] = { 2, 4, 8 };
  /* one mid-SNR check point per M where the bound is well above the noise
   * floor yet small enough that 2 dB of loss is clearly measurable. */
  double chk_db[3] = { 8.0, 11.0, 15.0 };
  int    rc        = 0;

  if (!check)
    printf ("# M  Es/N0(dB)  measured_SER  theory_SER\n");
  for (int mi = 0; mi < 3; mi++)
    {
      int m = ms[mi];
      if (!check)
        {
          for (double db = 4.0; db <= 16.0; db += 2.0)
            {
              double meas = measure_ser (m, db, 0.0005, 2024u + (unsigned)mi);
              printf ("%2d   %6.1f     %.3e   %.3e\n", m, db, meas,
                      theory_ser (m, pow (10.0, db / 10.0)));
            }
          continue;
        }
      double db   = chk_db[mi];
      double meas = measure_ser (m, db, 0.0005, 2024u + (unsigned)mi);
      /* implementation-loss bound: no worse than the theory at (Es/N0 - LOSS).
       */
      double bound = theory_ser (m, pow (10.0, (db - LOSS_DB) / 10.0));
      int    ok    = meas <= bound;
      printf ("M=%d Es/N0=%.1fdB SER=%.3e  bound@-%.0fdB=%.3e  %s\n", m, db,
              meas, LOSS_DB, bound, ok ? "OK" : "FAIL");
      if (!ok)
        rc = 1;
    }
  if (check)
    printf (rc ? "mpsk_receiver_ber FAILED\n" : "mpsk_receiver_ber PASSED\n");
  return rc;
}
