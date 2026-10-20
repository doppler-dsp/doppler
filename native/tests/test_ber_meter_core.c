/*
 * test_ber_meter_core.c — the alignment DECISION, which is the third of the
 * three primitives every receiver number here rests on.
 *
 * This file was a 24-line jm scaffold: create, reset, destroy. The object it
 * covers ships the whole alignment decision — `dp_ber_align_detect` and the
 * stateful `dp_ber_meter_align`/`score` around it — and `dp_ber_align_detect`
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
 *      the symbols that fixed the alignment cannot also flatter the rate;
 *   8. the false-alarm rate on unrelated truth holds the stated Pfa at
 *      m = 2 as at m = 4 -- measured as a RATE, since one refusal at 1e-6
 *      (claim 5) cannot see a threshold that is a thousand times too low.
 *      At m = 2 the null is rank-one (doppler#2496), and BPSK still finds a
 *      planted alignment at low Es/N0 under the higher threshold.
 */
#include "doppler/ber_meter/ber_meter_core.h"

#include "dp_rng_test.h"
#include "dp_test.h"

#include "doppler/dp_complex.h"
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

static uint8_t truth[NSYM];
static float _Complex rx[NSYM];

/* QPSK stream carrying `truth`, shifted by `lag` (rx[i] <-> truth[i + lag]),
   rotated by `phase`, at the given per-component noise sigma. */
static void
build (int lag, double phase, double sigma)
{
  for (size_t i = 0; i < NSYM; i++)
    {
      long t = (long)i + lag;
      int  q = (t >= 0 && t < (long)NSYM) ? truth[t] : (int)(dp_uni (&st) * M);
      double ph = 2.0 * M_PI * q / M + M_PI / M + phase;
      float _Complex n
          = (sigma > 0.0) ? (float _Complex) (sigma * dp_cgauss (&st)) : 0.0f;
      rx[i] = (float _Complex) (cos (ph) + sin (ph) * I) + n;
    }
}

/* Claim 8's stream: m-PSK carrying `sent`, rotated by `phase`, at the given
   per-component noise sigma, into `out` (n symbols, no lag). */
#define FA_NSYM 1600
static void
build_m (int m, const uint8_t *sent, double phase, double sigma,
         float _Complex *out)
{
  for (size_t i = 0; i < FA_NSYM; i++)
    {
      double ph = 2.0 * M_PI * sent[i] / m + phase;
      out[i]    = (float _Complex) (cos (ph) + sin (ph) * I)
                  + (float _Complex) (sigma * dp_cgauss (&st));
    }
}

/* Claim 8: of `trials` alignments against truth unrelated to the stream,
   how many does the gate accept, at whole-search Pfa `pfa`? The marker is
   `n_marker` symbols at `t0`, repeating every `period` (0: once); the
   occurrences the gate combined, K, come back in `*k`, so a caller can
   assert which threshold branch it exercised. */
static int
false_alarms (int m, int trials, double pfa, size_t t0, size_t n_marker,
              size_t period, size_t *k)
{
  static uint8_t sent[FA_NSYM], other[FA_NSYM];
  static float _Complex y[FA_NSYM];
  int n = 0;
  *k    = 0;
  for (int t = 0; t < trials; t++)
    {
      for (size_t i = 0; i < FA_NSYM; i++)
        {
          sent[i]  = (uint8_t)(dp_uni (&st) * m);
          other[i] = (uint8_t)(dp_uni (&st) * m);
        }
      /* Little noise: the data term dominates, which at m = 2 is the
         rank-one null in its purest form, the case #2496 measured. */
      build_m (m, sent, 0.9, 0.05, y);
      ber_align_t a = dp_ber_align_detect (y, FA_NSYM, other, FA_NSYM, m, t0,
                                           n_marker, period, 100, pfa);
      n += a.ok;
      *k = a.occurrences;
    }
  return n;
}

int
main (void)
{
  for (size_t i = 0; i < NSYM; i++)
    truth[i] = (uint8_t)(dp_uni (&st) * M);

  dp_ber_meter_state_t *obj = dp_ber_meter_create (M, 200, 0.99);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;
  DP_REQUIRE_MSG (dp_ber_meter_set_truth (obj, truth, NSYM) == 0, "set_truth");

  /* ── 1. a planted lag and rotation come back exactly ──────────────────── */
  {
    const int    lags[4] = { 0, 7, -13, 61 };
    const double phase   = 0.9; /* absolute, not a multiple of 2pi/M */
    for (int k = 0; k < 4; k++)
      {
        build (lags[k], phase, 0.15);
        ber_align_t a = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000,
                                             256, 0, 200, 0.0);
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
        ber_align_t a = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000,
                                             256, 0, 200, 0.0);
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
        = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 4, 0, 200, 0.0);
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
    ber_align_t once = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 32,
                                            0, 200, 0.0);
    ber_align_t many = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 32,
                                            256, 200, 0.0);
    char        msg[192];
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
        = dp_ber_align_detect (rx, NSYM, truth, NSYM, M, 1000, 256, 0, 8, 0.0);
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
    ber_align_t a = dp_ber_align_detect (rx, NSYM, other, NSYM, M, 1000, 256,
                                         0, 200, 0.0);
    char        msg[128];
    snprintf (msg, sizeof msg, "unrelated truth: ok=%d lag=%d margin %.1f",
              a.ok, a.lag, a.margin_db);
    DP_REQUIRE_MSG (!a.ok, msg);
  }

  /* ── 7. the marker's own symbols are excluded from the score ─────────── */
  {
    build (0, 0.0, 0.10);
    int ok = dp_ber_meter_align (obj, rx, NSYM, 1000, 256, 0, 200, 0.0);
    DP_REQUIRE_MSG (ok, "meter align on a clean stream");
    size_t scored  = dp_ber_meter_score (obj, rx, NSYM, 0, NSYM);
    size_t skipped = dp_ber_meter_get_skipped (obj);
    char   msg[160];
    snprintf (msg, sizeof msg, "scored %zu of %d, skipped %zu", scored, NSYM,
              skipped);
    DP_REQUIRE_MSG (skipped >= 256, msg);
    DP_REQUIRE_MSG (scored == NSYM - skipped, msg);
    /* And the alignment being right, a clean stream scores no errors. */
    snprintf (msg, sizeof msg, "clean stream scored %zu errors",
              dp_ber_meter_get_errors (obj));
    DP_REQUIRE_MSG (dp_ber_meter_get_errors (obj) == 0, msg);
  }

  /* ── 8. the false-alarm RATE holds at m = 2 as at m = 4 ─────────────────
     At a whole-search Pfa of 1e-3, at most `expect` false alarms are due
     (fewer, in practice: a false peak must also clear the 3 dB runner-up
     gate), and the bound sits 4.5 sd of a Poisson count above that. The
     circular threshold at m = 2 accepted about 0.9% at any Pfa from 1e-2
     to 1e-4 (#2496) -- about 28 of these 3000. A low count is allowed:
     the rank-one threshold is conservative where noise is not small.

     Every branch of the m = 2 threshold is run, and each case asserts the
     K it ran: a blind marker (K = 1, the Q^-1 form), a periodic one with
     an even K (2, the noncoherent form at K/2) and an odd K above one (3,
     the conservative K + 1). The geometry fixes K: 1500 symbols of room
     past the +-100 lag search, a 64-symbol marker from 200, every 1000 or
     600 symbols. */
  {
    const double pfa = 1e-3;
    const struct
    {
      int    m, trials;
      size_t t0, n_marker, period, k;
    } cases[] = {
      { 2, 3000, 1000, 256, 0, 1 },  /* BPSK, blind            */
      { 4, 1000, 1000, 256, 0, 1 },  /* QPSK, blind: circular  */
      { 2, 3000, 200, 64, 1000, 2 }, /* BPSK, periodic, even K */
      { 2, 3000, 200, 64, 600, 3 },  /* BPSK, periodic, odd K  */
    };
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++)
      {
        const double expect = pfa * cases[c].trials;
        const double bound  = expect + 4.5 * sqrt (expect);
        size_t       k      = 0;
        int  n = false_alarms (cases[c].m, cases[c].trials, pfa, cases[c].t0,
                               cases[c].n_marker, cases[c].period, &k);
        char msg[192];
        snprintf (msg, sizeof msg,
                  "m=%d K=%zu: %d false alignments in %d unrelated-truth "
                  "trials (at most %.0f due at Pfa %g, bound %.1f)",
                  cases[c].m, k, n, cases[c].trials, expect, pfa, bound);
        DP_REQUIRE_MSG (k == cases[c].k, msg);
        DP_REQUIRE_MSG (n <= bound, msg);
      }
    /* And the higher m = 2 threshold still finds a planted alignment at a
       per-symbol Es/N0 of 1.4 dB, with margin. */
    static uint8_t sent[FA_NSYM];
    static float _Complex y[FA_NSYM];
    for (size_t i = 0; i < FA_NSYM; i++)
      sent[i] = (uint8_t)(dp_uni (&st) * 2);
    build_m (2, sent, 0.9, 0.6, y);
    ber_align_t a = dp_ber_align_detect (y, FA_NSYM, sent, FA_NSYM, 2, 1000,
                                         256, 0, 200, 0.0);
    char        msg[128];
    snprintf (msg, sizeof msg, "BPSK at 1.4 dB: ok=%d lag=%d margin %.1f dB",
              a.ok, a.lag, a.margin_db);
    DP_REQUIRE_MSG (a.ok && a.lag == 0 && a.margin_db > 3.0, msg);
  }

  dp_ber_meter_reset (obj);
  DP_REQUIRE_MSG (dp_ber_meter_get_errors (obj) == 0
                      && dp_ber_meter_get_symbols (obj) == 0,
                  "reset clears the accumulation");
  dp_ber_meter_destroy (obj);
  DP_TEST_END ("test_ber_meter_core");
}
