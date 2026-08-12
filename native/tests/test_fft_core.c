#include "dp_test.h"
#include "fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOL64 1e-9
#define TOL32 5e-4f

static inline int
ceq64 (double complex a, double complex b)
{
  return fabs (creal (a) - creal (b)) < TOL64
         && fabs (cimag (a) - cimag (b)) < TOL64;
}

static inline int
ceq32 (float complex a, float complex b)
{
  return fabsf (crealf (a) - crealf (b)) < TOL32
         && fabsf (cimagf (a) - cimagf (b)) < TOL32;
}

int
main (void)
{
  const size_t N = 16;

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    fft_state_t *obj = fft_create (N, -1, 1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->n == N);
    DP_CHECK (obj->sign == -1);
    DP_CHECK (obj->plan_f64 != NULL);
    DP_CHECK (obj->plan_f32 != NULL);
    fft_reset (obj); /* no-op; must not crash */
    fft_destroy (obj);
    fft_destroy (NULL); /* must not crash */
  }

  /* ── CF64 forward/inverse round-trip ────────────────────────────── */
  {
    fft_state_t *fwd = fft_create (N, -1, 1);
    fft_state_t *inv = fft_create (N, +1, 1);
    DP_CHECK (fwd != NULL && inv != NULL);

    double complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i + 1) + 0.0 * I;

    fft_execute_cf64 (fwd, in, N, spec, N);
    fft_execute_cf64 (inv, spec, N, rec, N);

    /* IDFT without normalisation: rec[k] == N * in[k] */
    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq64 (rec[i], (double)N * in[i]));

    fft_destroy (fwd);
    fft_destroy (inv);
  }

  /* ── CF32 forward/inverse round-trip ────────────────────────────── */
  {
    fft_state_t *fwd = fft_create (N, -1, 1);
    fft_state_t *inv = fft_create (N, +1, 1);
    DP_CHECK (fwd != NULL && inv != NULL);

    float complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)(i + 1) + 0.0f * I;

    fft_execute_cf32 (fwd, in, N, spec, N);
    fft_execute_cf32 (inv, spec, N, rec, N);

    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq32 (rec[i], (float)N * in[i]));

    fft_destroy (fwd);
    fft_destroy (inv);
  }

  /* ── DC tone: only bin 0 is non-zero ────────────────────────────── */
  {
    fft_state_t   *obj = fft_create (N, -1, 1);
    double complex in[16], out[16];
    for (size_t i = 0; i < N; i++)
      in[i] = 1.0 + 0.0 * I;
    fft_execute_cf64 (obj, in, N, out, N);

    /* bin 0 = N; all others = 0 */
    DP_CHECK (ceq64 (out[0], (double)N + 0.0 * I));
    for (size_t k = 1; k < N; k++)
      DP_CHECK (ceq64 (out[k], 0.0 + 0.0 * I));
    fft_destroy (obj);
  }

  /* ── inplace CF64 matches out-of-place ──────────────────────────── */
  {
    fft_state_t   *obj = fft_create (N, -1, 1);
    double complex in[16], out_oop[16], out_ip[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)i - 7.5 + (double)i * I;

    fft_execute_cf64 (obj, in, N, out_oop, N);
    fft_execute_inplace_cf64 (obj, in, N, out_ip, N);

    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (out_ip[k], out_oop[k]));
    fft_destroy (obj);
  }

  /* ── inplace CF32 matches out-of-place ──────────────────────────── */
  {
    fft_state_t  *obj = fft_create (N, -1, 1);
    float complex in[16], out_oop[16], out_ip[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)i - 7.5f + (float)i * I;

    fft_execute_cf32 (obj, in, N, out_oop, N);
    fft_execute_inplace_cf32 (obj, in, N, out_ip, N);

    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq32 (out_ip[k], out_oop[k]));
    fft_destroy (obj);
  }

  /* ── max_out always returns n ────────────────────────────────────── */
  {
    fft_state_t *obj = fft_create (N, -1, 1);
    DP_CHECK (fft_execute_cf64_max_out (obj) == N);
    DP_CHECK (fft_execute_cf32_max_out (obj) == N);
    DP_CHECK (fft_execute_inplace_cf64_max_out (obj) == N);
    DP_CHECK (fft_execute_inplace_cf32_max_out (obj) == N);
    fft_destroy (obj);
  }

  /* ── short out: prefix of the full transform, nothing past max_out ──
   * The plan is fixed at n and writes all n bins, so a short buffer is
   * served from scratch and truncated -- see fft_core.c.  The canary
   * proves the tail is never touched. */
  {
    fft_state_t   *obj = fft_create (N, -1, 1);
    double complex in[16], full[16], part[16];
    float complex  in32[16], full32[16], part32[16];
    for (size_t i = 0; i < N; i++)
      {
        in[i]   = (double)(i % 7) - 3.0 + (double)(i % 5) * I;
        in32[i] = (float)(i % 7) - 3.0f + (float)(i % 5) * I;
      }
    const double complex CANARY   = -12345.0 - 6789.0 * I;
    const float complex  CANARY32 = -12345.0f - 6789.0f * I;
    const size_t         K        = 4;

    fft_execute_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft_execute_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft_execute_inplace_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft_execute_inplace_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft_execute_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (fft_execute_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    fft_execute_inplace_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (fft_execute_inplace_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    /* max_out == 0 writes nothing at all. */
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft_execute_cf64 (obj, in, N, part, 0) == 0);
    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft_destroy (obj);
  }

  DP_TEST_END ("test_fft_core");
}
