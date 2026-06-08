#include "fft/fft_core.h"
#include <string.h>

fft_state_t *
fft_create (size_t n, int sign, int nthreads)
{
  (void)nthreads;
  fft_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  state->plan_f64 = pocketfft_plan_1d (n, sign);
  state->plan_f32 = pocketfft_plan_1d (n, sign);
  if (!state->plan_f64 || !state->plan_f32)
    {
      pocketfft_destroy_plan (state->plan_f64);
      pocketfft_destroy_plan (state->plan_f32);
      free (state);
      return NULL;
    }
  state->n    = n;
  state->sign = sign;
  return state;
}

void
fft_destroy (fft_state_t *state)
{
  if (!state)
    return;
  pocketfft_destroy_plan (state->plan_f64);
  pocketfft_destroy_plan (state->plan_f32);
  free (state);
}

void
fft_reset (fft_state_t *state)
{
  (void)state; /* plans are immutable after creation */
}

size_t
fft_execute_cf64_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_cf64 (fft_state_t *state, const double complex *in, size_t n_in,
                  double complex *out)
{
  (void)n_in;
  pocketfft_execute_1d (state->plan_f64, in, out);
  return state->n;
}

size_t
fft_execute_cf32_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_cf32 (fft_state_t *state, const float complex *in, size_t n_in,
                  float complex *out)
{
  (void)n_in;
  pocketfft_execute_1d_cf32 (state->plan_f32, in, out);
  return state->n;
}

size_t
fft_execute_inplace_cf64_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_inplace_cf64 (fft_state_t *state, const double complex *in,
                          size_t n_in, double complex *out)
{
  /* Copy in→out so the plan can transform the buffer in place.
   * Avoids a scratch allocation inside pocketfft at the cost of
   * one memcpy per call. */
  memcpy (out, in, n_in * sizeof (*out));
  pocketfft_execute_1d (state->plan_f64, out, out);
  return state->n;
}

size_t
fft_execute_inplace_cf32_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_inplace_cf32 (fft_state_t *state, const float complex *in,
                          size_t n_in, float complex *out)
{
  memcpy (out, in, n_in * sizeof (*out));
  pocketfft_execute_1d_cf32 (state->plan_f32, out, out);
  return state->n;
}
