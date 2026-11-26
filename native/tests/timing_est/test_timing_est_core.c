/* test_timing_est_core.c -- the feedforward symbol-timing estimator's
 * claims, one section a claim of timing_est_core.h.
 *
 * The stimulus here is a stream whose POWER is written down exactly,
 *
 *     |x[n]|^2 = A + B cos (2 pi (t[n] - tau)),   t[n] = n / period
 *
 * so the answer is known without a pulse, a filter or noise: `tau` is the
 * eye center and `B / (2 A)` is the quality. What a real pulse and noise do
 * is the validator's question, not this file's.
 *
 * A finite block of such a stream is not exactly its Fourier coefficient:
 * the constant `A` and the line's image leak into the bin. Both are bounded
 * by a geometric sum, `1 / |sin (pi / sps)|` samples' worth, and every
 * tolerance below is that bound, computed, never a number chosen to pass.
 */
#include "doppler/timing_est/timing_est_core.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define A_DC 1.0
#define B_LINE 0.5

/* n samples of the stream above, on a carrier of `w` radians a sample from
   phase `th`. `period` is the symbols' own period in samples, which is the
   estimator's `sps` unless a test is moving the clock. `n0` is the index of
   the first sample, so two calls make one continuous stream. */
static void
make (float _Complex *x, size_t n, size_t n0, double period, double tau,
      double w, double th)
{
  for (size_t i = 0; i < n; i++)
    {
      double t = (double)(n0 + i) / period;
      double p = A_DC + B_LINE * cos (2.0 * M_PI * (t - tau));
      double a = w * (double)(n0 + i) + th;
      x[i] = CMPLXF ((float)(sqrt (p) * cos (a)), (float)(sqrt (p) * sin (a)));
    }
}

/* The leak of the constant and of the line's image into the bin, as a
   timing error in symbols, for a block of n samples. */
static double
leak (double sps, size_t n)
{
  double geo = 1.0 / fabs (sin (M_PI / sps));       /* sum of a tone  */
  double img = 1.0 / fabs (sin (2.0 * M_PI / sps)); /* at twice it    */
  double sig = 0.5 * B_LINE * (double)n;
  return (A_DC * geo + 0.5 * B_LINE * img) / sig / (2.0 * M_PI)
         + 1e-6; /* float32 samples */
}

static double
wrap (double d)
{
  return d - floor (d + 0.5);
}

/* Two estimates, field for field and bit for bit. The record has padding
   after its int, so a memcmp of the whole would compare garbage. */
static int
same (const dp_timing_estimate_t *a, const dp_timing_estimate_t *b)
{
  return memcmp (&a->tau, &b->tau, sizeof a->tau) == 0
         && memcmp (&a->rate, &b->rate, sizeof a->rate) == 0
         && memcmp (&a->quality, &b->quality, sizeof a->quality) == 0
         && memcmp (&a->power, &b->power, sizeof a->power) == 0
         && memcmp (&a->symbols, &b->symbols, sizeof a->symbols) == 0
         && memcmp (&a->eye, &b->eye, sizeof a->eye) == 0
         && a->has_rate == b->has_rate;
}

/* ── §1 create refuses what the line cannot carry ─────────────────────── */
static void
test_create (void)
{
  static const double bad[] = { 2.0, 1.0, 0.0, -4.0, NAN, INFINITY };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    DP_CHECK (dp_timing_est_create (bad[i]) == NULL);
  /* the control: just above the bound is taken */
  dp_timing_est_state_t *s = dp_timing_est_create (2.001);
  DP_CHECK (s != NULL);
  dp_timing_est_destroy (s);
  dp_timing_est_destroy (NULL);
}

/* ── §2 tau and quality, on whole and fractional samples a symbol ─────── */
static void
test_estimate (void)
{
  enum
  {
    NSYM = 256
  };
  static const double spss[] = { 4.0, 4.3, 3.1, 8.0, 17.33389, 2.5 };
  static const double taus[] = { 0.0, 0.25, -0.25, 0.4, -0.49, 0.123 };
  for (size_t a = 0; a < sizeof spss / sizeof *spss; a++)
    for (size_t b = 0; b < sizeof taus / sizeof *taus; b++)
      {
        size_t                 n = (size_t)(NSYM * spss[a]);
        float _Complex        *x = malloc (n * sizeof *x);
        dp_timing_est_state_t *s = dp_timing_est_create (spss[a]);
        DP_CHECK (x && s);
        if (!x || !s)
          {
            free (x);
            dp_timing_est_destroy (s);
            continue;
          }
        make (x, n, 0, spss[a], taus[b], 0.0, 0.0);
        dp_timing_estimate_t e   = dp_timing_est_estimate (s, x, n);
        double               tol = leak (spss[a], n);
        DP_CHECK (fabs (wrap (e.tau - taus[b])) < tol);
        DP_CHECK (fabs (e.quality - B_LINE / (2.0 * A_DC)) < 2.0 * M_PI * tol);
        DP_CHECK (fabs (e.symbols - (double)n / spss[a]) < 1e-9);
        DP_CHECK (fabs (e.power - A_DC) < 2.0 * M_PI * tol * A_DC);
        DP_CHECK (e.has_rate == 0 && e.rate == 0.0);
        if (!(fabs (wrap (e.tau - taus[b])) < tol))
          DP_TEST_ERR ("  sps %g tau %g: got %g, tol %g\n", spss[a], taus[b],
                       e.tau, tol);
        /* the control: the tolerance is tight enough to tell a timing a
           hundredth of a symbol away, so the check above can fail */
        DP_CHECK (tol < 0.01);
        dp_timing_est_destroy (s);
        free (x);
      }
}

/* ── §3 the carrier does not enter ────────────────────────────────────── */
static void
test_carrier (void)
{
  enum
  {
    N = 1024
  };
  float _Complex x0[N], x1[N];
  double pr = 0.0, pi = 0.0, sr = 0.0;
  make (x0, N, 0, 4.0, 0.2, 0.0, 0.0);
  /* An offset and a phase. The offset is an eighth of the sample rate on
     purpose: the real part's power then carries a tone at twice that, a
     quarter of the sample rate, which is exactly the symbol-rate bin at 4
     samples a symbol -- the worst carrier for an estimator that saw it. */
  make (x1, N, 0, 4.0, 0.2, M_PI / 4.0, 1.1);
  dp_timing_est_state_t *a = dp_timing_est_create (4.0);
  dp_timing_est_state_t *b = dp_timing_est_create (4.0);
  DP_CHECK (a && b);
  if (!a || !b)
    return;
  dp_timing_estimate_t ea = dp_timing_est_estimate (a, x0, N);
  dp_timing_estimate_t eb = dp_timing_est_estimate (b, x1, N);
  /* float32 samples: |x|^2 agrees to a few parts in 1e7 a sample */
  DP_CHECK (fabs (ea.tau - eb.tau) < 1e-6);
  DP_CHECK (fabs (ea.quality - eb.quality) < 1e-6);
  /* The control: an estimator that used the real part's power alone would
     see the carrier. Formed here by hand on the same samples, it moves by
     much more than the tolerance above. */
  for (size_t i = 0; i < N; i++)
    {
      double p = (double)crealf (x1[i]) * (double)crealf (x1[i]);
      double t = 2.0 * M_PI * (double)i / 4.0;
      pr += p * cos (t);
      pi -= p * sin (t);
      sr += p;
    }
  double tau_re = -atan2 (pi, pr) / (2.0 * M_PI);
  DP_CHECK (fabs (wrap (tau_re - ea.tau)) > 1e-4 || sr <= 0.0);
  dp_timing_est_destroy (a);
  dp_timing_est_destroy (b);
}

/* ── §4 one clock across calls, and the rate from two blocks ──────────── */
static void
test_rate (void)
{
  enum
  {
    N = 1024
  }; /* 256 symbols at 4 */
  static const double eps[] = { 0.0, 1e-3, -1e-3, 4e-4 };
  for (size_t i = 0; i < sizeof eps / sizeof *eps; i++)
    {
      float _Complex x[N];
      dp_timing_est_state_t *s = dp_timing_est_create (4.0);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      double period = 4.0 * (1.0 + eps[i]);
      make (x, N, 0, period, 0.1, 0.2, 0.0);
      dp_timing_estimate_t e1 = dp_timing_est_estimate (s, x, N);
      make (x, N, N, period, 0.1, 0.2, 0.0);
      dp_timing_estimate_t e2 = dp_timing_est_estimate (s, x, N);
      DP_CHECK (e1.has_rate == 0);
      DP_CHECK (e2.has_rate == 1);
      /* Two estimates, each within the leak, 256 symbols apart. The eye
         centers fall where n / period = tau + k, which on the clock
         (n / sps) is (tau + k) (1 + eps): eps later each symbol of the
         stream, so eps / (1 + eps) each symbol of the clock. */
      double want = eps[i] / (1.0 + eps[i]);
      double tol  = 2.0 * leak (4.0, N) / 256.0;
      DP_CHECK (fabs (e2.rate - want) < tol);
      if (!(fabs (e2.rate - want) < tol))
        DP_TEST_ERR ("  eps %g: rate %g, want %g, tol %g\n", eps[i], e2.rate,
                     want, tol);
      /* the control: the tolerance tells the smallest offset here from
         none at all */
      DP_CHECK (tol < 4e-4);
      dp_timing_est_destroy (s);
    }
}

/* ── §5 estimate() is accumulate() then finish(), and blocks are the
 *      caller's: nothing but the clock crosses a block ─────────────────── */
static void
test_forms (void)
{
  enum
  {
    N = 777
  };
  float _Complex x[N];
  make (x, N, 0, 4.3, -0.3, 0.1, 0.5);
  dp_timing_est_state_t *a = dp_timing_est_create (4.3);
  dp_timing_est_state_t *b = dp_timing_est_create (4.3);
  DP_CHECK (a && b);
  if (!a || !b)
    return;
  dp_timing_estimate_t ea = dp_timing_est_estimate (a, x, N);
  for (size_t i = 0; i < N; i++)
    {
      double re = (double)crealf (x[i]), im = (double)cimagf (x[i]);
      dp_timing_est_accumulate (b, re * re + im * im);
    }
  dp_timing_estimate_t eb = dp_timing_est_finish (b);
  DP_CHECK (same (&ea, &eb));
  /* an empty block: zeros out, nothing changed */
  dp_timing_estimate_t z = dp_timing_est_estimate (a, x, 0);
  DP_CHECK (z.symbols == 0.0 && z.quality == 0.0 && z.has_rate == 0);
  /* a block with no power: tau and quality 0, and it is not the previous
     block, so the next rate is still against the last block with power */
  float _Complex silent[64] = { 0 };
  dp_timing_estimate_t q    = dp_timing_est_estimate (a, silent, 64);
  DP_CHECK (q.tau == 0.0 && q.quality == 0.0 && q.power == 0.0);
  DP_CHECK (q.symbols > 0.0 && q.has_rate == 0);
  make (x, N, N + 64, 4.3, -0.3, 0.1, 0.5);
  dp_timing_estimate_t e3 = dp_timing_est_estimate (a, x, N);
  DP_CHECK (e3.has_rate == 1);
  DP_CHECK (fabs (e3.rate) < 2.0 * leak (4.3, N) / ((N + 64) / 4.3));
  dp_timing_est_destroy (a);
  dp_timing_est_destroy (b);
}

/* ── §6 reset returns the object to its post-create state ─────────────── */
static void
test_reset (void)
{
  enum
  {
    N = 500
  };
  float _Complex x[N];
  make (x, N, 0, 5.0, 0.3, 0.0, 0.0);
  dp_timing_est_state_t *s = dp_timing_est_create (5.0);
  DP_CHECK (s != NULL);
  if (!s)
    return;
  dp_timing_estimate_t first = dp_timing_est_estimate (s, x, N);
  (void)dp_timing_est_estimate (s, x, 123); /* move the clock off zero */
  dp_timing_est_accumulate (s, 1.0);        /* and leave a block open  */
  dp_timing_est_reset (s);
  dp_timing_estimate_t again = dp_timing_est_estimate (s, x, N);
  DP_CHECK (same (&first, &again));
  DP_CHECK (s->sps == 5.0);
  dp_timing_est_destroy (s);
}

/* ── §7 the state resumes bit for bit, mid-block, and refuses forgeries ─ */
static void
test_state (void)
{
  enum
  {
    N     = 900,
    SPLIT = 411
  };
  float _Complex x[N];
  make (x, N, 0, 4.3, 0.2, 0.3, 0.0);
  dp_timing_est_state_t *a = dp_timing_est_create (4.3);
  dp_timing_est_state_t *b = dp_timing_est_create (4.3);
  DP_CHECK (a && b);
  if (!a || !b)
    return;
  /* a first block in both, so the previous block is state too */
  (void)dp_timing_est_estimate (a, x, N);
  (void)dp_timing_est_estimate (b, x, N);
  /* a: the second block whole. b: split mid-block through a blob into c. */
  dp_timing_estimate_t whole = dp_timing_est_estimate (a, x, N);
  for (size_t i = 0; i < SPLIT; i++)
    {
      double re = (double)crealf (x[i]), im = (double)cimagf (x[i]);
      dp_timing_est_accumulate (b, re * re + im * im);
    }
  size_t                 nb   = dp_timing_est_state_bytes (b);
  uint8_t               *blob = malloc (nb);
  dp_timing_est_state_t *c    = dp_timing_est_create (4.3);
  DP_CHECK (blob && c);
  if (!blob || !c)
    return;
  dp_timing_est_get_state (b, blob);
  DP_CHECK (dp_timing_est_set_state (c, blob) == DP_OK);
  for (size_t i = SPLIT; i < N; i++)
    {
      double re = (double)crealf (x[i]), im = (double)cimagf (x[i]);
      dp_timing_est_accumulate (c, re * re + im * im);
    }
  /* The shared round trip, on the same mid-block state into a fresh
     object: every byte of the blob written, b re-serializing as a, and the
     clobbered envelope refused. */
  {
    dp_timing_est_state_t *fresh = dp_timing_est_create (4.3);
    DP_CHECK (fresh != NULL);
    if (fresh)
      DP_STATE_ROUNDTRIP_TEST (dp_timing_est, b, fresh);
    dp_timing_est_destroy (fresh);
  }
  dp_timing_estimate_t resumed = dp_timing_est_finish (c);
  DP_CHECK (same (&whole, &resumed));
  DP_CHECK (resumed.has_rate == 1); /* the previous block came across */

  /* Refusals. Each leaves the target as it was: its next estimate is the
     one an untouched twin gives. */
  dp_timing_est_state_t *t = dp_timing_est_create (4.3);
  dp_timing_est_state_t *u = dp_timing_est_create (4.3);
  DP_CHECK (t && u);
  if (!t || !u)
    return;
  uint8_t *bad = malloc (nb);
  DP_CHECK (bad != NULL);
  if (!bad)
    return;
  /* the envelope */
  memcpy (bad, blob, nb);
  bad[0] ^= 0xFF;
  DP_CHECK (dp_timing_est_set_state (t, bad) == DP_ERR_INVALID);
  /* a payload field the envelope cannot see: the last double is the
     previous block's middle, and a negative one is no state this object
     reaches */
  memcpy (bad, blob, nb);
  double neg = -1.0;
  memcpy (bad + nb - sizeof neg, &neg, sizeof neg);
  DP_CHECK (dp_timing_est_set_state (t, bad) == DP_ERR_INVALID);
  /* the one before it is the previous tau: outside (-1/2, 1/2] */
  memcpy (bad, blob, nb);
  double high = 0.75;
  memcpy (bad + nb - 2 * sizeof high, &high, sizeof high);
  DP_CHECK (dp_timing_est_set_state (t, bad) == DP_ERR_INVALID);
  /* the control: the untouched blob is taken by the same object */
  dp_timing_estimate_t et = dp_timing_est_estimate (t, x, N);
  dp_timing_estimate_t eu = dp_timing_est_estimate (u, x, N);
  DP_CHECK (same (&et, &eu));
  DP_CHECK (dp_timing_est_set_state (t, blob) == DP_OK);

  free (bad);
  free (blob);
  dp_timing_est_destroy (a);
  dp_timing_est_destroy (b);
  dp_timing_est_destroy (c);
  dp_timing_est_destroy (t);
  dp_timing_est_destroy (u);
}

/* ── §8 a whole sps folds the power's third harmonic onto the line ──────
 *
 * The header's caveat on `tau`: a pulse that is not band limited, read at a
 * whole number of samples a symbol, is off the eye center by where the
 * samples fall in the symbol. The mechanism needs no pulse: give the power
 * a third harmonic,
 *
 *     |x[n]|^2 = A + B cos (2 pi (t - tau)) + C cos (2 pi 3 (t - tau))
 *
 * and at sps 4 its image at -3 lands on the bin, so
 *
 *     c = (n / 2) exp (-j 2 pi tau) (B + C exp (j 8 pi tau))
 *     tau_hat = tau - arg (B + C exp (j 8 pi tau)) / (2 pi)
 *
 * a bias of period a quarter symbol (one sample) and size asin (C / B) /
 * (2 pi). C / B is 0.0707 here, which is 0.0113 symbols: what the
 * validation report's finding `whole_n_rect_sampling_phase` measured for
 * the rectangle. At sps 4.3 nothing folds and the bias is gone. */
#define C_THIRD (0.0707 * B_LINE)

static void
make3 (float _Complex *x, size_t n, double period, double tau)
{
  for (size_t i = 0; i < n; i++)
    {
      double t = (double)i / period;
      double p = A_DC + B_LINE * cos (2.0 * M_PI * (t - tau))
                 + C_THIRD * cos (6.0 * M_PI * (t - tau));
      x[i]     = CMPLXF ((float)sqrt (p), 0.0f);
    }
}

static void
test_whole_sps_fold (void)
{
  enum
  {
    NSYM = 256,
    NTAU = 8 /* the timings where the fold peaks, both signs */
  };
  static const double spss[2]  = { 4.0, 4.3 };
  double              worst[2] = { 0.0, 0.0 }; /* largest |tau_hat - tau| */
  for (int a = 0; a < 2; a++)
    for (int b = 0; b < NTAU; b++)
      {
        double                 tau = -0.5 + ((double)b + 0.5) / NTAU;
        size_t                 n   = (size_t)(NSYM * spss[a]);
        float _Complex        *x   = malloc (n * sizeof *x);
        dp_timing_est_state_t *s   = dp_timing_est_create (spss[a]);
        DP_CHECK (x && s);
        if (!x || !s)
          {
            free (x);
            dp_timing_est_destroy (s);
            continue;
          }
        make3 (x, n, spss[a], tau);
        dp_timing_estimate_t e = dp_timing_est_estimate (s, x, n);
        /* what does not fold still leaks: the third harmonic's images sit
           2 / sps and 4 / sps from the bin, each bounded by its own
           geometric sum (at sps 4 the second IS the fold, predicted below
           and not a leak) */
        double sig = 0.5 * B_LINE * (double)n;
        double g   = 1.0 / fabs (sin (2.0 * M_PI / spss[a]));
        if (a == 1)
          g += 1.0 / fabs (sin (4.0 * M_PI / spss[a]));
        double tol
            = leak (spss[a], n) + 0.5 * C_THIRD * g / sig / (2.0 * M_PI);
        double fold = a == 0
                          ? -atan2 (C_THIRD * sin (8.0 * M_PI * tau),
                                    B_LINE + C_THIRD * cos (8.0 * M_PI * tau))
                                / (2.0 * M_PI)
                          : 0.0;
        double off  = wrap (e.tau - tau);
        DP_CHECK (fabs (off - fold) < tol);
        if (!(fabs (off - fold) < tol))
          DP_TEST_ERR ("  sps %g tau %g: off %g, fold %g, tol %g\n", spss[a],
                       tau, off, fold, tol);
        if (fabs (off) > worst[a])
          worst[a] = fabs (off);
        /* the tolerance is far under the effect, so the check can fail */
        DP_CHECK (tol < 0.002);
        dp_timing_est_destroy (s);
        free (x);
      }
  /* The bias is there at the whole rate and not at the other: asin (C / B)
     / (2 pi) is 0.0113, reached to within the sampling of tau. The control
     is the pair: the same stream, one rate apart, on the same bound. */
  DP_CHECK (worst[0] > 0.010);
  DP_CHECK (worst[1] < 0.002);
}

/* ── §9 the matched flavor: its own filter, and the input's timeline ───── */

/* Alternating symbols through a root-raised cosine are one tone at half
   the symbol rate, for any roll-off: the next line of an alternating
   stream is at three halves, past the pulse's band. So this is what a
   root-raised-cosine transmitter sends for +1 -1 +1 ..., with a symbol
   instant at input sample `n0` and every `sps` after it, written down
   exactly and for any real `sps`. Matched-filtered it is the same tone,
   and its power is A + A cos (2 pi (t - n0) / sps): the stimulus of this
   file's other sections, with the eye center at `n0`. */
static void
make_alt (float _Complex *x, size_t n, double sps, double n0, double f_sym)
{
  for (size_t i = 0; i < n; i++)
    {
      double v = 0.25 * cos (M_PI * ((double)i - n0) / sps);
      /* a carrier offset of f_sym cycles a symbol */
      double a = 2.0 * M_PI * f_sym * (double)i / sps;
      x[i]     = CMPLXF ((float)(v * cos (a)), (float)(v * sin (a)));
    }
}

/* The same, held: +1 -1 +1 ... as a rectangle, symbol j over the samples
   n0 + j sps .. n0 + (j + 1) sps - 1, so its center is (sps - 1) / 2
   samples after n0. Whole sps only. */
static void
make_alt_rect (float _Complex *x, size_t n, size_t sps, size_t n0)
{
  for (size_t i = 0; i < n; i++)
    {
      size_t j = (i + 2 * sps * 64 - n0) / sps;
      x[i]     = CMPLXF ((j & 1u) ? -0.25f : 0.25f, 0.0f);
    }
}

static int
test_matched (void)
{
  /* refusals: the terminal stage does not interpolate, and the converter's
     own domain */
  DP_CHECK (dp_timing_est_create_matched (3.9, RC_PULSE_RRC, 0.35, 8) == NULL);
  DP_CHECK (dp_timing_est_create_matched (NAN, RC_PULSE_RRC, 0.35, 8) == NULL);
  DP_CHECK (dp_timing_est_create_matched (INFINITY, RC_PULSE_RRC, 0.35, 8)
            == NULL);
  DP_CHECK (dp_timing_est_create_matched (8.0, 7, 0.35, 8) == NULL);
  DP_CHECK (dp_timing_est_create_matched (8.0, RC_PULSE_RRC, 1.5, 8) == NULL);
  DP_CHECK (dp_timing_est_create_matched (8.0, RC_PULSE_RRC, 0.35, 0) == NULL);

  /* The eye, on the input's timeline, at whole and fractional sps, behind
     every kind of plan (one stage, halfbands, a CIC with its compensator
     folded in), with a carrier offset of 0.02 cycles a symbol on the
     root-raised-cosine rows.
       What the eye is held to is computed, a row at a time. The block's
     leak (the constant and the line's image, as in §2: here A = B and the
     clock runs at 4). For the root-raised cosine, how far the bank's
     low-frequency delay, which is what `eye` takes out, sits from its
     pulse's center, which is where the eye is: the bank's arms truncate
     the pulse's tails unevenly, 0.005 to 0.009 symbols at span 8
     (doppler#3368). For the rectangle, the sampling-phase bias the header
     states for a whole number of samples a symbol, 0.011 symbols.
       Sabotages, each shown red: the group delay left out of `eye` ->
     all twelve rows; the lead-in left out of it -> the eight rows whose
     lead-in is not a whole number of symbols; `eye` left in the clock's
     samples -> the nine rows off 4 samples a symbol whose eye is not at
     input 0 modulo a symbol; the lead-in not skipped -> the same eight
     rows, and the lead-in checks below. */
  static const struct
  {
    double sps, n0;
    int    pulse;
  } rows[] = {
    { 4.0, 1.25, RC_PULSE_RRC },    { 5.0, 3.5, RC_PULSE_RRC },
    { 8.0, 2.0, RC_PULSE_RRC },     { 8.0, 6.75, RC_PULSE_RRC },
    { 17.333, 11.1, RC_PULSE_RRC }, { 32.0, 20.0, RC_PULSE_RRC },
    { 64.0, 5.5, RC_PULSE_RRC },    { 67.3, 40.0, RC_PULSE_RRC },
    { 4.0, 1.0, RC_PULSE_IANDD },   { 8.0, 5.0, RC_PULSE_IANDD },
    { 16.0, 3.0, RC_PULSE_IANDD },  { 64.0, 50.0, RC_PULSE_IANDD },
  };
  enum
  {
    L = 256
  };
  /* A = B on a clock of 4: geo = 1 / sin (pi / 4), img = 1 / sin (pi / 2) */
  const double leak4
      = (1.0 / sin (M_PI / 4.0) + 0.5) / (0.5 * 4.0 * L) / (2.0 * M_PI) + 1e-6;
  for (size_t r = 0; r < sizeof rows / sizeof *rows; r++)
    {
      const double           sps = rows[r].sps;
      dp_timing_est_state_t *s
          = dp_timing_est_create_matched (sps, rows[r].pulse, 0.35, 8);
      DP_REQUIRE (s != NULL);
      DP_CHECK (s->lead > 0 && s->delay > 0.0 && s->sps == sps);
      const size_t n
          = (size_t)(((double)s->lead + 4.0 * L + 2.0) * sps / 4.0) + 8;
      float _Complex *x = malloc (n * sizeof *x);
      DP_REQUIRE (x != NULL);
      double want, tol;
      if (rows[r].pulse == RC_PULSE_RRC)
        {
          make_alt (x, n, sps, rows[r].n0, 0.02);
          want = rows[r].n0;
          /* the pulse's center in the bank: its span, and the folded
             droop compensator's own center when the plan has a CIC */
          double center
              = 8.0 * s->mf->bank_sps
                + (s->mf->stage_types[0] == RC_STAGE_CIC ? 3.0 : 0.0);
          tol = leak4 + fabs (s->mf->bank_delay - center) / s->mf->bank_sps
                + 1e-3;
        }
      else
        {
          make_alt_rect (x, n, (size_t)sps, (size_t)rows[r].n0);
          want = rows[r].n0 + 0.5 * (sps - 1.0);
          tol  = leak4 + 0.011;
        }
      dp_timing_estimate_t e    = dp_timing_est_estimate (s, x, n);
      double               miss = wrap ((e.eye - want) / sps); /* symbols */
      printf ("  matched sps %.3f pulse %d: lead %llu, delay %.3f, %.0f "
              "symbols, quality %.3f, eye off by %+.5f of %.5f symbols\n",
              sps, rows[r].pulse, (unsigned long long)s->lead, s->delay,
              e.symbols, e.quality, miss, tol);
      DP_CHECK (e.symbols >= (double)L);
      DP_CHECK (e.quality > 0.4);
      DP_CHECK (fabs (miss) <= tol);
      /* the eye it names is the one nearest the block's middle */
      double mid_in
          = ((double)s->lead + 0.5 * 4.0 * e.symbols) * sps / 4.0 - s->delay;
      DP_CHECK (fabs (e.eye - mid_in) <= 0.5 * sps + 1e-6);
      free (x);
      dp_timing_est_destroy (s);
    }

  /* The lead-in is the object's: a block that ends inside it gave nothing,
     and the block after it is counted from the first output past it.
     Sabotage: the lead-in not skipped -> the first block has symbols. */
  {
    dp_timing_est_state_t *s
        = dp_timing_est_create_matched (8.0, RC_PULSE_RRC, 0.35, 8);
    DP_REQUIRE (s != NULL);
    enum
    {
      N = 4096
    };
    static float _Complex x[N];
    make_alt (x, N, 8.0, 3.0, 0.0);
    const size_t         in_lead = (size_t)(s->lead - 1) * 2; /* rate 1/2 */
    dp_timing_estimate_t e0      = dp_timing_est_estimate (s, x, in_lead);
    DP_CHECK (e0.symbols == 0.0 && e0.power == 0.0 && e0.quality == 0.0);
    DP_CHECK (s->lead_left > 0 && s->count == 0);
    dp_timing_estimate_t e1
        = dp_timing_est_estimate (s, x + in_lead, N - in_lead);
    DP_CHECK (s->lead_left == 0);
    DP_CHECK (e1.symbols == (double)((N / 2) - s->lead) / 4.0);
    DP_CHECK (fabs (wrap ((e1.eye - 3.0) / 8.0)) < 0.011);

    /* reset is create's state: the same stream gives the same records.
       Sabotage: the lead-in not re-armed -> the first record. */
    dp_timing_est_reset (s);
    dp_timing_estimate_t r0 = dp_timing_est_estimate (s, x, in_lead);
    dp_timing_estimate_t r1
        = dp_timing_est_estimate (s, x + in_lead, N - in_lead);
    DP_CHECK (same (&e0, &r0) && same (&e1, &r1));

    /* The converter is reset with it. The lead-in outlasts the
       converter's memory, so what a stale delay line holds never reaches
       a record; what does is the terminal stage's phase, which at 5
       samples a symbol (rate 0.8) is left off zero by a stream whose
       length is not a multiple of 5. Sabotage: the converter not reset. */
    dp_timing_est_state_t *s5
        = dp_timing_est_create_matched (5.0, RC_PULSE_RRC, 0.35, 8);
    DP_REQUIRE (s5 != NULL);
    make_alt (x, N, 5.0, 2.0, 0.0);
    dp_timing_estimate_t f5 = dp_timing_est_estimate (s5, x, 2001);
    dp_timing_est_reset (s5);
    dp_timing_estimate_t g5 = dp_timing_est_estimate (s5, x, 2001);
    DP_CHECK (f5.symbols > 100.0 && same (&f5, &g5));
    dp_timing_est_destroy (s5);
    make_alt (x, N, 8.0, 3.0, 0.0);

    /* The state resumes bit for bit from inside the lead-in and from
       inside a block, and the converter is in it. Sabotage: the
       converter's sub-blob not written -> both splits. */
    static const size_t splits[2] = { 40, 1500 };
    for (size_t c = 0; c < 2; c++)
      {
        dp_timing_est_state_t *a
            = dp_timing_est_create_matched (8.0, RC_PULSE_RRC, 0.35, 8);
        dp_timing_est_state_t *b
            = dp_timing_est_create_matched (8.0, RC_PULSE_RRC, 0.35, 8);
        DP_REQUIRE (a != NULL && b != NULL);
        dp_timing_estimate_t whole = dp_timing_est_estimate (a, x, N);
        dp_timing_est_reset (a);
        /* a block left open across the hand-off: feed without finishing */
        dp_timing_estimate_t part = dp_timing_est_estimate (a, x, splits[c]);
        (void)part;
        const size_t nb   = dp_timing_est_state_bytes (a);
        uint8_t     *blob = malloc (nb);
        DP_REQUIRE (blob != NULL);
        DP_CHECK (nb > dp_RateConverter_state_bytes (a->mf));
        dp_timing_est_get_state (a, blob);
        (void)dp_timing_est_estimate (b, x, 777); /* b somewhere else */
        DP_CHECK (dp_timing_est_set_state (b, blob) == DP_OK);
        dp_timing_estimate_t ra
            = dp_timing_est_estimate (a, x + splits[c], N - splits[c]);
        dp_timing_estimate_t rb
            = dp_timing_est_estimate (b, x + splits[c], N - splits[c]);
        DP_CHECK (same (&ra, &rb));
        DP_CHECK (ra.symbols > 0.0);
        (void)whole;
        /* a refused converter leaves the object as it was: clobber the
           child's envelope, which starts right after this object's own
           fields */
        uint8_t      before[512];
        const size_t own = nb - dp_RateConverter_state_bytes (b->mf);
        dp_timing_est_get_state (b, blob);
        uint8_t *keep = malloc (nb);
        DP_REQUIRE (keep != NULL);
        memcpy (keep, blob, nb);
        (void)dp_timing_est_estimate (a, x, 333); /* a moves on */
        dp_timing_est_get_state (a, blob);
        blob[own] ^= 0xFFu;
        DP_CHECK (dp_timing_est_set_state (b, blob) == DP_ERR_INVALID);
        dp_timing_est_get_state (b, blob);
        DP_CHECK (memcmp (blob, keep, nb) == 0);
        /* a plain blob is not a matched one's, and the reverse */
        dp_timing_est_state_t *p = dp_timing_est_create (4.0);
        DP_REQUIRE (p != NULL);
        DP_CHECK (dp_timing_est_state_bytes (p) < sizeof before);
        dp_timing_est_get_state (p, before);
        DP_CHECK (dp_timing_est_set_state (b, before) == DP_ERR_INVALID);
        DP_CHECK (dp_timing_est_set_state (p, keep) == DP_ERR_INVALID);
        dp_timing_est_destroy (p);
        free (keep);
        free (blob);
        dp_timing_est_destroy (a);
        dp_timing_est_destroy (b);
      }
    dp_timing_est_destroy (s);
  }
  return 0;
}

/* ── §10 tau stays in (-1/2, 1/2], at the edge too ─────────────────────── */
/* Powers 0 1 0 1 on a clock of 4 put the coefficient on the negative real
   axis with an imaginary part of exactly +0 (the two samples' sines are +1
   and -1 and cancel), where atan2 returns +pi and -atan2 / 2 pi is -1/2:
   outside the interval the header states, and a `prev_tau` the object's
   own dp_timing_est_set_state() refuses. The estimate there is +1/2, and
   the blob that carries it restores. Sabotage: the fold removed -> tau
   reads -0.5 and the object refuses its own blob. */
static void
test_tau_edge (void)
{
  dp_timing_est_state_t *s = dp_timing_est_create (4.0);
  DP_CHECK (s != NULL);
  if (!s)
    return;
  static const double p[4] = { 0.0, 1.0, 0.0, 1.0 };
  for (int k = 0; k < 64; k++)
    dp_timing_est_accumulate (s, p[k & 3]);
  DP_CHECK (s->c_im == 0.0 && s->c_re < 0.0); /* the stimulus is the edge */
  dp_timing_estimate_t e = dp_timing_est_finish (s);
  DP_CHECK (e.tau == 0.5);
  uint8_t blob[256];
  DP_CHECK (dp_timing_est_state_bytes (s) <= sizeof blob);
  dp_timing_est_get_state (s, blob);
  DP_CHECK (dp_timing_est_set_state (s, blob) == DP_OK);
  dp_timing_est_destroy (s);
}

int
main (void)
{
  test_create ();
  test_estimate ();
  test_carrier ();
  test_rate ();
  test_forms ();
  test_reset ();
  test_state ();
  test_whole_sps_fold ();
  DP_CHECK (test_matched () == 0);
  test_tau_edge ();
  DP_TEST_END ("test_timing_est_core");
}
