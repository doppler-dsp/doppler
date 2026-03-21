// C/src/fft.c
// Simple global-plan FFT API for Python wrapper (true zero-copy).

#include "dp/fft.h"
#include <complex.h>
#include <fftw3.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------
 * Global state (ALL CAPS)
 * ------------------------------------------------------------ */

static fftw_plan PLAN_1D = NULL;
static fftw_plan PLAN_2D = NULL;

static size_t GLOBAL_N = 0;
static size_t GLOBAL_NY = 0;
static size_t GLOBAL_NX = 0;

static int GLOBAL_FORWARD = FFTW_FORWARD;
static int GLOBAL_FLAGS = FFTW_MEASURE;
static int GLOBAL_THREADS = 1;

/* Bound user buffers for zero-copy fast path */
static fftw_complex *BOUND_IN_1D = NULL;
static fftw_complex *BOUND_OUT_1D = NULL;
static fftw_complex *BOUND_IN_2D = NULL;
static fftw_complex *BOUND_OUT_2D = NULL;

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static int
planner_flag (const char *s)
{
  if (!s)
    return FFTW_ESTIMATE;

  if (strcmp (s, "estimate") == 0)
    return FFTW_ESTIMATE;
  if (strcmp (s, "measure") == 0)
    return FFTW_MEASURE;
  if (strcmp (s, "patient") == 0)
    return FFTW_PATIENT;
  if (strcmp (s, "exhaustive") == 0)
    return FFTW_EXHAUSTIVE;

  return FFTW_MEASURE;
}

/* ------------------------------------------------------------
 * Global setup
 * ------------------------------------------------------------ */

void
dp_fft_global_setup (const size_t *shape, size_t ndim, int sign, int nthreads,
                     const char *planner, const char *wisdom_path)
{
  /* Python convention: -1=Forward, +1=Inverse.
   * FFTW convention: FFTW_FORWARD=-1, FFTW_BACKWARD=+1. */
  GLOBAL_FORWARD = (sign < 0) ? FFTW_FORWARD : FFTW_BACKWARD;
  GLOBAL_FLAGS = planner_flag (planner);
  GLOBAL_THREADS = (nthreads > 0) ? nthreads : 1;

  fftw_init_threads ();
  fftw_plan_with_nthreads (GLOBAL_THREADS);

  if (wisdom_path && wisdom_path[0] != '\0')
    fftw_import_wisdom_from_filename (wisdom_path);

  /* Destroy old plans */
  if (PLAN_1D)
    {
      fftw_destroy_plan (PLAN_1D);
      PLAN_1D = NULL;
    }
  if (PLAN_2D)
    {
      fftw_destroy_plan (PLAN_2D);
      PLAN_2D = NULL;
    }

  /* Reset bound user buffers */
  BOUND_IN_1D = NULL;
  BOUND_OUT_1D = NULL;
  BOUND_IN_2D = NULL;
  BOUND_OUT_2D = NULL;

  if (ndim == 1)
    {
      GLOBAL_N = shape[0];
      GLOBAL_NY = 0;
      GLOBAL_NX = 0;
    }
  else if (ndim == 2)
    {
      GLOBAL_NY = shape[0];
      GLOBAL_NX = shape[1];
      GLOBAL_N = 0;
    }
  else
    {
      GLOBAL_N = GLOBAL_NY = GLOBAL_NX = 0;
    }
}

/* ------------------------------------------------------------
 * 1D execute (true zero-copy, no fallback)
 * ------------------------------------------------------------ */

void
dp_fft1d_execute (const double complex *input, double complex *output)
{
  if (!input || !output || GLOBAL_N == 0)
    return;

  if (!PLAN_1D)
    {
      /* First call defines the bound user buffers */
      BOUND_IN_1D = (fftw_complex *)input;
      BOUND_OUT_1D = (fftw_complex *)output;

      fftw_plan_with_nthreads (GLOBAL_THREADS);
      PLAN_1D = fftw_plan_dft_1d ((int)GLOBAL_N, BOUND_IN_1D, BOUND_OUT_1D,
                                  GLOBAL_FORWARD, GLOBAL_FLAGS);
    }

  /* Caller must reuse the same buffers as used for planning for max
   * performance. If they don't, we use the _dft fallback which is still fast.
   */
  if ((fftw_complex *)input != BOUND_IN_1D
      || (fftw_complex *)output != BOUND_OUT_1D)
    {
      fftw_execute_dft (PLAN_1D, (fftw_complex *)input,
                        (fftw_complex *)output);
      return;
    }

  fftw_execute (PLAN_1D);
}

void
dp_fft1d_execute_inplace (double complex *data)
{
  if (!data || GLOBAL_N == 0)
    return;

  if (!PLAN_1D)
    {
      BOUND_IN_1D = (fftw_complex *)data;
      BOUND_OUT_1D = (fftw_complex *)data;

      fftw_plan_with_nthreads (GLOBAL_THREADS);
      PLAN_1D = fftw_plan_dft_1d ((int)GLOBAL_N, BOUND_IN_1D, BOUND_OUT_1D,
                                  GLOBAL_FORWARD, GLOBAL_FLAGS);
    }

  if ((fftw_complex *)data != BOUND_IN_1D)
    {
      fftw_execute_dft (PLAN_1D, (fftw_complex *)data, (fftw_complex *)data);
      return;
    }

  fftw_execute (PLAN_1D);
}

/* ------------------------------------------------------------
 * 2D execute (true zero-copy, no fallback)
 * ------------------------------------------------------------ */

void
dp_fft2d_execute (const double complex *input, double complex *output)
{
  if (!input || !output || GLOBAL_NY == 0 || GLOBAL_NX == 0)
    return;

  if (!PLAN_2D)
    {
      BOUND_IN_2D = (fftw_complex *)input;
      BOUND_OUT_2D = (fftw_complex *)output;

      fftw_plan_with_nthreads (GLOBAL_THREADS);
      PLAN_2D = fftw_plan_dft_2d ((int)GLOBAL_NY, (int)GLOBAL_NX, BOUND_IN_2D,
                                  BOUND_OUT_2D, GLOBAL_FORWARD, GLOBAL_FLAGS);
    }

  if ((fftw_complex *)input != BOUND_IN_2D
      || (fftw_complex *)output != BOUND_OUT_2D)
    {
      fftw_execute_dft (PLAN_2D, (fftw_complex *)input,
                        (fftw_complex *)output);
      return;
    }

  fftw_execute (PLAN_2D);
}

void
dp_fft2d_execute_inplace (double complex *data)
{
  if (!data || GLOBAL_NY == 0 || GLOBAL_NX == 0)
    return;

  if (!PLAN_2D)
    {
      BOUND_IN_2D = (fftw_complex *)data;
      BOUND_OUT_2D = (fftw_complex *)data;

      fftw_plan_with_nthreads (GLOBAL_THREADS);
      PLAN_2D = fftw_plan_dft_2d ((int)GLOBAL_NY, (int)GLOBAL_NX, BOUND_IN_2D,
                                  BOUND_OUT_2D, GLOBAL_FORWARD, GLOBAL_FLAGS);
    }

  if ((fftw_complex *)data != BOUND_IN_2D)
    {
      fftw_execute_dft (PLAN_2D, (fftw_complex *)data, (fftw_complex *)data);
      return;
    }

  fftw_execute (PLAN_2D);
}
