/**
 * @file accumulator.c
 * @brief General-purpose scalar accumulator — f32 and cf64.
 *
 * All inner loops are written to be auto-vectorisation-friendly:
 *   - No aliasing between x and h (restrict-qualified in the header)
 *   - No loop-carried dependency other than the running sum
 *   - Simple stride-1 access pattern
 *
 * The 2-D variants flatten to 1-D: a row-major C array of [rows][cols]
 * is contiguous in memory, so madd2d(acc, x, h, r, c) is exactly
 * equivalent to madd(acc, x, h, r*c).
 */

#include "dp/accumulator.h"

#include <stdlib.h>

/* =========================================================================
 * Internal structs
 * ========================================================================= */

struct dp_acc_f32
{
  float val;
};

struct dp_acc_cf64
{
  double _Complex val;
};

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

dp_acc_f32_t *
dp_acc_f32_create (void)
{
  return (dp_acc_f32_t *)calloc (1, sizeof (dp_acc_f32_t));
}

dp_acc_cf64_t *
dp_acc_cf64_create (void)
{
  return (dp_acc_cf64_t *)calloc (1, sizeof (dp_acc_cf64_t));
}

void
dp_acc_f32_destroy (dp_acc_f32_t *acc)
{
  free (acc);
}

void
dp_acc_cf64_destroy (dp_acc_cf64_t *acc)
{
  free (acc);
}

/* =========================================================================
 * Reset / dump
 * ========================================================================= */

void
dp_acc_f32_reset (dp_acc_f32_t *acc)
{
  acc->val = 0.0f;
}

void
dp_acc_cf64_reset (dp_acc_cf64_t *acc)
{
  acc->val = 0.0;
}

float
dp_acc_f32_dump (dp_acc_f32_t *acc)
{
  float v = acc->val;
  acc->val = 0.0f;
  return v;
}

double _Complex
dp_acc_cf64_dump (dp_acc_cf64_t *acc)
{
  double _Complex v = acc->val;
  acc->val = 0.0;
  return v;
}

/* =========================================================================
 * Read
 * ========================================================================= */

float
dp_acc_f32_get (const dp_acc_f32_t *acc)
{
  return acc->val;
}

double _Complex
dp_acc_cf64_get (const dp_acc_cf64_t *acc)
{
  return acc->val;
}

/* =========================================================================
 * Scalar push
 * ========================================================================= */

void
dp_acc_f32_push (dp_acc_f32_t *acc, float x)
{
  acc->val += x;
}

void
dp_acc_cf64_push (dp_acc_cf64_t *acc, double _Complex x)
{
  acc->val += x;
}

/* =========================================================================
 * 1-D array operations
 * ========================================================================= */

void
dp_acc_f32_add (dp_acc_f32_t *acc, const float *x, size_t n)
{
  float sum = 0.0f;
  for (size_t k = 0; k < n; k++)
    sum += x[k];
  acc->val += sum;
}

void
dp_acc_cf64_add (dp_acc_cf64_t *acc, const double _Complex *x, size_t n)
{
  double _Complex sum = 0.0;
  for (size_t k = 0; k < n; k++)
    sum += x[k];
  acc->val += sum;
}

void
dp_acc_f32_madd (dp_acc_f32_t *acc, const float *restrict x,
                 const float *restrict h, size_t n)
{
  float sum = 0.0f;
  for (size_t k = 0; k < n; k++)
    sum += x[k] * h[k];
  acc->val += sum;
}

void
dp_acc_cf64_madd (dp_acc_cf64_t *acc, const double _Complex *restrict x,
                  const float *restrict h, size_t n)
{
  double _Complex sum = 0.0;
  for (size_t k = 0; k < n; k++)
    sum += x[k] * (double)h[k];
  acc->val += sum;
}

/* =========================================================================
 * 2-D array operations — delegate to 1-D (row-major contiguous)
 * ========================================================================= */

void
dp_acc_f32_add2d (dp_acc_f32_t *acc, const float *x, size_t rows, size_t cols)
{
  dp_acc_f32_add (acc, x, rows * cols);
}

void
dp_acc_cf64_add2d (dp_acc_cf64_t *acc, const double _Complex *x, size_t rows,
                   size_t cols)
{
  dp_acc_cf64_add (acc, x, rows * cols);
}

void
dp_acc_f32_madd2d (dp_acc_f32_t *acc, const float *restrict x,
                   const float *restrict h, size_t rows, size_t cols)
{
  dp_acc_f32_madd (acc, x, h, rows * cols);
}

void
dp_acc_cf64_madd2d (dp_acc_cf64_t *acc, const double _Complex *restrict x,
                    const float *restrict h, size_t rows, size_t cols)
{
  dp_acc_cf64_madd (acc, x, h, rows * cols);
}
