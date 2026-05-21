#include "corr2d/corr2d_core.h"
#include <string.h>

corr2d_state_t *corr2d_create(const float complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads) {
  corr2d_state_t *state = malloc(sizeof(*state));
  if (!state)
    return NULL;

  size_t n = ny * nx;

  state->fwd = fft2d_create(ny, nx, -1, nthreads);
  state->inv = fft2d_create(ny, nx, +1, nthreads);
  if (!state->fwd || !state->inv)
    goto fail_plans;

  state->ref_spec = malloc(n * sizeof(*state->ref_spec));
  state->work_fft = malloc(n * sizeof(*state->work_fft));
  state->work_ifft = malloc(n * sizeof(*state->work_ifft));
  state->accum = calloc(n, sizeof(*state->accum));
  if (!state->ref_spec || !state->work_fft || !state->work_ifft ||
      !state->accum)
    goto fail_bufs;

  /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT2(ref)). */
  fft2d_execute_cf32(state->fwd, ref, n, state->ref_spec);
  for (size_t k = 0; k < n; k++)
    state->ref_spec[k] = conjf(state->ref_spec[k]);

  state->ny = ny;
  state->nx = nx;
  state->n = n;
  state->dwell = dwell;
  state->count = 0;
  return state;

fail_bufs:
  free(state->ref_spec);
  free(state->work_fft);
  free(state->work_ifft);
  free(state->accum);
fail_plans:
  fft2d_destroy(state->fwd);
  fft2d_destroy(state->inv);
  free(state);
  return NULL;
}

void corr2d_destroy(corr2d_state_t *state) {
  if (!state)
    return;
  fft2d_destroy(state->fwd);
  fft2d_destroy(state->inv);
  free(state->ref_spec);
  free(state->work_fft);
  free(state->work_ifft);
  free(state->accum);
  free(state);
}

void corr2d_reset(corr2d_state_t *state) {
  memset(state->accum, 0, state->n * sizeof(*state->accum));
  state->count = 0;
}

void corr2d_set_ref(corr2d_state_t *state, const float complex *ref) {
  fft2d_execute_cf32(state->fwd, ref, state->n, state->ref_spec);
  for (size_t k = 0; k < state->n; k++)
    state->ref_spec[k] = conjf(state->ref_spec[k]);
  corr2d_reset(state);
}

size_t corr2d_execute_max_out(corr2d_state_t *state) { return state->n; }

size_t corr2d_execute(corr2d_state_t *state, const float complex *in,
                      size_t n_in, float complex *out) {
  (void)n_in;

  fft2d_execute_cf32(state->fwd, in, state->n, state->work_fft);

  for (size_t k = 0; k < state->n; k++)
    state->work_fft[k] *= state->ref_spec[k];

  fft2d_execute_cf32(state->inv, state->work_fft, state->n, state->work_ifft);

  const float inv_n = 1.0f / (float)state->n;
  for (size_t k = 0; k < state->n; k++)
    state->accum[k] += state->work_ifft[k] * inv_n;

  if (++state->count == state->dwell) {
    memcpy(out, state->accum, state->n * sizeof(*out));
    memset(state->accum, 0, state->n * sizeof(*state->accum));
    state->count = 0;
    return state->n;
  }
  return 0;
}
