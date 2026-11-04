/* Prototype for doppler#2732 with the code-rate ramp on: what does a
 * shared slow-time transform on a grid F times finer cost, and what does
 * rounding each bin's ramp shift to that grid lose?
 *
 * One tile, carrier hypothesis r0 (signed), dilation d samples an epoch.
 * The cross-spectrum of epoch k in the tile's frame (after the roll and
 * the replica) is built as a tone that drifts the way a dilated code does:
 *     P_k[j] = G[j] * exp(+i 2 pi (k - c) d j'/nx) * exp(i 2 pi f k)
 * so today's ramp exp(-i 2 pi (k - c) d j'/nx) (acq_tile_epoch) cancels it
 * exactly, and the slow-time peak lands at f. G is a random unit-power
 * spectrum (a code), j' the signed bin.
 *
 * A (exact, today): ramp per epoch, inverse per epoch, slow FFT per column.
 * B(F): slow FFT of P_k[j] over k on F*dI points, shared; per slow row i
 *   and bin j, read the fine bin round(F*(i + dI*d*j'/nx)) and the phase
 *   exp(+i 2 pi c d j'/nx); inverse per slow row.
 * Reported: B's peak over A's (the loss), worst over f across a bin, and
 * the slow-stage cost on F*dI points for the full nx.
 *
 * usage: ramp_grid nx D I d F [center=1] [tones=33]
 */
#include "doppler/fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}
static long
sb (size_t j, size_t n)
{
  return j < (n + 1) / 2 ? (long)j : (long)j - (long)n;
}
static unsigned long long rs = 88172645463325252ull;
static double
ur (void)
{
  rs ^= rs << 13;
  rs ^= rs >> 7;
  rs ^= rs << 17;
  return (double)(rs >> 11) / 9007199254740992.0;
}

int
main (int argc, char **argv)
{
  const size_t nx = strtoul (argv[1], 0, 10), D = strtoul (argv[2], 0, 10);
  const size_t IP = strtoul (argv[3], 0, 10);
  const double d  = atof (argv[4]);
  const size_t F  = strtoul (argv[5], 0, 10);
  /* center = 1: the phase each bin's pick is multiplied by uses its ROUNDED
     shift, which is what a slow-time transform with its origin at the
     block's middle gives; 0: the exact shift (an origin at epoch 0), whose
     rounding leaves a phase error of 2 pi c (rounded - exact) that differs
     bin to bin. */
  const int       center = argc > 6 ? atoi (argv[6]) : 1;
  const int       nf     = argc > 7 ? atoi (argv[7]) : 33;
  const size_t    dI = D * IP, fI = F * dI;
  const double    c = 0.5 * (double)(D - 1);
  float _Complex *G = malloc (nx * sizeof *G);
  for (size_t j = 0; j < nx; j++)
    G[j] = (float _Complex)cexp (2.0 * M_PI * _Complex_I * ur ());
  float _Complex *P     = malloc (D * nx * sizeof *P);
  float _Complex *z     = malloc (D * nx * sizeof *z);
  float _Complex *col   = calloc (fI, sizeof *col),
                 *co    = malloc (fI * sizeof *co);
  float _Complex *ZA    = malloc (dI * nx * sizeof *ZA);
  float _Complex *S     = malloc (fI * nx * sizeof *S);
  float _Complex *pr    = malloc (nx * sizeof *pr),
                 *row   = malloc (nx * sizeof *row);
  dp_fft_state_t *inv   = dp_fft_create (nx, +1, 1);
  dp_fft_state_t *sl    = dp_fft_create (dI, -1, 1);
  dp_fft_state_t *slF   = dp_fft_create (fI, -1, 1);
  double          worst = 0.0, mean = 0.0, tslow = 1e30;
  for (int fi = 0; fi < nf; fi++)
    {
      /* a slow-time tone from a row center to the next one (a whole bin
         of the interpolated grid is 1/dI cycles an epoch) */
      const double f = (3.0 + (double)fi / (double)(nf - 1)) / (double)dI;
      for (size_t k = 0; k < D; k++)
        for (size_t j = 0; j < nx; j++)
          P[k * nx + j]
              = G[j]
                * (float _Complex)cexp (
                    2.0 * M_PI * _Complex_I
                    * (((double)k - c) * d * (double)sb (j, nx) / (double)nx
                       + f * (double)k));
      /* A: ramp, inverse per epoch, slow FFT per column. */
      for (size_t k = 0; k < D; k++)
        {
          for (size_t j = 0; j < nx; j++)
            pr[j] = P[k * nx + j]
                    * (float _Complex)cexp (-2.0 * M_PI * _Complex_I
                                            * ((double)k - c) * d
                                            * (double)sb (j, nx) / (double)nx);
          dp_fft_execute_cf32 (inv, pr, nx, z + k * nx, nx);
        }
      double pa = 0;
      for (size_t m = 0; m < nx; m++)
        {
          for (size_t k = 0; k < dI; k++)
            col[k] = k < D ? z[k * nx + m] : 0;
          dp_fft_execute_cf32 (sl, col, dI, co, dI);
          for (size_t i = 0; i < dI; i++)
            {
              ZA[i * nx + m] = co[i];
              if (cabsf (co[i]) > pa)
                pa = cabsf (co[i]);
            }
        }
      /* B(F): one fine slow FFT per bin, then a shifted pick per row. */
      double t0 = now ();
      for (size_t j = 0; j < nx; j++)
        {
          for (size_t k = 0; k < fI; k++)
            col[k] = k < D ? P[k * nx + j] : 0;
          dp_fft_execute_cf32 (slF, col, fI, co, fI);
          for (size_t n = 0; n < fI; n++)
            S[n * nx + j] = co[n];
        }
      double t1 = now ();
      if (t1 - t0 < tslow)
        tslow = t1 - t0;
      double pb = 0;
      for (size_t i = 0; i < dI; i++)
        {
          for (size_t j = 0; j < nx; j++)
            {
              const double sh
                  = (double)dI * d * (double)sb (j, nx) / (double)nx;
              const long r     = lround ((double)F * sh); /* fine rows */
              long       n     = (long)(F * i) + r;
              n                = ((n % (long)fI) + (long)fI) % (long)fI;
              const double cyc = center ? (double)r / (double)fI
                                        : d * (double)sb (j, nx) / (double)nx;
              pr[j]
                  = S[(size_t)n * nx + j]
                    * (float _Complex)cexp (2.0 * M_PI * _Complex_I * c * cyc);
            }
          dp_fft_execute_cf32 (inv, pr, nx, row, nx);
          for (size_t m = 0; m < nx; m++)
            if (cabsf (row[m]) > pb)
              pb = cabsf (row[m]);
        }
      const double loss = -20.0 * log10 (pb / pa); /* dB, + = B lower */
      if (loss > worst)
        worst = loss;
      mean += loss / nf;
    }
  printf ("nx=%zu D=%zu I=%zu d=%.4f F=%zu center=%d tones=%d  max shift="
          "%.2f rows  worst loss=%.3f dB  mean loss=%.3f dB  slow on %zu pts "
          "for nx bins=%.2f ms\n",
          nx, D, IP, d, F, center, nf, (double)dI * d * 0.5, worst, mean, fI,
          1e3 * tslow);
  return 0;
}
