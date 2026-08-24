/*
 * test_ber_meter_core.c — the alignment DECISION, which is the third of the
 * three primitives every receiver number here rests on.
 *
 * This file was a 24-line jm scaffold: create, reset, destroy. The object it
 * covers ships the whole alignment decision — `ber_align_detect` and the
 * stateful `ber_meter_align`/`score` around it — and `ber_align_detect`
 * appeared in exactly ONE place tree-wide (`native/tests/dp_ber_test.h`),
 * asserted by nobody. The scaffold passing meant "it constructs".
 *
 * What makes this primitive worth testing rather than trusting is the failure
 * it exists to prevent. Scoring `min over (lag, rotation)` of the error count
 * is an optimisation over the answer: it finds a lucky alignment on garbage
 * (false PASS) and misses the true one on a healthy receiver (false FLOOR).
 * Both have shipped in this project. Detection replaces that with a gate — so
 * every claim below is about the gate's two directions: it must find a real
 * alignment, and it must REFUSE rather than return a plausible wrong one.
 *
 * Claims from the declaration in ber_meter/ber_meter_core.h:
 *
 *   1. detects a planted lag exactly, and recovers the ABSOLUTE rotation
 *      (which is what removes the min-over-rotation bias);
 *   2. `ok = 0` — not a plausible lag — when the marker is too short to
 *      identify one, since the processing gain is sqrt(2*K*L);
 *   3. repeats are combined NON-COHERENTLY, so a marker too short alone
 *      becomes detectable when it recurs;
 *   4. `saturated` when the peak lands on an edge of the lag search;
 *   5. a stream unrelated to the truth is refused outright;
 *   6. the noise floor is estimated from the off-peak lags (a CFAR
 *      reference), so nothing needs to know the Es/N0 — detection holds
 *      across a range of them;
 *   7. marker symbols are excluded from scoring and land in `skipped`, so
 *      the symbols that fixed the alignment cannot also flatter the rate.
 */
#include "ber_meter/ber_meter_core.h"

#include "dp_rng_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Randomness from dp_rng_test.h — the test layer's one generator and one
   Box-Muller (`make tests-ssot` enforces it). What makes the checks below
   independent of the code under test is the PLANTED lag and rotation, not the
   source of the draws. */
static uint32_t st = 0xD1B54A32u;

#define NSYM 8000
#define M 4

static uint8_t       truth[NSYM];
static float complex rx[NSYM];

/* QPSK stream carrying `truth`, shifted by `lag` (rx[i] <-> truth[i + lag]),
   rotated by `phase`, at the given per-component noise sigma. */
static void
build (int lag, double phase, double sigma)
{
  for (size_t i = 0; i < NSYM; i++)
    {
      long t = (long)i + lag;
      int  q = (t >= 0 && t < (long)NSYM) ? truth[t] : (int)(dp_uni (&st) * M);
      double        ph = 2.0 * M_PI * q / M + M_PI / M + phase;
      float complex n
          = (sigma > 0.0) ? (float complex) (sigma * dp_cgauss (&st)) : 0.0f;
      rx[i] = (float complex) (cos (ph) + sin (ph) * I) + n;
    }
}

int
main (void)
{
  for (size_t i = 0; i < NSYM; i++)
    truth[i] = (uint8_t)(dp_uni (&st) * M);

  ber_meter_state_t *obj = ber_meter_create (M, 200, 0.99);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;
  DP_REQUIRE_MSG (ber_meter_set_truth (obj, truth, NSYM) == 0, "set_truth");

  /* ── 1. a planted lag and rotation come back exactly ──────────────────── */
  {
    const int    lags[4] = { 0, 7, -13, 61 };
    const double phase   = 0.9; /* absolute, not a multiple of 2pi/M */
    for (int k = 0; k < 4; k++)
      {
        build (lags[k], phase, 0.15);
        ber_align_t a = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 256,
                                          0, 200, 0.0);
        char        msg[160];
        snprintf (msg, sizeof msg,
                  "planted lag %d: got %d, ok=%d, margin %.1f dB", lags[k],
                  a.lag, a.ok, a.margin_db);
        DP_REQUIRE_MSG (a.ok, msg);
        DP_REQUIRE_MSG (a.lag == lags[k], msg);
        DP_REQUIRE_MSG (a.margin_db > 3.0, msg);
        DP_REQUIRE_MSG (!a.saturated, msg);
        /* The phase is ABSOLUTE: modulo 2pi/M would leave the M-fold
           ambiguity the marker exists to resolve. */
        double dphi = fmod (a.phase - phase + 3.0 * M_PI, 2.0 * M_PI) - M_PI;
        snprintf (msg, sizeof msg, "planted phase %.3f: got %.3f (err %.3f)",
                  phase, a.phase, dphi);
        DP_REQUIRE_MSG (fabs (dphi) < 0.10, msg);
      }
  }

  /* ── 6. the gate needs no Es/N0: it holds across a range of them ─────── */
  {
    const double sigmas[4] = { 0.05, 0.2, 0.4, 0.6 };
    for (int k = 0; k < 4; k++)
      {
        build (5, 0.0, sigmas[k]);
        ber_align_t a = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 256,
                                          0, 200, 0.0);
        char        msg[128];
        snprintf (msg, sizeof msg, "sigma %.2f: ok=%d lag=%d margin %.1f",
                  sigmas[k], a.ok, a.lag, a.margin_db);
        DP_REQUIRE_MSG (a.ok && a.lag == 5, msg);
      }
  }

  /* ── 2. too short to identify one -> ok = 0, NOT a plausible lag ─────── */
  {
    build (5, 0.0, 0.55);
    ber_align_t a
        = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 4, 0, 200, 0.0);
    char msg[128];
    snprintf (msg, sizeof msg, "4-symbol marker: ok=%d lag=%d margin %.1f",
              a.ok, a.lag, a.margin_db);
    DP_REQUIRE_MSG (!a.ok, msg);
  }

  /* ── 3. repeats combine non-coherently and buy that gain back ────────── */
  {
    /* The same short marker, recurring: 32 symbols alone cannot clear the
       gate at this noise, and the identical marker repeated every 256 can.
       That difference IS the non-coherent combining claim. */
    build (5, 0.0, 0.5);
    ber_align_t once
        = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 32, 0, 200, 0.0);
    ber_align_t many
        = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 32, 256, 200, 0.0);
    char msg[192];
    snprintf (msg, sizeof msg,
              "32 syms alone: ok=%d margin %.1f (%zu occ); repeated: ok=%d "
              "margin %.1f (%zu occ)",
              once.ok, once.margin_db, once.occurrences, many.ok,
              many.margin_db, many.occurrences);
    DP_REQUIRE_MSG (many.occurrences > once.occurrences, msg);
    DP_REQUIRE_MSG (many.margin_db > once.margin_db, msg);
  }

  /* ── 4. a peak on the edge of the search is SATURATED, not a result ──── */
  {
    build (40, 0.0, 0.15);
    ber_align_t a
        = ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 256, 0, 8, 0.0);
    char msg[128];
    snprintf (msg, sizeof msg, "lag 40 in a +-8 search: ok=%d sat=%d lag=%d",
              a.ok, a.saturated, a.lag);
    DP_REQUIRE_MSG (!a.ok, msg);
  }

  /* ── 5. an unrelated stream is refused ───────────────────────────────── */
  {
    static uint8_t other[NSYM];
    for (size_t i = 0; i < NSYM; i++)
      other[i] = (uint8_t)(dp_uni (&st) * M);
    build (0, 0.0, 0.15); /* carries `truth` */
    ber_align_t a
        = ber_align_detect (rx, NSYM, other, NSYM, M, 1000, 256, 0, 200, 0.0);
    char msg[128];
    snprintf (msg, sizeof msg, "unrelated truth: ok=%d lag=%d margin %.1f",
              a.ok, a.lag, a.margin_db);
    DP_REQUIRE_MSG (!a.ok, msg);
  }

  /* ── 7. the marker's own symbols are excluded from the score ─────────── */
  {
    build (0, 0.0, 0.10);
    int ok = ber_meter_align (obj, rx, NSYM, 1000, 256, 0, 200, 0.0);
    DP_REQUIRE_MSG (ok, "meter align on a clean stream");
    size_t scored  = ber_meter_score (obj, rx, NSYM, 0, NSYM);
    size_t skipped = ber_meter_get_skipped (obj);
    char   msg[160];
    snprintf (msg, sizeof msg, "scored %zu of %d, skipped %zu", scored, NSYM,
              skipped);
    DP_REQUIRE_MSG (skipped >= 256, msg);
    DP_REQUIRE_MSG (scored == NSYM - skipped, msg);
    /* And the alignment being right, a clean stream scores no errors. */
    snprintf (msg, sizeof msg, "clean stream scored %zu errors",
              ber_meter_get_errors (obj));
    DP_REQUIRE_MSG (ber_meter_get_errors (obj) == 0, msg);
  }

  ber_meter_reset (obj);
  DP_REQUIRE_MSG (ber_meter_get_errors (obj) == 0
                      && ber_meter_get_symbols (obj) == 0,
                  "reset clears the accumulation");
  ber_meter_destroy (obj);
  DP_TEST_END ("test_ber_meter_core");
}
