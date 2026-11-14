/* Prototype: float Bluestein over pffft vs dp_fft's promote-to-double path.
 * The run behind correlator.md section 9.4 (#3047). */
#include "doppler/fft/fft_core.h"
#include "pffft.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef float _Complex cf;

static int
pf_ok (size_t n)
{
  if (n < 16 || n % 16)
    return 0;
  while (n % 2 == 0)
    n /= 2;
  while (n % 3 == 0)
    n /= 3;
  while (n % 5 == 0)
    n /= 5;
  return n == 1;
}

typedef struct
{
  size_t       n, m;
  PFFFT_Setup *s;
  cf          *w;  /* chirp w[k] = exp(-i pi k^2 / n), length n            */
  cf          *bh; /* FFT_m of the conj-chirp kernel, scaled by 1/m          */
  cf          *a, *b, *work;
} blue_t;

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}

static blue_t *
blue_create (size_t n)
{
  blue_t *p = calloc (1, sizeof *p);
  p->n      = n;
  size_t m  = 2 * n - 1;
  while (!pf_ok (m))
    m++;
  p->m    = m;
  p->s    = pffft_new_setup ((int)m, PFFFT_COMPLEX);
  p->w    = pffft_aligned_malloc (n * sizeof (cf));
  p->bh   = pffft_aligned_malloc (m * sizeof (cf));
  p->a    = pffft_aligned_malloc (m * sizeof (cf));
  p->b    = pffft_aligned_malloc (m * sizeof (cf));
  p->work = pffft_aligned_malloc (m * sizeof (cf));
  for (size_t k = 0; k < n; k++)
    {
      /* k^2 mod 2n keeps the angle exact in double */
      double ang = M_PI * (double)((k * k) % (2 * n)) / (double)n;
      p->w[k]    = (float)cos (ang) - I * (float)sin (ang);
    }
  memset (p->a, 0, m * sizeof (cf));
  p->a[0] = conjf (p->w[0]);
  for (size_t k = 1; k < n; k++)
    p->a[k] = p->a[m - k] = conjf (p->w[k]);
  pffft_transform_ordered (p->s, (float *)p->a, (float *)p->bh,
                           (float *)p->work, PFFFT_FORWARD);
  for (size_t k = 0; k < m; k++)
    p->bh[k] /= (float)m;
  return p;
}

static void
blue_fwd (blue_t *p, const cf *x, cf *y)
{
  size_t n = p->n, m = p->m;
  for (size_t k = 0; k < n; k++)
    p->a[k] = x[k] * p->w[k];
  memset (p->a + n, 0, (m - n) * sizeof (cf));
  pffft_transform_ordered (p->s, (float *)p->a, (float *)p->b,
                           (float *)p->work, PFFFT_FORWARD);
  for (size_t k = 0; k < m; k++)
    p->b[k] *= p->bh[k];
  pffft_transform_ordered (p->s, (float *)p->b, (float *)p->a,
                           (float *)p->work, PFFFT_BACKWARD);
  for (size_t k = 0; k < n; k++)
    y[k] = p->a[k] * p->w[k];
}

int
main (void)
{
  size_t ns[] = { 90, 160, 180, 188, 192, 720, 768, 2046, 2048 };
  for (size_t t = 0; t < sizeof ns / sizeof *ns; t++)
    {
      size_t           n  = ns[t];
      cf              *x  = pffft_aligned_malloc (n * sizeof (cf));
      cf              *y0 = pffft_aligned_malloc (n * sizeof (cf));
      cf              *y1 = pffft_aligned_malloc (n * sizeof (cf));
      double _Complex *xd = malloc (n * sizeof *xd),
                      *yd = malloc (n * sizeof *yd);
      srand (7);
      for (size_t k = 0; k < n; k++)
        {
          x[k]  = (rand () / (float)RAND_MAX - .5f)
                  + I * (rand () / (float)RAND_MAX - .5f);
          xd[k] = x[k];
        }
      dp_fft_state_t *f = dp_fft_create (n, -1, 1);
      blue_t         *b = blue_create (n);
      dp_fft_execute_cf64 (f, xd, n, yd, n);
      dp_fft_execute_cf32 (f, x, n, y0, n);
      blue_fwd (b, x, y1);
      double e0 = 0, e1 = 0, ref = 0;
      for (size_t k = 0; k < n; k++)
        {
          ref += creal (yd[k] * conj (yd[k]));
          e0 += cabs (y0[k] - yd[k]) * cabs (y0[k] - yd[k]);
          e1 += cabs (y1[k] - yd[k]) * cabs (y1[k] - yd[k]);
        }
      size_t reps  = 4000000 / n + 50;
      double best0 = 1e9, best1 = 1e9;
      for (int r = 0; r < 7; r++)
        {
          double t0 = now ();
          for (size_t i = 0; i < reps; i++)
            dp_fft_execute_cf32 (f, x, n, y0, n);
          double t1 = now ();
          for (size_t i = 0; i < reps; i++)
            blue_fwd (b, x, y1);
          double t2 = now ();
          if ((t1 - t0) / reps < best0)
            best0 = (t1 - t0) / reps;
          if ((t2 - t1) / reps < best1)
            best1 = (t2 - t1) / reps;
        }
      printf ("n=%5zu pffft=%d m=%5zu  dp_fft %8.3f us (err %5.1f dB)"
              "  bluestein %8.3f us (err %5.1f dB)  speedup %.2fx\n",
              n, pf_ok (n), b->m, best0 * 1e6, 10 * log10 (e0 / ref + 1e-300),
              best1 * 1e6, 10 * log10 (e1 / ref), best0 / best1);
    }
  return 0;
}
