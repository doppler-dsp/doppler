#include "fft2d/fft2d_core.h"
#include <string.h>

/* A pocketfft 2-D plan is fixed at ny*nx and writes the whole surface, so
 * it can never be pointed at a smaller buffer.  When a caller's capacity is
 * short, transform into scratch and hand back the prefix that fits.  See
 * fft_core.c for the 1-D twin -- same shape, same lazy allocation: every
 * in-tree caller and the Python binding size from *_max_out() and so never
 * reach this path. */
static double complex *
trunc_buf (fft2d_state_t *state)
{
  if (!state->work_trunc)
    state->work_trunc = (double complex *)dp_xcalloc (
        state->ny * state->nx, sizeof *state->work_trunc);
  return state->work_trunc;
}

fft2d_state_t *
fft2d_create (size_t ny, size_t nx, int sign, int nthreads)
{
  (void)nthreads;
  fft2d_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  state->plan_f64 = pocketfft_plan_2d (ny, nx, sign);
  state->plan_f32 = pocketfft_plan_2d (ny, nx, sign);
  if (!state->plan_f64 || !state->plan_f32)
    {
      pocketfft_destroy_plan (state->plan_f64);
      pocketfft_destroy_plan (state->plan_f32);
      free (state);
      return NULL;
    }
  state->ny         = ny;
  state->nx         = nx;
  state->sign       = sign;
  state->work_trunc = NULL;
  return state;
}

void
fft2d_destroy (fft2d_state_t *state)
{
  if (!state)
    return;
  pocketfft_destroy_plan (state->plan_f64);
  pocketfft_destroy_plan (state->plan_f32);
  free (state->work_trunc);
  free (state);
}

void
fft2d_reset (fft2d_state_t *state)
{
  (void)state; /* plans are immutable after creation */
}

size_t
fft2d_execute_cf64_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_cf64 (fft2d_state_t *state, const double complex *in,
                    size_t n_in, double complex *out, size_t max_out)
{
  (void)n_in;
  const size_t n = state->ny * state->nx;
  if (max_out >= n)
    {
      pocketfft_execute_2d (state->plan_f64, in, out);
      return n;
    }
  double complex *scratch = trunc_buf (state);
  pocketfft_execute_2d (state->plan_f64, in, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft2d_execute_cf32_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_cf32 (fft2d_state_t *state, const float complex *in, size_t n_in,
                    float complex *out, size_t max_out)
{
  (void)n_in;
  const size_t n = state->ny * state->nx;
  if (max_out >= n)
    {
      pocketfft_execute_2d_cf32 (state->plan_f32, in, out);
      return n;
    }
  float complex *scratch = (float complex *)trunc_buf (state);
  pocketfft_execute_2d_cf32 (state->plan_f32, in, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft2d_execute_inplace_cf64_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf64 (fft2d_state_t *state, const double complex *in,
                            size_t n_in, double complex *out, size_t max_out)
{
  /* n_in is documented as ny*nx; clamp so a longer input cannot walk off
   * the end of a correctly sized out. */
  const size_t n    = state->ny * state->nx;
  const size_t n_cp = n_in < n ? n_in : n;
  if (max_out >= n)
    {
      memcpy (out, in, n_cp * sizeof (*out));
      pocketfft_execute_2d (state->plan_f64, out, out);
      return n;
    }
  double complex *scratch = trunc_buf (state);
  memcpy (scratch, in, n_cp * sizeof (*scratch));
  pocketfft_execute_2d (state->plan_f64, scratch, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft2d_execute_inplace_cf32_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf32 (fft2d_state_t *state, const float complex *in,
                            size_t n_in, float complex *out, size_t max_out)
{
  const size_t n    = state->ny * state->nx;
  const size_t n_cp = n_in < n ? n_in : n;
  if (max_out >= n)
    {
      memcpy (out, in, n_cp * sizeof (*out));
      pocketfft_execute_2d_cf32 (state->plan_f32, out, out);
      return n;
    }
  float complex *scratch = (float complex *)trunc_buf (state);
  memcpy (scratch, in, n_cp * sizeof (*scratch));
  pocketfft_execute_2d_cf32 (state->plan_f32, scratch, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}
