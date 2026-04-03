/**
 * @file ddc.c
 * @brief Digital Down-Converter: NCO mix + optional DPMFS decimation.
 *
 * The execute hot path:
 *   1. dp_nco_execute_cf32  — fill mix_buf with e^{j·2π·f_n·t}
 *   2. element-wise CF32 multiply — mix_buf[i] = in[i] * mix_buf[i]
 *   3. dp_resamp_dpmfs_execute (or memcpy when no resampler)
 *
 * mix_buf is pre-allocated to num_in samples at creation time; no heap
 * allocation occurs during processing.
 */

#include "dp/ddc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dp/nco.h"
#include "dp/resamp_dpmfs.h"

/* ------------------------------------------------------------------
 * Built-in default DPMFS coefficients  M=3, N=19
 *
 * Designed with:
 *   kaiser_prototype(atten=60.0, passband=0.4, stopband=0.6) → fit_dpmfs(M=3)
 *
 * Frequencies normalised to the OUTPUT sample rate (fs_out/2 = 1):
 *   passband  ≤ 0.4 × (fs_out/2)   →  0.4 × rate/2 × fs_in  (input units)
 *   stopband  ≥ 0.6 × (fs_out/2)   →  0.6 × rate/2 × fs_in  (input units)
 *   rejection ≥ 60 dB
 *
 * The cutoffs scale with rate, so the same bank is valid for any
 * decimation factor.  Fit residual rms ≈ 8×10⁻⁵.
 *
 * Arrays are stored row-major [m * N + k] as required by
 * dp_resamp_dpmfs_create.
 * ------------------------------------------------------------------ */

#define DDC_DEF_M 3
#define DDC_DEF_N 19

/* clang-format off */

static const float s_c0[(DDC_DEF_M + 1) * DDC_DEF_N] = {
    /* m=0 */
    -1.16934330e-04f,  4.73825610e-04f, -1.19166356e-03f,  2.45564547e-03f,
    -4.52230871e-03f,  7.80567247e-03f, -1.31601924e-02f,  2.30139550e-02f,
    -4.93952595e-02f,  9.94322419e-01f,  5.66108637e-02f, -2.52397228e-02f,
     1.42384656e-02f, -8.43974855e-03f,  4.92028147e-03f, -2.70472351e-03f,
     1.33995316e-03f, -5.53859456e-04f,  8.66927803e-05f,
    /* m=1 */
     8.80004896e-04f, -3.92260263e-03f,  1.01457266e-02f, -2.12026592e-02f,
     3.93599384e-02f, -6.82006106e-02f,  1.14903398e-01f, -1.99056119e-01f,
     4.16760147e-01f,  1.05088279e-01f, -5.50753713e-01f,  2.40929484e-01f,
    -1.35243163e-01f,  8.01716074e-02f, -4.68757749e-02f,  2.59068869e-02f,
    -1.29461270e-02f,  5.43356221e-03f, -7.75158580e-04f,
    /* m=2 */
     1.69086654e-03f, -3.81081994e-03f,  7.18046678e-03f, -1.22501506e-02f,
     1.98423490e-02f, -3.19187157e-02f,  5.43430112e-02f, -1.10441282e-01f,
     3.21942121e-01f, -4.78356242e-01f,  2.87550747e-01f, -7.96665773e-02f,
     3.81802619e-02f, -2.25741938e-02f,  1.43792443e-02f, -9.17060301e-03f,
     5.57000516e-03f, -3.06685385e-03f,  1.64501579e-03f,
    /* m=3 */
    -9.08786838e-04f,  2.71153264e-03f, -6.01121085e-03f,  1.14890188e-02f,
    -2.01407541e-02f,  3.37455571e-02f, -5.63596413e-02f,  9.94417891e-02f,
    -1.28961951e-01f,  7.82487020e-02f,  4.00897115e-03f, -3.03890835e-02f,
     2.10647210e-02f, -1.26649253e-02f,  6.83699781e-03f, -3.15257278e-03f,
     1.05630129e-03f, -5.08069206e-05f, -9.86029860e-04f,
};

static const float s_c1[(DDC_DEF_M + 1) * DDC_DEF_N] = {
    /* m=0 */
     1.53192726e-03f, -4.51925257e-03f,  1.00700520e-02f, -1.94183458e-02f,
     3.43948975e-02f, -5.83376139e-02f,  9.93433967e-02f, -1.86422527e-01f,
     5.59528589e-01f,  7.00086713e-01f, -2.03125641e-01f,  1.05938062e-01f,
    -6.19319230e-02f,  3.66015099e-02f, -2.08103228e-02f,  1.09243952e-02f,
    -5.00694150e-03f,  1.77628070e-03f,  0.00000000e+00f,
    /* m=1 */
     1.59764744e-03f, -3.72488005e-03f,  7.29278848e-03f, -1.29298903e-02f,
     2.17107106e-02f, -3.60014960e-02f,  6.26852065e-02f, -1.30676478e-01f,
     6.75922215e-01f, -6.07608557e-01f,  2.22359207e-02f,  1.52034941e-03f,
    -2.55652890e-03f,  1.15510228e-03f,  4.18133932e-05f, -6.68466848e-04f,
     8.17867694e-04f, -6.76259166e-04f,  0.00000000e+00f,
    /* m=2 */
    -2.04953435e-03f,  6.53886562e-03f, -1.49536403e-02f,  2.91341916e-02f,
    -5.16984127e-02f,  8.70598853e-02f, -1.44155428e-01f,  2.35224113e-01f,
    -1.27136454e-01f, -1.84056893e-01f,  2.58344233e-01f, -1.47669032e-01f,
     8.82083699e-02f, -5.22393323e-02f,  2.94414070e-02f, -1.51442783e-02f,
     6.66055363e-03f, -2.12617568e-03f,  0.00000000e+00f,
    /* m=3 */
    -6.07282971e-04f,  5.26328688e-04f,  1.07739379e-05f, -1.23402057e-03f,
     3.26614734e-03f, -5.67266904e-03f,  4.88680368e-03f,  3.26315686e-02f,
    -1.13870047e-01f,  1.47821829e-01f, -1.02285333e-01f,  5.41415736e-02f,
    -3.19643356e-02f,  1.92881506e-02f, -1.13163898e-02f,  6.20048959e-03f,
    -3.01633636e-03f,  1.17914262e-03f,  0.00000000e+00f,
};

/* clang-format on */

/* ------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------ */

struct dp_ddc
{
  dp_nco_t *nco;
  dp_resamp_dpmfs_t *resampler; /* NULL → bypass                  */
  dp_cf32_t *mix_buf;           /* pre-allocated; size == num_in  */
  size_t num_in;                /* fixed input block size          */
  size_t max_out;               /* upper bound on output per call  */
  size_t nout;                  /* actual output count (last exec) */
};

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

static size_t
compute_max_out (size_t num_in, dp_resamp_dpmfs_t *r)
{
  if (!r)
    return num_in;
  double rate = dp_resamp_dpmfs_rate (r);
  return (size_t)ceil (num_in * rate) + 4;
}

static dp_ddc_t *
ddc_alloc (float norm_freq, size_t num_in, dp_resamp_dpmfs_t *r)
{
  dp_ddc_t *ddc = calloc (1, sizeof *ddc);
  if (!ddc)
    goto fail_alloc;

  ddc->nco = dp_nco_create (norm_freq);
  if (!ddc->nco)
    goto fail_nco;

  ddc->mix_buf = malloc (num_in * sizeof *ddc->mix_buf);
  if (!ddc->mix_buf)
    goto fail_buf;

  ddc->resampler = r;
  ddc->num_in = num_in;
  ddc->max_out = compute_max_out (num_in, r);
  return ddc;

fail_buf:
  dp_nco_destroy (ddc->nco);
fail_nco:
  free (ddc);
fail_alloc:
  dp_resamp_dpmfs_destroy (r);
  return NULL;
}

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

dp_ddc_t *
dp_ddc_create (float norm_freq, size_t num_in, double rate)
{
  dp_resamp_dpmfs_t *r = NULL;

  /* Bypass resampler when rate ≈ 1 */
  if (fabs (rate - 1.0) >= 1e-6)
    {
      r = dp_resamp_dpmfs_create (DDC_DEF_M, DDC_DEF_N, s_c0, s_c1, rate);
      if (!r)
        return NULL;
    }

  return ddc_alloc (norm_freq, num_in, r);
}

dp_ddc_t *
dp_ddc_create_custom (float norm_freq, size_t num_in, dp_resamp_dpmfs_t *r)
{
  return ddc_alloc (norm_freq, num_in, r);
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
 * Properties
 * ------------------------------------------------------------------ */

size_t
dp_ddc_max_out (const dp_ddc_t *ddc)
{
  return ddc->max_out;
}

size_t
dp_ddc_nout (const dp_ddc_t *ddc)
{
  return ddc->nout;
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

  /* Clamp to pre-allocated buffer capacity */
  if (num_in > ddc->num_in)
    num_in = ddc->num_in;

  /* Step 1: generate NCO phasors into mix_buf */
  dp_nco_execute_cf32 (ddc->nco, ddc->mix_buf, num_in);

  /* Step 2: in-place complex multiply  mix_buf[i] = in[i] * mix_buf[i]
   *
   *   (xi + j·xq)(ni + j·nq) = (xi·ni − xq·nq) + j(xi·nq + xq·ni)
   */
  for (size_t i = 0; i < num_in; i++)
    {
      float xi = in[i].i;
      float xq = in[i].q;
      float ni = ddc->mix_buf[i].i;
      float nq = ddc->mix_buf[i].q;
      ddc->mix_buf[i].i = xi * ni - xq * nq;
      ddc->mix_buf[i].q = xi * nq + xq * ni;
    }

  /* Step 3: resample or pass through */
  size_t n;
  if (ddc->resampler)
    n = dp_resamp_dpmfs_execute (ddc->resampler, ddc->mix_buf, num_in, out,
                                 max_out);
  else
    {
      n = num_in < max_out ? num_in : max_out;
      memcpy (out, ddc->mix_buf, n * sizeof *out);
    }

  ddc->nout = n;
  return n;
}
