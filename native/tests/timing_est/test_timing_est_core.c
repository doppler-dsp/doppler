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
  DP_TEST_END ("test_timing_est_core");
}
