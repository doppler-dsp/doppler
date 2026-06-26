/**
 * @file costas_jitter.c
 * @brief Monte-Carlo validation: the Costas loop's closed-loop phase jitter
 *        obeys the loop-noise-bandwidth relationship and shows a tracking
 *        threshold.
 *
 * The C harness reads the loop's actual tracking state — the integer NCO phase
 * `costas_state_t.nco.phase` — so it measures the true closed-loop phase-error
 * variance sigma_phi^2, not just the discriminator output. For a 2nd-order PLL
 * driven by white phase-measurement noise of variance sigma_disc^2, theory
 * says sigma_phi^2 = G * sigma_disc^2 where G is the loop NOISE GAIN — the sum
 * of squares of the loop's (noise -> phase) impulse response, computed here
 * analytically from the loop filter gains kp, ki. G is proportional to the
 * loop bandwidth bn, so the jitter scales with bn (the defining property of
 * the loop noise bandwidth). At low SNR the loop loses lock and sigma_phi^2
 * explodes — the PLL tracking threshold.
 *
 * Validated: sigma_phi^2 tracks G*sigma_disc^2 to a stable linearization
 * factor
 * (~1.2) across SNR and bn; sigma_phi^2 is proportional to bn; lock is lost at
 * low SNR. The decision-directed discriminator is |P|-normalised, so SNR here
 * is per-quadrature (signal 1, per-component noise variance 1/SNR).
 *
 * Usage:  costas_jitter [--check]
 */
#include "awgn/awgn_core.h"
#include "costas/costas_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586
#define NB (1 << 20)

static double
phi_rad (uint32_t p)
{
  return (double)(int32_t)p / 4294967296.0 * TWOPI;
}

/* Analytic loop noise gain G = sum phi[k]^2 for a unit measurement-noise
 * impulse through the linearized loop (Kd=1), in costas's update order: the
 * NCO advances by the integrator frequency, then the discriminator nudges
 * phase. */
static double
noise_gain (double kp, double ki)
{
  double phi = 0, freq = 0, g = 0;
  for (int k = 0; k < 200000; k++)
    {
      phi += freq;
      double n = (k == 0) ? 1.0 : 0.0;
      double e = -phi + n;
      phi += kp * e;
      freq += ki * e;
      g += phi * phi;
    }
  return g;
}

/* open-loop discriminator variance at phi=0 (frozen loop). */
static double
disc_var (double snr, awgn_state_t *g, float *nb, long n)
{
  awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  awgn_reset (g);
  costas_state_t s;
  costas_init (&s, 1e-9, 0.707, 0.0, 1, 0.0);
  long   have = 0, pos = 0;
  double m = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          awgn_generate (g, NB, (float complex *)nb);
          have = 2 * NB;
          pos  = 0;
        }
      float complex x = 1.0f + nb[pos] + nb[pos + 1] * I;
      pos += 2;
      costas_update (&s, costas_wipeoff (&s, x));
      double e = s.last_error;
      m += e;
      m2 += e * e;
    }
  return m2 / n - (m / n) * (m / n);
}

/* closed-loop NCO phase-error variance sigma_phi^2. */
static double
phase_var (double snr, double bn, awgn_state_t *g, float *nb, long n)
{
  awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  awgn_reset (g);
  costas_state_t s;
  costas_init (&s, bn, 0.707, 0.0, 1, 0.0);
  long   have = 0, pos = 0, warm = n / 4;
  double m = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          awgn_generate (g, NB, (float complex *)nb);
          have = 2 * NB;
          pos  = 0;
        }
      float complex x = 1.0f + nb[pos] + nb[pos + 1] * I;
      pos += 2;
      costas_update (&s, costas_wipeoff (&s, x));
      if (k >= warm)
        {
          double p = phi_rad (s.nco.phase);
          m += p;
          m2 += p * p;
        }
    }
  long c = n - warm;
  return m2 / c - (m / c) * (m / c);
}

int
main (int argc, char **argv)
{
  int           check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  awgn_state_t *g     = awgn_create (7, 1.0f);
  float        *nb    = malloc ((size_t)2 * NB * sizeof (float));
  long          N     = check ? 1500000 : 4000000;
  int           fail  = 0;

  costas_state_t s;
  costas_init (&s, 0.02, 0.707, 0.0, 1, 0.0);
  double gain = noise_gain (s.lf.kp, s.lf.ki);
  printf ("Costas phase jitter  (tsamps=1, bn=0.02, noise gain G=%.4f)\n",
          gain);
  printf ("  SNR(dB)  sig_disc^2   sig_phi^2    G*disc      ratio\n");
  double r_hi = 0;
  int    nhi  = 0;
  for (double sdb = 20; sdb >= 8; sdb -= 4)
    {
      double snr = pow (10, sdb / 10);
      double dv  = disc_var (snr, g, nb, check ? 400000 : 1000000);
      double pv  = phase_var (snr, 0.02, g, nb, N);
      double r   = pv / (gain * dv);
      printf ("   %4.1f   %.3e   %.3e   %.3e   %.3f\n", sdb, dv, pv, gain * dv,
              r);
      if (sdb >= 12)
        {
          r_hi += r;
          nhi++;
        }
    }
  r_hi /= nhi;

  /* sigma_phi^2 proportional to bn (the loop noise bandwidth) at SNR=16 dB */
  printf ("  --- jitter proportional to bn (SNR=16 dB) ---\n");
  double snr16 = pow (10, 1.6), prev = 0;
  int    mono = 1;
  for (double bn = 0.005; bn <= 0.041; bn *= 2)
    {
      double pv = phase_var (snr16, bn, g, nb, N);
      printf ("   bn=%.3f  sig_phi^2=%.3e  (sig_phi^2/bn=%.3e)\n", bn, pv,
              pv / bn);
      if (prev > 0 && !(pv > 1.6 * prev && pv < 2.5 * prev))
        mono = 0; /* ~doubles when bn doubles */
      prev = pv;
    }

  /* tracking threshold: jitter explodes at low SNR */
  double thr = phase_var (pow (10, 0.0), 0.02, g, nb, N); /* 0 dB */
  double hi  = phase_var (pow (10, 2.0), 0.02, g, nb, N); /* 20 dB */
  printf ("  threshold: sig_phi^2(0 dB)=%.2e  >> (20 dB)=%.2e  (x%.0f)\n", thr,
          hi, thr / hi);

  if (check)
    {
      if (r_hi < 1.0 || r_hi > 1.45) /* analytic-gain match, stable factor */
        fail = 1;
      if (!mono) /* jitter ~ proportional to bn */
        fail = 1;
      if (thr / hi < 20.0) /* clear tracking threshold */
        fail = 1;
    }
  free (nb);
  awgn_destroy (g);
  if (fail)
    {
      fprintf (stderr, "Costas jitter deviates from loop theory — FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: phase jitter ~ G*sigma_disc^2 (factor %.2f), prop. to bn, "
            "with a tracking threshold\n",
            r_hi);
  return 0;
}
