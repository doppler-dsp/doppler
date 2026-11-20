/**
 * @file test_ofdm_core.c
 * @brief The general OFDM modulator: its layout, its render, its state.
 *
 * What anchors each check, since the module is general and names no standard:
 *
 *   - the STRUCTURE is held by inverting the primitive: a forward transform
 *     of each OFDM symbol's body, written here as a plain double-precision
 *     sum, must return the grid that went in. The expected grid is spelled
 *     out in this file from the layout's own words (which subcarriers are
 *     data, which hold a pilot in which phase), not computed by the module;
 *   - the SHAPE of a symbol (prefix, cyclic body) and of the window are held
 *     at literals derivable by hand from the header's contract;
 *   - the STATE is held by resuming: a render split at every OFDM-symbol
 *     boundary, through get/set into a fresh modulator, must equal one call
 *     bit for bit.
 *
 * The full render is held against an independent numpy implementation in
 * src/doppler/ofdm/tests/. No standard is named here and none is claimed.
 */
#include "doppler/ofdm/ofdm_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TWO_PI 6.283185307179586476925286766559

typedef float _Complex cf;

/* Coefficient k of the forward sum over N samples: (1/N) sum x[m] e^{-j 2 pi
   k m / N}. The inverse of the module's own transform, and nothing else. */
static double _Complex coef (const cf *x, unsigned n, int k)
{
  double _Complex s = 0;
  for (unsigned m = 0; m < n; m++)
    s += (double _Complex)x[m]
         * cexp (-I * TWO_PI * (double)k * (double)m / (double)n);
  return s / (double)n;
}

static int
close_c (double _Complex a, double _Complex b, double tol)
{
  return cabs (a - b) <= tol;
}

/* A class with its optional arrays: the class, then its gains and polarity. */
static int
add_cls (dp_ofdm_state_t *o, uint32_t n_fft, const uint8_t *map,
         size_t map_len, uint32_t useful, uint32_t cp, uint32_t window,
         double scale, const cf *gain, size_t gain_len, uint32_t n_phase,
         const cf *pol, size_t pol_len)
{
  const int i = dp_ofdm_add_class (o, n_fft, map, map_len, useful, cp, window,
                                   scale, n_phase);
  if (i < 0)
    return i;
  if (gain_len && dp_ofdm_add_subcarrier_gain (o, (uint32_t)i, gain, gain_len))
    return -1;
  if (pol_len && dp_ofdm_add_polarity (o, (uint32_t)i, pol, pol_len))
    return -1;
  return i;
}

/* An 8-point class: k = -4 and DC null, six data subcarriers. */
static const uint8_t MAP8[8]  = { 0, 1, 1, 1, 0, 1, 1, 1 };
static const int     DATA8[6] = { -3, -2, -1, 1, 2, 3 };

/* The layout the structure and state checks share. Two phases, a polarity
   sequence of three, a pilot in every phase on a data subcarrier (k = 3) and
   one only in phase 1 (k = -1); run A is four symbols from polarity element
   1, run B is two symbols from phase 1 with no prefix. */
/* A layout the builders refuse is a broken test, not a finding: stop. */
#define MUST(cond)                                                            \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          DP_TEST_ERR ("layout setup failed: %s\n", #cond);                   \
          exit (1);                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

static dp_ofdm_state_t *
make_layout (uint32_t window, double scale)
{
  dp_ofdm_state_t *o = dp_ofdm_create ();
  MUST (o != NULL);
  const cf pol[3] = { 1.0f, -1.0f, 2.0f * I };
  MUST (add_cls (o, 8u, MAP8, 8u, 0u, 2u, window, scale, NULL, 0u, 2u, pol, 3u)
        == 0);
  MUST (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, 3, 1.0 * I) == 0);
  MUST (dp_ofdm_add_fixed (o, 0u, 1u, -1, 2.0) == 0);
  MUST (dp_ofdm_add_run (o, 0u, 4u, OFDM_CP_DEFAULT, 1u, 0u) == 0);
  MUST (dp_ofdm_add_run (o, 0u, 2u, 0u, 0u, 1u) == 1);
  return o;
}

/* Points each of the six symbols takes: 6 data subcarriers less one pilot in
   phase 0, less two in phase 1. Phases 0 1 0 1, then 1 0. */
static const unsigned NEED[6]   = { 5, 4, 5, 4, 4, 5 };
static const unsigned SYMLEN[6] = { 10, 10, 10, 10, 8, 8 };
static const unsigned PHASE[6]  = { 0, 1, 0, 1, 1, 0 };
/* Polarity element: run A starts at 1 (-1, 2j, 1, -1), run B at 0 (1, -1). */
static const cf POLV[6] = { -1.0f, 2.0f * I, 1.0f, -1.0f, 1.0f, -1.0f };

static void
fill_points (cf *p, size_t n)
{
  for (size_t i = 0; i < n; i++)
    p[i] = (float)(1 + (int)(i % 5)) + (float)((int)(i % 3) - 1) * I;
}

static int
bit_equal (const cf *a, const cf *b, size_t n)
{
  return memcmp (a, b, n * sizeof *a) == 0;
}

int
main (void)
{
  /* ── 1. what the builders refuse ───────────────────────────────────────*/
  {
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_REQUIRE (o != NULL);
    DP_CHECK_MSG (
        add_cls (o, 1u, MAP8, 1u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u) < 0,
        "a one-point transform is not an OFDM symbol");
    DP_CHECK_MSG (
        add_cls (o, 8u, MAP8, 7u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u) < 0,
        "the map must be n_fft long");
    DP_CHECK_MSG (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 0u, NULL, 0u) < 0,
        "a pattern period of 0");
    DP_CHECK_MSG (
        add_cls (o, 8u, MAP8, 8u, 0u, 9u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u) < 0,
        "a prefix longer than the body it is copied from");
    DP_CHECK_MSG (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 9u, 1.0, NULL, 0u, 1u, NULL, 0u) < 0,
        "a window longer than the symbol");
    const uint8_t bad[8] = { 0, 1, 2, 1, 0, 1, 1, 1 };
    DP_CHECK_MSG (
        add_cls (o, 8u, bad, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u) < 0,
        "a map value other than data or null");
    const cf g[4] = { 1, 1, 1, 1 };
    DP_CHECK_MSG (o->n_cls == 0, "a refused class leaves no trace");
    DP_REQUIRE (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, 1u) == 0);
    DP_CHECK_MSG (dp_ofdm_add_subcarrier_gain (o, 0u, g, 4u) < 0,
                  "a gain array that is not n_fft long");
    DP_CHECK_MSG (dp_ofdm_add_subcarrier_gain (o, 1u, g, 8u) < 0,
                  "gains for a class that does not exist");
    DP_CHECK_MSG (dp_ofdm_add_polarity (o, 0u, g, 0u) < 0,
                  "an empty polarity sequence");
    const cf g8[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    DP_CHECK (dp_ofdm_add_subcarrier_gain (o, 0u, g8, 8u) == 0);
    DP_CHECK_MSG (dp_ofdm_add_subcarrier_gain (o, 0u, g8, 8u) < 0,
                  "a class has one set of gains");
    DP_CHECK (dp_ofdm_add_polarity (o, 0u, g8, 8u) == 0);
    DP_CHECK_MSG (dp_ofdm_add_polarity (o, 0u, g8, 8u) < 0,
                  "and one polarity sequence");
    dp_ofdm_destroy (o);
    o = dp_ofdm_create ();
    DP_REQUIRE (o != NULL);

    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 2u, NULL, 0u)
        == 0);
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 1u, 0u, 0, 1.0) < 0,
                  "a fixed entry for a class that does not exist");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 2u, 0, 1.0) < 0,
                  "a phase at or past the period");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, -5, 1.0) < 0,
                  "k below -n_fft/2");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, 4, 1.0) < 0,
                  "k at n_fft/2 (the top is n_fft/2 - 1)");
    DP_CHECK (dp_ofdm_add_fixed (o, 0u, 0u, -4, 1.0) == 0);
    DP_CHECK (dp_ofdm_add_fixed (o, 0u, 0u, 3, 1.0) == 0);
    DP_CHECK_MSG (dp_ofdm_add_run (o, 1u, 1u, OFDM_CP_DEFAULT, 0u, 0u) < 0,
                  "a run of a class that does not exist");
    DP_CHECK_MSG (dp_ofdm_add_run (o, 0u, 0u, OFDM_CP_DEFAULT, 0u, 0u) < 0,
                  "a run of no OFDM symbols");
    DP_CHECK_MSG (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 2u) < 0,
                  "a first phase at or past the period");
    DP_CHECK_MSG (dp_ofdm_add_run (o, 0u, 1u, 9u, 0u, 0u) < 0,
                  "a per-run prefix longer than the body");
    DP_CHECK_MSG (dp_ofdm_check (o) < 0, "a layout with no run does not seal");
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_CHECK (dp_ofdm_check (o) == 0);
    DP_CHECK_MSG (dp_ofdm_check (o) == 0, "sealing again is idempotent");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, 1, 1.0) == 0 && !o->sealed,
                  "adding to a sealed layout re-opens it");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, 9, 1.0) < 0 && !o->sealed,
                  "a refused add leaves it as it was");
    DP_CHECK (dp_ofdm_check (o) == 0 && o->sealed);
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, 99, 1.0) < 0 && o->sealed,
                  "and a refused add does not re-open a sealed one");
    dp_ofdm_destroy (o);

    /* Two entries on one subcarrier in one phase make the grid ambiguous. */
    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 2u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, 2, 1.0) == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, 1u, 2, 1.0) == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_CHECK_MSG (dp_ofdm_check (o) < 0,
                  "an all-phase entry and a phase entry on one subcarrier");
    DP_CHECK (dp_ofdm_points_remaining (o) == 0);
    dp_ofdm_destroy (o);

    /* A per-run prefix that makes the symbol shorter than its window. */
    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 2u, 6u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, 0u, 0u, 0u) == 0);
    DP_CHECK_MSG (dp_ofdm_check (o) == 0, "window 6 fits an 8-sample symbol");
    dp_ofdm_destroy (o);
    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 4u, 2u, 6u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, 0u, 0u, 0u) == 0);
    DP_CHECK_MSG (dp_ofdm_check (o) < 0,
                  "a run whose prefix leaves the symbol shorter than the "
                  "window");
    dp_ofdm_destroy (o);
  }

  /* ── 1b. check says why ────────────────────────────────────────────────
   * dp_ofdm_check_why returns what dp_ofdm_check does and names the
   * condition that failed; each refusal beside its minimal pair (the same
   * layout with that one thing made right), which gives no sentence. */
  {
    const char      *why = "unset";
    dp_ofdm_state_t *o   = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_CHECK (dp_ofdm_check_why (o, &why) < 0);
    DP_CHECK_MSG (why && strstr (why, "no run"), "no run: says so");
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    why = "unset";
    DP_CHECK_MSG (dp_ofdm_check_why (o, &why) == 0 && why
                      && strcmp (why, "unset") == 0,
                  "a usable layout leaves *why as the caller set it");
    DP_CHECK_MSG (dp_ofdm_check_why (o, &why) == 0 && why
                      && strcmp (why, "unset") == 0,
                  "a sealed layout checks again and still leaves it");
    DP_CHECK (dp_ofdm_check_why (o, NULL) == 0);
    dp_ofdm_destroy (o);

    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 4u, 2u, 6u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, 0u, 0u, 0u) == 0);
    DP_CHECK (dp_ofdm_check_why (o, &why) < 0);
    DP_CHECK_MSG (why && strstr (why, "window"), "a long window: says so");
    dp_ofdm_destroy (o);

    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 2u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, 2, 1.0) == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, 1u, 2, 1.0) == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_CHECK (dp_ofdm_check_why (o, &why) < 0);
    DP_CHECK_MSG (why && strstr (why, "one subcarrier"),
                  "two fixed entries on one subcarrier: says so");
    dp_ofdm_destroy (o);

    why = "unset";
    DP_CHECK (dp_ofdm_check_why (NULL, &why) == -1 && why != NULL);
  }

  /* ── 2. the grid, by inverting the primitive ───────────────────────────*/
  {
    const double     scale = 0.5;
    dp_ofdm_state_t *o     = make_layout (0u, scale);
    DP_CHECK (dp_ofdm_points_remaining (o) == 27);
    cf pts[27];
    fill_points (pts, 27);
    cf out[56];
    DP_CHECK_MSG (dp_ofdm_modulate_max_out (o, 27) == 56,
                  "four symbols of 10 and two of 8 samples");
    DP_CHECK (dp_ofdm_modulate (o, pts, 27, out, 56) == 56);
    DP_CHECK (dp_ofdm_points_remaining (o) == 0);

    size_t at = 0, used = 0;
    for (int s = 0; s < 6; s++)
      {
        const unsigned cp   = SYMLEN[s] - 8u;
        const cf      *body = out + at + cp;

        /* the expected grid, from the layout's words */
        double _Complex want[8];
        for (int i = 0; i < 8; i++)
          want[i] = 0;
        int taken_k3 = 1, taken_km1 = PHASE[s] == 1;
        want[3 + 4] = I * (double _Complex)POLV[s];
        if (taken_km1)
          want[-1 + 4] = 2.0 * (double _Complex)POLV[s];
        unsigned drawn = 0;
        for (int i = 0; i < 6; i++)
          {
            const int k = DATA8[i];
            if ((k == 3 && taken_k3) || (k == -1 && taken_km1))
              continue;
            want[k + 4] = (double _Complex)pts[used + drawn];
            drawn++;
          }
        DP_CHECK_MSG (drawn == NEED[s], "the data count of the phase");
        for (int k = -4; k < 4; k++)
          DP_CHECK_MSG (
              close_c (coef (body, 8u, k) / scale, want[k + 4], 2e-6),
              "the forward transform of a body returns the grid");
        /* the cyclic prefix is the tail of the body */
        for (unsigned i = 0; i < cp; i++)
          DP_CHECK_MSG (out[at + i] == body[8 - cp + i],
                        "the prefix is the last samples of the body");
        used += drawn;
        at += SYMLEN[s];
      }
    DP_CHECK (used == 27 && at == 56);
    dp_ofdm_destroy (o);
  }

  /* ── 3. the shape of a symbol, at a literal ────────────────────────────*/
  {
    /* One data subcarrier, k = 1, in a 4-point transform: x[m] = j^m, which
       is 1, j, -1, -j. A body of ten samples reads it cyclically, and a
       prefix of three is the last three of the ten. */
    const uint8_t    map4[4] = { 0, 0, 0, 1 }; /* index k + 2: k = 1 is 3 */
    dp_ofdm_state_t *o       = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 4u, map4, 4u, 10u, 3u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    const cf one = 1.0f;
    cf       out[13];
    DP_REQUIRE (dp_ofdm_modulate (o, &one, 1, out, 13) == 13);
    const cf want[13] = { -I,   1.0f, I, /* the prefix: body[7..9] */
                          1.0f, I,    -1.0f, -I, 1.0f, I, -1.0f, -I, 1.0f, I };
    for (int i = 0; i < 13; i++)
      DP_CHECK_MSG (cabsf (out[i] - want[i]) < 1e-6f,
                    "a body read cyclically from the transform, prefixed");
    dp_ofdm_destroy (o);
  }

  /* ── 4. the subcarrier gain, the scale and the polarity ────────────────*/
  {
    /* Gain 2j on subcarrier k = 1 and 3 on k = -1; scale 0.25; one fixed
       entry whose polarity follows the sequence from element 2. */
    cf gain[8];
    for (int i = 0; i < 8; i++)
      gain[i] = 1.0f;
    gain[1 + 4]             = 2.0f * I;
    gain[-1 + 4]            = 3.0f;
    const cf         pol[3] = { 1.0f, -1.0f, 2.0f * I };
    dp_ofdm_state_t *o      = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 0.25, gain, 8u, 1u, pol, 3u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, 0u, 2, 1.0) == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 4u, OFDM_CP_DEFAULT, 2u, 0u) == 0);
    cf pts[20], out[32];
    fill_points (pts, 20);
    DP_REQUIRE (dp_ofdm_points_remaining (o) == 20);
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 20, out, 32) == 32);
    const cf pv[4]
        = { 2.0f * I, 1.0f, -1.0f, 2.0f * I }; /* elements 2,0,1,2 */
    for (int s = 0; s < 4; s++)
      {
        const cf *body = out + 8 * s;
        unsigned  d    = 0;
        for (int k = -4; k < 4; k++)
          {
            double _Complex want = 0;
            if (k == 2)
              want = (double _Complex)pv[s];
            else if (MAP8[k + 4])
              want = (double _Complex)pts[5 * s + d++];
            if (k == 1)
              want *= 2.0 * I;
            if (k == -1)
              want *= 3.0;
            DP_CHECK_MSG (close_c (coef (body, 8u, k) / 0.25, want, 4e-6),
                          "gain multiplies the grid, polarity multiplies "
                          "only the fixed entry, scale the transform");
          }
      }
    dp_ofdm_destroy (o);
  }

  /* ── 5. the window, against its formula ────────────────────────────────*/
  {
    /* cp = 0, a body of exactly one transform, so the continuation of a
       symbol past its end is its own first samples. A: k = 1, B: j times
       it; window 2: rise = sin^2 (pi (j + 1/2) / 4). */
    const uint8_t    map4[4] = { 0, 0, 0, 1 };
    dp_ofdm_state_t *o       = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 4u, map4, 4u, 0u, 0u, 2u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    const cf pts[2] = { 1.0f, I };
    cf       out[8], tail[2];
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 2, out, 8) == 8);
    DP_CHECK_MSG (dp_ofdm_finish_max_out (o, 99) == 2, "a held tail of 2");
    DP_CHECK (dp_ofdm_finish (o, 2, tail, 2) == 2);
    DP_CHECK_MSG (dp_ofdm_finish (o, 2, tail, 2) == 0, "finish clears it");

    const double r0 = sin (M_PI * 0.5 / 4.0), r1 = sin (M_PI * 1.5 / 4.0);
    const double rise[2] = { r0 * r0, r1 * r1 };
    cf           A[4], B[4];
    for (int m = 0; m < 4; m++)
      {
        A[m] = cexpf (I * (float)(TWO_PI * m / 4.0));
        B[m] = I * A[m];
      }
    cf want[8];
    for (int i = 0; i < 4; i++)
      want[i] = A[i] * (float)(i < 2 ? rise[i] : 1.0);
    for (int i = 0; i < 4; i++)
      want[4 + i] = B[i] * (float)(i < 2 ? rise[i] : 1.0)
                    + (i < 2 ? A[i] * (float)(1.0 - rise[i]) : 0.0f);
    for (int i = 0; i < 8; i++)
      DP_CHECK_MSG (cabsf (out[i] - want[i]) < 1e-6f,
                    "the rise on entry, the fall of the one before");
    for (int j = 0; j < 2; j++)
      DP_CHECK_MSG (cabsf (tail[j] - B[j] * (float)(1.0 - rise[j])) < 1e-6f,
                    "the last symbol's continuation, faded out");
    dp_ofdm_destroy (o);

    /* A body that is not a whole number of transforms: the continuation is
       the TRANSFORM's next samples, x[(useful + j) mod n_fft], not the
       body's first. useful 6 of 4: x[6] = x[2], x[7] = x[3]. */
    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 4u, map4, 4u, 6u, 0u, 2u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    cf o1[6], t1[2];
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 1, o1, 6) == 6);
    DP_REQUIRE (dp_ofdm_finish (o, 2, t1, 2) == 2);
    for (int j = 0; j < 2; j++)
      DP_CHECK_MSG (cabsf (t1[j] - A[(6 + j) % 4] * (float)(1.0 - rise[j]))
                        < 1e-6f,
                    "continued from the transform at (useful + j) mod n_fft");
    dp_ofdm_destroy (o);

    /* Two classes with different windows: the boundary uses the LATER
       symbol's class, for the rise and for the continuation alike. First
       class window 1, second 3, bodies of exactly 4. */
    o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 4u, map4, 4u, 0u, 0u, 1u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (
        add_cls (o, 4u, map4, 4u, 0u, 0u, 3u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 1);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 1u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    cf o2[8], t2[3];
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 2, o2, 8) == 8);
    DP_REQUIRE (dp_ofdm_finish (o, 3, t2, 3) == 3);
    double r3[3];
    for (int j = 0; j < 3; j++)
      {
        const double s = sin (M_PI * (j + 0.5) / 6.0);
        r3[j]          = s * s;
      }
    DP_CHECK_MSG (cabsf (o2[0] - A[0] * 0.5f) < 1e-6f,
                  "a window of 1 rises by sin^2 (pi/4) = 1/2");
    for (int i = 0; i < 4; i++)
      {
        const cf v = B[i] * (float)(i < 3 ? r3[i] : 1.0)
                     + (i < 3 ? A[i % 4] * (float)(1.0 - r3[i]) : 0.0f);
        DP_CHECK_MSG (cabsf (o2[4 + i] - v) < 1e-6f,
                      "the boundary's length is the later class's window");
      }
    for (int j = 0; j < 3; j++)
      DP_CHECK (cabsf (t2[j] - B[j] * (float)(1.0 - r3[j])) < 1e-6f);
    dp_ofdm_destroy (o);

    /* Identical symbols, back to back: a cross-fade of equal continuations
       is the signal itself, so apart from the first window's rise and the
       last window's tail the output is the windowless render. */
    dp_ofdm_state_t *w = dp_ofdm_create (), *n0 = dp_ofdm_create ();
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *q = pass ? n0 : w;
        DP_REQUIRE (add_cls (q, 4u, map4, 4u, 0u, 0u, pass ? 0u : 3u, 1.0,
                             NULL, 0u, 1u, NULL, 0u)
                    == 0);
        DP_REQUIRE (dp_ofdm_add_run (q, 0u, 5u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
      }
    const cf same[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    cf       ow[20], on[20];
    DP_REQUIRE (dp_ofdm_modulate (w, same, 5, ow, 20) == 20);
    DP_REQUIRE (dp_ofdm_modulate (n0, same, 5, on, 20) == 20);
    for (int i = 3; i < 20; i++)
      DP_CHECK_MSG (cabsf (ow[i] - on[i]) < 2e-6f,
                    "a cross-fade of equal signals is that signal");
    for (int i = 0; i < 3; i++)
      DP_CHECK (cabsf (ow[i]) < cabsf (on[i]) + 1e-6f);
    dp_ofdm_destroy (w);
    dp_ofdm_destroy (n0);
  }

  /* ── 6. what a call refuses, and that a refusal changes nothing ────────*/
  {
    dp_ofdm_state_t *o = make_layout (0u, 1.0);
    cf               pts[40], out[64], ref[64];
    fill_points (pts, 40);
    DP_CHECK (dp_ofdm_modulate (o, pts, 3, out, 64) == 0);
    DP_CHECK_MSG (dp_ofdm_modulate_max_out (o, 3) == 0,
                  "3 points ends inside the 5 the first symbol takes");
    DP_CHECK (dp_ofdm_modulate (o, pts, 28, out, 64) == 0);
    DP_CHECK_MSG (dp_ofdm_modulate (o, pts, 27, out, 55) == 0,
                  "short capacity is a refusal, not a truncation");
    DP_CHECK_MSG (dp_ofdm_points_remaining (o) == 27,
                  "no refusal moved the position");
    DP_CHECK (dp_ofdm_modulate (o, pts, 0, out, 64) == 0);
    DP_CHECK_MSG (dp_ofdm_points_remaining (o) == 27,
                  "no points renders nothing when the next symbol needs some");

    /* the same render from the start, whole, as the reference */
    DP_CHECK (dp_ofdm_modulate (o, pts, 27, ref, 64) == 56);
    DP_CHECK (dp_ofdm_modulate (o, pts, 0, out, 64) == 0);
    DP_CHECK_MSG (dp_ofdm_modulate (o, pts, 5, out, 64) == 0,
                  "points past the end of the layout");
    /* The _why form names each refusal, and tells a refusal (negative, a
       sentence) from a valid render of nothing (0, *why left as set). */
    {
      const char *why = "unset";
      DP_CHECK_MSG (dp_ofdm_modulate_why (o, pts, 0, out, 64, &why) == 0 && why
                        && strcmp (why, "unset") == 0,
                    "no points at the end of the layout is not a refusal");
      DP_CHECK (dp_ofdm_modulate_why (o, pts, 5, out, 64, &why) < 0);
      DP_CHECK_MSG (why && strstr (why, "more points than the layout"),
                    "points past the end say so");
      dp_ofdm_reset (o);
      why = "unset";
      DP_CHECK_MSG (dp_ofdm_modulate_why (o, pts, 0, out, 64, &why) == 0 && why
                        && strcmp (why, "unset") == 0,
                    "zero points mid-layout is a valid 0, not a refusal");
      DP_CHECK (dp_ofdm_modulate_why (o, pts, 3, out, 64, &why) < 0);
      DP_CHECK_MSG (why && strstr (why, "inside an OFDM symbol"),
                    "points ending inside a symbol say so");
      DP_CHECK (dp_ofdm_modulate_why (o, pts, 27, out, 55, &why) < 0);
      DP_CHECK_MSG (why && strstr (why, "shorter"), "a short buffer says so");
      DP_CHECK_MSG (dp_ofdm_modulate_why (o, pts, 27, out, 64, NULL) == 56,
                    "a NULL why is allowed, and a good call writes");
      why = "unset";
      DP_CHECK_MSG (dp_ofdm_modulate_why (o, pts, 0, out, 64, &why) == 0 && why
                        && strcmp (why, "unset") == 0,
                    "a success leaves *why as the caller set it");
      /* an unusable layout is refused with the sentence check gives */
      dp_ofdm_state_t *bare = dp_ofdm_create ();
      const char      *want = NULL;
      DP_REQUIRE (dp_ofdm_check_why (bare, &want) < 0 && want);
      why = NULL;
      DP_CHECK (dp_ofdm_modulate_why (bare, pts, 0, out, 64, &why) < 0);
      DP_CHECK_MSG (why == want,
                    "an unusable layout gets check's own sentence");
      dp_ofdm_destroy (bare);
    }
    dp_ofdm_reset (o);
    DP_CHECK_MSG (dp_ofdm_points_remaining (o) == 27, "reset");
    DP_CHECK (dp_ofdm_modulate (o, pts, 27, out, 64) == 56);
    DP_CHECK_MSG (bit_equal (out, ref, 56),
                  "a reset render equals the first, bit for bit");
    dp_ofdm_destroy (o);
  }

  /* ── 6b. a refusal leaves the output and the held window tail alone ────
   * A WINDOWED layout, one symbol rendered so a tail is held, the output
   * poisoned; every refusal must leave both the output and the blob (which
   * carries the tail and the position) byte for byte as they were. */
  {
    dp_ofdm_state_t *w = make_layout (2u, 1.0);
    cf               pts[40], out[64], poison[64];
    fill_points (pts, 40);
    DP_REQUIRE (dp_ofdm_modulate (w, pts, NEED[0], out, 64) == SYMLEN[0]);
    const size_t   nb = dp_ofdm_state_bytes (w);
    unsigned char *b0 = malloc (nb), *b1 = malloc (nb);
    DP_REQUIRE (b0 && b1);
    dp_ofdm_get_state (w, b0);
    memset (poison, 0xA5, sizeof poison);
    const size_t before = dp_ofdm_points_remaining (w);
    const size_t n[3]   = { NEED[1] - 1u, 40u, NEED[1] };
    const size_t cap[3] = { 64u, 64u, SYMLEN[1] - 1u };
    const char  *tag[3] = { "ends inside a symbol", "runs past the layout",
                            "a buffer one short" };
    for (int r = 0; r < 3; r++)
      {
        memcpy (out, poison, sizeof out);
        DP_CHECK_MSG (dp_ofdm_modulate_why (w, pts, n[r], out, cap[r], NULL)
                          < 0,
                      tag[r]);
        DP_CHECK_MSG (memcmp (out, poison, sizeof out) == 0,
                      "a refusal writes nothing to out");
        dp_ofdm_get_state (w, b1);
        DP_CHECK_MSG (memcmp (b0, b1, nb) == 0,
                      "a refusal leaves the position and the window tail");
        DP_CHECK_MSG (dp_ofdm_points_remaining (w) == before,
                      "a refusal does not move the layout");
      }
    free (b0);
    free (b1);
    dp_ofdm_destroy (w);
  }

  /* A reset forgets the held window tail along with the position. */
  {
    dp_ofdm_state_t *o = make_layout (3u, 1.0);
    cf               pts[27], first[64], second[64];
    fill_points (pts, 27);
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 27, first, 64) == 56);
    dp_ofdm_reset (o);
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 27, second, 64) == 56);
    DP_CHECK_MSG (bit_equal (first, second, 56),
                  "a windowed render after reset starts from silence");
    dp_ofdm_destroy (o);
  }

  /* ── 7. a training field takes no points ───────────────────────────────*/
  {
    const uint8_t    none8[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    dp_ofdm_state_t *o        = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 8u, none8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_fixed (o, 0u, 0u, 1, 1.0) == 0);
    DP_REQUIRE (
        add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 1);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 1u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    DP_CHECK_MSG (dp_ofdm_points_remaining (o) == 6,
                  "two training symbols take nothing, the data symbol six");
    cf pts[6], out[24];
    fill_points (pts, 6);
    DP_CHECK_MSG (dp_ofdm_modulate (o, pts, 0, out, 24) == 16,
                  "with no points, the symbols that need none render");
    DP_CHECK (dp_ofdm_points_remaining (o) == 6);
    DP_CHECK (dp_ofdm_modulate (o, pts, 6, out, 24) == 8);
    dp_ofdm_destroy (o);
  }

  /* ── 8. a transform that is not a power of two ─────────────────────────*/
  {
    uint8_t map12[12];
    memset (map12, 1, sizeof map12);
    map12[6]           = 0;
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_REQUIRE (
        add_cls (o, 12u, map12, 12u, 0u, 3u, 0u, 1.0, NULL, 0u, 1u, NULL, 0u)
        == 0);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    cf pts[11], out[15];
    fill_points (pts, 11);
    DP_REQUIRE (dp_ofdm_modulate (o, pts, 11, out, 15) == 15);
    unsigned d = 0;
    for (int k = -6; k < 6; k++)
      {
        const double _Complex want = k == 0 ? 0 : (double _Complex)pts[d++];
        DP_CHECK_MSG (close_c (coef (out + 3, 12u, k), want, 4e-6),
                      "any transform length");
      }
    dp_ofdm_destroy (o);
  }

  /* ── 9. the state: resume at every OFDM-symbol boundary ────────────────*/
  {
    for (uint32_t window = 0; window <= 3; window += 3)
      {
        dp_ofdm_state_t *ref = make_layout (window, 1.0);
        cf               pts[27], whole[64], tail_ref[3];
        fill_points (pts, 27);
        const size_t nw = dp_ofdm_modulate (ref, pts, 27, whole, 64);
        DP_REQUIRE (nw == 56);
        const size_t nt = dp_ofdm_finish (ref, 3, tail_ref, 3);
        DP_CHECK (nt == window);

        for (int split = 1; split <= 5; split++)
          {
            size_t p1 = 0, o1 = 0;
            for (int s = 0; s < split; s++)
              {
                p1 += NEED[s];
                o1 += SYMLEN[s];
              }
            dp_ofdm_state_t *a = make_layout (window, 1.0);
            cf               part1[64], part2[64], tail2[3];
            DP_REQUIRE (dp_ofdm_modulate (a, pts, p1, part1, 64) == o1);
            const size_t nb   = dp_ofdm_state_bytes (a);
            void        *blob = malloc (nb);
            DP_REQUIRE (blob != NULL);
            dp_ofdm_get_state (a, blob);

            dp_ofdm_state_t *b = make_layout (window, 1.0);
            DP_REQUIRE (dp_ofdm_set_state (b, blob) == DP_OK);
            DP_CHECK (dp_ofdm_points_remaining (b) == 27 - p1);
            DP_REQUIRE (dp_ofdm_modulate (b, pts + p1, 27 - p1, part2, 64)
                        == 56 - o1);
            DP_CHECK_MSG (bit_equal (part1, whole, o1)
                              && bit_equal (part2, whole + o1, 56 - o1),
                          "a split render, through the state, equals one "
                          "call bit for bit");
            DP_CHECK (dp_ofdm_finish (b, 3, tail2, 3) == nt);
            DP_CHECK_MSG (bit_equal (tail2, tail_ref, nt),
                          "and so does the window's final fall");

            /* a round trip through the same object changes nothing */
            void *again = malloc (nb);
            dp_ofdm_get_state (a, again);
            DP_CHECK (dp_ofdm_set_state (a, blob) == DP_OK);
            dp_ofdm_get_state (a, again);
            DP_CHECK (memcmp (blob, again, nb) == 0);
            free (again);
            free (blob);
            dp_ofdm_destroy (a);
            dp_ofdm_destroy (b);
          }
        dp_ofdm_destroy (ref);
      }

    /* what set_state refuses, and that a refusal leaves the object as it
       was: a render continued after each rejection still equals one call. */
    dp_ofdm_state_t *ref = make_layout (3u, 1.0);
    cf               pts[27], whole[64];
    fill_points (pts, 27);
    DP_REQUIRE (dp_ofdm_modulate (ref, pts, 27, whole, 64) == 56);

    dp_ofdm_state_t *src = make_layout (3u, 1.0);
    cf               part[64];
    DP_REQUIRE (dp_ofdm_modulate (src, pts, 9, part, 64) == 20);
    const size_t nb   = dp_ofdm_state_bytes (src);
    uint8_t     *blob = malloc (nb), *bad = malloc (nb);
    dp_ofdm_get_state (src, blob);

    dp_ofdm_state_t *dst = make_layout (3u, 1.0);
    DP_REQUIRE (dp_ofdm_modulate (dst, pts, 9, part, 64) == 20);

    memcpy (bad, blob, nb);
    bad[0] ^= 0xFF;
    DP_CHECK_MSG (dp_ofdm_set_state (dst, bad) == DP_ERR_INVALID,
                  "a clobbered envelope");

    dp_ofdm_state_t *other = make_layout (3u, 2.0);
    DP_CHECK_MSG (dp_ofdm_state_bytes (other) == nb,
                  "a layout that differs only in scale has the same size, so "
                  "only the hash can tell them apart");
    DP_CHECK_MSG (dp_ofdm_set_state (other, blob) == DP_ERR_INVALID,
                  "a blob from a different layout is a reject key");
    dp_ofdm_destroy (other);

    /* offsets: header 16, extra {hash u64, run u32, sym u32, tail u32, win} */
    uint32_t v;
    memcpy (bad, blob, nb);
    v = 7;
    memcpy (bad + 24, &v, 4);
    DP_CHECK_MSG (dp_ofdm_set_state (dst, bad) == DP_ERR_INVALID,
                  "a run index past the layout");
    memcpy (bad, blob, nb);
    v = 9;
    memcpy (bad + 28, &v, 4);
    DP_CHECK_MSG (dp_ofdm_set_state (dst, bad) == DP_ERR_INVALID,
                  "a symbol count past the run");
    memcpy (bad, blob, nb);
    v = 2;
    memcpy (bad + 32, &v, 4);
    DP_CHECK_MSG (dp_ofdm_set_state (dst, bad) == DP_ERR_INVALID,
                  "a held tail that is not the window the position implies");

    cf rest[64];
    DP_CHECK_MSG (dp_ofdm_points_remaining (dst) == 18,
                  "no refusal moved the position");
    DP_REQUIRE (dp_ofdm_modulate (dst, pts + 9, 18, rest, 64) == 36);
    DP_CHECK_MSG (bit_equal (part, whole, 20)
                      && bit_equal (rest, whole + 20, 36),
                  "the object continues as if nothing was refused");
    DP_CHECK (dp_ofdm_set_state (dst, blob) == DP_OK);
    DP_CHECK_MSG (dp_ofdm_points_remaining (dst) == 18,
                  "and the good blob is accepted");

    free (blob);
    free (bad);
    dp_ofdm_destroy (src);
    dp_ofdm_destroy (dst);
    dp_ofdm_destroy (ref);
  }

  /* ── the shared round trip, mid-render with a window tail held ─────────*/
  {
    dp_ofdm_state_t *a = make_layout (3u, 1.0), *b = make_layout (3u, 1.0);
    cf               pts[27], out[64];
    fill_points (pts, 27);
    MUST (dp_ofdm_modulate (a, pts, 9, out, 64) == 20);
    DP_CHECK (a->tail_len == 3);
    DP_STATE_ROUNDTRIP_TEST (dp_ofdm, a, b);
    DP_CHECK (b->run_index == a->run_index && b->sym_in_run == a->sym_in_run
              && b->tail_len == a->tail_len);
    dp_ofdm_destroy (a);
    dp_ofdm_destroy (b);
  }

  /* ── a signal extended while it is rendered ────────────────────────────*/
  {
    /* Run A rendered, run B of the same class added, the rest rendered: the
       position and the held window tail survive the re-open, so the whole
       equals one call over the full layout. */
    dp_ofdm_state_t *g = dp_ofdm_create (), *w = dp_ofdm_create ();
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *x = pass ? w : g;
        MUST (
            add_cls (x, 8u, MAP8, 8u, 0u, 2u, 3u, 1.0, NULL, 0u, 1u, NULL, 0u)
            == 0);
        MUST (dp_ofdm_add_run (x, 0u, 3u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
      }
    MUST (dp_ofdm_add_run (w, 0u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    cf pts[30], whole[64], p1[64], p2[64];
    fill_points (pts, 30);
    MUST (dp_ofdm_modulate (w, pts, 30, whole, 64) == 50);
    MUST (dp_ofdm_modulate (g, pts, 18, p1, 64) == 30);
    MUST (dp_ofdm_add_run (g, 0u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    DP_CHECK_MSG (dp_ofdm_points_remaining (g) == 12,
                  "the added run is the rest of the layout");
    MUST (dp_ofdm_modulate (g, pts + 18, 12, p2, 64) == 20);
    DP_CHECK_MSG (bit_equal (p1, whole, 30) && bit_equal (p2, whole + 30, 20),
                  "an extended layout renders as if built whole");
    dp_ofdm_destroy (g);
    dp_ofdm_destroy (w);
  }

  /* ── 10. a layout is its content, not the order it was entered in ──────*/
  {
    /* The same fixed entries entered in the opposite order are the same
       layout: the pattern is held sorted by (phase, k), so the reject key
       is canonical and a blob restores across the two. */
    dp_ofdm_state_t *p = dp_ofdm_create (), *q = dp_ofdm_create ();
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *x = pass ? q : p;
        MUST (
            add_cls (x, 8u, MAP8, 8u, 0u, 2u, 0u, 1.0, NULL, 0u, 3u, NULL, 0u)
            == 0);
        const int      ks[4] = { 3, -1, 2, -3 };
        const uint32_t ph[4] = { 1u, 0u, OFDM_ALL_PHASES, 2u };
        for (int i = 0; i < 4; i++)
          {
            const int j = pass ? 3 - i : i;
            MUST (dp_ofdm_add_fixed (x, 0u, ph[j], ks[j], 1.0 + j) == 0);
          }
        MUST (dp_ofdm_add_run (x, 0u, 3u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
      }
    DP_CHECK_MSG (dp_ofdm_points_remaining (p) == dp_ofdm_points_remaining (q),
                  "the same pattern, entered in either order");
    cf pts[20], out[40];
    fill_points (pts, 20);
    const size_t first = dp_ofdm_points_remaining (p) / 3;
    MUST (dp_ofdm_modulate (p, pts, first, out, 40) > 0);
    void *blob = malloc (dp_ofdm_state_bytes (p));
    dp_ofdm_get_state (p, blob);
    DP_CHECK_MSG (dp_ofdm_set_state (q, blob) == DP_OK,
                  "a blob restores across the two insertion orders");
    free (blob);

    /* A reset object is, byte for byte, a fresh one. */
    dp_ofdm_state_t *r = make_layout (3u, 1.0), *f = make_layout (3u, 1.0);
    cf               all[27], big[64];
    fill_points (all, 27);
    MUST (dp_ofdm_modulate (r, all, 27, big, 64) == 56);
    dp_ofdm_reset (r);
    const size_t nb = dp_ofdm_state_bytes (r);
    void        *br = malloc (nb), *bf = malloc (nb);
    dp_ofdm_get_state (r, br);
    dp_ofdm_get_state (f, bf);
    DP_CHECK_MSG (memcmp (br, bf, nb) == 0,
                  "reset returns the position and the held tail");
    free (br);
    free (bf);
    dp_ofdm_destroy (r);
    dp_ofdm_destroy (f);
    dp_ofdm_destroy (p);
    dp_ofdm_destroy (q);
  }

  /* ── Refusals that had no test, and what a partial finish does ──────────
   * Each is a branch of ofdm_core.c that the suite above never took (read
   * from gcov, not guessed). Each refusal has its minimal pair: the same call
   * with its one argument made good succeeds. The allocation failures are not
   * here: nothing in the suite can make an allocation fail. */
  {
    /* the sentinels the docs and the Python surface name as 2**32 - 1 */
    DP_CHECK (OFDM_ALL_PHASES == 4294967295u
              && OFDM_CP_DEFAULT == 4294967295u);

    /* NULL is refused everywhere it could be passed */
    dp_ofdm_destroy (NULL);
    DP_CHECK (dp_ofdm_check (NULL) == -1);
    DP_CHECK (dp_ofdm_modulate_max_out (NULL, 4u) == 0);
    cf one[8];
    DP_CHECK (dp_ofdm_modulate (NULL, one, 1u, one, 8u) == 0);
    DP_CHECK (dp_ofdm_finish_max_out (NULL, 4u) == 0);
    DP_CHECK (dp_ofdm_finish (NULL, 4u, one, 8u) == 0);

    /* add_polarity: each argument refused, then the good call */
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_REQUIRE (o != NULL);
    DP_REQUIRE (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, 1u) == 0);
    const cf pol[2] = { 1.0f, -1.0f };
    DP_CHECK (dp_ofdm_add_polarity (NULL, 0u, pol, 2u) < 0);
    DP_CHECK (dp_ofdm_add_polarity (o, 1u, pol, 2u) < 0);
    DP_CHECK (dp_ofdm_add_polarity (o, 0u, NULL, 2u) < 0);
    DP_CHECK (dp_ofdm_add_polarity (o, 0u, pol, 2u) == 0);

    /* a layout with a class and no run fails its check: modulate and its
     * capacity return nothing, and with a run the same calls work */
    cf pts[8], out[64];
    fill_points (pts, 8);
    DP_CHECK (dp_ofdm_check (o) < 0);
    DP_CHECK (dp_ofdm_modulate_max_out (o, 6u) == 0);
    DP_CHECK (dp_ofdm_modulate (o, pts, 6u, out, 64u) == 0);
    /* set_state has no position to restore into either */
    uint8_t any[256] = { 0 };
    DP_CHECK (dp_ofdm_set_state (o, any) == DP_ERR_INVALID);
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_CHECK (dp_ofdm_check (o) == 0);
    DP_CHECK (dp_ofdm_modulate_max_out (o, 6u) > 0);
    DP_CHECK (dp_ofdm_modulate (o, pts, 6u, out, 64u) > 0);
    dp_ofdm_destroy (o);

    /* A partial finish: the held tail is two samples. Asked for one, finish
     * returns one and CLEARS the rest (documented: "return ... and clear
     * it"), so a second call has nothing. Capped by max_out the same way. */
    dp_ofdm_state_t *f = make_layout (2u, 1.0);
    cf               x[27], y[64], t[4];
    fill_points (x, 27);
    DP_REQUIRE (dp_ofdm_modulate (f, x, 27, y, 64) > 0);
    DP_REQUIRE (dp_ofdm_finish_max_out (f, 99u) == 2u);
    DP_CHECK (dp_ofdm_finish_max_out (f, 1u) == 1u);
    DP_CHECK (dp_ofdm_finish (f, 1u, t, 4u) == 1u);
    DP_CHECK (dp_ofdm_finish (f, 99u, t, 4u) == 0u);
    dp_ofdm_state_t *g = make_layout (2u, 1.0);
    DP_REQUIRE (dp_ofdm_modulate (g, x, 27, y, 64) > 0);
    DP_CHECK (dp_ofdm_finish (g, 99u, t, 1u) == 1u);
    DP_CHECK (dp_ofdm_finish (g, 99u, t, 4u) == 0u);
    dp_ofdm_destroy (f);
    dp_ofdm_destroy (g);

    /* set_state at the END of the layout (the position equal to the run
     * count): a symbol count there, and a held tail that is not the last
     * class's window, are refused; the unmodified blob is accepted. Offsets
     * as above (header 16; run 24, symbol 28, tail 32). */
    dp_ofdm_state_t *e = make_layout (2u, 1.0);
    DP_REQUIRE (dp_ofdm_modulate (e, x, 27, y, 64) > 0);
    const size_t nb   = dp_ofdm_state_bytes (e);
    uint8_t     *blob = malloc (nb), *bad = malloc (nb);
    DP_REQUIRE (blob != NULL && bad != NULL);
    dp_ofdm_get_state (e, blob);
    dp_ofdm_state_t *d = make_layout (2u, 1.0);
    uint32_t         v;
    memcpy (bad, blob, nb);
    v = 1;
    memcpy (bad + 28, &v, 4);
    DP_CHECK_MSG (dp_ofdm_set_state (d, bad) == DP_ERR_INVALID,
                  "a symbol count at the end of the layout");
    memcpy (bad, blob, nb);
    v = 1;
    memcpy (bad + 32, &v, 4);
    DP_CHECK_MSG (dp_ofdm_set_state (d, bad) == DP_ERR_INVALID,
                  "a held tail that is not the last class's window");
    DP_CHECK_MSG (dp_ofdm_set_state (d, blob) == DP_OK,
                  "and the unmodified end-of-layout blob is accepted");
    free (blob);
    free (bad);
    dp_ofdm_destroy (e);
    dp_ofdm_destroy (d);
  }

  /* ── 11. The header's claims the sections above left unasserted ─────────
   * From the certification's claim inventory (tests/validation/ofdm). Each
   * claim beside its control: the nearest case the claim excludes, which must
   * fail the same assertion. */

  /* A subcarrier gain multiplies a fixed entry as well as data ("the whole
   * grid, data and fixed entries alike"). Gain 3 on k = 2, a fixed 1.0 there:
   * the grid reads 3. Control: gain 1 on k = 2 reads 1. */
  {
    cf g3[8], g1[8];
    for (int i = 0; i < 8; i++)
      g3[i] = g1[i] = 1.0f;
    g3[2 + 4]        = 3.0f;
    double    got[2] = { 0, 0 };
    const cf *gs[2]  = { g3, g1 };
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *o = dp_ofdm_create ();
        MUST (add_cls (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, gs[pass], 8u, 1u,
                       NULL, 0u)
              == 0);
        MUST (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, 2, 1.0) == 0);
        MUST (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
        cf pts[5], out[8];
        fill_points (pts, 5);
        MUST (dp_ofdm_modulate (o, pts, 5, out, 8) == 8);
        got[pass] = creal (coef (out, 8u, 2));
        dp_ofdm_destroy (o);
      }
    DP_CHECK_MSG (fabs (got[0] - 3.0) < 4e-6,
                  "a gain multiplies the fixed entry on its subcarrier");
    DP_CHECK_MSG (fabs (got[1] - 3.0) > 1.0, "control: gain 1 does not");
  }

  /* Every builder re-opens a sealed layout, not only add_fixed and add_run,
   * and the position and the held tail survive: a render continued through
   * add_class, add_subcarrier_gain and add_polarity equals the layout built
   * whole. Gain 1 and polarity 1 on a class with no fixed entry leave its
   * samples bit for bit. Control: a refused add leaves the layout sealed. */
  {
    const cf         ones[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    dp_ofdm_state_t *g = dp_ofdm_create (), *w = dp_ofdm_create ();
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *x = pass ? w : g;
        MUST (
            add_cls (x, 8u, MAP8, 8u, 0u, 2u, 3u, 1.0, NULL, 0u, 1u, NULL, 0u)
            == 0);
        MUST (dp_ofdm_add_run (x, 0u, 3u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
      }
    MUST (add_cls (w, 8u, MAP8, 8u, 0u, 2u, 3u, 1.0, ones, 8u, 1u, ones, 1u)
          == 1);
    MUST (dp_ofdm_add_run (w, 1u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    cf pts[30], whole[64], p1[64], p2[64];
    fill_points (pts, 30);
    MUST (dp_ofdm_modulate (w, pts, 30, whole, 64) == 50);
    MUST (dp_ofdm_modulate (g, pts, 18, p1, 64) == 30);
    DP_REQUIRE (g->sealed);
    DP_CHECK_MSG (dp_ofdm_add_class (g, 8u, MAP8, 8u, 0u, 2u, 3u, 1.0, 1u) == 1
                      && !g->sealed,
                  "add_class re-opens a sealed layout");
    DP_REQUIRE (dp_ofdm_check (g) == 0 && g->sealed);
    DP_CHECK_MSG (dp_ofdm_add_subcarrier_gain (g, 1u, ones, 7u) < 0
                      && g->sealed,
                  "control: a refused gain leaves it sealed");
    DP_CHECK_MSG (dp_ofdm_add_subcarrier_gain (g, 1u, ones, 8u) == 0
                      && !g->sealed,
                  "add_subcarrier_gain re-opens it");
    DP_REQUIRE (dp_ofdm_check (g) == 0 && g->sealed);
    DP_CHECK_MSG (dp_ofdm_add_polarity (g, 1u, ones, 0u) < 0 && g->sealed,
                  "control: a refused polarity leaves it sealed");
    DP_CHECK_MSG (dp_ofdm_add_polarity (g, 1u, ones, 1u) == 0 && !g->sealed,
                  "add_polarity re-opens it");
    MUST (dp_ofdm_add_run (g, 1u, 2u, OFDM_CP_DEFAULT, 0u, 0u) == 1);
    DP_CHECK (dp_ofdm_points_remaining (g) == 12);
    MUST (dp_ofdm_modulate (g, pts + 18, 12, p2, 64) == 20);
    DP_CHECK_MSG (bit_equal (p1, whole, 30) && bit_equal (p2, whole + 30, 20),
                  "the position and the tail survive each re-open");
    dp_ofdm_destroy (g);
    dp_ofdm_destroy (w);
  }

  /* reset on a layout that has never sealed leaves it as it was. Control:
   * the same layout, once a run is added, seals. */
  {
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_REQUIRE (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, 1u) == 0);
    DP_REQUIRE (dp_ofdm_check (o) < 0);
    dp_ofdm_reset (o);
    DP_CHECK_MSG (o->n_cls == 1 && o->n_runs == 0 && !o->sealed,
                  "reset leaves an unsealed layout as it was");
    DP_REQUIRE (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    DP_CHECK (dp_ofdm_check (o) == 0);
    dp_ofdm_destroy (o);
  }

  /* state_bytes is a function of the layout alone: before a render, inside
   * it and at its end. Control: the same layout with a window one longer
   * differs by exactly one tail slot. */
  {
    dp_ofdm_state_t *o = make_layout (3u, 1.0);
    cf               pts[27], out[64];
    fill_points (pts, 27);
    const size_t nb0 = dp_ofdm_state_bytes (o);
    MUST (dp_ofdm_modulate (o, pts, 9, out, 64) == 20);
    const size_t nb1 = dp_ofdm_state_bytes (o);
    MUST (dp_ofdm_modulate (o, pts + 9, 18, out, 64) == 36);
    const size_t nb2 = dp_ofdm_state_bytes (o);
    DP_CHECK_MSG (nb0 == nb1 && nb1 == nb2,
                  "the blob size does not move as the render advances");
    dp_ofdm_state_t *o4 = make_layout (4u, 1.0);
    DP_CHECK_MSG (dp_ofdm_state_bytes (o4) == nb0 + sizeof (cf),
                  "control: one more window sample is one more slot");
    dp_ofdm_destroy (o);
    dp_ofdm_destroy (o4);
  }

  /* The size limits, each edge beside its neighbor. */
  {
    static uint8_t big[OFDM_N_FFT_MAX + 1u];
    memset (big, 1, sizeof big);
    const uint8_t    map2[2] = { 0, 1 };
    dp_ofdm_state_t *o       = dp_ofdm_create ();
    DP_CHECK_MSG (dp_ofdm_add_class (o, 2u, map2, 2u, 0u, 0u, 0u, 1.0, 1u)
                      == 0,
                  "a two-point transform is the smallest class");
    DP_CHECK_MSG (dp_ofdm_add_class (o, OFDM_N_FFT_MAX, big, OFDM_N_FFT_MAX,
                                     0u, 0u, 0u, 1.0, 1u)
                      == 1,
                  "n_fft = OFDM_N_FFT_MAX is accepted");
    DP_CHECK_MSG (dp_ofdm_add_class (o, OFDM_N_FFT_MAX + 1u, big,
                                     OFDM_N_FFT_MAX + 1u, 0u, 0u, 0u, 1.0, 1u)
                      < 0,
                  "and one more is refused");
    DP_CHECK_MSG (
        dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, OFDM_N_PHASE_MAX)
            == 2,
        "n_phase = OFDM_N_PHASE_MAX is accepted");
    DP_CHECK_MSG (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0,
                                     OFDM_N_PHASE_MAX + 1u)
                      < 0,
                  "and one more is refused");
    /* cp = useful and window = the symbol length are accepted; one more of
     * each is refused (section 1 holds the refusals) */
    DP_CHECK_MSG (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 8u, 0u, 1.0, 1u)
                      == 3,
                  "a prefix of the whole body is accepted");
    DP_CHECK_MSG (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 8u, 1.0, 1u)
                      == 4,
                  "a window of the whole symbol is accepted");
    DP_CHECK (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 8u, 16u, 1.0, 1u) == 5);
    DP_CHECK (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 8u, 17u, 1.0, 1u) < 0);
    dp_ofdm_destroy (o);

    /* OFDM_COUNT_MAX: runs, fixed entries, classes and the polarity length */
    o = dp_ofdm_create ();
    DP_REQUIRE (dp_ofdm_add_class (o, 2u, map2, 2u, 0u, 0u, 0u, 1.0, 1u) == 0);
    int ok = 1;
    for (uint32_t i = 0; i < OFDM_COUNT_MAX && ok; i++)
      ok = dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == (int)i;
    DP_CHECK_MSG (ok, "OFDM_COUNT_MAX runs are accepted");
    DP_CHECK_MSG (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) < 0,
                  "and one more is refused");
    ok = 1;
    for (uint32_t i = 0; i < OFDM_COUNT_MAX && ok; i++)
      ok = dp_ofdm_add_fixed (o, 0u, 0u, 0, 1.0) == 0;
    DP_CHECK_MSG (ok, "OFDM_COUNT_MAX fixed entries are accepted");
    DP_CHECK_MSG (dp_ofdm_add_fixed (o, 0u, 0u, 0, 1.0) < 0,
                  "and one more is refused");
    ok = 1;
    for (uint32_t i = 1; i < OFDM_COUNT_MAX && ok; i++)
      ok = dp_ofdm_add_class (o, 2u, map2, 2u, 0u, 0u, 0u, 1.0, 1u) == (int)i;
    DP_CHECK_MSG (ok, "OFDM_COUNT_MAX classes are accepted");
    DP_CHECK_MSG (dp_ofdm_add_class (o, 2u, map2, 2u, 0u, 0u, 0u, 1.0, 1u) < 0,
                  "and one more is refused");
    cf *pol = calloc (OFDM_COUNT_MAX + 1u, sizeof *pol);
    DP_REQUIRE (pol != NULL);
    DP_CHECK_MSG (dp_ofdm_add_polarity (o, 1u, pol, OFDM_COUNT_MAX + 1u) < 0,
                  "a polarity sequence one longer than the cap is refused");
    DP_CHECK_MSG (dp_ofdm_add_polarity (o, 1u, pol, OFDM_COUNT_MAX) == 0,
                  "and one of the cap is accepted");
    free (pol);
    dp_ofdm_destroy (o);
  }

  /* A fixed entry on a subcarrier the map nulls is placed there: a training
   * field. Control: the same layout without the entry reads 0 there. */
  {
    const uint8_t none8[8] = { 0 };
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *o = dp_ofdm_create ();
        MUST (dp_ofdm_add_class (o, 8u, none8, 8u, 0u, 0u, 0u, 1.0, 1u) == 0);
        if (pass == 0)
          MUST (dp_ofdm_add_fixed (o, 0u, 0u, 1, 1.0) == 0);
        MUST (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
        cf       out[8];
        const cf none_pts[1] = { 0 };
        MUST (dp_ofdm_modulate (o, none_pts, 0, out, 8) == 8);
        for (int k = -4; k < 4; k++)
          {
            const double want = (pass == 0 && k == 1) ? 1.0 : 0.0;
            DP_CHECK_MSG (close_c (coef (out, 8u, k), want, 2e-6),
                          pass ? "control: nothing without the entry"
                               : "a fixed entry on a null subcarrier");
          }
        dp_ofdm_destroy (o);
      }
  }

  /* check derives one inverse transform per distinct n_fft. Control: three
   * distinct lengths give three. */
  {
    const uint8_t map4[4] = { 0, 0, 0, 1 };
    const uint8_t map16[16]
        = { 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 };
    for (int pass = 0; pass < 2; pass++)
      {
        dp_ofdm_state_t *o = dp_ofdm_create ();
        MUST (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 0u, 0u, 1.0, 1u) == 0);
        MUST (dp_ofdm_add_class (o, 4u, map4, 4u, 0u, 0u, 0u, 1.0, 1u) == 1);
        if (pass == 0)
          MUST (dp_ofdm_add_class (o, 8u, MAP8, 8u, 0u, 2u, 0u, 1.0, 1u) == 2);
        else
          MUST (dp_ofdm_add_class (o, 16u, map16, 16u, 0u, 0u, 0u, 1.0, 1u)
                == 2);
        MUST (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
        MUST (dp_ofdm_check (o) == 0);
        DP_CHECK_MSG (o->n_plans == (pass ? 3u : 2u),
                      pass ? "control: three lengths, three transforms"
                           : "two classes of one length share a transform");
        dp_ofdm_destroy (o);
      }
  }

  /* modulate_max_out is 0 past the layout and for no points where the next
   * symbol needs some. Control: the first symbol's 5 points give 10. */
  {
    dp_ofdm_state_t *o = make_layout (0u, 1.0);
    DP_CHECK_MSG (dp_ofdm_modulate_max_out (o, 28) == 0,
                  "one point past the layout");
    DP_CHECK_MSG (dp_ofdm_modulate_max_out (o, 0) == 0,
                  "no points when the first symbol needs five");
    DP_CHECK_MSG (dp_ofdm_modulate_max_out (o, 5) == 10, "control: five");
    DP_CHECK (dp_ofdm_modulate_max_out (o, 27) == 56);
    dp_ofdm_destroy (o);
  }

  /* Before anything is rendered there is no tail to finish. Control: after
   * one symbol of a window-2 layout there are two samples. */
  {
    dp_ofdm_state_t *o = make_layout (2u, 1.0);
    cf               pts[5], out[16], t[4];
    fill_points (pts, 5);
    DP_CHECK_MSG (dp_ofdm_finish_max_out (o, 99u) == 0
                      && dp_ofdm_finish (o, 99u, t, 4u) == 0,
                  "no tail before the first render");
    MUST (dp_ofdm_modulate (o, pts, 5, out, 16) == 10);
    DP_CHECK_MSG (dp_ofdm_finish_max_out (o, 99u) == 2,
                  "control: a tail after one symbol");
    dp_ofdm_destroy (o);
  }

  /* The float32 render of a 64-point OFDM symbol, against the double
   * precision sum of the header's formula, within the error bound DERIVED
   * for the transform that runs (not measured, not fitted).
   *
   * Which transform: a 64-point cf32 plan takes pffft's native-float path
   * (pocketfft.c:127-129, 335-347; the size gate pffft_cf32_ok, :71), three
   * radix-4 passes: cffti1_ps (N/SIMD_SZ = 16) decomposes as 4 x 4
   * (pffft.c:1137, ntryh {5,3,4,2}), and the cross-lane stage is the third
   * (pffft.c:1260-1267); a scalar build (SIMD_SZ = 1) decomposes 64 as
   * 4 x 4 x 4 directly. A radix-4 pass is one twiddle multiply per element
   * and then two add levels whose only multipliers are +-1 and +-j, which
   * are exact, so it costs at most two radix-2 levels' error and Higham's
   * radix-2 bound over log2 N = 6 levels covers it (Accuracy and Stability of
   * Numerical Algorithms, 2nd ed., Theorem 24.2):
   *   ||y^ - y|| / ||y|| <= L eta / (1 - L eta),
   *   eta = mu + gamma_4 (sqrt 2 + mu),  gamma_4 = 4u / (1 - 4u),
   * with mu the twiddles' error. Both twiddle tables are cosf/sinf of an
   * angle formed in float with three roundings (pffft.c:1141, 1157, 1161 and
   * :1264; the division by a power of two is exact), so the angle is off by
   * at most 3u of 2 pi, and cosf / sinf add u/2 per component: mu <=
   * 3u * 2 pi + (sqrt 2 / 2) u < 20u (twiddles accurate to u would give
   * 2.4e-6, but these are formed in float, not rounded from double). The
   * scale multiply adds one rounding, u. With u = 2^-24 that is 9.2e-6; the
   * expectation is about u sqrt(log2 N), 1.5e-7, and the bound is what is
   * asserted. The bound is derived from those lines: if the 64-point plan
   * leaves pffft (pocketfft.c:71's gate) or the twiddle formation changes,
   * re-derive it rather than move the number. Only
   * N = 64 is asserted: the bound is for a power-of-two transform, and a
   * length such as 704 runs another algorithm (a sweep finding,
   * native/validation/ofdm_fft_error.c). Control: one grid value moved by
   * 1e-3 is 1.25e-4 relative (one bin of 64 unit-power bins), over it. */
  {
    const double u     = ldexp (1.0, -24);
    const double mu    = 20.0 * u;
    const double g4    = 4.0 * u / (1.0 - 4.0 * u);
    const double eta   = mu + g4 * (sqrt (2.0) + mu);
    const double tol64 = 6.0 * eta / (1.0 - 6.0 * eta) + u;

    uint8_t map64[64];
    memset (map64, 1, sizeof map64);
    cf       pts[64];
    uint32_t st = 2026u;
    for (int i = 0; i < 64; i++)
      {
        const uint32_t r = dp_xs32 (&st);
        pts[i] = (float)sqrt (0.5)
                 * ((r & 1u ? 1.0f : -1.0f) + (r & 2u ? 1.0f : -1.0f) * I);
      }
    dp_ofdm_state_t *o = dp_ofdm_create ();
    MUST (dp_ofdm_add_class (o, 64u, map64, 64u, 0u, 0u, 0u, 0.125, 1u) == 0);
    MUST (dp_ofdm_add_run (o, 0u, 1u, OFDM_CP_DEFAULT, 0u, 0u) == 0);
    cf out[64];
    MUST (dp_ofdm_modulate (o, pts, 64, out, 64) == 64);
    dp_ofdm_destroy (o);

    double err[2] = { 0, 0 };
    for (int pass = 0; pass < 2; pass++)
      {
        double num = 0, den = 0;
        for (int m = 0; m < 64; m++)
          {
            double _Complex y = 0;
            for (int k = -32; k < 32; k++)
              {
                double _Complex v = (double _Complex)pts[k + 32];
                if (pass && k == 5)
                  v += 1e-3;
                y += v * cexp (I * TWO_PI * (double)k * (double)m / 64.0);
              }
            y *= 0.125;
            num += pow (cabs ((double _Complex)out[m] - y), 2.0);
            den += pow (cabs (y), 2.0);
          }
        err[pass] = sqrt (num / den);
      }
    DP_CHECK_MSG (err[0] <= tol64,
                  "a 64-point render is within the derived float32 bound");
    DP_CHECK_MSG (err[1] > tol64, "control: a 1e-3 grid error is not");
  }

  DP_TEST_END ("test_ofdm_core");
}
