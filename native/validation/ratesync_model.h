/**
 * @file ratesync_model.h
 * @brief What the RateSync model validators share: the cascade ahead of the
 *        matched bank, measured from a built converter.
 *
 * `ratesync_pulse_model.c` and `ratesync_loop_model.c` both start from the
 * response `g[n]` of the integer stages ahead of the bank, at the input
 * rate, read through the converter's own pre-terminal tap. This header
 * holds that measurement and the small solve both need, once.
 *
 * Validation-only: it includes a public header's struct and nothing here is
 * part of the library.
 */
#ifndef RATESYNC_MODEL_H
#define RATESYNC_MODEL_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/dp_complex.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define LEN 8192  /* input samples a probe run feeds               */
#define I0 2048   /* where a probe's impulse or symbol starts      */
#define MAXT 96   /* most taps an arm may have here                */
#define GOFF 4096 /* g[] index of n = 0                            */
#define GLEN 12288

/* Solve A x = b for a symmetric positive definite A (n x n, row-major),
   by Gaussian elimination with partial pivoting. A and b are destroyed. */
static int
solve (double *a, double *b, size_t n)
{
  for (size_t c = 0; c < n; c++)
    {
      size_t piv = c;
      for (size_t r = c + 1; r < n; r++)
        if (fabs (a[r * n + c]) > fabs (a[piv * n + c]))
          piv = r;
      if (fabs (a[piv * n + c]) < 1e-14)
        return -1;
      if (piv != c)
        {
          for (size_t k = 0; k < n; k++)
            {
              double t       = a[c * n + k];
              a[c * n + k]   = a[piv * n + k];
              a[piv * n + k] = t;
            }
          double t = b[c];
          b[c]     = b[piv];
          b[piv]   = t;
        }
      for (size_t r = c + 1; r < n; r++)
        {
          double f = a[r * n + c] / a[c * n + c];
          for (size_t k = c; k < n; k++)
            a[r * n + k] -= f * a[c * n + k];
          b[r] -= f * b[c];
        }
    }
  for (size_t c = n; c-- > 0;)
    {
      for (size_t k = c + 1; k < n; k++)
        b[c] -= a[c * n + k] * b[k];
      b[c] /= a[c * n + c];
    }
  return 0;
}

/* The cascade's response at the input rate, g[GOFF + n], from an impulse
   at each of the D input positions, read at the pre-terminal tap. Returns
   D, or 0 on a failure. `first` receives the input index of the first
   pre-terminal sample, which fixes the decimation phase. */
static size_t
measure_g (dp_RateConverter_state_t *rc, double *g, size_t *first)
{
  size_t        d = 0, f0 = 0;
  static double base[LEN];
  memset (g, 0, GLEN * sizeof *g);
  /* The first pass feeds zeros alone. It gives the decimation and its
     phase, from where the first two bank samples fall, and the cascade's
     own output for no input, which a CIC plan does not start at zero
     (doppler#3365) and which every later pass has subtracted. */
  for (int pass = 0, k = 0; pass == 0 || (size_t)k < d; pass++)
    {
      dp_RateConverter_reset (rc);
      size_t j = 0;
      for (size_t i = 0; i < LEN; i++)
        {
          float _Complex out[8], pre = 0.0f;
          int np = 0;
          (void)dp_RateConverter_execute_ctrl_push_tap (
              rc, pass > 0 && i == I0 + (size_t)k ? 1.0f : 0.0f, 0.0, out, 8,
              &pre, &np);
          if (!np)
            continue;
          if (pass == 0)
            {
              if (j == 0)
                f0 = i;
              else if (j == 1)
                d = i - f0;
              base[j] = (double)crealf (pre);
            }
          else
            {
              /* bank sample j sits at input f0 + j*d; the impulse at
                 I0 + k */
              long n = (long)(f0 + j * d) - (long)(I0 + (size_t)k);
              if (n + GOFF >= 0 && n + GOFF < GLEN)
                g[n + GOFF] = (double)crealf (pre) - base[j];
            }
          j++;
        }
      if (!d)
        return 0;
      if (pass > 0)
        k++;
    }
  *first = f0;
  return d;
}

static double
gat (const double *g, long n)
{
  return (n + GOFF >= 0 && n + GOFF < GLEN) ? g[n + GOFF] : 0.0;
}

#endif /* RATESYNC_MODEL_H */
