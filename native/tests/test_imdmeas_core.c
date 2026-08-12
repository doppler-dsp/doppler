#include "dp_test.h"
#include "imdmeas/imdmeas_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
  float *x = (float *)malloc (NCAP * sizeof (float));
  for (size_t i = 0; i < NCAP; i++)
    x[i] = 0.0f;

  /* two full-scale tones at 200 and 250 cycles, with -40 dBc 3rd-order
   * products at 150 (2f1-f2) and 300 (2f2-f1) and a -50.5 dBc IMD2 at 50. */
  add_cos (x, NCAP, 200.0, 1.0);
  add_cos (x, NCAP, 250.0, 1.0);
  add_cos (x, NCAP, 150.0, 0.01);
  add_cos (x, NCAP, 300.0, 0.01);
  add_cos (x, NCAP, 50.0, 0.003);

  /* dynamic_range_db = 90 -> Kaiser beta ~12, matching the old default. */
  imdmeas_state_t *m = imdmeas_create (NCAP, 1.0, 1.0, 0, 90.0);
  DP_CHECK (m != NULL);
  imd_meas_t r;
  r = imdmeas_analyze (m, x, NCAP);

  DP_CHECK (fabs (r.f1 - 200.0 / NCAP) < 2e-3);
  DP_CHECK (fabs (r.f2 - 250.0 / NCAP) < 2e-3);
  DP_CHECK (fabs (r.imd3_dbc - (-40.0)) < 0.5);
  DP_CHECK (fabs (r.imd3_lo_freq - 150.0 / NCAP) < 2e-3);
  DP_CHECK (fabs (r.imd3_hi_freq - 300.0 / NCAP) < 2e-3);
  DP_CHECK (fabs (r.imd2_freq - 50.0 / NCAP) < 2e-3);
  /* TOI = mean-tone level (0 dBFS) + |IMD3|/2 = 20 dBFS */
  DP_CHECK (fabs (r.toi_dbfs - 20.0) < 0.5);
  DP_CHECK (fabs (r.p1_dbfs) < 0.2 && fabs (r.p2_dbfs) < 0.2);

  imdmeas_destroy (m);
  free (x);
  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* The count argument is the INPUT capture length; the output length is
     * the analyser's nfft, so max_out is the only real bound here.
     * NB: feed a FULL capture (NCAP), not a short one -- with fewer samples
     * than one frame nothing accumulates, spectrum_dbfs returns 0, and a
     * "<= max_out" assertion would hold whether or not the clamp exists. */
    imdmeas_state_t *m   = imdmeas_create (NCAP, 1.0, 1.0, 0, 90.0);
    size_t           cap = imdmeas_spectrum_dbfs_max_out (m); /* == nfft */
    float           *xs  = (float *)malloc (NCAP * sizeof (float));
    float           *o   = (float *)malloc (cap * sizeof (float));
    DP_CHECK (m && xs && o);
    for (size_t i = 0; i < NCAP; i++)
      xs[i] = (float)sin (0.05 * (double)i);
    for (size_t i = 0; i < cap; i++)
      o[i] = 42.0f;

    DP_CHECK (imdmeas_spectrum_dbfs (m, xs, NCAP, o, 5) == 5);
    for (size_t i = 5; i < cap; i++)
      DP_CHECK (o[i] == 42.0f); /* tail untouched */

    /* Zero capacity emits nothing. */
    for (size_t i = 0; i < cap; i++)
      o[i] = 42.0f;
    DP_CHECK (imdmeas_spectrum_dbfs (m, xs, NCAP, o, 0) == 0);
    for (size_t i = 0; i < cap; i++)
      DP_CHECK (o[i] == 42.0f);
    free (xs);
    free (o);
    imdmeas_destroy (m);
  }

  DP_TEST_END ("test_imdmeas_core");
}
