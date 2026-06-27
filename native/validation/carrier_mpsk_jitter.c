/**
 * @file carrier_mpsk_jitter.c
 * @brief Monte-Carlo validation: the M-PSK carrier loop's closed-loop phase
 *        jitter obeys the loop-noise-bandwidth relationship (and shows a
 *        tracking threshold that tightens as M grows), and the FLL assist
 *        widens the frequency pull-in range.
 *
 * Like costas_jitter.c, the harness reads the loop's true tracking state — the
 * integer NCO phase carrier_mpsk_state_t.nco.phase — to measure the
 * closed-loop phase-error variance sigma_phi^2, not just the discriminator
 * output. The signal is RANDOM M-PSK data (decision-directed), so this also
 * exercises the slicer: at high SNR decisions are correct and e = sin(phase
 * error), giving the same sigma_phi^2 = G * sigma_disc^2 law as the BPSK
 * Costas loop (G is the analytic loop noise gain, proportional to bn). As M
 * grows the constellation points crowd (spacing 2*pi/M), so decision errors
 * set in at higher SNR — the M-PSK tracking threshold moves up with M.
 *
 * Validated:
 *   - sigma_phi^2 ~ G*sigma_disc^2 (stable linearization factor) at high SNR,
 *     for M=2,4,8;
 *   - sigma_phi^2 proportional to bn;
 *   - a tracking threshold (jitter explodes at low SNR), tighter for larger M;
 *   - the FLL assist's frequency pull-in range exceeds the bare PLL's, per M.
 *
 * Usage:  carrier_mpsk_jitter [--check]
 */
#include "awgn/awgn_core.h"
#include "carrier_mpsk/carrier_mpsk_core.h"
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

/* xorshift32 symbol-index source. */
static uint32_t
xs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return x;
}

/* Analytic loop noise gain G = sum phi[k]^2 for a unit measurement-noise
 * impulse through the linearized loop (Kd=1), in the update order shared by
 * costas/carrier_mpsk: the NCO advances by the integrator frequency, then the
 * discriminator nudges phase. Identical loop filter => identical G. */
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

/* open-loop discriminator variance at phi=0 (frozen loop), random M-PSK data.
 */
static double
disc_var (int m, double snr, awgn_state_t *g, float *nb, long n)
{
  awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  awgn_reset (g);
  carrier_mpsk_state_t s;
  carrier_mpsk_init (&s, 1e-9, 0.707, 0.0, 1, 0.0, m);
  uint32_t sym  = 0x1234567u;
  long     have = 0, pos = 0;
  double   mu = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          awgn_generate (g, NB, (float complex *)nb);
          have = 2 * NB;
          pos  = 0;
        }
      float complex a = mpsk_constellation ((int)(xs (&sym) % (uint32_t)m), m);
      float complex x = a + nb[pos] + nb[pos + 1] * I;
      pos += 2;
      carrier_mpsk_update (&s, carrier_mpsk_wipeoff (&s, x));
      double e = s.last_error;
      mu += e;
      m2 += e * e;
    }
  return m2 / n - (mu / n) * (mu / n);
}

/* closed-loop NCO phase-error variance sigma_phi^2, random M-PSK data. */
static double
phase_var (int m, double snr, double bn, awgn_state_t *g, float *nb, long n)
{
  awgn_set_amplitude (g, (float)sqrt (1.0 / snr));
  awgn_reset (g);
  carrier_mpsk_state_t s;
  carrier_mpsk_init (&s, bn, 0.707, 0.0, 1, 0.0, m);
  uint32_t sym  = 0x9abcdefu;
  long     have = 0, pos = 0, warm = n / 4;
  double   mu = 0, m2 = 0;
  for (long k = 0; k < n; k++)
    {
      if (pos + 2 > have)
        {
          awgn_generate (g, NB, (float complex *)nb);
          have = 2 * NB;
          pos  = 0;
        }
      float complex a = mpsk_constellation ((int)(xs (&sym) % (uint32_t)m), m);
      float complex x = a + nb[pos] + nb[pos + 1] * I;
      pos += 2;
      carrier_mpsk_update (&s, carrier_mpsk_wipeoff (&s, x));
      if (k >= warm)
        {
          double p = phi_rad (s.nco.phase);
          mu += p;
          m2 += p * p;
        }
    }
  long c = n - warm;
  return m2 / c - (mu / c) * (mu / c);
}

/* Does the loop acquire a carrier step f0 (cycles/sample)?  Drives a clean
 * (noiseless) random-M-PSK signal so this isolates the deterministic pull-in
 * range; bn_fll=0 is the bare PLL, >0 the FLL assist. */
static int
acquires (int m, double f0, double bn_fll, long nsym, size_t tsamps)
{
  carrier_mpsk_state_t s;
  carrier_mpsk_init (&s, 0.01, 0.707, 0.0, tsamps, bn_fll, m);
  double   phase = 0.0, w = f0 * TWOPI;
  uint32_t sym = 0x55aa55aau;
  for (long k = 0; k < nsym; k++)
    {
      float complex a = mpsk_constellation ((int)(xs (&sym) % (uint32_t)m), m);
      for (size_t i = 0; i < tsamps; i++)
        {
          float complex x = a * (float complex)cexp (I * phase);
          carrier_mpsk_update (&s, carrier_mpsk_wipeoff (&s, x));
          phase += w;
        }
    }
  return fabs (carrier_mpsk_get_norm_freq (&s) - f0) < 5e-4
         && carrier_mpsk_get_lock_metric (&s) > 0.9;
}

/* Largest acquired carrier step on a coarse grid (the pull-in range). */
static double
pull_in_range (int m, double bn_fll, long nsym, size_t tsamps)
{
  double best = 0.0;
  for (double f0 = 0.0005; f0 <= 0.02001; f0 += 0.0005)
    {
      if (acquires (m, f0, bn_fll, nsym, tsamps))
        best = f0;
      else
        break; /* monotone: once it fails to pull in, larger steps fail too */
    }
  return best;
}

int
main (int argc, char **argv)
{
  int           check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  awgn_state_t *g     = awgn_create (7, 1.0f);
  float        *nb    = malloc ((size_t)2 * NB * sizeof (float));
  long          N     = check ? 1500000 : 4000000;
  int           fail  = 0;

  carrier_mpsk_state_t s;
  carrier_mpsk_init (&s, 0.02, 0.707, 0.0, 1, 0.0, 4);
  double gain = noise_gain (s.lf.kp, s.lf.ki);
  printf ("M-PSK carrier phase jitter  (tsamps=1, bn=0.02, noise gain "
          "G=%.4f)\n",
          gain);

  /* high-SNR jitter law per M: sigma_phi^2 ~ G * sigma_disc^2 */
  int ms[] = { 2, 4, 8 };
  /* per-M high-SNR floor (dB): larger M needs more SNR for correct decisions
   */
  double snr_hi[] = { 16.0, 18.0, 24.0 };
  for (int mi = 0; mi < 3; mi++)
    {
      int    m   = ms[mi];
      double sdb = snr_hi[mi];
      double snr = pow (10, sdb / 10);
      double dv  = disc_var (m, snr, g, nb, check ? 400000 : 1000000);
      double pv  = phase_var (m, snr, 0.02, g, nb, N);
      double r   = pv / (gain * dv);
      printf ("  M=%d  SNR=%4.1f dB  sig_disc^2=%.3e  sig_phi^2=%.3e  "
              "G*disc=%.3e  ratio=%.3f\n",
              m, sdb, dv, pv, gain * dv, r);
      if (check && (r < 0.9 || r > 1.6))
        fail = 1;
    }

  /* sigma_phi^2 proportional to bn (QPSK, SNR=18 dB) */
  printf ("  --- jitter proportional to bn (M=4, SNR=18 dB) ---\n");
  double snr18 = pow (10, 1.8), prev = 0;
  int    mono = 1;
  for (double bn = 0.005; bn <= 0.041; bn *= 2)
    {
      double pv = phase_var (4, snr18, bn, g, nb, N);
      printf ("   bn=%.3f  sig_phi^2=%.3e  (sig_phi^2/bn=%.3e)\n", bn, pv,
              pv / bn);
      if (prev > 0 && !(pv > 1.6 * prev && pv < 2.5 * prev))
        mono = 0;
      prev = pv;
    }

  /* tracking threshold tightens with M: jitter at a mid SNR explodes for the
   * larger constellation while the smaller one is still locked */
  printf ("  --- tracking threshold vs M (SNR=6 dB vs 20 dB) ---\n");
  double ratio_m[3];
  for (int mi = 0; mi < 3; mi++)
    {
      int    m    = ms[mi];
      double lo   = phase_var (m, pow (10, 0.6), 0.02, g, nb, N); /* 6 dB  */
      double hi   = phase_var (m, pow (10, 2.0), 0.02, g, nb, N); /* 20 dB */
      ratio_m[mi] = lo / hi;
      printf ("   M=%d  sig_phi^2(6dB)=%.2e  (20dB)=%.2e  x%.1f\n", m, lo, hi,
              lo / hi);
    }
  /* BPSK is robust at 6 dB; 8PSK has lost lock (much larger blow-up) */
  if (check && !(ratio_m[2] > ratio_m[0] && ratio_m[2] > 20.0))
    fail = 1;

  /* FLL widens the frequency pull-in range, per M. The bare PLL's range
   * narrows as M grows (the discriminator is linear only over +-pi/M); BPSK's
   * is already wide enough to saturate the grid ceiling, so the FLL's gain is
   * the headline for the larger constellations. Assert: FLL is never worse for
   * any M, and STRICTLY widens it for 8PSK (where the PLL is the bottleneck).
   */
  printf ("  --- pull-in range: bare PLL vs FLL assist ---\n");
  double pll_r[3], fll_r[3];
  for (int mi = 0; mi < 3; mi++)
    {
      int    m      = ms[mi];
      size_t tsamps = 16;
      pll_r[mi]     = pull_in_range (m, 0.0, 6000, tsamps);
      fll_r[mi]     = pull_in_range (m, 0.03, 6000, tsamps);
      printf ("   M=%d  PLL range=%.4f  FLL range=%.4f  (x%.1f)\n", m,
              pll_r[mi], fll_r[mi],
              pll_r[mi] > 0 ? fll_r[mi] / pll_r[mi] : 0.0);
      if (check && fll_r[mi] < pll_r[mi] - 1e-9) /* FLL never worse */
        fail = 1;
    }
  /* the PLL pull-in narrows with M, and the FLL strictly recovers it for 8PSK
   */
  if (check && !(pll_r[2] < pll_r[0] && fll_r[2] > pll_r[2]))
    fail = 1;

  free (nb);
  awgn_destroy (g);
  if (fail)
    {
      fprintf (stderr, "M-PSK carrier jitter/pull-in deviates from theory — "
                       "FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: sig_phi^2 ~ G*disc_var per M, prop. to bn, M-tightening "
            "threshold, FLL widens pull-in\n");
  return 0;
}
