#include "doppler/dp_complex.h"
#include "doppler/fft/fft_core.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOL64 1e-9
#define TOL32 5e-4f

static inline int
ceq64 (double _Complex a, double _Complex b)
{
  return fabs (creal (a) - creal (b)) < TOL64
         && fabs (cimag (a) - cimag (b)) < TOL64;
}

static inline int
ceq32 (float _Complex a, float _Complex b)
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
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->n == N);
    DP_CHECK (obj->sign == -1);
    DP_CHECK (obj->plan_f64 != NULL);
    DP_CHECK (obj->plan_f32 != NULL);
    dp_fft_reset (obj); /* no-op; must not crash */
    dp_fft_destroy (obj);
    dp_fft_destroy (NULL); /* must not crash */
  }

  /* ── CF64 forward/inverse round-trip ────────────────────────────── */
  {
    dp_fft_state_t *fwd = dp_fft_create (N, -1, 1);
    dp_fft_state_t *inv = dp_fft_create (N, +1, 1);
    DP_CHECK (fwd != NULL && inv != NULL);

    double _Complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i + 1) + 0.0 * I;

    dp_fft_execute_cf64 (fwd, in, N, spec, N);
    dp_fft_execute_cf64 (inv, spec, N, rec, N);

    /* IDFT without normalisation: rec[k] == N * in[k] */
    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq64 (rec[i], (double)N * in[i]));

    dp_fft_destroy (fwd);
    dp_fft_destroy (inv);
  }

  /* ── CF32 forward/inverse round-trip ────────────────────────────── */
  {
    dp_fft_state_t *fwd = dp_fft_create (N, -1, 1);
    dp_fft_state_t *inv = dp_fft_create (N, +1, 1);
    DP_CHECK (fwd != NULL && inv != NULL);

    float _Complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)(i + 1) + 0.0f * I;

    dp_fft_execute_cf32 (fwd, in, N, spec, N);
    dp_fft_execute_cf32 (inv, spec, N, rec, N);

    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq32 (rec[i], (float)N * in[i]));

    dp_fft_destroy (fwd);
    dp_fft_destroy (inv);
  }

  /* ── DC tone: only bin 0 is non-zero ────────────────────────────── */
  {
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    double _Complex in[16], out[16];
    for (size_t i = 0; i < N; i++)
      in[i] = 1.0 + 0.0 * I;
    dp_fft_execute_cf64 (obj, in, N, out, N);

    /* bin 0 = N; all others = 0 */
    DP_CHECK (ceq64 (out[0], (double)N + 0.0 * I));
    for (size_t k = 1; k < N; k++)
      DP_CHECK (ceq64 (out[k], 0.0 + 0.0 * I));
    dp_fft_destroy (obj);
  }

  /* ── inplace CF64 matches out-of-place ──────────────────────────── */
  {
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    double _Complex in[16], out_oop[16], out_ip[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)i - 7.5 + (double)i * I;

    dp_fft_execute_cf64 (obj, in, N, out_oop, N);
    dp_fft_execute_inplace_cf64 (obj, in, N, out_ip, N);

    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (out_ip[k], out_oop[k]));
    dp_fft_destroy (obj);
  }

  /* ── inplace CF32 matches out-of-place ──────────────────────────── */
  {
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    float _Complex in[16], out_oop[16], out_ip[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)i - 7.5f + (float)i * I;

    dp_fft_execute_cf32 (obj, in, N, out_oop, N);
    dp_fft_execute_inplace_cf32 (obj, in, N, out_ip, N);

    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq32 (out_ip[k], out_oop[k]));
    dp_fft_destroy (obj);
  }

  /* ── max_out always returns n ────────────────────────────────────── */
  {
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    DP_CHECK (dp_fft_execute_cf64_max_out (obj) == N);
    DP_CHECK (dp_fft_execute_cf32_max_out (obj) == N);
    DP_CHECK (dp_fft_execute_inplace_cf64_max_out (obj) == N);
    DP_CHECK (dp_fft_execute_inplace_cf32_max_out (obj) == N);
    dp_fft_destroy (obj);
  }

  /* ── short out: prefix of the full transform, nothing past max_out ──
   * The plan is fixed at n and writes all n bins, so a short buffer is
   * served from scratch and truncated -- see fft_core.c.  The canary
   * proves the tail is never touched. */
  {
    dp_fft_state_t *obj = dp_fft_create (N, -1, 1);
    double _Complex in[16], full[16], part[16];
    float _Complex in32[16], full32[16], part32[16];
    for (size_t i = 0; i < N; i++)
      {
        in[i]   = (double)(i % 7) - 3.0 + (double)(i % 5) * I;
        in32[i] = (float)(i % 7) - 3.0f + (float)(i % 5) * I;
      }
    const double _Complex CANARY  = -12345.0 - 6789.0 * I;
    const float _Complex CANARY32 = -12345.0f - 6789.0f * I;
    const size_t K                = 4;

    dp_fft_execute_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (dp_fft_execute_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    dp_fft_execute_inplace_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (dp_fft_execute_inplace_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    dp_fft_execute_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (dp_fft_execute_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    dp_fft_execute_inplace_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (dp_fft_execute_inplace_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    /* max_out == 0 writes nothing at all. */
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (dp_fft_execute_cf64 (obj, in, N, part, 0) == 0);
    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    dp_fft_destroy (obj);
  }

  DP_TEST_END ("test_fft_core");
}
