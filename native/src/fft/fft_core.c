#include "fft/fft_core.h"
#include <string.h>

/* A pocketfft plan is fixed at state->n and writes every one of those n
 * bins, so it can never be pointed at a buffer smaller than n.  When a
 * caller's capacity is short, transform into scratch and hand back the
 * prefix that fits -- the same answer the sized call would have put in
 * those slots, just fewer of them.  The scratch is sized for CF64 so one
 * allocation serves both plans, and it is allocated on first use: every
 * in-tree caller (and the Python binding, which sizes from *_max_out())
 * passes max_out == n and never touches this path. */
static double complex *
trunc_buf (fft_state_t *state)
{
  if (!state->work_trunc)
    state->work_trunc
        = (double complex *)dp_xcalloc (state->n, sizeof *state->work_trunc);
  return state->work_trunc;
}

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
  state->n          = n;
  state->sign       = sign;
  state->work_trunc = NULL;
  return state;
}

void
fft_destroy (fft_state_t *state)
{
  if (!state)
    return;
  pocketfft_destroy_plan (state->plan_f64);
  pocketfft_destroy_plan (state->plan_f32);
  free (state->work_trunc);
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
                  double complex *out, size_t max_out)
{
  (void)n_in;
  if (max_out >= state->n)
    {
      pocketfft_execute_1d (state->plan_f64, in, out);
      return state->n;
    }
  double complex *scratch = trunc_buf (state);
  pocketfft_execute_1d (state->plan_f64, in, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft_execute_cf32_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_cf32 (fft_state_t *state, const float complex *in, size_t n_in,
                  float complex *out, size_t max_out)
{
  (void)n_in;
  if (max_out >= state->n)
    {
      pocketfft_execute_1d_cf32 (state->plan_f32, in, out);
      return state->n;
    }
  float complex *scratch = (float complex *)trunc_buf (state);
  pocketfft_execute_1d_cf32 (state->plan_f32, in, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft_execute_inplace_cf64_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_inplace_cf64 (fft_state_t *state, const double complex *in,
                          size_t n_in, double complex *out, size_t max_out)
{
  /* Copy in→out so the plan can transform the buffer in place.
   * Avoids a scratch allocation inside pocketfft at the cost of
   * one memcpy per call.  n_in is documented as state->n; clamp so a
   * longer input cannot walk off the end of a correctly sized out. */
  const size_t n = n_in < state->n ? n_in : state->n;
  if (max_out >= state->n)
    {
      memcpy (out, in, n * sizeof (*out));
      pocketfft_execute_1d (state->plan_f64, out, out);
      return state->n;
    }
  double complex *scratch = trunc_buf (state);
  memcpy (scratch, in, n * sizeof (*scratch));
  pocketfft_execute_1d (state->plan_f64, scratch, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft_execute_inplace_cf32_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_inplace_cf32 (fft_state_t *state, const float complex *in,
                          size_t n_in, float complex *out, size_t max_out)
{
  const size_t n = n_in < state->n ? n_in : state->n;
  if (max_out >= state->n)
    {
      memcpy (out, in, n * sizeof (*out));
      pocketfft_execute_1d_cf32 (state->plan_f32, out, out);
      return state->n;
    }
  float complex *scratch = (float complex *)trunc_buf (state);
  memcpy (scratch, in, n * sizeof (*scratch));
  pocketfft_execute_1d_cf32 (state->plan_f32, scratch, scratch);
  memcpy (out, scratch, max_out * sizeof (*out));
  return max_out;
}

size_t
fft_execute_ci16_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_ci16 (fft_state_t *state, const int16_t *in, size_t n_in,
                  float complex *out)
{
  (void)n_in;
  pocketfft_execute_1d_ci16 (state->plan_f32, in, out);
  return state->n;
}

size_t
fft_execute_ci8_max_out (fft_state_t *state)
{
  return state->n;
}

size_t
fft_execute_ci8 (fft_state_t *state, const int8_t *in, size_t n_in,
                 float complex *out)
{
  (void)n_in;
  pocketfft_execute_1d_ci8 (state->plan_f32, in, out);
  return state->n;
}
