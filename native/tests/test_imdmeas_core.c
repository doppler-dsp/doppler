#include "imdmeas/imdmeas_core.h"
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

#define NCAP 4096u

static void
add_cos (float *x, size_t n, double cyc, double amp)
{
  for (size_t i = 0; i < n; i++)
    x[i] += (float)(amp * cos (2.0 * M_PI * cyc * (double)i / (double)n));
}

int
main (void)
{
  int    _fails = 0;
  float *x      = (float *)malloc (NCAP * sizeof (float));
  for (size_t i = 0; i < NCAP; i++)
    x[i] = 0.0f;

  /* two full-scale tones at 200 and 250 cycles, with -40 dBc 3rd-order
   * products at 150 (2f1-f2) and 300 (2f2-f1) and a -50.5 dBc IMD2 at 50. */
  add_cos (x, NCAP, 200.0, 1.0);
  add_cos (x, NCAP, 250.0, 1.0);
  add_cos (x, NCAP, 150.0, 0.01);
  add_cos (x, NCAP, 300.0, 0.01);
  add_cos (x, NCAP, 50.0, 0.003);

  imdmeas_state_t *m = imdmeas_create (NCAP, 1.0, 1, 12.0f, 2, 1.0);
  CHECK (m != NULL);
  imd_meas_t r;
  imdmeas_analyze (m, x, NCAP, &r, 1);

  CHECK (fabs (r.f1 - 200.0 / NCAP) < 2e-3);
  CHECK (fabs (r.f2 - 250.0 / NCAP) < 2e-3);
  CHECK (fabs (r.imd3_dbc - (-40.0)) < 0.5);
  CHECK (fabs (r.imd3_lo_freq - 150.0 / NCAP) < 2e-3);
  CHECK (fabs (r.imd3_hi_freq - 300.0 / NCAP) < 2e-3);
  CHECK (fabs (r.imd2_freq - 50.0 / NCAP) < 2e-3);
  /* TOI = mean-tone level (0 dBFS) + |IMD3|/2 = 20 dBFS */
  CHECK (fabs (r.toi_dbfs - 20.0) < 0.5);
  CHECK (fabs (r.p1_dbfs) < 0.2 && fabs (r.p2_dbfs) < 0.2);

  imdmeas_destroy (m);
  free (x);
  if (_fails)
    {
      fprintf (stderr, "test_imdmeas_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_imdmeas_core PASSED\n");
  return 0;
}
