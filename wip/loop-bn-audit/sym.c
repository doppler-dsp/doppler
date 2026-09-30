/* symsync realised-Bn audit: ensemble-mean strobe-timing step response. */
#include "doppler/symsync/symsync_core.h"
#include "doppler/wfm/wfm_dsp.h"
#include "dp_rng_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define SPS 8
#define SPAN 10
static double acc[200000];
int
main (int argc, char **argv)
{
  double bn = argc > 1 ? atof (argv[1]) : 0.005, zeta = 0.707, beta = 0.35;
  int    ted         = argc > 2 ? atoi (argv[2]) : SYMSYNC_TED_GARDNER;
  const double delta = 0.03; /* symbols */
  size_t       pre = (size_t)(20.0 / bn), post = (size_t)(40.0 / bn);
  size_t       nsym = pre + post + 2 * SPAN, nseed = 64;
  double      *a = malloc (nsym * sizeof *a);
  /* frozen-loop Kd: mean TED output at +-eps, random data */
  double kd = dp_symsync_ted_slope (ted, SYMSYNC_PULSE_RRC, beta, SPAN);
  for (size_t sd = 0; sd < nseed; sd++)
    {
      uint32_t st = 1234u + 77u * (uint32_t)sd;
      for (size_t j = 0; j < nsym; j++)
        a[j] = (dp_xs32 (&st) & 1u) ? 1.0 : -1.0;
      dp_symsync_state_t *s = dp_symsync_create (SPS, bn, zeta, 3, ted);
      size_t              k = 0, kpost = 0;
      double              base = 0.0;
      size_t              nb   = 0;
      for (size_t n = 0; n < (nsym - SPAN) * SPS; n++)
        {
          /* sample n: sum over symbols j with timing D_j */
          double t = (double)n / SPS, v = 0.0;
          long   jc = (long)t;
          for (long j = jc - SPAN; j <= jc + SPAN; j++)
            {
              if (j < 0 || (size_t)j >= nsym)
                continue;
              double D = (size_t)j >= pre + SPAN ? delta : 0.0;
              v += a[j] * wfm_rc_h (t - (double)j - D, beta);
            }
          float complex y;
          uint32_t      inc  = s->timing.phase_inc;
          int           emit = symsync_step_ted (s, (float complex)v, &y, ted);
          if (!emit)
            continue;
          double mu   = 1.0 - (double)s->timing.phase / (double)inc;
          double that = ((double)n - 2.0 + mu) / SPS; /* symbols */
          size_t jj
              = (size_t)llround (that - (that >= pre + SPAN ? delta : 0.0));
          double D = jj >= pre + SPAN ? delta : 0.0;
          double e = that - ((double)jj + D);
          k++;
          if (jj < pre + SPAN - 1 && jj > pre / 2)
            {
              base += e;
              nb++;
            }
          if (jj >= pre + SPAN && kpost < post)
            acc[kpost++] += e;
        }
      acc[199999] += base / nb;
      dp_symsync_destroy (s);
    }
  double base = acc[199999] / nseed;
  /* err = truth - estimate convention: e here is estimate - truth; a step
     of +delta in the truth makes e start at -delta. */
  double sh2 = 0, skE = 0, prev = 0, d = delta;
  size_t nn  = post * 3 / 10;
  double ess = 0;
  for (size_t i = post / 2; i < post; i++)
    ess += -(acc[i] / nseed - base);
  ess /= (double)(post - post / 2);
  d -= ess;
  for (size_t i = 0; i < nn; i++)
    {
      double err = -(acc[i] / nseed - base) - ess;
      double sv = 1.0 - err / d, h = sv - prev;
      prev = sv;
      sh2 += h * h;
      skE += (double)i * err;
    }
  if (getenv ("DBG"))
    for (size_t i = 0; i < 60; i += 1)
      printf ("  %zu %.5f\n", i, -(acc[i] / nseed - base));
  /* robust moments: sum e^2 = d^2/(4 zeta wn) (minus the tail noise
     floor), sum k e = -d/wn^2 */
  double var = 0, se2 = 0;
  for (size_t i = post / 2; i < post; i++)
    {
      double err = -(acc[i] / nseed - base) - ess;
      var += err * err;
    }
  var /= (double)(post - post / 2);
  for (size_t i = 0; i < nn; i++)
    {
      double err = -(acc[i] / nseed - base) - ess;
      se2 += err * err - var;
    }
  double w = sqrt (-d / skE);
  double z = d * d / (4.0 * w * se2);
  double B = w * (z + 1.0 / (4.0 * z)) / 2.0;
  printf ("symsync ted=%d Kd(analytic S-curve)=%.4f bn_cfg=%.4f Bn=%.5f "
          "(x%.3f) zeta=%.3f base=%.4f\n",
          ted, kd, bn, B, B / bn, z, base);
  return 0;
}
