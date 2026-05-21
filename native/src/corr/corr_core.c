#include "corr/corr_core.h"
#include <string.h>

corr_state_t *corr_create(const float complex *ref, size_t n, size_t dwell,
                          int nthreads) {
  corr_state_t *state = malloc(sizeof(*state));
  if (!state)
    return NULL;

  state->fwd = fft_create(n, -1, nthreads);
  state->inv = fft_create(n, +1, nthreads);
  if (!state->fwd || !state->inv)
    goto fail_plans;

  state->ref_spec = malloc(n * sizeof(*state->ref_spec));
  state->work_fft = malloc(n * sizeof(*state->work_fft));
  state->work_ifft = malloc(n * sizeof(*state->work_ifft));
  state->accum = calloc(n, sizeof(*state->accum));
  if (!state->ref_spec || !state->work_fft || !state->work_ifft ||
      !state->accum)
    goto fail_bufs;

  /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT(ref)). */
  fft_execute_cf32(state->fwd, ref, n, state->ref_spec);
  for (size_t k = 0; k < n; k++)
    state->ref_spec[k] = conjf(state->ref_spec[k]);

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
  fft_destroy(state->fwd);
  fft_destroy(state->inv);
  free(state);
  return NULL;
}

void corr_destroy(corr_state_t *state) {
  if (!state)
    return;
  fft_destroy(state->fwd);
  fft_destroy(state->inv);
  free(state->ref_spec);
  free(state->work_fft);
  free(state->work_ifft);
  free(state->accum);
  free(state);
}

void corr_reset(corr_state_t *state) {
  memset(state->accum, 0, state->n * sizeof(*state->accum));
  state->count = 0;
}

void corr_set_ref(corr_state_t *state, const float complex *ref) {
  fft_execute_cf32(state->fwd, ref, state->n, state->ref_spec);
  for (size_t k = 0; k < state->n; k++)
    state->ref_spec[k] = conjf(state->ref_spec[k]);
  corr_reset(state);
}

size_t corr_execute_max_out(corr_state_t *state) { return state->n; }

size_t corr_execute(corr_state_t *state, const float complex *in, size_t n_in,
                    float complex *out) {
  (void)n_in; /* must equal state->n; caller's responsibility */

  /* Forward FFT of input frame. */
  fft_execute_cf32(state->fwd, in, state->n, state->work_fft);

  /* Frequency-domain multiplication with conjugate reference spectrum.
   * Equivalent to circular cross-correlation in the lag domain. */
  for (size_t k = 0; k < state->n; k++)
    state->work_fft[k] *= state->ref_spec[k];

  /* Inverse FFT — pocketfft is unnormalized (returns N * true_IFFT).
   * Divide by n when accumulating to get the true correlation value. */
  fft_execute_cf32(state->inv, state->work_fft, state->n, state->work_ifft);

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
