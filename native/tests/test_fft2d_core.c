#include "dp_test.h"
#include "fft2d/fft2d_core.h"
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
  const size_t NY = 8, NX = 8, N = NY * NX;

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->ny == NY);
    DP_CHECK (obj->nx == NX);
    DP_CHECK (obj->sign == -1);
    DP_CHECK (obj->plan_f64 != NULL);
    DP_CHECK (obj->plan_f32 != NULL);
    fft2d_reset (obj);
    fft2d_destroy (obj);
    fft2d_destroy (NULL);
  }

  /* ── max_out == ny*nx ───────────────────────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    DP_CHECK (fft2d_execute_cf64_max_out (obj) == N);
    DP_CHECK (fft2d_execute_cf32_max_out (obj) == N);
    DP_CHECK (fft2d_execute_inplace_cf64_max_out (obj) == N);
    DP_CHECK (fft2d_execute_inplace_cf32_max_out (obj) == N);
    fft2d_destroy (obj);
  }

  /* ── CF64 forward/inverse round-trip ────────────────────────────── */
  {
    fft2d_state_t *fwd = fft2d_create (NY, NX, -1, 1);
    fft2d_state_t *inv = fft2d_create (NY, NX, +1, 1);
    DP_CHECK (fwd != NULL && inv != NULL);

    double complex in[64], spec[64], rec[64];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i + 1) + 0.0 * I;

    fft2d_execute_cf64 (fwd, in, N, spec, N);
    fft2d_execute_cf64 (inv, spec, N, rec, N);

    /* IDFT without normalisation: rec[k] == N * in[k] */
    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq64 (rec[i], (double)N * in[i]));

    fft2d_destroy (fwd);
    fft2d_destroy (inv);
  }

  /* ── CF32 forward/inverse round-trip ────────────────────────────── */
  {
    fft2d_state_t *fwd = fft2d_create (NY, NX, -1, 1);
    fft2d_state_t *inv = fft2d_create (NY, NX, +1, 1);

    float complex in[64], spec[64], rec[64];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)(i + 1) + 0.0f * I;

    fft2d_execute_cf32 (fwd, in, N, spec, N);
    fft2d_execute_cf32 (inv, spec, N, rec, N);

    for (size_t i = 0; i < N; i++)
      DP_CHECK (ceq32 (rec[i], (float)N * in[i]));

    fft2d_destroy (fwd);
    fft2d_destroy (inv);
  }

  /* ── DC input: only bin (0,0) is non-zero ───────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    double complex in[64], out[64];
    for (size_t i = 0; i < N; i++)
      in[i] = 1.0 + 0.0 * I;
    fft2d_execute_cf64 (obj, in, N, out, N);

    DP_CHECK (ceq64 (out[0], (double)N + 0.0 * I));
    for (size_t k = 1; k < N; k++)
      DP_CHECK (ceq64 (out[k], 0.0 + 0.0 * I));
    fft2d_destroy (obj);
  }

  /* ── inplace CF64 matches out-of-place ──────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    double complex in[64], out_oop[64], out_ip[64];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i % 5) - 2.0 + (double)(i % 3) * I;

    fft2d_execute_cf64 (obj, in, N, out_oop, N);
    fft2d_execute_inplace_cf64 (obj, in, N, out_ip, N);

    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (out_ip[k], out_oop[k]));
    fft2d_destroy (obj);
  }

  /* ── short out: prefix of the full surface, nothing past max_out ──── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    double complex in[64], full[64], part[64];
    float complex  in32[64], full32[64], part32[64];
    for (size_t i = 0; i < N; i++)
      {
        in[i]   = (double)(i % 7) - 3.0 + (double)(i % 5) * I;
        in32[i] = (float)(i % 7) - 3.0f + (float)(i % 5) * I;
      }
    const double complex CANARY   = -12345.0 - 6789.0 * I;
    const float complex  CANARY32 = -12345.0f - 6789.0f * I;
    const size_t         K        = 10;

    fft2d_execute_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft2d_execute_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft2d_execute_inplace_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft2d_execute_inplace_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft2d_execute_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (fft2d_execute_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    fft2d_execute_inplace_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    DP_CHECK (fft2d_execute_inplace_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      DP_CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      DP_CHECK (ceq32 (part32[k], CANARY32));

    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    DP_CHECK (fft2d_execute_cf64 (obj, in, N, part, 0) == 0);
    for (size_t k = 0; k < N; k++)
      DP_CHECK (ceq64 (part[k], CANARY));

    fft2d_destroy (obj);
  }

  DP_TEST_END ("test_fft2d_core");
}
