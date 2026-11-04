/* Prototype for doppler#2732 / #2752 step 2.1: is a slow-time transform
 * shared by every tile faster than today's per-tile column stage?
 *
 * A (today, acq_tile_epoch + acq_tile_block): per tile r and epoch k,
 *   prod = roll_r(X_k) * C, padded to lb, inverted -> blk[r][k];
 *   then per lag column, the D epochs zero-padded to dI and transformed.
 * B (#2732): per capture bin j, the D epoch spectra X_k[j] zero-padded to
 *   dI and transformed once -> S[i][j], shared by all tiles; then per tile
 *   r and slow row i, prod = roll_r(S[i]) * C, padded, inverted.
 * No code-rate ramp (no pinned row sets a carrier), so B is an exact
 * reordering of A. Both write the same combined FFT-order surface.
 *
 * With a carrier (dil > 0): tile r's dilation is d = ffreq(r, W) * dil
 * samples an epoch. A ramps each epoch's product exactly, a phasor
 * recurrence over the signed bins as acq_ramp does; B takes slow time on a
 * grid F times finer, its origin at the block's middle, and each tile reads
 * bin j at fine row F*i + round(F*dI*d*j'/cb), times the phase that
 * centering leaves, both precomputed per tile and bin (at regrid, in an
 * engine).
 *
 * usage: shared_slow cb lb D I W reps [dil=0] [F=1] [seed]
 */
#include "doppler/fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHUNK 8

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

static long
ffreq (size_t i, size_t n)
{
  return i < (n + 1) / 2 ? (long)i : (long)i - (long)n;
}

static unsigned long long rs = 88172645463325252ull;
static float
urand (void)
{
  rs ^= rs << 13;
  rs ^= rs >> 7;
  rs ^= rs << 17;
  return (float)((double)(rs >> 11) / 9007199254740992.0) - 0.5f;
}

int
main (int argc, char **argv)
{
  if (argc < 7)
    {
      fprintf (stderr, "usage: %s cb lb D I W reps\n", argv[0]);
      return 2;
    }
  const size_t cb = strtoul (argv[1], 0, 10), lb = strtoul (argv[2], 0, 10);
  const size_t D = strtoul (argv[3], 0, 10), IP = strtoul (argv[4], 0, 10);
  const size_t W    = strtoul (argv[5], 0, 10);
  const int    reps = atoi (argv[6]);
  const double dil  = argc > 7 ? atof (argv[7]) : 0.0;
  const size_t F    = argc > 8 ? strtoul (argv[8], 0, 10) : 1;
  if (argc > 9)
    rs = 88172645463325252ull ^ strtoull (argv[9], 0, 10);
  const size_t dI = D * IP, rows = W * dI, fI = F * dI;
  const double cmid = 0.5 * (double)(D - 1);

  float _Complex *X   = malloc (D * cb * sizeof *X);
  float _Complex *C   = malloc (cb * sizeof *C);
  float _Complex *pr  = malloc (cb * sizeof *pr);
  float _Complex *pd  = malloc (lb * sizeof *pd);
  float _Complex *blk = malloc (W * D * lb * sizeof *blk);
  float _Complex *S   = malloc (fI * cb * sizeof *S);
  /* B's per tile and bin pick: fine-row offset and the centering phase. */
  long           *roff = calloc (W * cb, sizeof *roff);
  float _Complex *PR   = malloc (dI * cb * sizeof *PR);
  float _Complex *rph  = malloc (W * cb * sizeof *rph);
  for (size_t r = 0; r < W; r++)
    for (size_t j = 0; j < cb; j++)
      {
        const double d   = (double)ffreq (r, W) * dil;
        const long   ro  = lround ((double)F * (double)dI * d
                                   * (double)ffreq (j, cb) / (double)cb);
        roff[r * cb + j] = ro;
        rph[r * cb + j]  = (float _Complex)cexp (2.0 * M_PI * _Complex_I * cmid
                                                 * (double)ro / (double)fI);
      }
  float _Complex *sA   = malloc (rows * lb * sizeof *sA);
  float _Complex *sB   = malloc (rows * lb * sizeof *sB);
  float _Complex *cin  = calloc (CHUNK * dI, sizeof *cin);
  float _Complex *cout = malloc (CHUNK * fI * sizeof *cout);
  /* B's own column buffer: its zero tails must stay zero, and A writes
     its columns at stride dI. */
  float _Complex *cinF = calloc (CHUNK * fI, sizeof *cinF);
  size_t         *u    = malloc (rows * sizeof *u);
  for (size_t k = 0; k < D * cb; k++)
    X[k] = urand () + _Complex_I * urand ();
  for (size_t j = 0; j < cb; j++)
    C[j] = urand () + _Complex_I * urand ();
  for (size_t r = 0; r < W; r++)
    for (size_t i = 0; i < dI; i++)
      u[r * dI + i]
          = (size_t)(((ffreq (r, W) * (long)dI + ffreq (i, dI)) % (long)rows
                      + (long)rows)
                     % (long)rows);
  dp_fft_state_t *inv   = dp_fft_create (lb, +1, 1);
  dp_fft_state_t *slow  = dp_fft_create (dI, -1, 1);
  dp_fft_state_t *slowF = dp_fft_create (fI, -1, 1);

  double bestA = 1e30, bestB = 1e30, a_rows = 1e30, a_cols = 1e30,
         b_slow = 1e30, b_rows = 1e30;
  for (int rep = 0; rep < reps; rep++)
    {
      /* ---- A: rows per epoch per tile, then columns per tile ---- */
      double t0 = now ();
      for (size_t r = 0; r < W; r++)
        {
          const size_t roll
              = (size_t)((ffreq (r, W) % (long)cb + (long)cb) % (long)cb);
          for (size_t k = 0; k < D; k++)
            {
              const float _Complex *x = X + k * cb;
              for (size_t j = 0; j < cb; j++)
                pr[j] = x[(j + roll) % cb] * C[j];
              const double d = (double)ffreq (r, W) * dil;
              if (d != 0.0)
                {
                  /* the exact ramp, as acq_ramp runs it */
                  const double w
                      = -2.0 * M_PI * ((double)k - cmid) * d / (double)cb;
                  const size_t half  = cb / 2 + cb % 2;
                  double _Complex ph = 1.0, st = cexp (_Complex_I * w);
                  for (size_t j = 0; j < cb; j++)
                    {
                      if ((j & 255) == 0 || j == half)
                        ph = cexp (_Complex_I * w * (double)ffreq (j, cb));
                      pr[j] *= (float _Complex)ph;
                      ph *= st;
                    }
                }
              dp_fft_pad_spectrum (pr, cb, pd, lb);
              dp_fft_execute_cf32 (inv, pd, lb, blk + (r * D + k) * lb, lb);
            }
        }
      double t1 = now ();
      for (size_t r = 0; r < W; r++)
        {
          const float _Complex *b = blk + r * D * lb;
          for (size_t j0 = 0; j0 < lb; j0 += CHUNK)
            {
              const size_t nc = lb - j0 < CHUNK ? lb - j0 : CHUNK;
              for (size_t i = 0; i < D; i++)
                for (size_t c = 0; c < nc; c++)
                  cin[c * dI + i] = b[i * lb + j0 + c];
              for (size_t c = 0; c < nc; c++)
                dp_fft_execute_cf32 (slow, cin + c * dI, dI, cout + c * dI,
                                     dI);
              for (size_t i = 0; i < dI; i++)
                {
                  float _Complex *dst = sA + u[r * dI + i] * lb + j0;
                  for (size_t c = 0; c < nc; c++)
                    dst[c] = cout[c * dI + i];
                }
            }
        }
      double t2 = now ();
      /* ---- B: one slow transform per capture bin, shared ---- */
      for (size_t j0 = 0; j0 < cb; j0 += CHUNK)
        {
          const size_t nc = cb - j0 < CHUNK ? cb - j0 : CHUNK;
          for (size_t i = 0; i < D; i++)
            for (size_t c = 0; c < nc; c++)
              cinF[c * fI + i] = X[i * cb + j0 + c];
          for (size_t c = 0; c < nc; c++)
            dp_fft_execute_cf32 (slowF, cinF + c * fI, fI, cout + c * fI, fI);
          /* bin-major: bin p's fI fine rows contiguous */
          memcpy (S + j0 * fI, cout, nc * fI * sizeof *S);
        }
      double t3 = now ();
      for (size_t r = 0; r < W; r++)
        {
          const size_t roll
              = (size_t)((ffreq (r, W) % (long)cb + (long)cb) % (long)cb);
          /* One row stage with or without a carrier: no carrier is a
             zero offset and a unit phase on every bin (F = 1). */
          {
            /* Gather the tile's dI x cb product, CHUNK bins at a time:
               the CHUNK bins' fine rows stay in cache while every slow
               row reads them. */
            const long           *ro = roff + r * cb;
            const float _Complex *ph = rph + r * cb;
            for (size_t j0 = 0; j0 < cb; j0 += CHUNK)
              {
                const size_t nc = cb - j0 < CHUNK ? cb - j0 : CHUNK;
                /* No division in the loop: each bin's fine-row offset is
                   wrapped into [0, fI) once, and a slow row moves it by
                   F, so one compare wraps it again. */
                const float _Complex *col[CHUNK];
                float _Complex cw[CHUNK];
                long n0[CHUNK];
                for (size_t c = 0; c < nc; c++)
                  {
                    const size_t j = j0 + c,
                                 p = j + roll < cb ? j + roll : j + roll - cb;
                    col[c]         = S + p * fI;
                    cw[c]          = C[j] * ph[j];
                    n0[c] = ((ro[j] % (long)fI) + (long)fI) % (long)fI;
                  }
                for (size_t i = 0; i < dI; i++)
                  for (size_t c = 0; c < nc; c++)
                    {
                      long n = n0[c] + (long)(F * i);
                      if (n >= (long)fI)
                        n -= (long)fI;
                      PR[i * cb + j0 + c] = col[c][n] * cw[c];
                    }
              }
            for (size_t i = 0; i < dI; i++)
              {
                dp_fft_pad_spectrum (PR + i * cb, cb, pd, lb);
                dp_fft_execute_cf32 (inv, pd, lb, sB + u[r * dI + i] * lb, lb);
              }
          }
        }
      double t4 = now ();
      if (t2 - t0 < bestA)
        bestA = t2 - t0;
      if (t4 - t2 < bestB)
        bestB = t4 - t2;
      if (t1 - t0 < a_rows)
        a_rows = t1 - t0;
      if (t2 - t1 < a_cols)
        a_cols = t2 - t1;
      if (t3 - t2 < b_slow)
        b_slow = t3 - t2;
      if (t4 - t3 < b_rows)
        b_rows = t4 - t3;
    }
  /* N (W = 1 only): today's narrow path, acq_make_surface. Slow time first
     on the raw epochs, then each of the dI rows correlated through
     Corr2D's fast path: a forward at cb, the product, a padded inverse.
     Against it, B plus the D forward transforms B's input needs. */
  if (W == 1)
    {
      dp_fft_state_t *fwd   = dp_fft_create (cb, -1, 1);
      float _Complex *rowt  = malloc (dI * cb * sizeof *rowt);
      float _Complex *spec  = malloc (cb * sizeof *spec);
      float _Complex *sN    = malloc (dI * lb * sizeof *sN); /* not sA */
      double          bestN = 1e30, bestF = 1e30;
      for (int rep = 0; rep < reps; rep++)
        {
          double t0 = now ();
          for (size_t j0 = 0; j0 < cb; j0 += CHUNK)
            {
              const size_t nc = cb - j0 < CHUNK ? cb - j0 : CHUNK;
              for (size_t i = 0; i < D; i++)
                for (size_t c = 0; c < nc; c++)
                  cin[c * dI + i] = X[i * cb + j0 + c];
              for (size_t c = 0; c < nc; c++)
                dp_fft_execute_cf32 (slow, cin + c * dI, dI, cout + c * dI,
                                     dI);
              for (size_t i = 0; i < dI; i++)
                for (size_t c = 0; c < nc; c++)
                  rowt[i * cb + j0 + c] = cout[c * dI + i];
            }
          for (size_t i = 0; i < dI; i++)
            {
              dp_fft_execute_cf32 (fwd, rowt + i * cb, cb, spec, cb);
              for (size_t j = 0; j < cb; j++)
                pr[j] = spec[j] * C[j];
              dp_fft_pad_spectrum (pr, cb, pd, lb);
              dp_fft_execute_cf32 (inv, pd, lb, sN + i * lb, lb);
            }
          double t1 = now ();
          for (size_t k = 0; k < D; k++)
            dp_fft_execute_cf32 (fwd, X + k * cb, cb, spec, cb);
          double t2 = now ();
          if (t1 - t0 < bestN)
            bestN = t1 - t0;
          if (t2 - t1 < bestF)
            bestF = t2 - t1;
        }
      printf ("  narrow today (slow + dI x (fwd+inv)) = %.2f ms;  B + D "
              "forwards = %.2f ms;  (B+fwd)/narrow = %.3f\n",
              1e3 * bestN, 1e3 * (bestB + bestF), (bestB + bestF) / bestN);
    }
  double num = 0, den = 0, mx = 0;
  for (size_t k = 0; k < rows * lb; k++)
    {
      const double d = cabs ((double _Complex) (sA[k] - sB[k]));
      num += d * d;
      den += cabs ((double _Complex)sA[k]) * cabs ((double _Complex)sA[k]);
      if (d > mx)
        mx = d;
    }
  double amax = 0;
  for (size_t k = 0; k < rows * lb; k++)
    if (cabsf (sA[k]) > amax)
      amax = cabsf (sA[k]);
  /* The decided oracle bound (#2752), |d| <= c eps (log2 N + log2 L +
     log2 dI) sqrt(D) |x|_2 |r|_2, solved for the c this run needs: x the
     block's samples and r the template, in time, by Parseval from the
     spectra the prototype holds (|x|^2 = sum |X_k|^2 / N). */
  double xx = 0, rr = 0;
  for (size_t k = 0; k < D * cb; k++)
    xx += cabsf (X[k]) * cabsf (X[k]);
  for (size_t j = 0; j < cb; j++)
    rr += cabsf (C[j]) * cabsf (C[j]);
  const double eps = 1.1920928955078125e-07;
  const double bnd
      = eps * (log2 ((double)cb) + log2 ((double)lb) + log2 ((double)dI))
        * sqrt ((double)D) * sqrt (xx / (double)cb) * sqrt (rr / (double)cb);
  if (dil == 0.0)
    printf ("  oracle: max |A-B| = %.3e, bound/c = %.3e, c needed = %.3e\n",
            mx, bnd, mx / bnd);
  printf ("cb=%zu lb=%zu D=%zu I=%zu W=%zu  A=%.2f ms (rows %.2f + cols "
          "%.2f)  B=%.2f ms (slow %.2f + rows %.2f)  B/A=%.3f  "
          "rel_rms=%.2e max_abs/max=%.2e\n",
          cb, lb, D, IP, W, 1e3 * bestA, 1e3 * a_rows, 1e3 * a_cols,
          1e3 * bestB, 1e3 * b_slow, 1e3 * b_rows, bestB / bestA,
          sqrt (num / den), mx / amax);
  return 0;
}
