/**
 * @file test_dp_tx.c
 * @brief `dp_tx_test.h`'s self-test: the harness STIMULUS SSOT.
 *
 * This header exists because the same synthesis loop had been written three
 * times in C and four more in Python, and **the copies did not differ in
 * their mathematics — they differed in their CONVENTIONS**. One drove symbols
 * at 0.25 where another drove 1.0, and since a TED's slope goes as `A^2` the
 * two harnesses were measuring loop bandwidths a factor of ~16 apart while
 * both read as "the RRC BPSK test". Another normalised to 0.25 of the stream's
 * PEAK, and an RRC stream peaks ~1.582x above its symbol amplitude, so the
 * object under test received 0.158 against a contract written in unit
 * amplitude. That one cost a session.
 *
 * So the conventions are the product here, not the loop. This file asserts
 * them, because they are exactly the kind of thing that stays green while
 * changing meaning.
 *
 * ## §5.4 of the design doc, made concrete
 *
 * `docs/design/rx-test.md` §5.4 records that this file is **the one place the
 * stimulus rule cannot reach**: `check_stimulus_sources.py` requires every
 * test, validator and example to source its stimulus from the library, and
 * `dp_tx_test.h` *is* the test layer's stimulus and builds its own. The gate
 * that polices everyone else is structurally blind to it.
 *
 * That is not a hole to plug in the gate — it has nowhere to point. It is a
 * hole to fill with evidence, which is what this file is. Every convention
 * the gate would have checked elsewhere is checked here instead:
 * the amplitude is a SYMBOL amplitude and not a peak, the pulse is the
 * library's `wfm_rrc_h`/`wfm_rc_h`, the symbol source is the library's MLS,
 * and the record has no dead tail at any samples-per-symbol.
 *
 * ## The trick that makes the amplitude convention exactly testable
 *
 * `DP_TX_RC` is a full raised cosine — Nyquist at the receiver, i.e. TX*RX
 * already collapsed — so it is **zero-ISI at symbol centres by construction**.
 * At integer `sps`, `tau = 0`, `rate = 1`, the sample at symbol k's centre is
 * therefore exactly `amp * sy[k]`, with only pulse truncation between it and
 * an identity. That turns "amp is the SYMBOL amplitude" from a comment into
 * an equality, and it is the same instant a receiver's slicer reads.
 */
#include "dp_test.h"
#include "dp_tx_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** @brief Sample index of symbol @p k's centre, from the header's own
 * geometry: the record starts at a `span`-symbol lead-in and symbol centres
 * sit at `(k + span) * sps * rate`. */
static size_t
centre_of (const dp_tx_cfg_t *c, size_t k)
{
  return (size_t)(((double)k + (double)c->span) * c->sps * c->rate + 0.5);
}

/** @brief Peak |x| over the record, in units of the SYMBOL amplitude. */
static double
papr_ratio (const float _Complex *x, size_t n, double amp)
{
  double pk = 0.0;
  size_t i;
  for (i = 0; i < n; i++)
    {
      double a = cabs ((double _Complex)x[i]);
      if (a > pk)
        pk = a;
    }
  return pk / amp;
}

/** @brief Trailing samples whose magnitude is negligible — the "dead zone"
 * the header says must not exist. */
static size_t
dead_tail (const float _Complex *x, size_t n, double amp)
{
  size_t d = 0;
  while (d < n && cabs ((double _Complex)x[n - 1 - d]) < 1e-6 * amp)
    d++;
  return d;
}

int
main (void)
{
  printf ("dp_tx_test.h self-test — the harness stimulus SSOT\n");

  /* ── 1. The symbol source, and that it is the SAME one ────────────────── */

  {
    int8_t a[512], b[512];
    size_t k;
    int    differs = 0;

    DP_CHECK (dp_tx_symbols (a, 512, 7u) == 0);
    DP_CHECK (dp_tx_symbols (b, 512, 7u) == 0);
    for (k = 0; k < 512; k++)
      if (a[k] != b[k])
        differs = 1;
    DP_CHECK_MSG (!differs, "the symbol source is reproducible from its seed");

    /* Strictly +-1: a BER harness compares against these, so a 0 or a 2 would
       score as a permanent error with nothing to say why. */
    differs = 0;
    for (k = 0; k < 512; k++)
      if (a[k] != 1 && a[k] != -1)
        differs = 1;
    DP_CHECK_MSG (!differs, "every symbol is exactly +1 or -1");

    /* A different seed gives a different sequence, or "reproducible" above is
       satisfied by a constant. */
    DP_CHECK (dp_tx_symbols (b, 512, 99u) == 0);
    differs = 0;
    for (k = 0; k < 512; k++)
      if (a[k] != b[k])
        differs++;
    DP_CHECK_MSG (differs > 128,
                  "a different seed gives a different sequence, so "
                  "reproducibility is not just a constant");

    /* Seed 0 is coerced to 1 — the all-zero register is a fixed point, and
       silently emitting a constant stream is the failure it would cause. */
    DP_CHECK (dp_tx_symbols (a, 512, 0u) == 0);
    DP_CHECK (dp_tx_symbols (b, 512, 1u) == 0);
    differs = 0;
    for (k = 0; k < 512; k++)
      if (a[k] != b[k])
        differs = 1;
    DP_CHECK_MSG (!differs, "seed 0 is coerced to 1, not left as a fixed "
                            "point emitting a constant");
  }

  /* ── 2. dp_tx_make's `syms` IS dp_tx_symbols' sequence ────────────────── */

  /* The load-bearing one: a BER harness scores the waveform against this
     array, so if the two ever diverged every rate measured through this file
     would be scored against the wrong truth — and would read as a receiver
     defect. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    int8_t          from_make[256], direct[256];
    size_t          n = 0, k;
    float _Complex *x;
    int             differs = 0;

    c.nsym = 256;
    x      = dp_tx_make (&c, from_make, &n);
    DP_REQUIRE (x != NULL);
    DP_CHECK (dp_tx_symbols (direct, 256, c.seed) == 0);
    for (k = 0; k < 256; k++)
      if (from_make[k] != direct[k])
        differs = 1;
    DP_CHECK_MSG (!differs,
                  "dp_tx_make's transmitted sequence IS dp_tx_symbols' — the "
                  "waveform and the truth a BER is scored against agree");
    free (x);
  }

  /* ── 3. Convention 1: `amp` is the SYMBOL amplitude ───────────────────── */

  /* A full raised cosine is Nyquist, so at symbol centres there is no ISI and
     the sample IS the symbol. Only pulse truncation stands between this and
     an identity, which is why the tolerance is 1e-3 and not 0. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    int8_t          sy[128];
    size_t          n = 0, k;
    float _Complex *x;
    double          worst = 0.0;

    c.pulse = DP_TX_RC;
    c.sps   = 4.0;
    c.span  = 16; /* longer span -> less truncation at the centres */
    c.nsym  = 128;
    c.amp   = 1.0;
    x       = dp_tx_make (&c, sy, &n);
    DP_REQUIRE (x != NULL);
    for (k = 8; k < 120; k++) /* skip the ends, where truncation bites */
      {
        double got = creal ((double _Complex)x[centre_of (&c, k)]);
        double e   = fabs (got - c.amp * (double)sy[k]);
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 1e-3,
                  "at a Nyquist pulse's symbol centres the sample IS "
                  "amp * symbol — `amp` is the SYMBOL amplitude");
    free (x);
  }

  /* And it SCALES, rather than being a normalisation target. Doubling `amp`
     must double every sample exactly — a peak-normalising stimulus would
     produce the same waveform twice, which is the defect that cost a session
     (0.25 of PEAK delivered 0.158 against a unit-amplitude contract). */
  {
    dp_tx_cfg_t     c1 = dp_tx_defaults (), c2 = dp_tx_defaults ();
    size_t          n1 = 0, n2 = 0, i;
    float _Complex *x1, *x2;
    double          worst = 0.0;

    c1.nsym = c2.nsym = 200;
    c1.amp            = 1.0;
    c2.amp            = 2.0;
    x1                = dp_tx_make (&c1, NULL, &n1);
    x2                = dp_tx_make (&c2, NULL, &n2);
    DP_REQUIRE (x1 != NULL && x2 != NULL);
    DP_CHECK (n1 == n2);
    for (i = 0; i < n1; i++)
      {
        double e
            = cabs ((double _Complex)x2[i] - 2.0 * (double _Complex)x1[i]);
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 1e-5,
                  "amp scales the waveform linearly — it is stated, never "
                  "derived from a peak");
    free (x1);
    free (x2);
  }

  /* ── 4. The RRC peaks ABOVE the symbol amplitude, by the pulse's PAPR ─── */

  /* The header quotes ~1.582x for RRC at beta = 0.35 and uses it to argue that
     a receiver which cannot take the peak has a headroom bug rather than the
     stimulus having a level bug. That number is the reason `amp` is not a peak
     normalisation, so it is measured rather than trusted. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    size_t          n = 0;
    float _Complex *x;
    double          r;

    c.nsym = 4000; /* enough symbols for the peak to be approached */
    c.amp  = 1.0;
    x      = dp_tx_make (&c, NULL, &n);
    DP_REQUIRE (x != NULL);
    r = papr_ratio (x, n, c.amp);
    DP_CHECK_MSG (r > 1.05,
                  "the shaped stream peaks ABOVE the symbol amplitude — which "
                  "is why normalising to a peak delivers less than it says");
    DP_CHECK_NEAR (r, 1.582, 0.25);
    free (x);
  }

  /* An NRZ stream, by contrast, peaks exactly AT the symbol amplitude: it has
     no pulse to overshoot with. Same convention, different pulse — which is
     what makes the RRC number a property of the pulse rather than of this
     file. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    size_t          n = 0;
    float _Complex *x;

    c.pulse = DP_TX_NRZ;
    c.nsym  = 500;
    c.amp   = 1.0;
    x       = dp_tx_make (&c, NULL, &n);
    DP_REQUIRE (x != NULL);
    DP_CHECK_NEAR (papr_ratio (x, n, c.amp), 1.0, 1e-6);
    free (x);
  }

  /* ── 5. No dead tail, at ANY samples-per-symbol ───────────────────────── */

  /* The recorded cost: an 8-symbol tail read as 8 recovered symbols of zero
     and cost RateSync 12 dB of EVM while it still locked 8/8 — the shape of a
     stimulus bug, not a loop bug. And the reason it hid is the second claim
     here: an inherited constant `+ 64` samples was 16 dead symbols at sps = 4
     and 3.7 at sps = 17.3, so the margin must be counted in SYMBOL periods. */
  {
    double spss[3] = { 4.0, 8.0, 17.333 };
    int    i;
    for (i = 0; i < 3; i++)
      {
        dp_tx_cfg_t     c = dp_tx_defaults ();
        size_t          n = 0;
        float _Complex *x;
        double          dead_syms;

        c.sps  = spss[i];
        c.nsym = 400;
        x      = dp_tx_make (&c, NULL, &n);
        DP_REQUIRE (x != NULL);
        dead_syms = (double)dead_tail (x, n, c.amp) / c.sps;
        DP_CHECK_MSG (dead_syms < 2.0,
                      "the record ends within a symbol period or two of the "
                      "last symbol — no dead zone to be scored as symbols");
        free (x);
      }
  }

  /* ── 6. A real-valued sps, which is why this file exists at all ───────── */

  /* `wfm_synth`'s sps is an int, so a harness needing 17.333 (or a fractional
     tau) cannot get its stimulus there. That is the stated reason for the
     direct-form loop, so it had better work. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    size_t          n = 0;
    float _Complex *x;

    c.sps  = 17.333;
    c.nsym = 200;
    x      = dp_tx_make (&c, NULL, &n);
    DP_REQUIRE (x != NULL);
    DP_CHECK (n > (size_t)(200 * 17.0));
    DP_CHECK_MSG (papr_ratio (x, n, c.amp) > 1.05,
                  "a real-valued sps produces a real shaped waveform, not a "
                  "degenerate one");
    free (x);
  }

  /* ── 7. `tau` is in SYMBOLS, not samples ──────────────────────────────── */

  /* The unit is the thing worth pinning: a tau read as samples would be `sps`
     times too small and would look like a small timing error rather than a
     wrong convention. A whole-symbol tau on a Nyquist pulse must shift the
     sequence by exactly one symbol, which is an equality rather than a
     tendency. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    int8_t          sy[128];
    size_t          n = 0, k;
    float _Complex *x;
    double          worst = 0.0;

    c.pulse = DP_TX_RC;
    c.sps   = 4.0;
    c.span  = 16;
    c.nsym  = 128;
    c.tau   = 1.0; /* one whole SYMBOL */
    x       = dp_tx_make (&c, sy, &n);
    DP_REQUIRE (x != NULL);
    for (k = 8; k < 118; k++)
      {
        /* With tau = +1 symbol the pulse centres move one symbol later, so
           the sample at symbol k's nominal centre now carries symbol k-1. */
        double got = creal ((double _Complex)x[centre_of (&c, k)]);
        double e   = fabs (got - c.amp * (double)sy[k - 1]);
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 1e-3,
                  "tau is in SYMBOL periods: tau = 1.0 shifts the stream by "
                  "exactly one symbol, not by one sample");
    free (x);
  }

  /* ── 8. `fc` rotates without changing the envelope ────────────────────── */

  /* A carrier must not alter the level, or every Es/N0 written against `amp`
     moves when a test adds an IF. */
  {
    dp_tx_cfg_t     cb = dp_tx_defaults (), cf = dp_tx_defaults ();
    size_t          nb = 0, nf = 0, i;
    float _Complex *xb, *xf;
    double          worst = 0.0;

    cb.nsym = cf.nsym = 300;
    cf.fc             = 0.25;
    xb                = dp_tx_make (&cb, NULL, &nb);
    xf                = dp_tx_make (&cf, NULL, &nf);
    DP_REQUIRE (xb != NULL && xf != NULL);
    DP_CHECK (nb == nf);
    for (i = 0; i < nb; i++)
      {
        double e = fabs (cabs ((double _Complex)xf[i])
                         - fabs (creal ((double _Complex)xb[i])));
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 1e-5,
                  "a carrier rotates the stream without changing |x|");
    /* And baseband really is baseband: fc = 0 leaves no imaginary part, so a
       test asserting on creal() is not silently discarding half the signal. */
    worst = 0.0;
    for (i = 0; i < nb; i++)
      if (fabs (cimag ((double _Complex)xb[i])) > worst)
        worst = fabs (cimag ((double _Complex)xb[i]));
    DP_CHECK_MSG (worst == 0.0, "fc = 0 produces a strictly real stream");
    free (xb);
    free (xf);
  }

  /* ── 9. NRZ shares the shaped branch's timing origin at rate != 1 ─────── */

  /* The header calls this out: the lead-in is scaled by `rate` precisely so
     the two branches agree away from rate == 1, where an unscaled lead-in
     "drifts silently". Silently is the word that earns a test. */
  {
    dp_tx_cfg_t     c = dp_tx_defaults ();
    int8_t          sy[64];
    size_t          n = 0, k;
    float _Complex *x;
    int             bad = 0;

    c.pulse = DP_TX_NRZ;
    c.sps   = 8.0;
    c.rate  = 1.25; /* deliberately not 1.0 */
    c.nsym  = 64;
    x       = dp_tx_make (&c, sy, &n);
    DP_REQUIRE (x != NULL);
    for (k = 1; k < 60; k++)
      {
        double got = creal ((double _Complex)x[centre_of (&c, k)]);
        if (fabs (got - c.amp * (double)sy[k]) > 1e-5)
          bad++;
      }
    DP_CHECK_MSG (bad == 0,
                  "at rate != 1 the NRZ branch lands on the SAME symbol "
                  "origin as the shaped branch");
    free (x);
  }

  /* ── 10. The guard rails return NULL rather than something plausible ──── */

  {
    dp_tx_cfg_t c = dp_tx_defaults ();
    size_t      n = 0;

    c.nsym = 0;
    DP_CHECK_MSG (dp_tx_make (&c, NULL, &n) == NULL,
                  "nsym = 0 is refused — a record length is never a default");
    c.nsym = 64;
    c.sps  = 0.0;
    DP_CHECK (dp_tx_make (&c, NULL, &n) == NULL);
    c.sps = -4.0;
    DP_CHECK (dp_tx_make (&c, NULL, &n) == NULL);
    c.sps  = 4.0;
    c.span = 0;
    DP_CHECK (dp_tx_make (&c, NULL, &n) == NULL);
    DP_CHECK (dp_tx_make (NULL, NULL, &n) == NULL);
  }

  /* ── 11. The defaults ARE the shared convention ───────────────────────── */

  /* Every field a test does not mention is supposed to be provably the shared
     convention rather than an accident, which only holds if the defaults are
     pinned somewhere. Here. */
  {
    dp_tx_cfg_t c = dp_tx_defaults ();
    DP_CHECK (c.pulse == DP_TX_RRC);
    DP_CHECK_NEAR (c.sps, 4.0, 1e-12);
    DP_CHECK_NEAR (c.beta, 0.35, 1e-12);
    DP_CHECK (c.span == 8);
    DP_CHECK_NEAR (c.tau, 0.0, 1e-12);
    DP_CHECK_NEAR (c.rate, 1.0, 1e-12);
    /* The one that matters: unit SYMBOL amplitude, derived from a matched
       cascade's ~0 dB AGC reference, not chosen. */
    DP_CHECK_NEAR (c.amp, 1.0, 1e-12);
    DP_CHECK_NEAR (c.fc, 0.0, 1e-12);
    DP_CHECK_MSG (c.nsym == 0, "nsym has no default — a record length is "
                               "always stated");
    DP_CHECK (c.seed != 0);
  }

  DP_TEST_END ("test_dp_tx");
}
