/**
 * @file costas_jitter.c
 * @brief Monte-Carlo validation: the Costas loop's closed-loop phase jitter
 *        obeys the loop-noise-bandwidth relationship and shows a tracking
 *        threshold.
 *
 * The C harness reads the loop's actual tracking state — the integer NCO phase
 * `dp_costas_state_t.nco.phase` — so it measures the true closed-loop
 * phase-error variance sigma_phi^2, not just the discriminator output. For a
 * 2nd-order PLL driven by white phase-measurement noise of variance
 * sigma_disc^2, theory says sigma_phi^2 = G * sigma_disc^2 where G is the loop
 * NOISE GAIN — the sum of squares of the loop's (noise -> phase) impulse
 * response, computed here analytically from the loop filter gains kp, ki. G is
 * proportional to the loop bandwidth bn, so the jitter scales with bn (the
 * defining property of the loop noise bandwidth). At low SNR the loop loses
 * lock and sigma_phi^2 explodes — the PLL tracking threshold.
 *
 * Validated: sigma_phi^2 tracks G*sigma_disc^2 to a stable linearization
 * factor
 * (~1.2) across SNR and bn; sigma_phi^2 is proportional to bn; lock is lost at
 * low SNR. The decision-directed discriminator is |P|-normalised, so SNR here
 * is per-quadrature (signal 1, per-component noise variance 1/SNR).
 *
 * Usage:  costas_jitter [--check]
 */
#include "dp_test.h"

#include "doppler/awgn/awgn_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/dp_complex.h"
#include "doppler/state_filter/state_filter_core.h"
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
 * phase, then the filter steps (order 3: the rate first, then the frequency
 * by the rate). The loop is the canonical one dp_state_filter_loop_bandwidth()
 * models; what differs is where phi is read. phase_var() reads nco.phase
 * after each update, past the kp*e step, so G is summed at that same point.
 * Read at the detector instant (before the step) the sum is 2 Bn of that
 * function exactly; read here it is 1% under at bn 0.005 and 7% under at
 * 0.04 (docs/design/costas.md section 1). */
static double
noise_gain_at (double kp, double ki, double kr, int at_detector)
{
  double phi = 0, freq = 0, rate = 0, g = 0;
  for (int k = 0; k < 400000; k++)
    {
      phi += freq;
      double n = (k == 0) ? 1.0 : 0.0;
      double e = -phi + n;
      if (at_detector) /* the phase the error is formed from */
        g += phi * phi;
      phi += kp * e;
      rate += kr * e;
      freq += rate + ki * e;
      if (!at_detector) /* the phase phase_var() reads */
        g += phi * phi;
    }
  return g;
}

static double
noise_gain (double kp, double ki, double kr)
{
  return noise_gain_at (kp, ki, kr, 0);
}

/* The order the closed-loop runs take (docs/design/costas.md section 3). */
static int g_order = 2;

static void
init_loop (dp_costas_state_t *s, double bn)
{
  dp_costas_init (s, bn, 0.707, 0.0, 1, 0.0);
  if (g_order == 3)
    (void)dp_costas_set_order (s, 3, bn);
}

/* open-loop discriminator variance at phi=0 (frozen loop). */
static double
disc_var (double snr, dp_awgn_state_t *g, float *nb, long n)
{
  dp_awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  dp_awgn_reset (g);
  dp_costas_state_t s;
  dp_costas_init (&s, 1e-9, 0.707, 0.0, 1, 0.0);
  long   have = 0, pos = 0;
  double m = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          dp_awgn_generate (g, NB, (float complex *)nb, NB);
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
phase_var (double snr, double bn, dp_awgn_state_t *g, float *nb, long n)
{
  dp_awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  dp_awgn_reset (g);
  dp_costas_state_t s;
  init_loop (&s, bn);
  long   have = 0, pos = 0, warm = n / 4;
  double m = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          dp_awgn_generate (g, NB, (float complex *)nb, NB);
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
  int              check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  dp_awgn_state_t *g     = dp_awgn_create (7, 1.0f);
  float           *nb    = malloc ((size_t)2 * NB * sizeof (float));
  long             N     = check ? 1500000 : 4000000;

  /* The noise gain against the library's exact bandwidth
     (dp_state_filter_loop_bandwidth(), docs/design/costas.md section 1).
     Read at the detector instant, the phase the error is formed from, the
     impulse response's sum of squares IS 2 Bn: asserted to 1e-9, the solve's
     stated accuracy (1e-10) with the 400,000-term sum's truncation far
     below it. Read after the update, past the kp e step, where phase_var()
     reads it, the sum is smaller, and by how much is printed. */
  printf ("Noise gain against 2 Bn (dp_state_filter_loop_bandwidth)\n");
  for (int o = 2; o <= 3; o++)
    for (double bn = 0.005; bn <= 0.041; bn *= 2)
      {
        dp_costas_state_t s;
        g_order = o;
        init_loop (&s, bn);
        const double kp = o == 3 ? s.sf.kp : s.lf.kp,
                     ki = o == 3 ? s.sf.ki : s.lf.ki,
                     kr = o == 3 ? s.sf.kr : 0.0;
        const double bw2
            = 2.0 * dp_state_filter_loop_bandwidth (o, kp, ki, kr, 0.0, 1.0);
        const double det = noise_gain_at (kp, ki, kr, 1) / bw2;
        const double aft = noise_gain_at (kp, ki, kr, 0) / bw2;
        printf ("  order %d bn %.3f: at the detector %.12f, after the step "
                "%.4f of 2 Bn\n",
                o, bn, det, aft);
        DP_CHECK_MSG (fabs (det - 1.0) < 1e-9,
                      "the noise gain at the detector instant is 2 Bn");
        DP_CHECK_MSG (aft < 1.0, "read after the step, the sum is smaller");
      }
  double r_all[2] = { 0.0, 0.0 };
  for (g_order = 2; g_order <= 3; g_order++)
    {
      dp_costas_state_t s;
      init_loop (&s, 0.02);
      double gain = g_order == 3 ? noise_gain (s.sf.kp, s.sf.ki, s.sf.kr)
                                 : noise_gain (s.lf.kp, s.lf.ki, 0.0);
      printf ("[order %d] Costas phase jitter  (tsamps=1, bn=0.02, noise gain "
              "G=%.4f)\n",
              g_order, gain);
      printf ("  SNR(dB)  sig_disc^2   sig_phi^2    G*disc      ratio\n");
      double r_hi = 0;
      int    nhi  = 0;
      for (double sdb = 20; sdb >= 8; sdb -= 4)
        {
          double snr = pow (10, sdb / 10);
          double dv  = disc_var (snr, g, nb, check ? 400000 : 1000000);
          double pv  = phase_var (snr, 0.02, g, nb, N);
          double r   = pv / (gain * dv);
          printf ("   %4.1f   %.3e   %.3e   %.3e   %.3f\n", sdb, dv, pv,
                  gain * dv, r);
          if (sdb >= 12)
            {
              r_hi += r;
              nhi++;
            }
        }
      r_hi /= nhi;

      /* sigma_phi^2 proportional to bn (the loop noise bandwidth) at SNR=16 dB
       */
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
      printf ("  threshold: sig_phi^2(0 dB)=%.2e  >> (20 dB)=%.2e  (x%.0f)\n",
              thr, hi, thr / hi);
      const double dv0 = disc_var (1.0, g, nb, check ? 400000 : 1000000);
      const double r0  = thr / (gain * dv0);
      printf ("  at 0 dB the jitter is %.0fx the linear prediction G "
              "sig_disc^2\n",
              r0);

      /* The same assertions in the sweep and in --check (#1566), at each
         order.
         - The jitter is G sig_disc^2, G this order's own noise gain, to
           within 10% at 12 dB and above (measured 1.06 at order 2, 1.07 at
           order 3): the precision a caller sizing jitter from G may rely
           on. The estimator's own error over this run is about 0.5%
           (roughly 1e6 samples over a correlation of 1/(4 Bn)), so 10% is
           the claim's precision, not the noise's.
         - Lost lock at 0 dB: the jitter is more than 10x the linear
           prediction. A linear loop reads near 1 at every SNR (1.2 at
           8 dB), so this cannot be met by a loop that kept tracking, where
           a ratio of the 0 and 20 dB jitter could (x100 is linear). */
      DP_CHECK_MSG (fabs (r_hi - 1.0) < 0.10,
                    "the jitter is G sig_disc^2 within 10% at 12 dB and up");
      DP_CHECK_MSG (mono, "the jitter doubles when bn doubles");
      DP_CHECK_MSG (r0 > 10.0,
                    "at 0 dB the loop has lost lock: >10x linear jitter");
      r_all[g_order - 2] = r_hi;
    }
  free (nb);
  dp_awgn_destroy (g);
  printf ("factor %.3f at order 2, %.3f at order 3\n", r_all[0], r_all[1]);
  DP_TEST_END ("validate_costas_jitter");
}
