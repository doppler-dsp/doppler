/**
 * ofdm_demo.c -- an OFDM signal from a layout, and the grid back out of it.
 *
 * The lifecycle a caller manages, in the order it happens:
 *
 *   1. dp_ofdm_create()                  the modulator, empty
 *   2. dp_ofdm_add_class / _polarity / _fixed / _run
 *                                        the layout; each call COPIES what
 *                                        it is given, so the caller's arrays
 *                                        need not outlive the call
 *   3. dp_ofdm_check_why()               the layout is usable, or WHY it is
 *                                        not (modulate itself refuses an
 *                                        unusable layout with a bare 0)
 *   4. dp_ofdm_points_remaining(),       how many points to supply and how
 *      dp_ofdm_modulate_max_out()        many samples will come out: the
 *                                        CALLER allocates both buffers
 *   5. dp_ofdm_modulate(), dp_ofdm_finish()
 *                                        the samples, written into the
 *                                        caller's buffer; finish returns
 *                                        the window's last fall
 *   6. dp_ofdm_destroy()                 frees the modulator, never the
 *                                        caller's buffers
 *
 * The layout is a generic one: a 64-point transform with 52 occupied
 * subcarriers (DC and the band edges null), four pilots that follow a
 * polarity sequence, a 16-sample cyclic prefix and a 2-sample window,
 * carrying QPSK, ten OFDM symbols. The samples are the UNNORMALIZED inverse
 * transform of the grid (the default scale is 1.0), so the mean power is the
 * grid's energy, 52, not 1. A forward transform of each symbol's body, with
 * the prefix dropped and divided by the transform length, returns the grid
 * that went in: that is how the structure is checked here.
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/ofdm_demo
 */
#include "doppler/ofdm/ofdm_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 64u
#define CP 16u
#define WINDOW 2u
#define N_SYM 10u
#define TWO_PI 6.283185307179586476925286766559

typedef float _Complex cf;

/* Coefficient k of the forward sum over one body, divided by its length: the
   inverse of the modulator's own transform, and nothing else. */
static double _Complex coef (const cf *body, int k)
{
  double _Complex s = 0;
  for (unsigned m = 0; m < N; m++)
    s += (double _Complex)body[m]
         * cexp (-I * TWO_PI * (double)k * (double)m / (double)N);
  return s / (double)N;
}

int
main (void)
{
  printf ("=== doppler OFDM modulator demo ===\n\n");
  int ok = 1;

  /* 1-2. Build the layout. The map is indexed k + N/2; the pilots are fixed
     entries in every pattern phase, multiplied by the polarity sequence. */
  static const int   pilot_k[4] = { -21, -7, 7, 21 };
  static const float pol[4]     = { 1.0f, 1.0f, 1.0f, -1.0f };
  uint8_t            used[N];
  memset (used, 0, sizeof used);
  for (int k = -26; k <= 26; k++)
    used[k + (int)N / 2] = (k != 0);

  dp_ofdm_state_t *o = dp_ofdm_create ();
  if (!o)
    return 1;
  const int cls = dp_ofdm_add_class (o, N, used, N, 0u, CP, WINDOW, 1.0, 1u);
  cf        polarity[4];
  for (int i = 0; i < 4; i++)
    polarity[i] = pol[i];
  ok &= (cls == 0);
  ok &= (dp_ofdm_add_polarity (o, 0u, polarity, 4u) == 0);
  for (int i = 0; i < 4; i++)
    ok &= (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, pilot_k[i], 1.0) == 0);
  ok &= (dp_ofdm_add_run (o, 0u, N_SYM, OFDM_CP_DEFAULT, 0u, 0u) == 0);

  /* 3. An unusable layout says why. A second modulator with a class and no
     run is refused with a sentence, by check and by modulate_why alike;
     plain modulate would return a bare 0. */
  {
    const char      *why   = NULL;
    dp_ofdm_state_t *empty = dp_ofdm_create ();
    dp_ofdm_add_class (empty, N, used, N, 0u, CP, WINDOW, 1.0, 1u);
    const int bad = dp_ofdm_check_why (empty, &why);
    printf ("--- 3. a layout with no run: check = %d (%s)\n", bad,
            why ? why : "no reason");
    ok &= (bad < 0 && why != NULL);
    cf          pts1[1] = { 0 }, out1[1];
    const char *why2    = NULL;
    ok &= (dp_ofdm_modulate_why (empty, pts1, 0u, out1, 0u, &why2) < 0
           && why2 == why);
    dp_ofdm_destroy (empty);
  }

  const char *why = NULL;
  ok &= (dp_ofdm_check_why (o, &why) == 0 && why == NULL);

  /* 4. The caller sizes and owns both buffers. Points are QPSK, one per data
     subcarrier: 52 occupied less 4 pilots, 48 a symbol. */
  const size_t n_points = dp_ofdm_points_remaining (o);
  const size_t n_out    = dp_ofdm_modulate_max_out (o, n_points);
  printf ("--- 4. %zu points in, %zu samples out (%u OFDM symbols of %u)\n",
          n_points, n_out, N_SYM, N + CP);
  ok &= (n_points == (size_t)48u * N_SYM);
  ok &= (n_out == (size_t)N_SYM * (N + CP));
  cf *points = malloc (n_points * sizeof *points);
  cf *out    = malloc ((n_out + WINDOW) * sizeof *out);
  if (!points || !out)
    return 1;
  unsigned rng = 1234567u;
  for (size_t i = 0; i < n_points; i++)
    {
      rng       = rng * 1664525u + 1013904223u;
      points[i] = (float)(((rng >> 16) & 1u) ? 0.70710678 : -0.70710678)
                  + (float)(((rng >> 17) & 1u) ? 0.70710678 : -0.70710678) * I;
    }

  /* 5. Render. The whole layout in one call, then the window's last fall. */
  const size_t got = dp_ofdm_modulate (o, points, n_points, out, n_out);
  const size_t fin = dp_ofdm_finish (o, WINDOW, out + got, WINDOW);
  printf ("--- 5. modulate wrote %zu samples, finish %zu\n", got, fin);
  ok &= (got == n_out && fin == WINDOW);
  ok &= (dp_ofdm_points_remaining (o) == 0);

  /* modulate_why tells a refusal from a render of nothing: at the end of the
     layout, no points is a valid 0 that leaves *why as it was set; one point
     more than the layout has is refused, with the reason. */
  {
    const char   *kept = "untouched", *reason = NULL;
    const int64_t nothing
        = dp_ofdm_modulate_why (o, points, 0u, out, 0u, &kept);
    const int64_t refused
        = dp_ofdm_modulate_why (o, points, 1u, out, n_out, &reason);
    printf ("--- 5b. no points at the end: %lld, why %s; one point too many: "
            "%lld (%s)\n",
            (long long)nothing, kept, (long long)refused,
            reason ? reason : "no reason");
    ok &= (nothing == 0 && strcmp (kept, "untouched") == 0);
    ok &= (refused < 0 && reason != NULL);
  }

  /* Invert it: each body's forward transform is the grid. */
  int    grid_ok = 1;
  size_t drawn   = 0;
  for (unsigned s = 0; s < N_SYM; s++)
    {
      const cf *body = out + (size_t)s * (N + CP) + CP;
      for (int k = -(int)N / 2; k < (int)N / 2; k++)
        {
          double _Complex want = 0;
          int is_pilot         = 0;
          for (int i = 0; i < 4; i++)
            if (k == pilot_k[i])
              {
                want     = pol[s % 4];
                is_pilot = 1;
              }
          if (!is_pilot && used[k + (int)N / 2])
            want = points[drawn++];
          grid_ok &= (cabs (coef (body, k) - want) < 1e-5);
        }
    }
  printf ("--- the grid read back from the samples: %s\n",
          grid_ok ? "every point, pilot and null where it was put" : "WRONG");
  ok &= grid_ok && (drawn == n_points);

  double p = 0;
  for (size_t i = CP; i < n_out; i++)
    p += (double)crealf (out[i]) * crealf (out[i])
         + (double)cimagf (out[i]) * cimagf (out[i]);
  p /= (double)(n_out - CP);
  printf ("--- mean power %.1f: the grid's energy (52), not 1; scale = 1/sqrt "
          "(52) would give 1\n",
          p);
  ok &= (fabs (p - 52.0) < 2.0);

  /* 6. Free the modulator; the two buffers are the caller's own. */
  dp_ofdm_destroy (o);
  free (points);
  free (out);

  printf ("\nofdm_demo: %s\n", ok ? "OK" : "FAILED");
  return ok ? 0 : 1;
}
