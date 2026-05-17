#include "fft2d/fft2d_core.h"
#include <string.h>

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
  state->ny = ny;
  state->nx = nx;
  state->sign = sign;
  return state;
}

void
fft2d_destroy (fft2d_state_t *state)
{
  if (!state)
    return;
  pocketfft_destroy_plan (state->plan_f64);
  pocketfft_destroy_plan (state->plan_f32);
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
                    size_t n_in, double complex *out)
{
  (void)n_in;
  pocketfft_execute_2d (state->plan_f64, in, out);
  return state->ny * state->nx;
}

size_t
fft2d_execute_cf32_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_cf32 (fft2d_state_t *state, const float complex *in, size_t n_in,
                    float complex *out)
{
  (void)n_in;
  pocketfft_execute_2d_cf32 (state->plan_f32, in, out);
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf64_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf64 (fft2d_state_t *state, const double complex *in,
                            size_t n_in, double complex *out)
{
  memcpy (out, in, n_in * sizeof (*out));
  pocketfft_execute_2d (state->plan_f64, out, out);
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf32_max_out (fft2d_state_t *state)
{
  return state->ny * state->nx;
}

size_t
fft2d_execute_inplace_cf32 (fft2d_state_t *state, const float complex *in,
                            size_t n_in, float complex *out)
{
  memcpy (out, in, n_in * sizeof (*out));
  pocketfft_execute_2d_cf32 (state->plan_f32, out, out);
  return state->ny * state->nx;
}
