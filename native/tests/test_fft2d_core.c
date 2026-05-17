#include "fft2d/fft2d_core.h"
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
  int _fails = 0;
  const size_t NY = 8, NX = 8, N = NY * NX;

  /* ── lifecycle ──────────────────────────────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    CHECK (obj != NULL);
    CHECK (obj->ny == NY);
    CHECK (obj->nx == NX);
    CHECK (obj->sign == -1);
    CHECK (obj->plan_f64 != NULL);
    CHECK (obj->plan_f32 != NULL);
    fft2d_reset (obj);
    fft2d_destroy (obj);
    fft2d_destroy (NULL);
  }

  /* ── max_out == ny*nx ───────────────────────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    CHECK (fft2d_execute_cf64_max_out (obj) == N);
    CHECK (fft2d_execute_cf32_max_out (obj) == N);
    CHECK (fft2d_execute_inplace_cf64_max_out (obj) == N);
    CHECK (fft2d_execute_inplace_cf32_max_out (obj) == N);
    fft2d_destroy (obj);
  }

  /* ── CF64 forward/inverse round-trip ────────────────────────────── */
  {
    fft2d_state_t *fwd = fft2d_create (NY, NX, -1, 1);
    fft2d_state_t *inv = fft2d_create (NY, NX, +1, 1);
    CHECK (fwd != NULL && inv != NULL);

    double complex in[64], spec[64], rec[64];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i + 1) + 0.0 * I;

    fft2d_execute_cf64 (fwd, in, N, spec);
    fft2d_execute_cf64 (inv, spec, N, rec);

    /* IDFT without normalisation: rec[k] == N * in[k] */
    for (size_t i = 0; i < N; i++)
      CHECK (ceq64 (rec[i], (double)N * in[i]));

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

    fft2d_execute_cf32 (fwd, in, N, spec);
    fft2d_execute_cf32 (inv, spec, N, rec);

    for (size_t i = 0; i < N; i++)
      CHECK (ceq32 (rec[i], (float)N * in[i]));

    fft2d_destroy (fwd);
    fft2d_destroy (inv);
  }

  /* ── DC input: only bin (0,0) is non-zero ───────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    double complex in[64], out[64];
    for (size_t i = 0; i < N; i++)
      in[i] = 1.0 + 0.0 * I;
    fft2d_execute_cf64 (obj, in, N, out);

    CHECK (ceq64 (out[0], (double)N + 0.0 * I));
    for (size_t k = 1; k < N; k++)
      CHECK (ceq64 (out[k], 0.0 + 0.0 * I));
    fft2d_destroy (obj);
  }

  /* ── inplace CF64 matches out-of-place ──────────────────────────── */
  {
    fft2d_state_t *obj = fft2d_create (NY, NX, -1, 1);
    double complex in[64], out_oop[64], out_ip[64];
    for (size_t i = 0; i < N; i++)
      in[i] = (double)(i % 5) - 2.0 + (double)(i % 3) * I;

    fft2d_execute_cf64 (obj, in, N, out_oop);
    fft2d_execute_inplace_cf64 (obj, in, N, out_ip);

    for (size_t k = 0; k < N; k++)
      CHECK (ceq64 (out_ip[k], out_oop[k]));
    fft2d_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_fft2d_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_fft2d_core PASSED\n");
  return 0;
}
