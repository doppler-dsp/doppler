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
#include "pffft/pffft.h"
#include "pocketfft/pocketfft_c99.h"

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct pocketfft_plan
{
  int    sign; /* -1 forward (e^-i), +1 inverse; unnormalised. */
  size_t n, ny, nx;
  int    is2d;

  cfft_plan row; /* length nx for 2-D, length n for 1-D. */
  cfft_plan col; /* length ny for 2-D; NULL for 1-D.      */

  int use_transpose; /* 2-D: 1 ⇒ transpose pass, 0 ⇒ column gather/scatter. */

  /* Pre-allocated scratch (interleaved doubles), sized at create:
   *   1-D : promote    = 2*n         (cf32 promote/demote)
   *   2-D : promote    = 2*ny*nx     (cf32 whole-array promote)
   *   2-D transpose    : tbuf        = 2*ny*nx (transposed copy)
   *   2-D gather       : colscratch  = 2*ny    (one gathered column)  */
  double *promote;
  double *colscratch;
  double *tbuf;

  /* Native-float SIMD path (PFFFT) for the cf32/integer-IQ executes, used when
   * the size is PFFFT-friendly (see pffft_cf32_ok).  NULL ⇒ fall back to the
   * promote-to-double pocketfft path above.  pf is the 1-D plan; pf_row/pf_col
   * the 2-D per-axis plans.  fa/fb are 16-byte-aligned interleaved-float work
   * buffers (2*N each); fw is the PFFFT per-call work buffer (2*max-dim). */
  PFFFT_Setup *pf, *pf_row, *pf_col;
  float       *fa, *fb, *fw;
};

/* PFFFT complex needs N a multiple of 16 (SIMD_SZ²) and 5-smooth (factors
 * 2/3/5), N ≥ 16.  PFFFT only assert()s these (a no-op in release), so we gate
 * ourselves and never hand it a bad size. */
static int
pffft_cf32_ok (size_t n)
{
  if (n < 16 || n % 16 != 0)
    return 0;
  size_t m = n;
  while (m % 2 == 0)
    m /= 2;
  while (m % 3 == 0)
    m /= 3;
  while (m % 5 == 0)
    m /= 5;
  return m == 1;
}

/* A power-of-two column stride (nx) is the worst case for the strided column
 * pass: consecutive column elements land in conflicting cache sets, so the
 * gather/scatter thrashes.  For those (the common FFT sizes) we instead make
 * both passes contiguous via a blocked transpose; for other strides the gather
 * is already fine and the double transpose would only add overhead. */
static int
is_pow2 (size_t n)
{
  return n != 0 && (n & (n - 1)) == 0;
}

/* Free any partially-set-up PFFFT state and clear the fields, so execute falls
 * back to the pocketfft path. */
static void
pffft_clear (pocketfft_plan *p)
{
  if (p->pf)
    pffft_destroy_setup (p->pf);
  if (p->pf_row)
    pffft_destroy_setup (p->pf_row);
  if (p->pf_col)
    pffft_destroy_setup (p->pf_col);
  if (p->fa)
    pffft_aligned_free (p->fa);
  if (p->fb)
    pffft_aligned_free (p->fb);
  if (p->fw)
    pffft_aligned_free (p->fw);
  p->pf = p->pf_row = p->pf_col = NULL;
  p->fa = p->fb = p->fw = NULL;
}

/* Try to set up the native-float PFFFT fast path for the cf32/integer
 * executes. Non-fatal: on any failure (unsupported size, alloc/setup fail) it
 * leaves the fields NULL and the executes use the pocketfft promote-to-double
 * path. */
static void
pffft_try_setup (pocketfft_plan *p)
{
  size_t data, work; /* complex element counts */
  if (!p->is2d)
    {
      if (!pffft_cf32_ok (p->n))
        return;
      p->pf = pffft_new_setup ((int)p->n, PFFFT_COMPLEX);
      if (!p->pf)
        return;
      data = p->n;
      work = p->n;
    }
  else
    {
      if (!pffft_cf32_ok (p->nx) || !pffft_cf32_ok (p->ny))
        return;
      p->pf_row = pffft_new_setup ((int)p->nx, PFFFT_COMPLEX);
      p->pf_col = pffft_new_setup ((int)p->ny, PFFFT_COMPLEX);
      if (!p->pf_row || !p->pf_col)
        {
          pffft_clear (p);
          return;
        }
      data = p->ny * p->nx;
      work = p->nx > p->ny ? p->nx : p->ny;
    }
  p->fa = (float *)pffft_aligned_malloc (sizeof (float) * 2 * data);
  p->fb = (float *)pffft_aligned_malloc (sizeof (float) * 2 * data);
  p->fw = (float *)pffft_aligned_malloc (sizeof (float) * 2 * work);
  if (!p->fa || !p->fb || !p->fw)
    pffft_clear (p);
}

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
  pffft_try_setup (p); /* non-fatal native-float fast path for cf32 */
  return p;
}

pocketfft_plan *
pocketfft_plan_2d (size_t ny, size_t nx, int sign)
{
  pocketfft_plan *p = (pocketfft_plan *)calloc (1, sizeof (*p));
  if (!p)
    return NULL;
  p->ny            = ny;
  p->nx            = nx;
  p->sign          = sign;
  p->is2d          = 1;
  p->use_transpose = is_pow2 (nx);
  p->row           = make_cfft_plan (nx);
  p->col           = make_cfft_plan (ny);
  p->promote       = (double *)malloc (sizeof (double) * 2 * ny * nx);
  if (p->use_transpose)
    p->tbuf = (double *)malloc (sizeof (double) * 2 * ny * nx);
  else
    p->colscratch = (double *)malloc (sizeof (double) * 2 * ny);
  if (!p->row || !p->col || !p->promote
      || (p->use_transpose ? !p->tbuf : !p->colscratch))
    {
      pocketfft_destroy_plan (p);
      return NULL;
    }
  pffft_try_setup (p); /* non-fatal native-float fast path for cf32 */
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
  free (p->tbuf);
  pffft_clear (p);
  free (p);
}

/* Blocked complex transpose: t[j,i] = s[i,j] for an src*[ny][nx] ->
 * dst*[nx][ny] (interleaved doubles).  Cache-blocked so neither side strides
 * badly. */
#define DP_TBLK 16
static void
transpose_cplx (const double *s, double *t, size_t ny, size_t nx)
{
  for (size_t i0 = 0; i0 < ny; i0 += DP_TBLK)
    for (size_t j0 = 0; j0 < nx; j0 += DP_TBLK)
      {
        size_t imax = i0 + DP_TBLK < ny ? i0 + DP_TBLK : ny;
        size_t jmax = j0 + DP_TBLK < nx ? j0 + DP_TBLK : nx;
        for (size_t i = i0; i < imax; i++)
          for (size_t j = j0; j < jmax; j++)
            {
              t[2 * (j * ny + i)]     = s[2 * (i * nx + j)];
              t[2 * (j * ny + i) + 1] = s[2 * (i * nx + j) + 1];
            }
      }
}

/* Float twin of transpose_cplx, for the PFFFT 2-D path. */
static void
transpose_cplx_f (const float *s, float *t, size_t ny, size_t nx)
{
  for (size_t i0 = 0; i0 < ny; i0 += DP_TBLK)
    for (size_t j0 = 0; j0 < nx; j0 += DP_TBLK)
      {
        size_t imax = i0 + DP_TBLK < ny ? i0 + DP_TBLK : ny;
        size_t jmax = j0 + DP_TBLK < nx ? j0 + DP_TBLK : nx;
        for (size_t i = i0; i < imax; i++)
          for (size_t j = j0; j < jmax; j++)
            {
              t[2 * (j * ny + i)]     = s[2 * (i * nx + j)];
              t[2 * (j * ny + i) + 1] = s[2 * (i * nx + j) + 1];
            }
      }
}

/* ----------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------- */

/* PFFFT direction for our sign convention (sign<0 forward), unscaled —
 * matching the pocketfft path's unnormalised contract. */
static pffft_direction_t
pf_dir (int sign)
{
  return sign < 0 ? PFFFT_FORWARD : PFFFT_BACKWARD;
}

/* PFFFT requires 16-byte-aligned in/out.  numpy's complex64 buffers are
 * aligned in practice, so we transform straight in→out (no copy); only an
 * unaligned caller pays the bounce through fa/fb. */
static int
aligned16 (const void *p)
{
  return ((uintptr_t)p & 15u) == 0;
}

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
  size_t n = p->n;

  /* Native-float SIMD fast path. Transform straight in→out when both are
   * aligned (the common case); otherwise bounce the unaligned side through
   * fa/fb. */
  if (p->pf)
    {
      pffft_direction_t dir = pf_dir (p->sign);
      if (aligned16 (in) && aligned16 (out))
        pffft_transform_ordered (p->pf, (const float *)in, (float *)out, p->fw,
                                 dir);
      else
        {
          memcpy (p->fa, in, sizeof (float complex) * n);
          pffft_transform_ordered (p->pf, p->fa, p->fb, p->fw, dir);
          memcpy (out, p->fb, sizeof (float complex) * n);
        }
      return;
    }

  /* Fallback: promote to double, transform, demote. */
  const float complex *fin  = (const float complex *)in;
  float complex       *fout = (float complex *)out;
  double              *d    = p->promote;
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

/* Transform an interleaved double[2*ny*nx] array in place: FFT every row
 * (contiguous), then every column.  The column pass is the only choice point —
 * see is_pow2()/use_transpose: for a power-of-two column stride we transpose
 * so the column FFTs run on contiguous data (avoiding cache-conflict thrash);
 * otherwise we gather each column through colscratch. */
static void
xform_2d (pocketfft_plan *p, double *d)
{
  size_t ny = p->ny, nx = p->nx;

  for (size_t r = 0; r < ny; ++r)
    xform (p->row, p->sign, d + 2 * r * nx, nx);

  if (p->use_transpose)
    {
      double *t = p->tbuf;
      transpose_cplx (d, t, ny, nx);  /* d[ny][nx] -> t[nx][ny]            */
      for (size_t r = 0; r < nx; ++r) /* each t-row is an original column */
        xform (p->col, p->sign, t + 2 * r * ny, ny);
      transpose_cplx (t, d, nx, ny); /* t[nx][ny] -> d[ny][nx]            */
      return;
    }

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
  size_t ny = p->ny, nx = p->nx, total = ny * nx;

  /* Native-float SIMD fast path: rows via pf_row, float transpose, cols via
   * pf_col, transpose back.  fa/fb are 16-byte-aligned; per-row sub-pointers
   * stay aligned because nx and ny are multiples of 16 (pffft_cf32_ok). */
  if (p->pf_row && p->pf_col)
    {
      float            *a = p->fa, *b = p->fb;
      pffft_direction_t dir = pf_dir (p->sign);
      memcpy (a, in, sizeof (float complex) * total);
      for (size_t r = 0; r < ny; ++r)
        pffft_transform_ordered (p->pf_row, a + 2 * r * nx, b + 2 * r * nx,
                                 p->fw, dir);
      transpose_cplx_f (b, a, ny, nx); /* b[ny][nx] -> a[nx][ny] */
      for (size_t r = 0; r < nx; ++r)
        pffft_transform_ordered (p->pf_col, a + 2 * r * ny, b + 2 * r * ny,
                                 p->fw, dir);
      transpose_cplx_f (b, (float *)out, nx,
                        ny); /* b[nx][ny] -> out[ny][nx] */
      return;
    }

  /* Fallback: promote to double, 2-D double transform, demote. */
  const float complex *fin  = (const float complex *)in;
  float complex       *fout = (float complex *)out;
  double              *d    = p->promote;
  for (size_t i = 0; i < total; ++i)
    {
      d[2 * i]     = (double)crealf (fin[i]);
      d[2 * i + 1] = (double)cimagf (fin[i]);
    }
  xform_2d (p, d);
  for (size_t i = 0; i < total; ++i)
    fout[i] = (float)d[2 * i] + (float)d[2 * i + 1] * I;
}
