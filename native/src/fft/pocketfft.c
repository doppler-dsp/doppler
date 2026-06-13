/*
 * pocketfft.c — pure-C99 wrapper exposing doppler's small FFT C API
 * (native/inc/pocketfft/pocketfft.h) over the upstream pure-C pocketfft
 * core (pocketfft_c99.c).  Replaces the former C++ wrapper (pocketfft.cc
 * over the header-only C++ pocketfft); doppler links only -lm now.
 *
 * The upstream `cfft` core is double-precision, 1-D, and transforms
 * in-place on an interleaved double[2*n] (re,im,...).  This wrapper adds
 * the three things doppler's API needs on top of it:
 *
 *   1. out-of-place execution        — memcpy in -> out, transform out.
 *   2. single-precision (cf32) paths — promote float->double, transform,
 *                                       demote double->float.
 *   3. 2-D transforms                — row passes (contiguous) followed by
 *                                       column passes (gather/scatter), since
 *                                       the core is 1-D only.
 *
 * Scratch buffers live in the plan and are sized at create time.  An
 * instance is therefore single-threaded (matching fft_core.h, which
 * documents pocketfft as single-threaded); doppler's thread-per-shard
 * model uses a distinct instance per thread.
 */

#include "pocketfft/pocketfft.h"
#include "pocketfft/pocketfft_c99.h"

#include <complex.h>
#include <stdlib.h>
#include <string.h>

struct pocketfft_plan
{
  int    sign; /* -1 forward (e^-i), +1 inverse; unnormalised. */
  size_t n, ny, nx;
  int    is2d;

  cfft_plan row; /* length nx for 2-D, length n for 1-D. */
  cfft_plan col; /* length ny for 2-D; NULL for 1-D.      */

  /* Pre-allocated scratch (interleaved doubles), sized at create:
   *   1-D : promote   = 2*n          (cf32 promote/demote)
   *   2-D : promote   = 2*ny*nx      (cf32 whole-array promote)
   *         colscratch = 2*ny        (one gathered column)        */
  double *promote;
  double *colscratch;
};

/* ----------------------------------------------------------------------
 * Plan lifecycle
 * -------------------------------------------------------------------- */
pocketfft_plan *
pocketfft_plan_1d (size_t n, int sign)
{
  pocketfft_plan *p = (pocketfft_plan *)calloc (1, sizeof (*p));
  if (!p)
    return NULL;
  p->n       = n;
  p->sign    = sign;
  p->is2d    = 0;
  p->row     = make_cfft_plan (n);
  p->promote = (double *)malloc (sizeof (double) * 2 * n);
  if (!p->row || !p->promote)
    {
      pocketfft_destroy_plan (p);
      return NULL;
    }
  return p;
}

pocketfft_plan *
pocketfft_plan_2d (size_t ny, size_t nx, int sign)
{
  pocketfft_plan *p = (pocketfft_plan *)calloc (1, sizeof (*p));
  if (!p)
    return NULL;
  p->ny         = ny;
  p->nx         = nx;
  p->sign       = sign;
  p->is2d       = 1;
  p->row        = make_cfft_plan (nx);
  p->col        = make_cfft_plan (ny);
  p->promote    = (double *)malloc (sizeof (double) * 2 * ny * nx);
  p->colscratch = (double *)malloc (sizeof (double) * 2 * ny);
  if (!p->row || !p->col || !p->promote || !p->colscratch)
    {
      pocketfft_destroy_plan (p);
      return NULL;
    }
  return p;
}

void
pocketfft_destroy_plan (pocketfft_plan *p)
{
  if (!p)
    return;
  if (p->row)
    destroy_cfft_plan (p->row);
  if (p->col)
    destroy_cfft_plan (p->col);
  free (p->promote);
  free (p->colscratch);
  free (p);
}

/* ----------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------- */

/* In-place 1-D transform of one interleaved double[2*len] vector. */
static void
xform (cfft_plan plan, int sign, double *c, size_t len)
{
  /* The upstream core uses fct=1.0 (unnormalised); the inverse does NOT
   * divide by len, matching doppler's documented contract. */
  if (sign < 0)
    cfft_forward (plan, c, 1.0);
  else
    cfft_backward (plan, c, 1.0);
}

/* ----------------------------------------------------------------------
 * 1-D execute
 * -------------------------------------------------------------------- */
void
pocketfft_execute_1d (pocketfft_plan *p, const void *in, void *out)
{
  /* cf64: copy in -> out, transform out in place. */
  if (in != out)
    memcpy (out, in, sizeof (double complex) * p->n);
  xform (p->row, p->sign, (double *)out, p->n);
}

void
pocketfft_execute_1d_cf32 (pocketfft_plan *p, const void *in, void *out)
{
  const float complex *fin  = (const float complex *)in;
  float complex       *fout = (float complex *)out;
  double              *d    = p->promote;
  size_t               n    = p->n;

  for (size_t i = 0; i < n; ++i)
    {
      d[2 * i]     = (double)crealf (fin[i]);
      d[2 * i + 1] = (double)cimagf (fin[i]);
    }
  xform (p->row, p->sign, d, n);
  for (size_t i = 0; i < n; ++i)
    fout[i] = (float)d[2 * i] + (float)d[2 * i + 1] * I;
}

/* ----------------------------------------------------------------------
 * 2-D execute (row passes, then column passes)
 * -------------------------------------------------------------------- */

/* Transform every row (contiguous) then every column (strided, gathered
 * through colscratch) of an interleaved double[2*ny*nx] array in place. */
static void
xform_2d (pocketfft_plan *p, double *d)
{
  size_t ny = p->ny, nx = p->nx;

  for (size_t r = 0; r < ny; ++r)
    xform (p->row, p->sign, d + 2 * r * nx, nx);

  for (size_t c = 0; c < nx; ++c)
    {
      double *col = p->colscratch;
      for (size_t r = 0; r < ny; ++r)
        {
          col[2 * r]     = d[2 * (r * nx + c)];
          col[2 * r + 1] = d[2 * (r * nx + c) + 1];
        }
      xform (p->col, p->sign, col, ny);
      for (size_t r = 0; r < ny; ++r)
        {
          d[2 * (r * nx + c)]     = col[2 * r];
          d[2 * (r * nx + c) + 1] = col[2 * r + 1];
        }
    }
}

void
pocketfft_execute_2d (pocketfft_plan *p, const void *in, void *out)
{
  size_t total = p->ny * p->nx;
  if (in != out)
    memcpy (out, in, sizeof (double complex) * total);
  xform_2d (p, (double *)out);
}

void
pocketfft_execute_2d_cf32 (pocketfft_plan *p, const void *in, void *out)
{
  const float complex *fin   = (const float complex *)in;
  float complex       *fout  = (float complex *)out;
  double              *d     = p->promote;
  size_t               total = p->ny * p->nx;

  for (size_t i = 0; i < total; ++i)
    {
      d[2 * i]     = (double)crealf (fin[i]);
      d[2 * i + 1] = (double)cimagf (fin[i]);
    }
  xform_2d (p, d);
  for (size_t i = 0; i < total; ++i)
    fout[i] = (float)d[2 * i] + (float)d[2 * i + 1] * I;
}
