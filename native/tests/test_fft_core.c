#include "fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

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
  int          _fails = 0;
  const size_t N      = 16;

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    fft_state_t *obj = fft_create (N, -1, 1);
    CHECK (obj != NULL);
    CHECK (obj->n == N);
    CHECK (obj->sign == -1);
    CHECK (obj->plan_f64 != NULL);
    CHECK (obj->plan_f32 != NULL);
    fft_reset (obj); /* no-op; must not crash */
    fft_destroy (obj);
    fft_destroy (NULL); /* must not crash */
  }

  /* ── CF64 forward/inverse round-trip ────────────────────────────── */
  {
    fft_state_t *fwd = fft_create (N, -1, 1);
    fft_state_t *inv = fft_create (N, +1, 1);
    CHECK (fwd != NULL && inv != NULL);

    double complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i + 1) + 0.0 * I;

    fft_execute_cf64 (fwd, in, N, spec, N);
    fft_execute_cf64 (inv, spec, N, rec, N);

    /* IDFT without normalisation: rec[k] == N * in[k] */
    for (size_t i = 0; i < N; i++)
      CHECK (ceq64 (rec[i], (double)N * in[i]));

    fft_destroy (fwd);
    fft_destroy (inv);
  }

  /* ── CF32 forward/inverse round-trip ────────────────────────────── */
  {
    fft_state_t *fwd = fft_create (N, -1, 1);
    fft_state_t *inv = fft_create (N, +1, 1);
    CHECK (fwd != NULL && inv != NULL);

    float complex in[16], spec[16], rec[16];
    for (size_t i = 0; i < N; i++)
      in[i] = (float)(i + 1) + 0.0f * I;

    fft_execute_cf32 (fwd, in, N, spec, N);
    fft_execute_cf32 (inv, spec, N, rec, N);

    for (size_t i = 0; i < N; i++)
      CHECK (ceq32 (rec[i], (float)N * in[i]));

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
    CHECK (ceq64 (out[0], (double)N + 0.0 * I));
    for (size_t k = 1; k < N; k++)
      CHECK (ceq64 (out[k], 0.0 + 0.0 * I));
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
      CHECK (ceq64 (out_ip[k], out_oop[k]));
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
      CHECK (ceq32 (out_ip[k], out_oop[k]));
    fft_destroy (obj);
  }

  /* ── max_out always returns n ────────────────────────────────────── */
  {
    fft_state_t *obj = fft_create (N, -1, 1);
    CHECK (fft_execute_cf64_max_out (obj) == N);
    CHECK (fft_execute_cf32_max_out (obj) == N);
    CHECK (fft_execute_inplace_cf64_max_out (obj) == N);
    CHECK (fft_execute_inplace_cf32_max_out (obj) == N);
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
    CHECK (fft_execute_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      CHECK (ceq64 (part[k], CANARY));

    fft_execute_inplace_cf64 (obj, in, N, full, N);
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    CHECK (fft_execute_inplace_cf64 (obj, in, N, part, K) == K);
    for (size_t k = 0; k < K; k++)
      CHECK (ceq64 (part[k], full[k]));
    for (size_t k = K; k < N; k++)
      CHECK (ceq64 (part[k], CANARY));

    fft_execute_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    CHECK (fft_execute_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      CHECK (ceq32 (part32[k], CANARY32));

    fft_execute_inplace_cf32 (obj, in32, N, full32, N);
    for (size_t k = 0; k < N; k++)
      part32[k] = CANARY32;
    CHECK (fft_execute_inplace_cf32 (obj, in32, N, part32, K) == K);
    for (size_t k = 0; k < K; k++)
      CHECK (ceq32 (part32[k], full32[k]));
    for (size_t k = K; k < N; k++)
      CHECK (ceq32 (part32[k], CANARY32));

    /* max_out == 0 writes nothing at all. */
    for (size_t k = 0; k < N; k++)
      part[k] = CANARY;
    CHECK (fft_execute_cf64 (obj, in, N, part, 0) == 0);
    for (size_t k = 0; k < N; k++)
      CHECK (ceq64 (part[k], CANARY));

    fft_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_fft_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_fft_core PASSED\n");
  return 0;
}
