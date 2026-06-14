#include "nprmeas/nprmeas_core.h"
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

#define NCAP 32768u

static uint32_t _rng = 999u;
static double
urand (void)
{
  _rng = _rng * 1664525u + 1013904223u;
  return ((double)_rng / 4294967296.0) * 2.0 - 1.0;
}

/* Build notched broadband noise: bins of a real spectrum filled with random
 * phase/magnitude over [active], zeroed (to `depth` below) inside [notch]. */
static void
notched_noise (float *x, size_t n, double depth_db)
{
  /* time-domain white noise, then notch by summing out the notch band is
   * awkward; instead synthesise directly in frequency via a crude IDFT-free
   * approach: sum of many random sinusoids, skipping the notch band. */
  for (size_t i = 0; i < n; i++)
    x[i] = 0.0f;
  double lin = pow (10.0, depth_db / 20.0);
  for (int k = 60; k < 460; k++) /* active band ~ [0.06,0.46] of fs=1 */
    {
      double cyc      = (double)k * (double)n / 1000.0; /* map to cycles */
      cyc             = (double)k;                      /* k cycles in n */
      double amp      = 1.0;
      int    in_notch = (k >= 200 && k <= 250);
      if (in_notch)
        amp = lin;
      double ph = urand () * M_PI;
      for (size_t i = 0; i < n; i++)
        x[i] += (float)(amp
                        * cos (2.0 * M_PI * cyc * (double)i / (double)n + ph));
    }
}

int
main (void)
{
  int    _fails = 0;
  float *x      = (float *)malloc (NCAP * sizeof (float));

  nprmeas_state_t *m = nprmeas_create (NCAP, 1.0, 1, 12.0f, 2, 1.0);
  CHECK (m != NULL);

  /* active band [0.06, 0.46], notch [0.20, 0.25] (k in [200,250] over n) ->
   * normalised freq k/n.  fs=1, df = 1/nfft.  active in Hz = [60,460]/NCAP. */
  double alo = 60.0 / NCAP, ahi = 460.0 / NCAP;
  double nlo = 200.0 / NCAP, nhi = 250.0 / NCAP;
  double guard = 4.0 / NCAP;

  for (int depth = 40; depth <= 60; depth += 20)
    {
      notched_noise (x, NCAP, -(double)depth);
      npr_meas_t r;
      r = nprmeas_analyze (m, x, NCAP, alo, ahi, nlo, nhi, guard);
      /* measured NPR should track the synthesised notch depth (loose: the
       * window skirts and finite bins blur it). */
      CHECK (fabs (r.npr_db - (double)depth) < 8.0);
      CHECK (r.n_inband_bins > 0 && r.n_notch_bins > 0);
    }

  nprmeas_destroy (m);
  free (x);
  if (_fails)
    {
      fprintf (stderr, "test_nprmeas_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_nprmeas_core PASSED\n");
  return 0;
}
