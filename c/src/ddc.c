/**
 * @file ddc.c
 * @brief Digital Down-Converter: NCO mix + optional DPMFS decimation.
 *
 * The execute hot path:
 *   1. dp_nco_execute_cf32  — fill mix_buf with e^{j·2π·f_n·t}
 *   2. element-wise CF32 multiply — mix_buf[i] = in[i] * mix_buf[i]
 *   3. dp_resamp_dpmfs_execute (or memcpy when no resampler)
 *
 * mix_buf is a lazily allocated internal scratch buffer; it grows via
 * realloc to match the largest num_in seen so far and is never shrunk.
 */

#include "dp/ddc.h"

#include <stdlib.h>
#include <string.h>

#include "dp/nco.h"
#include "dp/resamp_dpmfs.h"

/* ------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------ */

struct dp_ddc
{
  dp_nco_t *nco;
  dp_resamp_dpmfs_t *resampler; /* NULL → bypass */
  dp_cf32_t *mix_buf;           /* scratch: NCO output then mixed IQ */
  size_t mix_cap;               /* capacity of mix_buf in samples    */
};

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

dp_ddc_t *
dp_ddc_create (float norm_freq, dp_resamp_dpmfs_t *r)
{
  dp_ddc_t *ddc = calloc (1, sizeof *ddc);
  if (!ddc)
    {
      dp_resamp_dpmfs_destroy (r);
      return NULL;
    }

  ddc->nco = dp_nco_create (norm_freq);
  if (!ddc->nco)
    {
      free (ddc);
      dp_resamp_dpmfs_destroy (r);
      return NULL;
    }

  ddc->resampler = r; /* take ownership */
  return ddc;
}

void
dp_ddc_destroy (dp_ddc_t *ddc)
{
  if (!ddc)
    return;
  dp_nco_destroy (ddc->nco);
  dp_resamp_dpmfs_destroy (ddc->resampler);
  free (ddc->mix_buf);
  free (ddc);
}

/* ------------------------------------------------------------------
 * Control
 * ------------------------------------------------------------------ */

void
dp_ddc_set_freq (dp_ddc_t *ddc, float norm_freq)
{
  dp_nco_set_freq (ddc->nco, norm_freq);
}

float
dp_ddc_get_freq (const dp_ddc_t *ddc)
{
  return dp_nco_get_freq (ddc->nco);
}

void
dp_ddc_reset (dp_ddc_t *ddc)
{
  dp_nco_reset (ddc->nco);
  if (ddc->resampler)
    dp_resamp_dpmfs_reset (ddc->resampler);
}

/* ------------------------------------------------------------------
 * Processing
 * ------------------------------------------------------------------ */

size_t
dp_ddc_execute (dp_ddc_t *ddc, const dp_cf32_t *in, size_t num_in,
                dp_cf32_t *out, size_t max_out)
{
  if (num_in == 0)
    return 0;

  /* Grow mix buffer on demand */
  if (num_in > ddc->mix_cap)
    {
      dp_cf32_t *p = realloc (ddc->mix_buf, num_in * sizeof *p);
      if (!p)
        return 0;
      ddc->mix_buf = p;
      ddc->mix_cap = num_in;
    }

  /* Step 1: generate NCO phasors into mix_buf */
  dp_nco_execute_cf32 (ddc->nco, ddc->mix_buf, num_in);

  /* Step 2: in-place complex multiply  mix_buf[i] = in[i] * mix_buf[i]
   *
   *   (xi + j·xq)(ni + j·nq) = (xi·ni − xq·nq) + j(xi·nq + xq·ni)
   */
  for (size_t i = 0; i < num_in; i++)
    {
      float xi = in[i].i, xq = in[i].q;
      float ni = ddc->mix_buf[i].i, nq = ddc->mix_buf[i].q;
      ddc->mix_buf[i].i = xi * ni - xq * nq;
      ddc->mix_buf[i].q = xi * nq + xq * ni;
    }

  /* Step 3: resample or pass through */
  if (ddc->resampler)
    return dp_resamp_dpmfs_execute (ddc->resampler, ddc->mix_buf, num_in, out,
                                    max_out);

  size_t n = num_in < max_out ? num_in : max_out;
  memcpy (out, ddc->mix_buf, n * sizeof *out);
  return n;
}
