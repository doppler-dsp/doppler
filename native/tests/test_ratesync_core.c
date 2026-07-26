/*
 * test_ratesync_core.c — C-level unit tests for RateSync.
 *
 * Tests cover:
 *   - Parameter validation (every out-of-range knob, NaN, sps < m)
 *   - The object owns a planned cascade whose terminal stage carries the pulse
 *   - Lock + EVM from every initial timing offset, across three cascades whose
 *     planned shapes differ in the ways that matter
 *   - A tracked sample-clock offset shows up in rate_est
 *   - step() == steps() bit-exact, and block-boundary invariance
 *   - reset() restores post-create behaviour
 *   - State round-trip mid-stream, blob equality, and envelope reject
 */

#include "ratesync/ratesync_core.h"

#include "wfm/wfm_dsp.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _fails = 0;

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

#define _BETA 0.35
#define _SPAN 8
#define _NSYM 3000

/* Deterministic +-1 BPSK from an ITERATED xorshift32.
 *
 * A one-shot LCG of the symbol index (x = k*A + C) looks random per sample but
 * is strongly periodic across consecutive k -- it produced the run pattern
 * "---+++----+++---+++" here. That is fine for measuring a filter, and it was
 * fine in the Layer 1 tests, but it starves a Gardner detector: the TED only
 * learns from symbol TRANSITIONS, so a pattern with few independent ones
 * leaves the eye statistic near zero and the loop unable to declare lock even
 * while its output EVM is excellent. Generate the symbol sequence once, with a
 * real PRNG, and index it. */
static void
_symbols (int *out, size_t n, unsigned seed)
{
  unsigned st = seed ? seed : 1u;
  for (size_t k = 0; k < n; k++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      out[k] = (st & 1u) ? 1 : -1;
    }
}

/* Analytic RRC-shaped BPSK at `sps` samples/symbol with timing offset `tau`
 * symbols.  Amplitude stays well inside the CIC's +-1.0 input bound —
 * overdriving it clips, which costs 25 dB for reasons that have nothing to do
 * with timing (see cic_core.h). */
static float complex *
_tx (double sps, double tau, size_t *n_out)
{
  size_t         n  = (size_t)(_NSYM * sps) + 64;
  float complex *x  = calloc (n, sizeof *x);
  int           *sy = malloc (_NSYM * sizeof *sy);
  if (!x || !sy)
    {
      free (x);
      free (sy);
      return NULL;
    }
  _symbols (sy, _NSYM, 7u);
  for (size_t i = 0; i < n; i++)
    {
      double a = 0.0;
      for (int k = 0; k < _NSYM; k++)
        {
          double t = ((double)i - (k + _SPAN) * sps) / sps - tau;
          if (fabs (t) > _SPAN)
            continue;
          a += sy[k] * wfm_rrc_h (t, _BETA);
        }
      x[i] = (float)(0.25 * a);
    }
  free (sy);
  *n_out = n;
  return x;
}

/* Steady-state EVM in dB against the LS-scaled hard decision, over the final
 * quarter only.  A window containing an acquisition cycle slip reads ~20 dB
 * worse with a perfectly healthy eye, so measure where the loop has settled
 * and let lock_stat make the lock decision. */
static double
_evm_db (const float complex *y, size_t n)
{
  if (n < 400)
    return 0.0;
  size_t               off = 3 * n / 4, cnt = n - off;
  const float complex *v   = y + off;
  double               num = 0.0;
  for (size_t i = 0; i < cnt; i++)
    num += (crealf (v[i]) >= 0.0f ? 1.0 : -1.0) * (double)crealf (v[i]);
  double g = num / (double)cnt;
  if (g == 0.0)
    return 0.0;
  double e = 0.0;
  for (size_t i = 0; i < cnt; i++)
    {
      double d
          = (double)crealf (v[i]) - g * (crealf (v[i]) >= 0.0f ? 1.0 : -1.0);
      e += d * d + (double)cimagf (v[i]) * (double)cimagf (v[i]);
    }
  return 20.0 * log10 (sqrt (e / (g * g * (double)cnt)));
}

/* ------------------------------------------------------------------ */

static void
test_invalid_params (void)
{
  /* Every knob rejects rather than silently coercing. */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 1.5, 8, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL); /* beta > 1 */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, -0.1, 8, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL);
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 0, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL); /* span 0 */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 1, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL); /* m < 2: no half-symbol gate exists */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 3, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL); /* m odd: the gate would not land on m/2 */
  CHECK (ratesync_create (40.0, RATESYNC_PULSE_RRC, 0.35, 8,
                          RATESYNC_MAX_M + 2u, 1024, 0.01, 0.707,
                          RATESYNC_TED_GARDNER)
         == NULL); /* m beyond the in-struct ring */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1000, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL); /* num_phases not a power of two */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, -0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL);
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01, 0.0,
                          RATESYNC_TED_GARDNER)
         == NULL);
  CHECK (ratesync_create (4.0, 7, 0.35, 8, 2, 1024, 0.01, 0.707,
                          RATESYNC_TED_GARDNER)
         == NULL); /* unknown pulse */
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                          0.707, 9)
         == NULL); /* unknown TED */
  /* sps < m would make the terminal stage interpolate, and one input could
     then complete several strobes — which the single-symbol step() contract
     cannot express. */
  CHECK (ratesync_create (1.5, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL);
  /* NaN must be rejected, not accepted by a comparison that happens to be
     false. */
  CHECK (ratesync_create (NAN, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL);
  CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, NAN, 8, 2, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER)
         == NULL);
  ratesync_destroy (NULL); /* documented no-op */
}

static void
test_owns_a_matched_cascade (void)
{
  /* RateSync builds no filters: it owns a RateConverter whose terminal stage
     carries the pulse.  That stage must exist even when the rate divides
     exactly — it is simultaneously the matched filter and the timing
     element. */
  const double sps[] = { 4.0, 17.333333333, 64.0 };
  char         buf[64];
  for (size_t i = 0; i < 3; i++)
    {
      ratesync_state_t *s
          = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (!s)
        continue;
      int last = s->mf->n_stages - 1;
      CHECK (s->mf->stage_types[last] == RC_STAGE_RESAMP);
      CHECK (RateConverter_stage_label (s->mf, last, buf, sizeof buf));
      CHECK (strstr (buf, "rrc") != NULL);
      /* The control is referenced to the TERMINAL stage's rate, not the
         cascade's; getting that wrong under-drives the loop by the whole
         integer decimation in front. */
      CHECK (s->loop.term_rate
             == resamp_get_rate ((resamp_state_t *)s->mf->stage_ptrs[last]));
      /* The bank is sized by the post-decimation rate, so a 16x span of input
         rates leaves it the same size. */
      CHECK (
          resamp_get_num_taps ((const resamp_state_t *)s->mf->stage_ptrs[last])
          < 4u * _SPAN * 2u + 16u);
      ratesync_destroy (s);
    }
}

/* Run one cascade from every initial timing offset; report the worst case. */
static void
_lock_sweep (double sps, double evm_max_db, const char *label)
{
  const double taus[] = { 0.0, 0.13, 0.25, 0.37, 0.5, 0.62, 0.75, 0.9 };
  int          locked = 0;
  double       worst  = -200.0;
  for (size_t t = 0; t < 8; t++)
    {
      size_t         n;
      float complex *x = _tx (sps, taus[t], &n);
      float complex *y = calloc (n, sizeof *y);
      CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          return;
        }
      ratesync_state_t *s
          = ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (s)
        {
          size_t ns = ratesync_steps (s, x, n, y, n);
          double ev = _evm_db (y, ns);
          if (ev > worst)
            worst = ev;
          /* lock_stat, not EVM, is the lock decision: a single acquisition
             cycle slip drags a windowed EVM by 20 dB with the eye wide
             open. */
          if (ratesync_get_lock_stat (s) > 0.55)
            locked++;
          CHECK (ratesync_get_clipped (s) == 0); /* drive stayed in range */
          ratesync_destroy (s);
        }
      free (x);
      free (y);
    }
  CHECK (locked == 8);
  CHECK (worst < evm_max_db);
  if (locked != 8 || worst >= evm_max_db)
    fprintf (stderr, "  %s: %d/8 locked, worst EVM %.1f dB (limit %.1f)\n",
             label, locked, worst, evm_max_db);
}

static void
test_locks_across_planned_cascades (void)
{
  /* Three cascades whose planned shapes differ in the ways that matter: a
     halfband front end, a CIC with a fractional terminal rate, and a CIC with
     a terminal rate of exactly 1.0 (where one input can complete two output
     periods).  Each of the three exposed a different bug during development,
     and each hid the others. */
  _lock_sweep (4.0, -34.0, "sps=4 (HB + Resampler(1))");
  _lock_sweep (17.333333333, -32.0, "sps=17.333 (CIC(8) + Resampler(0.923))");
  _lock_sweep (64.0, -32.0, "sps=64 (CIC(32) + Resampler(1))");
}

static void
test_tracks_a_clock_offset (void)
{
  /* The point of an arbitrary-rate receiver: transmit at a clock the receiver
     was not told about and let the loop find it.  rate_est is read from the
     loop INTEGRATOR, so it is the estimator a rate-disciplining caller
     reads. */
  const double nominal  = 8.0;
  const double actual[] = { 8.0, 8.008, 7.992 }; /* 0, +1000, -1000 ppm */
  for (size_t i = 0; i < 3; i++)
    {
      size_t         n;
      float complex *x = _tx (actual[i], 0.2, &n);
      float complex *y = calloc (n, sizeof *y);
      CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      ratesync_state_t *s
          = ratesync_create (nominal, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                             1024, 0.005, 0.707, RATESYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (s)
        {
          (void)ratesync_steps (s, x, n, y, n);
          double est = ratesync_get_rate (s);
          CHECK (fabs (est - actual[i]) < 0.01);
          if (!(fabs (est - actual[i]) < 0.01))
            fprintf (stderr, "  clock offset: true %.4f, est %.4f\n",
                     actual[i], est);
          ratesync_destroy (s);
        }
      free (x);
      free (y);
    }
}

static void
test_step_equals_steps (void)
{
  /* One block, one sample at a time, or two arbitrary chunks — the object is
     block-boundary invariant, so all three must agree bit-for-bit. */
  size_t         n;
  float complex *x = _tx (17.333333333, 0.3, &n);
  float complex *a = calloc (n, sizeof *a);
  float complex *b = calloc (n, sizeof *b);
  float complex *c = calloc (n, sizeof *c);
  CHECK (x && a && b && c);
  if (!x || !a || !b || !c)
    {
      free (x);
      free (a);
      free (b);
      free (c);
      return;
    }

  ratesync_state_t *s1
      = ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                         1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  ratesync_state_t *s2
      = ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                         1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  ratesync_state_t *s3
      = ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                         1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  CHECK (s1 && s2 && s3);
  if (s1 && s2 && s3)
    {
      size_t na = ratesync_steps (s1, x, n, a, n);

      size_t nb = 0;
      for (size_t i = 0; i < n; i++)
        if (ratesync_step (s2, x[i], &b[nb]))
          nb++;

      size_t cut = n / 3;
      size_t nc  = ratesync_steps (s3, x, cut, c, n);
      nc += ratesync_steps (s3, x + cut, n - cut, c + nc, n - nc);

      CHECK (na == nb && na == nc);
      CHECK (na > 0);
      CHECK (memcmp (a, b, na * sizeof (float complex)) == 0);
      CHECK (memcmp (a, c, na * sizeof (float complex)) == 0);
    }
  ratesync_destroy (s1);
  ratesync_destroy (s2);
  ratesync_destroy (s3);
  free (x);
  free (a);
  free (b);
  free (c);
}

static void
test_reset (void)
{
  size_t         n;
  float complex *x = _tx (4.0, 0.4, &n);
  float complex *a = calloc (n, sizeof *a);
  float complex *b = calloc (n, sizeof *b);
  CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }
  ratesync_state_t *s
      = ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  CHECK (s != NULL);
  if (s)
    {
      size_t na = ratesync_steps (s, x, n, a, n);
      ratesync_reset (s);
      /* Post-reset the object must behave exactly as freshly created — the
         prime countdown re-arms along with the cascade's delay lines. */
      CHECK (ratesync_get_ctrl (s) == 0.0);
      CHECK (ratesync_get_locked (s) == 0);
      size_t nb = ratesync_steps (s, x, n, b, n);
      CHECK (na == nb);
      CHECK (na > 0 && memcmp (a, b, na * sizeof (float complex)) == 0);
      ratesync_destroy (s);
    }
  free (x);
  free (a);
  free (b);
}

static void
test_state_roundtrip (void)
{
  size_t         n;
  float complex *x = _tx (17.333333333, 0.2, &n);
  float complex *a = calloc (n, sizeof *a);
  float complex *b = calloc (n, sizeof *b);
  CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }
  ratesync_state_t *s1
      = ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                         1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  ratesync_state_t *s2
      = ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                         1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  CHECK (s1 && s2);
  if (s1 && s2)
    {
      size_t cut = n / 2;
      (void)ratesync_steps (s1, x, cut, a, n);

      size_t sb   = ratesync_state_bytes (s1);
      void  *blob = malloc (sb);
      CHECK (blob != NULL);
      if (blob)
        {
          ratesync_get_state (s1, blob);
          CHECK (ratesync_set_state (s2, blob) == DP_OK);

          /* Compare the BLOBS too, not just the symbols: a size miscount
             leaves uninitialised trailing bytes that a symbol comparison
             happily ignores — the bug the state matrix caught in the first
             RateSync prototype. */
          void *blob2 = malloc (sb);
          CHECK (blob2 != NULL);
          if (blob2)
            {
              ratesync_get_state (s2, blob2);
              CHECK (memcmp (blob, blob2, sb) == 0);
              free (blob2);
            }

          size_t na = ratesync_steps (s1, x + cut, n - cut, a, n);
          size_t nb = ratesync_steps (s2, x + cut, n - cut, b, n);
          CHECK (na == nb);
          CHECK (na > 0 && memcmp (a, b, na * sizeof (float complex)) == 0);

          /* Standard envelope: a clobbered blob is rejected, never
             reinterpreted. */
          ((char *)blob)[0] ^= (char)0xFF;
          CHECK (ratesync_set_state (s2, blob) == DP_ERR_INVALID);
          free (blob);
        }
    }
  ratesync_destroy (s1);
  ratesync_destroy (s2);
  free (x);
  free (a);
  free (b);
}

int
main (void)
{
  test_invalid_params ();
  test_owns_a_matched_cascade ();
  test_locks_across_planned_cascades ();
  test_tracks_a_clock_offset ();
  test_step_equals_steps ();
  test_reset ();
  test_state_roundtrip ();

  if (_fails)
    {
      fprintf (stderr, "test_ratesync_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_ratesync_core PASSED\n");
  return 0;
}
