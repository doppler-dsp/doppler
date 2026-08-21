/*
 * test_resample_core.c — the resample module's three free functions.
 *
 * All three are design-time: they answer "what filter do I need?" rather
 * than filtering anything, and none of them is checked anywhere else,
 * because the objects that consume them (`Resampler`, `RateConverter`)
 * are tested through their outputs and would still look correct with a
 * filter one tap too short.
 *
 * `kaiser_num_taps` is the one with history. It ends in an INTEGER divide
 * by `num_phases`, so passing 0 was a SIGFPE rather than a wrong answer,
 * and a Python shadow in `src/doppler/resample/__init__.py` hid that from
 * every caller until this file existed. The degenerate-argument checks
 * below are the regression for both halves of that.
 */
#include "dp_test.h"
#include "resample/resample_core.h"
#include <math.h>
#include <stdio.h>

int
main (void)
{
  /* ── kaiser_beta: the three-branch Kaiser rule ──────────────────── */
  {
    /* Below 21 dB a rectangular window already meets the spec, so beta
       is 0 -- the branch a caller sweeping attenuation walks through. */
    DP_CHECK (kaiser_beta (0.0) == 0.0);
    DP_CHECK (kaiser_beta (20.999) == 0.0);

    /* The published closed forms, at the two branch points. */
    DP_CHECK (fabs (kaiser_beta (60.0) - 0.1102 * (60.0 - 8.7)) < 1e-12);
    DP_CHECK (
        fabs (kaiser_beta (30.0)
              - (0.5842 * pow (30.0 - 21.0, 0.4) + 0.07886 * (30.0 - 21.0)))
        < 1e-12);

    /* Continuous at 21 dB (both branches give 0 there) and monotone
       above it -- a beta that dipped would make a stricter spec cheaper. */
    DP_CHECK (fabs (kaiser_beta (21.0)) < 1e-12);
    double prev = -1.0;
    for (double a = 21.0; a <= 120.0; a += 1.0)
      {
        const double b = kaiser_beta (a);
        DP_CHECK (b >= prev);
        prev = b;
      }
  }

  /* ── kaiser_num_taps: monotone in the spec, and safe at the rails ── */
  {
    /* The header's own worked example. */
    DP_CHECK (kaiser_num_taps (4096, 60.0, 0.4, 0.6) == 19);

    /* A tighter transition costs taps; a looser one saves them. */
    DP_CHECK (kaiser_num_taps (1, 60.0, 0.20, 0.22)
              > kaiser_num_taps (1, 60.0, 0.20, 0.40));
    /* More attenuation costs taps at a fixed transition. */
    DP_CHECK (kaiser_num_taps (1, 90.0, 0.20, 0.30)
              > kaiser_num_taps (1, 40.0, 0.20, 0.30));
    /* Never below the documented floor of 1. */
    DP_CHECK (kaiser_num_taps (1, 21.0, 0.1, 0.9) >= 1);

    /* THE REGRESSION: the final divide is integer, so num_phases == 0
       used to raise SIGFPE and take a Python caller's interpreter with
       it. A value below 1 is not a bank; it returns 0. */
    DP_CHECK (kaiser_num_taps (0, 60.0, 0.4, 0.6) == 0);
    DP_CHECK (kaiser_num_taps (-1, 60.0, 0.4, 0.6) == 0);
    DP_CHECK (kaiser_num_taps (-4096, 60.0, 0.4, 0.6) == 0);
  }

  /* ── ciccompmf: DC gain is exactly 1, and the taps are linear phase ─ */
  {
    double h[19];

    /* The header's worked example, to the digits it prints. */
    for (int i = 0; i < 19; i++)
      h[i] = 0.0;
    ciccompmf (h, 4, 16, 5);
    DP_CHECK (fabs (h[0] - 0.029) < 5e-4);
    DP_CHECK (fabs (h[1] - (-0.282)) < 5e-4);
    DP_CHECK (fabs (h[2] - 1.5061) < 5e-4);

    /* "DC gain is exactly 1.0" is the design's defining property: the
       compensator must not change the level the CIC already set. */
    for (uint32_t m = 1; m <= 19; m++)
      {
        double sum = 0.0;
        for (uint32_t i = 0; i < m; i++)
          h[i] = 0.0;
        ciccompmf (h, 4, 16, m);
        for (uint32_t i = 0; i < m; i++)
          sum += h[i];
        DP_CHECK (fabs (sum - 1.0) < 1e-9);
      }

    /* "Odd M gives symmetric linear-phase taps." Even M is
       half-sample-shifted and is symmetric about a point between two
       taps, so only the odd case is checkable elementwise. */
    for (uint32_t m = 3; m <= 19; m += 2)
      {
        for (uint32_t i = 0; i < m; i++)
          h[i] = 0.0;
        ciccompmf (h, 4, 16, m);
        for (uint32_t i = 0; i < m; i++)
          DP_CHECK (fabs (h[i] - h[m - 1 - i]) < 1e-12);
      }

    /* Out of range is the ALL-ZERO filter, and it still writes M
       elements. The header used to say `out` was left unmodified, which
       would let a caller pre-fill a fallback design and keep it; measured
       here, the fallback is silently zeroed -- a muted signal path rather
       than a degraded one. The doc now says what this asserts.

       The two parities do not share a bound: the Bernoulli table is nine
       entries, so odd M reaches 19 and even M only 18. */
    for (uint32_t m = 1; m <= 24; m++)
      {
        const int in_range = (m % 2 != 0) ? (m <= 19) : (m <= 18);
        double    g[32]; /* > 24 + 1: the sentinel read below is g[m] */
        for (uint32_t i = 0; i < 32; i++)
          g[i] = -12345.0;
        ciccompmf (g, 4, 16, m);

        double sum = 0.0;
        for (uint32_t i = 0; i < m; i++)
          sum += g[i];
        DP_CHECK (fabs (sum - (in_range ? 1.0 : 0.0)) < 1e-9);

        /* It writes exactly M and not one more, at every M -- which is
           the contract a caller sizes the buffer from. */
        DP_CHECK (g[m] == -12345.0);
      }

    /* M = 0 writes nothing at all, so the sentinel survives. */
    {
      double g[4] = { -12345.0, -12345.0, -12345.0, -12345.0 };
      ciccompmf (g, 4, 16, 0);
      DP_CHECK (g[0] == -12345.0);
    }
  }

  DP_TEST_END ("test_resample_core");
}
