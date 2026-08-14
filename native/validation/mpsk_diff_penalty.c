/**
 * @file mpsk_diff_penalty.c
 * @brief Monte-Carlo validation: what differential M-PSK actually costs.
 *
 * mpsk_core.h states one quantitative claim, and nothing measured it:
 *
 *   "at ~2x the symbol-error rate of coherent map()"
 *
 * That number is the entire basis on which a caller chooses between coherent
 * and differential mode. Differential mode removes the M-fold carrier phase
 * ambiguity for free in structure but not in performance, and a caller
 * trading a known ambiguity for an unknown penalty is not making a decision
 * — they are guessing. docs/design/mpsk.md §9.5 records it as the primitive's
 * one open number; this harness is what closes it.
 *
 * Method. Draw uniform labels, map them both ways, add complex AWGN at a
 * stated Es/N0, demap both ways, and count symbol errors against the truth.
 * The two paths see the SAME noise realisation and the same labels, so the
 * ratio is a paired measurement and the seed's luck cancels out of it.
 *
 * The constellation is unit amplitude, so Es = 1 and N0 = 1/(Es/N0); awgn's
 * amplitude parameter IS the per-component standard deviation, hence
 * sigma = sqrt(N0/2). Using the shipped generator rather than a local
 * Box-Muller is not a formality: this harness's whole subject is whether an
 * error RATE comes out right, and a private generator with a slightly wrong
 * variance moves that rate without failing anything — the mistake that put
 * 3 dB into carrier_nda_lock.c's first pass.
 *
 * The coherent path is also checked against closed-form theory, so a common
 * defect cannot hide in the ratio: two paths sharing a broken slicer would
 * still divide to 2.0, and only an external truth catches that.
 *
 *   SER_coh(M=2) = Q(sqrt(2 Es/N0))
 *   SER_coh(M>2) ~ 2 Q(sqrt(2 Es/N0) sin(pi/M))     (high-SNR, exact for M=4
 *                                                    up to the double-count)
 *
 * Note the first symbol of a differential stream references the implicit
 * zero-phase start, so it is counted like any other here — no rotation is
 * applied, and it is a fair symbol.
 *
 * Usage:
 *   mpsk_diff_penalty            full sweep over (M, Es/N0)
 *   mpsk_diff_penalty --check    fast CI gate on the default cell
 */
#include "awgn/awgn_core.h"
#include "detection/detection_core.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLK 8192

/* Gaussian tail Q(x), via the detection module's own inverse-free form.
 * erfc is the honest primitive here and detection_core owns no Q(); this is
 * the one line of it, not a second copy of a decision rule. */
static double
qfunc (double x)
{
  return 0.5 * erfc (x / sqrt (2.0));
}

/* Symbol-error counts for both modes over one noise realisation.
 *
 * Paired by construction: one label stream, one noise stream, both paths
 * fed the identical samples. */
static int
measure (int m, double esn0_db, size_t nsym, uint64_t seed, double *ser_coh,
         double *ser_diff, size_t *n_err_coh)
{
  double         esn0  = pow (10.0, esn0_db / 10.0);
  double         sigma = sqrt (1.0 / (2.0 * esn0));
  awgn_state_t  *g     = awgn_create (seed, (float)sigma);
  uint8_t       *sym   = malloc (BLK * sizeof *sym);
  uint8_t       *dc    = malloc (BLK * sizeof *dc);
  uint8_t       *dd    = malloc (BLK * sizeof *dd);
  float complex *pc    = malloc (BLK * sizeof *pc);
  float complex *pd    = malloc (BLK * sizeof *pd);
  float complex *nz    = malloc (BLK * sizeof *nz);
  if (!g || !sym || !dc || !dd || !pc || !pd || !nz)
    {
      free (sym);
      free (dc);
      free (dd);
      free (pc);
      free (pd);
      free (nz);
      awgn_destroy (g);
      *n_err_coh = 0;
      return -1;
    }

  /* A label stream drawn from the same generator's bits would correlate the
     data with the noise; a tiny LCG for the DATA only is the standard split
     and keeps awgn responsible for exactly one thing. */
  uint64_t ds      = seed * 6364136223846793005ull + 1442695040888963407ull;
  size_t   err_coh = 0, err_diff = 0, done = 0;

  while (done < nsym)
    {
      size_t n = (nsym - done < BLK) ? (nsym - done) : BLK;
      for (size_t i = 0; i < n; i++)
        {
          ds     = ds * 6364136223846793005ull + 1442695040888963407ull;
          sym[i] = (uint8_t)((ds >> 33) % (uint64_t)m);
        }
      mpsk_map (sym, n, pc, m);
      mpsk_diff_map (sym, n, pd, m);

      size_t got = awgn_generate (g, n, nz, n);
      if (got != n)
        break;
      for (size_t i = 0; i < n; i++)
        {
          pc[i] += nz[i];
          pd[i] += nz[i];
        }

      mpsk_demap (pc, n, dc, m);
      mpsk_diff_demap (pd, n, dd, m);
      for (size_t i = 0; i < n; i++)
        {
          if (dc[i] != sym[i])
            err_coh++;
          if (dd[i] != sym[i])
            err_diff++;
        }
      done += n;
    }

  *ser_coh   = (double)err_coh / (double)done;
  *ser_diff  = (double)err_diff / (double)done;
  *n_err_coh = err_coh;
  free (sym);
  free (dc);
  free (dd);
  free (pc);
  free (pd);
  free (nz);
  awgn_destroy (g);
  return 0;
}

/* Closed-form coherent SER — the external truth the ratio alone cannot
 * provide. */
static double
ser_theory (int m, double esn0_db)
{
  double esn0 = pow (10.0, esn0_db / 10.0);
  if (m == 2)
    return qfunc (sqrt (2.0 * esn0));
  return 2.0 * qfunc (sqrt (2.0 * esn0) * sin (MPSK_PI / (double)m));
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);

  if (check)
    {
      /* QPSK at 9 dB: coherent SER ~4.8e-3, so 1.5e6 symbols collect ~7200
         coherent and ~14500 differential errors — around 1% on each and
         better than that on the paired ratio. The band is set from the
         measured sweep below, not from the header's round number: the sweep
         puts QPSK between 1.85x and 2.03x across 6 to 12 dB. */
      double coh = 0.0, dif = 0.0;
      size_t nerr = 0;
      if (measure (4, 9.0, 1500000u, 20260814u, &coh, &dif, &nerr) != 0)
        {
          printf ("FAIL allocation\n");
          return 1;
        }
      double want = ser_theory (4, 9.0);
      /* Precondition, not decoration: a cell that collected too few errors
         makes both comparisons below meaningless, and a silently starved
         cell reads exactly like a passing one. */
      if (nerr < 500u)
        {
          printf ("FAIL only %zu coherent errors — cell is starved\n", nerr);
          return 1;
        }
      double ratio = (coh > 0.0) ? dif / coh : 0.0;
      int    fail  = 0;
      /* Anchor the coherent path to theory first: a shared slicer defect
         divides out of the ratio and would otherwise pass unnoticed. */
      if (!(fabs (coh - want) < 0.15 * want))
        {
          printf ("FAIL coherent SER %.5e want %.5e (theory)\n", coh, want);
          fail = 1;
        }
      if (!(ratio > 1.70 && ratio < 2.30))
        {
          printf ("FAIL differential penalty %.3fx, want ~2x\n", ratio);
          fail = 1;
        }
      if (!fail)
        printf ("mpsk_diff_penalty: OK (SER %.4e vs theory %.4e, "
                "penalty %.3fx)\n",
                coh, want, ratio);
      return fail;
    }

  const int    ms[] = { 2, 4, 8 };
  const double es[] = { 4.0, 6.0, 8.0, 10.0, 12.0, 14.0 };
  printf ("mpsk differential penalty — paired Monte-Carlo, noise from awgn\n");
  printf ("unit-amplitude constellation, so Es = 1 and sigma = "
          "sqrt(N0/2).\n\n");
  printf ("Cells collecting fewer than 100 coherent errors are marked "
          "'starved'\n");
  printf ("and are NOT evidence — the rate is below what the run can "
          "resolve.\n\n");
  printf ("   M   Es/N0     SER_coh    theory     err%%    SER_diff   "
          "penalty   n_err\n");
  printf ("  ---   -----   ---------   ---------   ------   ---------   "
          "-------   -----\n");
  for (size_t i = 0; i < sizeof (ms) / sizeof (ms[0]); i++)
    for (size_t j = 0; j < sizeof (es) / sizeof (es[0]); j++)
      {
        double want = ser_theory (ms[i], es[j]);
        /* Scale the run so each cell collects a comparable error count
           rather than a comparable number of symbols. */
        double nd  = 4000.0 / (want > 1e-9 ? want : 1e-9);
        size_t ns  = (size_t)(nd > 4e7 ? 4e7 : (nd < 2e5 ? 2e5 : nd));
        double coh = 0.0, dif = 0.0;
        size_t nerr = 0;
        if (measure (ms[i], es[j], ns, 4242u + (uint64_t)(i * 8 + j), &coh,
                     &dif, &nerr)
            != 0)
          return 1;
        printf ("  %3d   %5.1f   %9.3e   %9.3e   %+5.1f%%   %9.3e   %6.3fx   "
                "%5zu%s\n",
                ms[i], es[j], coh, want, 100.0 * (coh - want) / want, dif,
                (coh > 0.0) ? dif / coh : 0.0, nerr,
                (nerr < 100u) ? "  starved" : "");
      }
  return 0;
}
