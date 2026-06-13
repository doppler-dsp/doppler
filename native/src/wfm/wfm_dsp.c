/*
 * wfm_dsp.c — DSSS spreading + root-raised-cosine taps (Phase B).
 */
#include "wfm/wfm_dsp.h"

#include <complex.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void
wfm_rrc_taps (double beta, int sps, int span, float *taps)
{
  size_t n      = wfm_rrc_ntaps (sps, span);
  double center = (double)(span * sps);
  double sumsq  = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      /* t in symbol periods (T = 1) */
      double t = ((double)i - center) / (double)sps;
      double h;
      if (fabs (t) < 1e-9)
        {
          /* limit at t = 0 */
          h = 1.0 - beta + 4.0 * beta / M_PI;
        }
      else if (beta > 0.0 && fabs (fabs (t) - 1.0 / (4.0 * beta)) < 1e-9)
        {
          /* limit at t = ±1/(4β) (0/0 in the general form) */
          double a = M_PI / (4.0 * beta);
          h = (beta / sqrt (2.0))
              * ((1.0 + 2.0 / M_PI) * sin (a) + (1.0 - 2.0 / M_PI) * cos (a));
        }
      else
        {
          double pt  = M_PI * t;
          double num = sin (pt * (1.0 - beta))
                       + 4.0 * beta * t * cos (pt * (1.0 + beta));
          double den = pt * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));
          h          = num / den;
        }
      taps[i] = (float)h;
      sumsq += h * h;
    }
  /* normalise to unit energy */
  double norm = (sumsq > 0.0) ? 1.0 / sqrt (sumsq) : 1.0;
  for (size_t i = 0; i < n; i++)
    taps[i] = (float)(taps[i] * norm);
}

void
wfm_dsss_spread (const float _Complex *syms, size_t n_sym, const uint8_t *code,
                 size_t sf, float _Complex *out)
{
  for (size_t i = 0; i < n_sym; i++)
    {
      float _Complex s = syms[i];
      for (size_t j = 0; j < sf; j++)
        out[i * sf + j] = (code[j] & 1u) ? -s : s;
    }
}
