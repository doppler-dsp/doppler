#include "ppe/ppe_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

/* y[m] = exp(j2π(f·m + ½·r·m²)) — a unit-amplitude linear chirp. */
static void
synth_chirp (float complex *y, size_t L, double f, double r)
{
  for (size_t m = 0; m < L; m++)
    {
      double ph
          = 2.0 * M_PI * (f * (double)m + 0.5 * r * (double)m * (double)m);
      y[m] = (float)cos (ph) + (float)sin (ph) * I;
    }
}

int
main (void)
{
  int          _fails = 0;
  const size_t L      = 2048;
  const float  ftol = 5e-3f, rtol = 5e-6f;

  /* Argument validation. */
  CHECK (ppe_create (2, 0) == NULL); /* max_len < 4 */

  ppe_state_t *p = ppe_create (L, 0);
  CHECK (p != NULL);
  if (!p)
    return 1;
  float complex *y = malloc (L * sizeof *y);
  CHECK (y != NULL);

  /* 1) Positive frequency + chirp: recover both within a fraction of a bin. */
  synth_chirp (y, L, 0.05, 1e-5);
  ppe_result_t e = ppe_estimate (p, y, L);
  CHECK (fabs (e.freq_norm - 0.05) <= ftol);
  CHECK (fabs (e.rate_norm - 1e-5) <= rtol);

  /* 2) Negative frequency + negative chirp (sign handling). */
  synth_chirp (y, L, -0.08, -2e-5);
  e = ppe_estimate (p, y, L);
  CHECK (fabs (e.freq_norm + 0.08) <= ftol);
  CHECK (fabs (e.rate_norm + 2e-5) <= rtol);

  /* 3) Pure tone (r = 0): the autocorrelation is DC, so rate ≈ 0. */
  synth_chirp (y, L, 0.1, 0.0);
  e = ppe_estimate (p, y, L);
  CHECK (fabs (e.rate_norm) <= rtol);
  CHECK (fabs (e.freq_norm - 0.1) <= ftol);

  /* 4) Out-of-range length → zeroed result, no crash. */
  e = ppe_estimate (p, y, L + 1);
  CHECK (e.freq_norm == 0.0 && e.rate_norm == 0.0);

  free (y);
  ppe_destroy (p);
  if (_fails)
    {
      fprintf (stderr, "test_ppe_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ppe_core PASSED\n");
  return 0;
}
