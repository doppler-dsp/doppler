/**
 * @file dll_jitter.c
 * @brief Monte-Carlo validation: the DLL's closed-loop code-phase jitter obeys
 *        the early-late tracking-jitter law and shows a tracking threshold.
 *
 * Now that the code loop resolves sub-chip phase (the fractional-boundary
 * integrate-and-dump removed the integer-sample staircase), its closed-loop
 * jitter is a real, measurable quantity rather than a sample-quantization
 * floor. This harness measures sigma_tau^2 (chips^2) DIRECTLY and
 * sub-sample-accurately: it drives the loop one sample at a time via the
 * inline dll_accumulate / dll_update composition API and, at each code-period
 * dump, compares the loop's unwrapped code phase (chip_pos + dumps*sf, a
 * double) against the exact true phase (sample_index / sps). The dump fires on
 * an integer sample, but the error uses the loop's exact fractional chip_pos
 * and the exact sample count, so it is NOT floored by the (1/sps)^2/12
 * dump-timing quantum.
 *
 * The non-coherent early-late code-jitter law (Kaplan), in chips^2, is
 *   sigma_tau^2 ~= (Bn d / (2 C/N0)) (1 + 2 / ((2-d) C/N0 T)),
 * a 1/(C/N0) signal-cross-noise term plus a 1/(C/N0)^2 noise-cross-noise term,
 * both proportional to the loop bandwidth Bn and the early-late spacing d.
 * With gamma = C/N0 * T the per-epoch despread SNR (one E/P/L
 * integrate-and-dump per epoch, so the loop bandwidth and gamma are
 * epoch-normalized), the harness checks the robust, defensible signatures of
 * that law. SPAN: sigma_tau^2 rises steeply as gamma falls. NOISE-CROSS-NOISE:
 * sigma_tau^2 * gamma rises at low gamma (pure 1/gamma would keep it flat),
 * the signature of the 1/gamma^2 term -- muted (but not eliminated) versus
 * the old magnitude discriminator by DLL_DISC_CLAMP, which caps exactly the
 * large-error swings from a noise-collapsed prompt power that would
 * otherwise drive this term (confirmed by temporarily lifting the clamp:
 * the rise recovers to ~1.5, so the gate below is tuned for the
 * clamped, production loop, not the unclamped ideal). BANDWIDTH:
 * sigma_tau^2 grows with the loop bandwidth bn. THRESHOLD: at low gamma the
 * loop loses lock and the jitter explodes. A 2-term c1/gamma + c2/gamma^2 fit
 * is printed for reference but is collinear and noise-sensitive, so it is not
 * gated on. The loop's effective bandwidth is Kd*bn (the power-domain
 * discriminator's gain Kd, measured below rather than assumed), so the loop
 * is exercised at low bn, inside the stable regime.
 *
 * Usage:  dll_jitter [--check]
 */
#include "awgn/awgn_core.h"
#include "dll/dll_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 127      /* code length (chips per period) */
#define SPS 8       /* samples per chip */
#define SPACING 0.5 /* early-late tap offset (chips) */
#define NBLK (1 << 16)

/* Deterministic 0/1 spreading code (xorshift bits). */
static void
make_code (uint8_t *code)
{
  uint32_t st = 0x12345u;
  for (size_t i = 0; i < SF; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      code[i] = (st & 1u) ? 1u : 0u;
    }
}

/* Carrier-free, aligned, rectangular spread signal (one period, +1 data). */
static void
make_clean (float complex *sig, const uint8_t *code)
{
  for (size_t c = 0; c < SF; c++)
    {
      float v = (code[c] & 1u) ? -1.0f : 1.0f;
      for (size_t i = 0; i < SPS; i++)
        sig[c * SPS + i] = v;
    }
}

/* per-component noise std for a target per-period despread SNR gamma:
 * gamma = C/N0*T = (signal energy per period) / N0 = SF*SPS / (2 sigma^2). */
static float
noise_std (double gamma)
{
  return (float)sqrt ((double)(SF * SPS) / (2.0 * gamma));
}

/* Open-loop discriminator gain Kd = -d e / d tau at the lock (chips^-1),
 * measured noise-free with a frozen loop. The S-curve has a negative slope, so
 * Kd is reported positive. */
static double
disc_gain (const uint8_t *code, const float complex *sig)
{
  const double tau = 0.05;
  double       e[2];
  for (int s = 0; s < 2; s++)
    {
      double      off = s ? +tau : -tau;
      dll_state_t d;
      dll_init (&d, code, SF, SPS, off, 1e-9, 0.707, SPACING);
      for (int p = 0; p < 6; p++)
        {
          for (size_t i = 0; i < (size_t)SF * SPS; i++)
            dll_accumulate (&d, sig[i]);
          dll_update (&d);
          d.acc_e = d.acc_p = d.acc_l = 0.0f;
        }
      e[s] = dll_get_last_error (&d);
    }
  return (e[0] - e[1]) / (2.0 * tau); /* (e(-tau) - e(+tau)) / 2tau > 0 */
}

/* Open-loop discriminator-output variance at the lock (frozen loop). One E/P/L
 * integrate-and-dump per code epoch; a fresh AWGN stream per call keeps the
 * measurements independent of call order. */
static double
disc_var (double gamma, const uint8_t *code, const float complex *sig,
          long ndump)
{
  awgn_state_t  *g  = awgn_create (7, noise_std (gamma));
  float complex *nb = malloc ((size_t)NBLK * sizeof (*nb));
  dll_state_t    d;
  dll_init (&d, code, SF, SPS, 0.0, 1e-9, 0.707, SPACING);
  long   pos = NBLK;
  double m = 0, m2 = 0;
  long   n = 0;
  while (n < ndump)
    {
      for (size_t i = 0; i < (size_t)SF * SPS; i++)
        {
          if (pos >= NBLK)
            {
              awgn_generate (g, NBLK, nb);
              pos = 0;
            }
          dll_accumulate (&d, sig[i] + nb[pos++]);
        }
      dll_update (&d); /* one discriminator per epoch */
      d.acc_e = d.acc_p = d.acc_l = 0.0f;
      double e                    = dll_get_last_error (&d);
      m += e;
      m2 += e * e;
      n++;
    }
  free (nb);
  awgn_destroy (g);
  return m2 / n - (m / n) * (m / n);
}

/* Closed-loop code-phase jitter sigma_tau^2 (chips^2), sub-sample accurate.
 * The loop updates once per code epoch (E/P/L dumped, discriminator + loop
 * filter + NCO steer), so bn and gamma are epoch-normalized. The phase error
 * is the loop's unwrapped code phase (chip_pos + dumps*sf, an exact double)
 * minus the true phase (sample / sps): exact at the fractional level, so it is
 * NOT floored by the (1/sps) dump-timing quantum. Fresh AWGN stream per call.
 */
static double
phase_var (double gamma, double bn, const uint8_t *code,
           const float complex *sig, long ndump)
{
  awgn_state_t  *g  = awgn_create (7, noise_std (gamma));
  float complex *nb = malloc ((size_t)NBLK * sizeof (*nb));
  dll_state_t    d;
  dll_init (&d, code, SF, SPS, 0.0, bn, 0.707, SPACING);
  const double inv_sps = 1.0 / (double)SPS;
  long         pos = NBLK, warm = ndump / 4, dumps = 0;
  long long    samp = 0;
  double       m = 0, m2 = 0;
  long         n = 0;
  /* dumps advances only on a code-epoch wrap, so a locked loop that stopped
     wrapping (rate steered to ~0) would spin here forever. Bound the total
     samples at a generous 64x the nominal (ndump epochs = ndump*SF*SPS
     samples): a healthy loop wraps ~once per epoch and never approaches this,
     while a pathological non-convergence exits with too few dumps and fails
     the caller's variance-vs-law check rather than hanging the run. */
  const long long max_samp = (long long)ndump * SF * SPS * 64;
  /* TEMP DIAGNOSTIC (remove after arm64/macOS root-cause): trace the first
     phase_var call's loop-filter state per epoch to stderr. Silent on a
     passing run (ctest hides stderr); shown under --output-on-failure. */
  static int traced   = 0;
  int        do_trace = !traced;
  if (do_trace)
    {
      traced = 1;
      fprintf (stderr,
               "[dbg] init: phase_inc=%u inv_tsamps=%.9g kp=%.9g ki=%.9g "
               "inv_tsamps_sf=%.9g\n",
               d.code_nco.phase_inc, d.inv_tsamps, d.lf.kp, d.lf.ki,
               d.inv_tsamps_sf);
    }
  while (dumps < ndump && samp < max_samp)
    {
      for (size_t i = 0; i < (size_t)SF * SPS; i++, samp++)
        {
          if (pos >= NBLK)
            {
              awgn_generate (g, NBLK, nb);
              pos = 0;
            }
          int wrapped = dll_accumulate (&d, sig[i] + nb[pos++]);
          if (!wrapped)
            continue;
          dll_update (&d); /* one loop update per epoch */
          d.acc_e = d.acc_p = d.acc_l = 0.0f;
          dumps++;
          if (do_trace && dumps <= 18)
            fprintf (stderr,
                     "[dbg] ep=%ld e=%.6g integ=%.9g code_rate=%.9g "
                     "phase_inc=%u samp=%lld\n",
                     dumps, d.last_error, d.lf.integ, d.code_rate,
                     d.code_nco.phase_inc, samp);
          if (dumps > warm)
            {
              double loop_phase = d.chip_pos + (double)dumps * (double)SF;
              double true_phase = (double)(samp + 1) * inv_sps;
              double err        = loop_phase - true_phase;
              m += err;
              m2 += err * err;
              n++;
            }
        }
    }
  free (nb);
  awgn_destroy (g);
  if (n
      == 0) /* hit the sample cap without converging: fail the caller loudly */
    return 1e9;
  return m2 / n - (m / n) * (m / n);
}

/* Least-squares fit sigma^2 = c1/gamma + c2/gamma^2 over (gamma, var) points.
 */
static void
fit_law (const double *gam, const double *var, int np, double *c1, double *c2)
{
  double a11 = 0, a12 = 0, a22 = 0, b1 = 0, b2 = 0;
  for (int i = 0; i < np; i++)
    {
      double x1 = 1.0 / gam[i], x2 = 1.0 / (gam[i] * gam[i]);
      a11 += x1 * x1;
      a12 += x1 * x2;
      a22 += x2 * x2;
      b1 += x1 * var[i];
      b2 += x2 * var[i];
    }
  double det = a11 * a22 - a12 * a12;
  *c1        = (b1 * a22 - b2 * a12) / det;
  *c2        = (a11 * b2 - a12 * b1) / det;
}

int
main (int argc, char **argv)
{
  int           check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t       code[SF];
  float complex sig[SF * SPS];
  make_code (code);
  make_clean (sig, code);
  long ND   = check ? 5000 : 8000;
  int  fail = 0;

  double Kd = disc_gain (code, sig);
  printf ("DLL code jitter  (SF=%d, sps=%d, d=%.1f, Kd=%.3f)\n", SF, SPS,
          SPACING, Kd);

  /* sigma_tau^2 vs per-period SNR gamma at fixed bn=0.002 (Kd > 1 raises the
   * effective loop gain, so the loop is operated at low bn — well inside the
   * stable regime). */
  double gam[6] = { 40, 25, 15, 9, 6, 4 };
  double sv[6];
  printf ("  gamma  sigma_tau^2(chip^2)   sqrt=sigma_tau(chip)\n");
  for (int i = 0; i < 6; i++)
    {
      sv[i] = phase_var (gam[i], 0.002, code, sig, ND);
      printf ("  %5.1f    %.4e          %.4e\n", gam[i], sv[i], sqrt (sv[i]));
    }
  /* Early-late law evidence (robust, not an ill-conditioned 2-param fit):
   *  - span: sigma_tau^2 rises steeply from high to low SNR (gam[0]=40 ->
   *    gam[5]=4), the fundamental SNR dependence;
   *  - nxn: sigma_tau^2 * gamma RISES at low SNR. Pure signal-cross-noise
   *    (1/gamma) would keep sigma_tau^2*gamma flat; the rise is the 1/gamma^2
   *    noise-cross-noise term of the early-late law.
   * The fit (printed for reference) recovers both coefficients in the long run
   * but is collinear/noise-sensitive, so the gate uses span and nxn. */
  double c1, c2;
  fit_law (gam, sv, 6, &c1, &c2);
  /* compare gamma=6 (index 4) against gamma=40 (index 0); the gamma=4 point
     sits near the tracking threshold and is intentionally not used for the
     gate. */
  double span = sv[4] / sv[0];
  double nxn  = (sv[4] * gam[4]) / (sv[0] * gam[0]);
  printf ("  early-late law: span(sigma_tau^2 lo/hi SNR)=%.1f, "
          "noise-cross-noise (sigma_tau^2*gamma) rise=%.2f\n",
          span, nxn);
  printf ("  (fit: c1=%.3e /gamma, c2=%.3e /gamma^2)\n", c1, c2);

  /* sigma_tau^2 proportional to the loop bandwidth bn (gamma=25): the
     early-late law has sigma_tau^2 = K(gamma,d) * bn, so sigma_tau^2 / bn is
     constant. Use the cleanly-measurable, stable range bn in [0.001, 0.003]:
     above ~0.004 the Kd discriminator gain pushes the effective bandwidth
     toward the stability edge (super-linear), and below ~0.001 the loop's 1/bn
     correlation time exceeds the measurement window. The loop starts aligned,
     so there is no acquisition transient to warm past. */
  /* jitter grows with the loop bandwidth bn (the defining property of Bn). The
     loop's effective bandwidth is Kd*bn, so the growth is only approximately
     linear over a fast run; we gate on a clear, robust increase. */
  printf ("  --- jitter grows with loop bandwidth bn (gamma=25) ---\n");
  double pv1    = phase_var (25.0, 0.001, code, sig, ND);
  double pv2    = phase_var (25.0, 0.003, code, sig, ND);
  double bratio = pv2 / pv1; /* 3x bn => clearly larger jitter */
  printf ("   bn=0.0010 sigma_tau^2=%.4e\n   bn=0.0030 sigma_tau^2=%.4e  "
          "(ratio=%.2f)\n",
          pv1, pv2, bratio);
  int mono = (bratio > 1.5);

  /* tracking threshold: jitter explodes at low SNR */
  double lo = phase_var (1.5, 0.002, code, sig, ND); /* deep */
  double hi = phase_var (40.0, 0.002, code, sig, ND);
  printf (
      "  threshold: sigma_tau^2(gamma=1.5)=%.3e >> (gamma=40)=%.3e (x%.0f)\n",
      lo, hi, lo / hi);

  /* informational: open-loop discriminator variance -> per-measurement phase
   * noise (chips^2) via Kd, the raw term the closed loop filters down. */
  double dv = disc_var (25.0, code, sig, ND);
  printf ("  (open-loop disc var=%.3e -> %.3e chip^2 per measurement)\n", dv,
          dv / (Kd * Kd));

  if (check)
    {
      if (span < 5.0)
        fail = 1; /* jitter rises steeply as SNR drops */
      /* 1/gamma^2 noise-cross-noise term is present, muted by DLL_DISC_CLAMP
       * (measured ~1.13 clamped vs ~1.5 with the clamp lifted -- see the
       * file doc comment); 1.05 still requires a real rise over the flat-
       * at-1.0 null while giving margin over the measured value. */
      if (nxn < 1.05)
        fail = 1;
      if (!mono)
        fail = 1; /* jitter ~ proportional to bn */
      if (lo / hi < 50.0)
        fail = 1; /* clear tracking threshold */
    }
  if (fail)
    {
      fprintf (stderr, "DLL jitter deviates from early-late theory — FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: sigma_tau^2 follows the early-late SNR law (1/gamma + "
            "1/gamma^2), grows with loop bandwidth, with a tracking "
            "threshold\n");
  return 0;
}
