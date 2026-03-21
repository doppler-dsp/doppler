// MIT-licensed backend using pocketfft (BSD).
// Drop-in replacement for fft.c (FFTW backend).

#include "dp/fft.h"
#include "dp/pocketfft.h"
#include <complex.h>
#include <stddef.h>
#include <stdlib.h>

/* ------------------------------------------------------------
 * Global state (mirrors FFTW backend)
 * ------------------------------------------------------------ */

static size_t GLOBAL_N = 0;
static size_t GLOBAL_NY = 0;
static size_t GLOBAL_NX = 0;

static int GLOBAL_FORWARD = +1;
static int GLOBAL_THREADS = 1;

/* Bound user buffers */
static double complex *BOUND_IN_1D = NULL;
static double complex *BOUND_OUT_1D = NULL;
static double complex *BOUND_IN_2D = NULL;
static double complex *BOUND_OUT_2D = NULL;

/* pocketfft plans */
static pocketfft_plan *PLAN_1D = NULL;
static pocketfft_plan *PLAN_2D = NULL;

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static inline int
normalize_sign (int sign)
{
  return (sign >= 0) ? +1 : -1;
}

/* ------------------------------------------------------------
 * Global setup
 * ------------------------------------------------------------ */

void
fft_global_setup (const size_t *shape, size_t ndim, int sign, int nthreads,
                  const char *planner, const char *wisdom_path)
{
  (void)planner;
  (void)wisdom_path;

  GLOBAL_FORWARD = normalize_sign (sign);
  GLOBAL_THREADS = (nthreads > 0) ? nthreads : 1;

  if (PLAN_1D)
    {
      pocketfft_destroy_plan (PLAN_1D);
      PLAN_1D = NULL;
    }
  if (PLAN_2D)
    {
      pocketfft_destroy_plan (PLAN_2D);
      PLAN_2D = NULL;
    }

  BOUND_IN_1D = BOUND_OUT_1D = NULL;
  BOUND_IN_2D = BOUND_OUT_2D = NULL;

  if (ndim == 1)
    {
      GLOBAL_N = shape[0];
      GLOBAL_NY = GLOBAL_NX = 0;
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
 * 1D execute
 * ------------------------------------------------------------ */

void
fft1d_execute (const double complex *input, double complex *output)
{
  if (!input || !output || GLOBAL_N == 0)
    return;

  if (!PLAN_1D)
    {
      BOUND_IN_1D = (double complex *)input;
      BOUND_OUT_1D = (double complex *)output;
      PLAN_1D = pocketfft_plan_1d (GLOBAL_N, GLOBAL_FORWARD);
    }

  if (input != BOUND_IN_1D || output != BOUND_OUT_1D)
    return;

  pocketfft_execute_1d (PLAN_1D, BOUND_IN_1D, BOUND_OUT_1D);
}

void
fft1d_execute_inplace (double complex *data)
{
  if (!data || GLOBAL_N == 0)
    return;

  if (!PLAN_1D)
    {
      BOUND_IN_1D = BOUND_OUT_1D = data;
      PLAN_1D = pocketfft_plan_1d (GLOBAL_N, GLOBAL_FORWARD);
    }

  if (data != BOUND_IN_1D)
    return;

  pocketfft_execute_1d (PLAN_1D, data, data);
}

/* ------------------------------------------------------------
 * 2D execute
 * ------------------------------------------------------------ */

void
fft2d_execute (const double complex *input, double complex *output)
{
  if (!input || !output || GLOBAL_NY == 0 || GLOBAL_NX == 0)
    return;

  if (!PLAN_2D)
    {
      BOUND_IN_2D = (double complex *)input;
      BOUND_OUT_2D = (double complex *)output;
      PLAN_2D = pocketfft_plan_2d (GLOBAL_NY, GLOBAL_NX, GLOBAL_FORWARD);
    }

  if (input != BOUND_IN_2D || output != BOUND_OUT_2D)
    return;

  pocketfft_execute_2d (PLAN_2D, BOUND_IN_2D, BOUND_OUT_2D);
}

void
fft2d_execute_inplace (double complex *data)
{
  if (!data || GLOBAL_NY == 0 || GLOBAL_NX == 0)
    return;

  if (!PLAN_2D)
    {
      BOUND_IN_2D = BOUND_OUT_2D = data;
      PLAN_2D = pocketfft_plan_2d (GLOBAL_NY, GLOBAL_NX, GLOBAL_FORWARD);
    }

  if (data != BOUND_IN_2D)
    return;

  pocketfft_execute_2d (PLAN_2D, data, data);
}
