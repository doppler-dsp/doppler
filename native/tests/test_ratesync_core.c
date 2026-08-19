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
 *
 * Sections numbered below (§8 onward) were added by the validation campaign,
 * which enumerated ratesync_core.h's prose claims and asked of each whether
 * anything ran it. Each was proven by sabotage before being trusted. The claim
 * numbers (C8, C24, ...) index the table in
 * src/doppler/track/tests/validation/ratesync/results.md.
 */

#include "dp_test.h"
#include "ratesync/ratesync_core.h"

#include "dp_sym_test.h" /* EVM / M2M4 / settling — the shared primitives */
#include "dp_tx_test.h"  /* the shaped symbol stream — the shared stimulus */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _BETA 0.35
#define _SPAN 8
#define _NSYM 3000

/* This object's stimulus, stated once as a deviation from the shared
 * conventions in dp_tx_test.h. Everything the wrappers below do NOT set --
 * the timing origin, the MLS symbol source, the meaning of `tau` -- is the
 * shared convention, which is the point of taking them from there.
 *
 * The one convention worth restating is the AMPLITUDE. The TED normalises by
 * its own slope alone (ratesync_core.h), so amplitude is the caller's to
 * supply: a unit-amplitude symbol stream drives the loop at exactly the
 * bandwidth `bn` names, and anything smaller under-drives it by A^2. The
 * stream peaks at 1.582x its symbol amplitude — a pulse property — which the
 * CIC's input encoding budgets for explicitly (CIC_PAPR_HEADROOM), so this
 * does not have to be backed off to avoid clipping the way it used to.
 * `clipped == 0` is asserted below and is the live check on that. */
static dp_tx_cfg_t
_tx_cfg (double sps, double tau)
{
  dp_tx_cfg_t c = dp_tx_defaults ();
  c.sps         = sps;
  c.beta        = _BETA;
  c.span        = _SPAN;
  c.tau         = tau;
  c.nsym        = _NSYM;
  return c;
}

/* RRC-shaped BPSK at `sps` samples/symbol, timing offset `tau` symbols, at a
 * STATED symbol amplitude. */
static float complex *
_tx_amp (double sps, double tau, double amp, size_t *n_out)
{
  dp_tx_cfg_t c = _tx_cfg (sps, tau);
  c.amp         = amp;
  return dp_tx_make (&c, NULL, n_out);
}

/* The same at the loop's CONTRACTED symbol amplitude of 1.0. */
static float complex *
_tx (double sps, double tau, size_t *n_out)
{
  return _tx_amp (sps, tau, 1.0, n_out);
}

/* Rectangular NRZ at `sps` samples/symbol — the pulse RATESYNC_PULSE_IANDD is
 * matched to, and the stream the m >= 4 guidance is stated against. */
static float complex *
_tx_nrz (double sps, size_t *n_out)
{
  dp_tx_cfg_t c = _tx_cfg (sps, 0.0);
  c.pulse       = DP_TX_NRZ;
  return dp_tx_make (&c, NULL, n_out);
}

/* Steady-state EVM in dB, from the library's own primitive.
 *
 * This used to be a private least-squares EVM over "the final quarter". Both
 * halves of that were a reimplementation: `ber_evm_db` (via dp_sym_test.h) is
 * the canonical self-referenced EVM, and `ber_settle_syms` is the canonical
 * answer to where a steady-state window may START. A window pinned to a
 * FRACTION of the record is the documented way a receiver test measures the
 * acquisition transient and reports it as steady state — the more so here,
 * where the record length varies with sps and the loop's settling does not.
 *
 * One loop is running, so the budget is ber_settle_syms(bn, 0) = 2*(5/bn)
 * symbols; the carrier argument is 0 because RateSync recovers timing only. */
static size_t
_settle (double bn)
{
  return dp_test_settle_syms (bn, 0.0);
}

static double
_evm_db_bn (const float complex *y, size_t n, double bn)
{
  size_t lo = _settle (bn);
  if (n < lo + 100)
    return 0.0; /* nothing settled to measure */
  return dp_test_evm_db_hard_range (y, lo, n, 2);
}

/* ------------------------------------------------------------------ */

static void
test_invalid_params (void)
{
  /* Every knob rejects rather than silently coercing. */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 1.5, 8, 2, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL); /* beta > 1 */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, -0.1, 8, 2, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 0, 2, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL); /* span 0 */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 1, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL); /* m < 2: no half-symbol gate exists */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 3, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL); /* m odd: the gate would not land on m/2 */
  DP_CHECK (ratesync_create (40.0, RATESYNC_PULSE_RRC, 0.35, 8,
                             RATESYNC_MAX_M + 2u, 1024, 0.01, 0.707,
                             RATESYNC_TED_GARDNER)
            == NULL); /* m beyond the in-struct ring */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1000, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL); /* num_phases not a power of two */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, -0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                             0.0, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (ratesync_create (4.0, 7, 0.35, 8, 2, 1024, 0.01, 0.707,
                             RATESYNC_TED_GARDNER)
            == NULL); /* unknown pulse */
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                             0.707, 9)
            == NULL); /* unknown TED */
  /* sps < m would make the terminal stage interpolate, and one input could
     then complete several strobes — which the single-symbol step() contract
     cannot express. */
  DP_CHECK (ratesync_create (1.5, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL);
  /* NaN must be rejected, not accepted by a comparison that happens to be
     false. */
  DP_CHECK (ratesync_create (NAN, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (ratesync_create (4.0, RATESYNC_PULSE_RRC, NAN, 8, 2, 1024, 0.01,
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
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      int last = s->mf->n_stages - 1;
      DP_CHECK (s->mf->stage_types[last] == RC_STAGE_RESAMP);
      DP_CHECK (RateConverter_stage_label (s->mf, last, buf, sizeof buf));
      DP_CHECK (strstr (buf, "rrc") != NULL);
      /* The control is referenced to the TERMINAL stage's rate, not the
         cascade's; getting that wrong under-drives the loop by the whole
         integer decimation in front. */
      DP_CHECK (
          s->loop.term_rate
          == resamp_get_rate ((resamp_state_t *)s->mf->stage_ptrs[last]));
      /* The bank is sized by the post-decimation rate, so a 16x span of input
         rates leaves it the same size. */
      DP_CHECK (
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
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          return;
        }
      ratesync_state_t *s
          = ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          size_t ns = ratesync_steps (s, x, n, y, n);
          double ev = _evm_db_bn (y, ns, 0.01);
          if (ev > worst)
            worst = ev;
          /* lock_stat, not EVM, is the lock decision: a single acquisition
             cycle slip drags a windowed EVM by 20 dB with the eye wide
             open. */
          if (ratesync_get_lock_stat (s) > 0.55)
            locked++;
          DP_CHECK (ratesync_get_clipped (s) == 0); /* drive stayed in range */
          ratesync_destroy (s);
        }
      free (x);
      free (y);
    }
  DP_CHECK (locked == 8);
  DP_CHECK (worst < evm_max_db);
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
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      ratesync_state_t *s
          = ratesync_create (nominal, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                             1024, 0.005, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          (void)ratesync_steps (s, x, n, y, n);
          double est = ratesync_get_rate (s);
          DP_CHECK (fabs (est - actual[i]) < 0.01);
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
  DP_CHECK (x && a && b && c);
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
  DP_CHECK (s1 && s2 && s3);
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

      DP_CHECK (na == nb && na == nc);
      DP_CHECK (na > 0);
      DP_CHECK (memcmp (a, b, na * sizeof (float complex)) == 0);
      DP_CHECK (memcmp (a, c, na * sizeof (float complex)) == 0);
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
  DP_CHECK (x && a && b);
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
  DP_CHECK (s != NULL);
  if (s)
    {
      size_t na = ratesync_steps (s, x, n, a, n);
      ratesync_reset (s);
      /* Post-reset the object must behave exactly as freshly created — the
         prime countdown re-arms along with the cascade's delay lines. */
      DP_CHECK (ratesync_get_ctrl (s) == 0.0);
      DP_CHECK (ratesync_get_locked (s) == 0);
      size_t nb = ratesync_steps (s, x, n, b, n);
      DP_CHECK (na == nb);
      DP_CHECK (na > 0 && memcmp (a, b, na * sizeof (float complex)) == 0);
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
  DP_CHECK (x && a && b);
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
  DP_CHECK (s1 && s2);
  if (s1 && s2)
    {
      size_t cut = n / 2;
      (void)ratesync_steps (s1, x, cut, a, n);

      size_t sb   = ratesync_state_bytes (s1);
      void  *blob = malloc (sb);
      DP_CHECK (blob != NULL);
      if (blob)
        {
          ratesync_get_state (s1, blob);
          DP_CHECK (ratesync_set_state (s2, blob) == DP_OK);

          /* Compare the BLOBS too, not just the symbols: a size miscount
             leaves uninitialised trailing bytes that a symbol comparison
             happily ignores — the bug the state matrix caught in the first
             RateSync prototype. */
          void *blob2 = malloc (sb);
          DP_CHECK (blob2 != NULL);
          if (blob2)
            {
              ratesync_get_state (s2, blob2);
              DP_CHECK (memcmp (blob, blob2, sb) == 0);
              free (blob2);
            }

          size_t na = ratesync_steps (s1, x + cut, n - cut, a, n);
          size_t nb = ratesync_steps (s2, x + cut, n - cut, b, n);
          DP_CHECK (na == nb);
          DP_CHECK (na > 0 && memcmp (a, b, na * sizeof (float complex)) == 0);

          /* Standard envelope: a clobbered blob is rejected, never
             reinterpreted. */
          ((char *)blob)[0] ^= (char)0xFF;
          DP_CHECK (ratesync_set_state (s2, blob) == DP_ERR_INVALID);
          free (blob);
        }
    }
  ratesync_destroy (s1);
  ratesync_destroy (s2);
  free (x);
  free (a);
  free (b);
}

/* ── §8 — the prime countdown, and where its length comes from ───────────
 *
 * C8: "the loop stays open until the cascade is primed ... ratesync_create()
 * computes the prime length from the terminal bank's own geometry."
 * C25: "the loop discards `prime_taps + 1` outputs."
 * C20: `term` is NULL when the geometry was bound by hand.
 *
 * Nothing ran any of the three. The prime length is not a free parameter —
 * it is the terminal bank's tap count, read off the stage — so both halves
 * are asserted here rather than the countdown alone. */
static void
test_prime_geometry (void)
{
  const double sps[] = { 4.0, 17.333333333, 64.0 };
  for (size_t i = 0; i < 3; i++)
    {
      ratesync_state_t *s
          = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      int    last = s->mf->n_stages - 1;
      size_t taps = resamp_get_num_taps (
          (const resamp_state_t *)s->mf->stage_ptrs[last]);
      /* The prime length IS the terminal bank's geometry, not a constant. */
      DP_CHECK (s->loop.prime_taps == taps);
      DP_CHECK (taps > 0);
      DP_CHECK (s->loop.prime_left == taps + 1u);
      /* A cascade-bound loop keeps the stage for the `mu` probe. */
      DP_CHECK (s->loop.term
                == (const resamp_state_t *)s->mf->stage_ptrs[last]);
      /* reset() re-arms the countdown along with the delay lines. */
      float complex y;
      for (size_t k = 0; k < 64; k++)
        (void)ratesync_step (s, 0.1f, &y);
      DP_CHECK (s->loop.prime_left < taps + 1u); /* it really counted down */
      ratesync_reset (s);
      DP_CHECK (s->loop.prime_left == taps + 1u);
      ratesync_destroy (s);
    }

  /* Geometry given by hand: prime_left follows the stated tap count, and the
     telemetry pointer is dropped so the probe cannot report another object's
     phase. */
  ratesync_loop_t l;
  ratesync_loop_init (&l, 8.0, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (l.term == NULL); /* nothing bound yet */
  DP_CHECK (l.ted_scale == 1.0);
  ratesync_loop_set_cascade (&l, 0.5, 99);
  DP_CHECK (l.term_rate == 0.5);
  DP_CHECK (l.prime_taps == 99);
  DP_CHECK (l.prime_left == 100);
  DP_CHECK (l.term == NULL);
}

/* ── §9 — one input can complete TWO terminal outputs ────────────────────
 *
 * C48: the `ys[4]` buffer in ratesync_step_ted() exists because "one input
 * can complete MORE THAN ONE output period ... asking for only one silently
 * DROPS the second". Nothing demonstrated that it ever happens, so nothing
 * would notice the buffer being narrowed back to one.
 *
 * It happens on exactly the cascades whose terminal rate is 1.0 — an integer
 * sps — which is why the same assertion is made at 17.333 (terminal rate
 * 0.923), where it must NOT happen. */
static void
test_two_outputs_per_input (void)
{
  const double sps[]      = { 4.0, 17.333333333, 64.0 };
  const int    expect_two = 1; /* index 0 and 2 have terminal rate 1.0 */
  for (size_t i = 0; i < 3; i++)
    {
      size_t         n;
      float complex *x = _tx (sps[i], 0.3, &n);
      DP_CHECK (x != NULL);
      if (!x)
        continue;
      ratesync_state_t *s
          = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          size_t doubles = 0, most = 0;
          for (size_t k = 0; k < n; k++)
            {
              float complex ys[4];
              size_t        got = RateConverter_execute_ctrl_push (
                  s->mf, x[k], s->loop.ctrl, ys, 4);
              if (got > most)
                most = got;
              if (got >= 2)
                doubles++;
              for (size_t oi = 0; oi < got; oi++)
                {
                  float complex yo;
                  (void)ratesync_loop_take_output (&s->loop, ys[oi], &yo,
                                                   RATESYNC_TED_GARDNER);
                }
            }
          /* Never more than two: the cascade rate is m/sps <= 1. */
          DP_CHECK (most <= 2);
          if (s->loop.term_rate >= 1.0)
            {
              DP_CHECK (doubles > 0); /* the buffer is load-bearing here */
              if (!doubles)
                fprintf (stderr,
                         "  sps=%g: terminal rate %.4f never emitted two\n",
                         sps[i], s->loop.term_rate);
            }
          else
            DP_CHECK (doubles == 0);
          ratesync_destroy (s);
        }
      free (x);
    }
  (void)expect_two;
}

/* ── §10 — the DTTL detector ─────────────────────────────────────────────
 *
 * C42: `ted` selects RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL. Every test
 * above passes GARDNER, so half the documented detector surface — including
 * its own construct-time slope, which differs from Gardner's — was executed
 * by nothing. */
static void
test_dttl_detector (void)
{
  const double sps[]   = { 4.0, 17.333333333, 64.0 };
  double       g_scale = 0.0, d_scale = 0.0;
  for (size_t i = 0; i < 3; i++)
    {
      size_t         n;
      float complex *x = _tx (sps[i], 0.3, &n);
      float complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      float complex *yg = calloc (n, sizeof *yg);
      DP_CHECK (yg != NULL);
      ratesync_state_t *d
          = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_DTTL);
      ratesync_state_t *g
          = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (d && g && yg);
      if (d && g && yg)
        {
          size_t ns = ratesync_steps (d, x, n, y, n);
          double ev = _evm_db_bn (y, ns, 0.01);
          DP_CHECK (ratesync_get_lock_stat (d) > 0.55);
          DP_CHECK (ev < -35.0);
          if (!(ratesync_get_lock_stat (d) > 0.55) || !(ev < -35.0))
            fprintf (stderr, "  DTTL sps=%g: lock %.3f EVM %.1f dB\n", sps[i],
                     ratesync_get_lock_stat (d), ev);
          /* The two detectors have different slopes against the same pulse,
             so the construct-time reciprocal must differ between them. */
          d_scale = d->loop.ted_scale;
          g_scale = g->loop.ted_scale;
          DP_CHECK (d_scale > 0.0 && g_scale > 0.0);
          DP_CHECK (d_scale != g_scale);

          size_t ng = ratesync_steps (g, x, n, yg, n);
          DP_CHECK (ng > 0 && ns > 0);
          /* Gardner locks on this stream too, so "the DTTL run locks well"
             is vacuous as a check on the DISPATCH. Comparing the two whole
             runs is not enough either: the loops also differ by ted_scale,
             so a dispatch that silently collapsed to Gardner would STILL
             produce a different trajectory and pass. Isolate the
             discriminator instead — two loops identical in every respect,
             including ted_scale, fed identical outputs, differing only in
             the `ted` literal passed to take_output. */
          ratesync_loop_t p, q;
          ratesync_loop_init (&p, sps[i], 2, 0.01, 0.707,
                              RATESYNC_TED_GARDNER);
          ratesync_loop_init (&q, sps[i], 2, 0.01, 0.707,
                              RATESYNC_TED_GARDNER);
          ratesync_loop_bind_cascade (&p, d->mf);
          ratesync_loop_bind_cascade (&q, d->mf);
          DP_CHECK (p.ted_scale == q.ted_scale);
          int differed = 0;
          for (size_t k = 0; k < 64; k++)
            {
              /* A deterministic non-trivial complex sequence: both
                 detectors see exactly these outputs. */
              float complex v = (float)cos (0.7 * (double)k)
                                + (float)sin (0.31 * (double)k + 0.4) * I;
              float complex yp, yq;
              int rp = ratesync_loop_take_output (&p, v, &yp,
                                                  RATESYNC_TED_GARDNER);
              int rq
                  = ratesync_loop_take_output (&q, v, &yq, RATESYNC_TED_DTTL);
              DP_CHECK (
                  rp == rq); /* the strobe cadence is detector-independent */
              if (rp && p.last_error != q.last_error)
                differed = 1;
            }
          DP_CHECK (differed);
          if (!differed)
            fprintf (stderr,
                     "  sps=%g: GARDNER and DTTL produced the SAME error — "
                     "the take_output dispatch is not reaching both bodies\n",
                     sps[i]);
        }
      ratesync_destroy (d);
      ratesync_destroy (g);
      free (x);
      free (y);
      free (yg);
    }
}

/* ── §11 — the timing loop holds no cascade ──────────────────────────────
 *
 * C18: "it never touches the cascade, so a receiver that owns its cascade
 * inside a DDC drives this with exactly the same call RateSync makes ... the
 * two are not peers that can drift apart."
 *
 * The strongest form of that claim is bit-exactness: drive a hand-owned
 * RateConverter through ratesync_loop_init/bind_cascade/take_output and the
 * symbols must equal RateSync's own, sample for sample. If the object ever
 * grows a second copy of the loop, this is what turns red. */
static void
test_loop_without_the_object (void)
{
  const double   sps = 17.333333333;
  size_t         n;
  float complex *x = _tx (sps, 0.3, &n);
  float complex *a = calloc (n, sizeof *a);
  float complex *b = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }

  ratesync_state_t *s
      = ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  size_t na = s ? ratesync_steps (s, x, n, a, n) : 0;

  /* The same cascade RateSync builds, owned here instead. */
  RateConverter_state_t *rc = RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  DP_CHECK (rc != NULL);
  ratesync_loop_t l;
  ratesync_loop_init (&l, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  ratesync_loop_bind_cascade (&l, rc);
  /* bind_cascade reads the geometry off the stage rather than being told. */
  DP_CHECK (
      l.term_rate
      == resamp_get_rate ((resamp_state_t *)rc->stage_ptrs[rc->n_stages - 1]));
  DP_CHECK (l.prime_taps > 0 && l.prime_left == l.prime_taps + 1u);
  DP_CHECK (l.term != NULL);

  size_t nb = 0;
  if (rc)
    for (size_t k = 0; k < n; k++)
      {
        float complex ys[4];
        size_t got = RateConverter_execute_ctrl_push (rc, x[k], l.ctrl, ys, 4);
        for (size_t oi = 0; oi < got; oi++)
          if (ratesync_loop_take_output (&l, ys[oi], &b[nb],
                                         RATESYNC_TED_GARDNER))
            nb++;
      }
  DP_CHECK (na > 0 && na == nb);
  DP_CHECK (na && memcmp (a, b, na * sizeof (float complex)) == 0);
  if (s)
    {
      DP_CHECK (l.lock_stat == ratesync_get_lock_stat (s));
      DP_CHECK (l.rate_est == ratesync_get_rate (s));
    }

  RateConverter_destroy (rc);
  ratesync_destroy (s);
  free (x);
  free (a);
  free (b);
}

/* ── §12 — `ctrl` is referenced to the TERMINAL stage's rate ─────────────
 *
 * C24: referencing it to the cascade rate instead "would under-drive the loop
 * by exactly that factor (32x at sps=64 behind a CIC(32), which is why it
 * could barely track)".
 *
 * test_owns_a_matched_cascade checks that term_rate EQUALS the terminal
 * stage's rate, which pins the wiring but says nothing about the consequence.
 * This measures the consequence, by binding the wrong scale on purpose. */
static void
test_ctrl_scale_is_the_terminal_rate (void)
{
  const double   sps = 64.0; /* CIC(32) + Resampler(1.0): a 32x decimation */
  size_t         n;
  float complex *x = _tx (sps, 0.3, &n);
  float complex *y = calloc (n, sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    {
      free (x);
      free (y);
      return;
    }
  double lock[2] = { 0.0, 0.0 }, evm[2] = { 0.0, 0.0 };
  for (int wrong = 0; wrong < 2; wrong++)
    {
      RateConverter_state_t *rc = RateConverter_create_matched (
          2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
      DP_CHECK (rc != NULL);
      if (!rc)
        continue;
      ratesync_loop_t l;
      ratesync_loop_init (&l, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
      ratesync_loop_bind_cascade (&l, rc);
      if (wrong) /* the cascade rate, m/sps — 32x too small */
        ratesync_loop_set_cascade (&l, 2.0 / sps, l.prime_taps);
      size_t ns = 0;
      for (size_t k = 0; k < n; k++)
        {
          float complex ys[4];
          size_t        got
              = RateConverter_execute_ctrl_push (rc, x[k], l.ctrl, ys, 4);
          for (size_t oi = 0; oi < got; oi++)
            if (ratesync_loop_take_output (&l, ys[oi], &y[ns],
                                           RATESYNC_TED_GARDNER))
              ns++;
        }
      lock[wrong] = l.lock_stat;
      evm[wrong]  = _evm_db_bn (y, ns, 0.01);
      RateConverter_destroy (rc);
    }
  DP_CHECK (lock[0] > 0.55); /* terminal rate: locks and demodulates */
  DP_CHECK (evm[0] < -30.0);
  DP_CHECK (evm[1] > -25.0); /* cascade rate: 32x under-driven, tracks badly */
  DP_CHECK (evm[1] - evm[0] > 10.0); /* and the gap is large, not marginal */
  /* NB the lock statistic does NOT separate these: given a long enough
     stream the under-driven loop crawls into a nominally open eye
     (lock_stat ~0.59 here) while demodulating 18 dB worse. `locked` is
     therefore not a check on this misconfiguration — the EVM is. */
  if (!(evm[1] > -25.0) || !(evm[1] - evm[0] > 10.0))
    fprintf (stderr,
             "  ctrl scale: terminal lock %.3f EVM %.1f; cascade lock %.3f "
             "EVM %.1f\n",
             lock[0], evm[0], lock[1], evm[1]);
  free (x);
  free (y);
}

/* ── §13 — `clipped` reports over-drive, and only where a CIC exists ─────
 *
 * C36: "Over-driving ... IS reported, by ratesync_get_clipped(): a CIC bounds
 * its input to +-1.0."
 * C57: "Always 0 when the plan has no CIC stage."
 *
 * The lock sweep asserts clipped == 0 on a stream that does not over-drive,
 * which passes whether or not the flag can ever fire. Both directions are
 * needed for that assertion to mean anything. */
static void
test_clipped_reports_overdrive (void)
{
  /* sps = 4 plans HalfbandDecimator + Resampler — no CIC anywhere. */
  const double sps[]     = { 4.0, 17.333333333, 64.0 };
  const int    has_cic[] = { 0, 1, 1 };
  for (size_t i = 0; i < 3; i++)
    for (int over = 0; over < 2; over++)
      {
        size_t         n;
        float complex *x = _tx_amp (sps[i], 0.2, over ? 4.0 : 1.0, &n);
        float complex *y = calloc (n, sizeof *y);
        DP_CHECK (x && y);
        if (!x || !y)
          {
            free (x);
            free (y);
            continue;
          }
        ratesync_state_t *s
            = ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                               1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
        DP_CHECK (s != NULL);
        if (s)
          {
            (void)ratesync_steps (s, x, n, y, n);
            int c = ratesync_get_clipped (s);
            /* Unit amplitude never clips on any plan — the pulse's 1.582x
               PAPR is inside the CIC's budgeted headroom. */
            int want = over && has_cic[i];
            DP_CHECK (c == want);
            if (c != want)
              fprintf (stderr, "  clipped: sps=%g over=%d got %d want %d\n",
                       sps[i], over, c, want);
            /* reset() clears the flag along with everything else. */
            if (c)
              {
                ratesync_reset (s);
                DP_CHECK (ratesync_get_clipped (s) == 0);
              }
            ratesync_destroy (s);
          }
        free (x);
        free (y);
      }
}

/* ── §14 — use m >= 4 with RATESYNC_PULSE_IANDD ──────────────────────────
 *
 * C39: "at m = 2 its matched filter is a two-tap sum and the eye barely
 * opens". A guidance sentence with a measurement behind it and no test: the
 * only thing that would catch the rectangle's m = 2 case regressing further,
 * or the guidance becoming unnecessary, is this. */
static void
test_iandd_needs_m4 (void)
{
  double lock[5] = { 0 };
  for (size_t m = 2; m <= 4; m += 2)
    {
      size_t         n;
      float complex *x = _tx_nrz (4.0, &n);
      float complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      ratesync_state_t *s
          = ratesync_create (4.0, RATESYNC_PULSE_IANDD, 0.0, 1, m, 1024, 0.01,
                             0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          (void)ratesync_steps (s, x, n, y, n);
          lock[m] = ratesync_get_lock_stat (s);
          if (m == 4)
            {
              DP_CHECK (lock[m] > 0.55);
              DP_CHECK (ratesync_get_locked (s) == 1);
            }
          else
            {
              /* Below the 0.311 declare threshold: the eye never opens, and
                 the detector correctly declines to call it a lock. */
              DP_CHECK (lock[m] < 0.311);
              DP_CHECK (ratesync_get_locked (s) == 0);
            }
          ratesync_destroy (s);
        }
      free (x);
      free (y);
    }
  DP_CHECK (lock[4] - lock[2] > 0.4); /* the gap is the reason for the rule */
  if (!(lock[4] - lock[2] > 0.4))
    fprintf (stderr, "  IANDD: lock_stat m=2 %.3f, m=4 %.3f\n", lock[2],
             lock[4]);
}

/* ── §15 — the loop's own state triplet ──────────────────────────────────
 *
 * C32: the loop is "nested by every owner", so it has a self-validating
 * envelope of its own (RSLP / v2). test_state_roundtrip exercises it only
 * through the whole object, which cannot tell a working child envelope from
 * one the parent happens to be covering.
 *
 * Run at BOTH strobe parities. The blob has to carry `out_count` because a
 * resumed instance that forgot which terminal output was on-time would
 * restart the parity search — but with m = 2 a lost count still lands on the
 * right parity half the time, so a single cut point tests that at a coin
 * flip. Sabotaging the packed field left a one-cut version of this section
 * green; covering both parities is what makes it a gate. */
static void
_loop_state_roundtrip_at_parity (int parity)
{
  const double   sps = 17.333333333;
  size_t         n;
  float complex *x = _tx (sps, 0.25, &n);
  float complex *a = calloc (n, sizeof *a);
  float complex *b = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }

  RateConverter_state_t *rc1 = RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  RateConverter_state_t *rc2 = RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  ratesync_loop_t l1, l2;
  ratesync_loop_init (&l1, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  ratesync_loop_init (&l2, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (rc1 && rc2);
  if (rc1 && rc2)
    {
      ratesync_loop_bind_cascade (&l1, rc1);
      ratesync_loop_bind_cascade (&l2, rc2);

      /* Advance to the half-way mark, then on to the next input at which the
         strobe phase has the requested parity. */
      size_t cut = 0, na = 0;
      for (size_t k = 0; k < n; k++)
        {
          if (k >= n / 2 && (int)(l1.out_count & 1u) == parity)
            {
              cut = k;
              break;
            }
          float complex ys[4];
          size_t        got
              = RateConverter_execute_ctrl_push (rc1, x[k], l1.ctrl, ys, 4);
          for (size_t oi = 0; oi < got; oi++)
            if (ratesync_loop_take_output (&l1, ys[oi], &a[na],
                                           RATESYNC_TED_GARDNER))
              na++;
        }
      DP_CHECK (cut > 0);
      DP_CHECK ((int)(l1.out_count & 1u) == parity);

      size_t sb   = ratesync_loop_state_bytes (&l1);
      void  *blob = malloc (sb);
      DP_CHECK (blob != NULL);
      if (blob)
        {
          ratesync_loop_get_state (&l1, blob);
          DP_CHECK (ratesync_loop_set_state (&l2, blob) == DP_OK);
          void *blob2 = malloc (sb);
          DP_CHECK (blob2 != NULL);
          if (blob2)
            {
              ratesync_loop_get_state (&l2, blob2);
              DP_CHECK (memcmp (blob, blob2, sb) == 0);
              free (blob2);
            }
          /* The cascade is the loop's peer, not its child: carry it across
             by hand so the resumed pair is genuinely identical. */
          size_t rb    = RateConverter_state_bytes (rc1);
          void  *rblob = malloc (rb);
          DP_CHECK (rblob != NULL);
          if (rblob)
            {
              RateConverter_get_state (rc1, rblob);
              DP_CHECK (RateConverter_set_state (rc2, rblob) == DP_OK);
              free (rblob);
            }

          size_t ka = na, kb = na;
          for (size_t k = cut; k < n; k++)
            {
              float complex ys[4];
              size_t got = RateConverter_execute_ctrl_push (rc1, x[k], l1.ctrl,
                                                            ys, 4);
              for (size_t oi = 0; oi < got; oi++)
                if (ratesync_loop_take_output (&l1, ys[oi], &a[ka],
                                               RATESYNC_TED_GARDNER))
                  ka++;
              got = RateConverter_execute_ctrl_push (rc2, x[k], l2.ctrl, ys,
                                                     4);
              for (size_t oi = 0; oi < got; oi++)
                if (ratesync_loop_take_output (&l2, ys[oi], &b[kb],
                                               RATESYNC_TED_GARDNER))
                  kb++;
            }
          DP_CHECK (ka == kb && ka > na);
          DP_CHECK (memcmp (a + na, b + na, (ka - na) * sizeof (float complex))
                    == 0);
          if (ka != kb
              || memcmp (a + na, b + na, (ka - na) * sizeof (float complex))
                     != 0)
            fprintf (stderr, "  loop blob: parity %d resumed %zu vs %zu\n",
                     parity, ka - na, kb - na);

          /* Its own envelope rejects, independently of the parent's. */
          ((char *)blob)[0] ^= (char)0xFF;
          DP_CHECK (ratesync_loop_set_state (&l2, blob) == DP_ERR_INVALID);
          free (blob);
        }
    }
  RateConverter_destroy (rc1);
  RateConverter_destroy (rc2);
  free (x);
  free (a);
  free (b);
}

static void
test_loop_state_roundtrip (void)
{
  _loop_state_roundtrip_at_parity (0);
  _loop_state_roundtrip_at_parity (1);
}

/* ── §16 — a telemetry attach fails WHOLE ────────────────────────────────
 *
 * C30: "@return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
 * six probes (the attach fails whole; the object stays detached)."
 *
 * The Python doctest pins the success path. The failure path is the one that
 * matters: a partial attach would leave emit sites firing on ids that were
 * never registered. */
static void
test_telemetry_attach_is_atomic (void)
{
  ratesync_state_t *s
      = ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  dp_tlm_t *t = dp_tlm_create (1 << 12);
  DP_CHECK (s && t);
  if (s && t)
    {
      DP_CHECK (ratesync_set_telemetry (s, t, "ok", 1) == DP_OK);
      DP_CHECK (dp_tlm_probe_count (t) == 6);
      DP_CHECK (s->loop.tlm.ctx == t);

      /* NULL detaches, and detaching cannot fail. */
      DP_CHECK (ratesync_set_telemetry (s, NULL, NULL, 1) == DP_OK);
      DP_CHECK (s->loop.tlm.ctx == NULL);

      /* Fill the table so that fewer than six slots remain, then attach from
         the DETACHED state: the object must still be detached afterwards.
         Starting from an attached state would pass whether or not the
         partial registration went live, since ctx would equal `t` either
         way — which is the whole point of setting ctx last. */
      char nm[32];
      while (dp_tlm_probe_count (t) < DP_TLM_MAX_PROBES - 2)
        {
          (void)snprintf (nm, sizeof nm, "filler.%zu", dp_tlm_probe_count (t));
          DP_CHECK (dp_tlm_probe (t, nm, 1) >= 0);
        }
      DP_CHECK (ratesync_set_telemetry (s, t, "nope", 1) == DP_ERR_INVALID);
      DP_CHECK (s->loop.tlm.ctx == NULL); /* the attach failed WHOLE */
    }
  dp_tlm_destroy (t);
  ratesync_destroy (s);
}

/* ── §17 — retune keeps the lock; a lock retune drops it ─────────────────
 *
 * C28: ratesync_configure "preserves the integrator (and so the lock)".
 * C29/C58: configure_lock_raw "clears the in-flight block sum and drops the
 * lock", and avgs is "clamped >= 1".
 *
 * Both are pinned by .pyi doctests on the Python face only. A doctest is not
 * a gate a C-side regression would ever reach. */
static void
test_configure_semantics (void)
{
  size_t         n;
  float complex *x = _tx (8.0, 0.2, &n);
  float complex *y = calloc (n, sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    {
      free (x);
      free (y);
      return;
    }
  ratesync_state_t *s
      = ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (s)
    {
      (void)ratesync_steps (s, x, n, y, n);
      DP_CHECK (ratesync_get_locked (s) == 1);
      double integ = s->loop.lf.integ, rate = ratesync_get_rate (s);

      ratesync_configure (s, 0.002, 0.707);
      DP_CHECK (ratesync_get_bn (s) == 0.002);
      DP_CHECK (s->loop.lf.integ == integ); /* the rate memory survives */
      DP_CHECK (ratesync_get_rate (s) == rate);
      DP_CHECK (ratesync_get_locked (s) == 1); /* and so does the lock */

      /* An invalid retune is ignored rather than applied destructively. */
      ratesync_configure (s, -1.0, 0.707);
      DP_CHECK (ratesync_get_bn (s) == 0.002);
      ratesync_configure (s, 0.002, 0.0);
      DP_CHECK (ratesync_get_bn (s) == 0.002);

      /* set_bn is the property face of the same call. */
      ratesync_set_bn (s, 0.004);
      DP_CHECK (ratesync_get_bn (s) == 0.004);
      DP_CHECK (ratesync_get_locked (s) == 1);

      /* A lock retune drops the decision and the in-flight block. */
      ratesync_configure_lock_raw (s, 0, 0.5, 0.4, 2, 4);
      DP_CHECK (s->loop.avgs == 1); /* clamped up from 0 */
      DP_CHECK (ratesync_get_locked (s) == 0);
      DP_CHECK (ratesync_get_lock_stat (s) == 0.0);
      DP_CHECK (s->loop.lock_count == 0);
      /* but not the timing estimate */
      DP_CHECK (ratesync_get_rate (s) == rate);
      ratesync_destroy (s);
    }
  free (x);
  free (y);
}

/* ── §18 — max_out caps the block, and the hint is 0 ─────────────────────
 *
 * C50: "0 means 'the input length is already a safe bound'". Nothing checked
 * either the hint or that `steps` honours a capacity smaller than it. */
static void
test_max_out (void)
{
  size_t         n;
  float complex *x = _tx (4.0, 0.2, &n);
  float complex *y = calloc (n, sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    {
      free (x);
      free (y);
      return;
    }
  ratesync_state_t *s
      = ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (s)
    {
      DP_CHECK (ratesync_steps_max_out (s) == 0);
      size_t full = ratesync_steps (s, x, n, y, n);
      DP_CHECK (full > 0);
      DP_CHECK (full
                <= n); /* symbols can never exceed inputs: sps >= m >= 2 */
      ratesync_reset (s);
      DP_CHECK (ratesync_steps (s, x, n, y, 10) == 10);
      ratesync_reset (s);
      DP_CHECK (ratesync_steps (s, x, n, y, 0) == 0);
      ratesync_destroy (s);
    }
  free (x);
  free (y);
}

/* ── §19 — sps == m is the boundary, and it is inclusive ─────────────────
 *
 * C37: "any double >= m ... The bound is `m`, not 2, because the terminal
 * stage must not be asked to interpolate: rate = m/sps <= 1." The rejection
 * side is pinned at sps = 1.5; the accepted edge is not, so a bound that
 * silently became exclusive would pass. */
static void
test_sps_equals_m_boundary (void)
{
  ratesync_state_t *s
      = ratesync_create (2.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024, 0.01,
                         0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL); /* rate = m/sps = 1.0 exactly: allowed */
  if (s)
    {
      DP_CHECK (s->loop.term_rate <= 1.0);
      ratesync_destroy (s);
    }
  /* One ulp below is not. */
  DP_CHECK (ratesync_create (1.999, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  /* The same edge at the other supported m. */
  s = ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 8, 1024, 0.01,
                       0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  ratesync_destroy (s);
  DP_CHECK (ratesync_create (7.999, RATESYNC_PULSE_RRC, _BETA, _SPAN, 8, 1024,
                             0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
}

/* ── §20 — each detector's amplitude law, and the scale that ignores it ──
 *
 * T5/T6/T7 of the ted claim inventory, all three of which were uncovered
 * here while the report measured them in Python. The inventory is in
 * docs/dev/contributing/validation.md's order: header claim -> this file ->
 * only then the report. These three arrived the wrong way round, and this
 * section is the correction.
 *
 * The claims, from symsync_core.h and ratesync_core.h:
 *
 *   - gardner_ted's raw output carries `A^2` -- both factors are signal.
 *   - dttl_ted's carries `A^1` -- only `mid` is signal, the transition term
 *     being a difference of hard-decision SIGNS and so amplitude-free.
 *   - ted_scale is `1 / symsync_ted_slope(ted, pulse, beta, span)`, a
 *     construct-time constant computed at `A = 1`, which therefore divides
 *     out NEITHER amplitude.
 *
 * Why this belongs in C and not only in the report: it is exact. No
 * cascade, no stimulus, no statistics -- two evaluations of a pure function
 * at two amplitudes, whose ratio is 4 and 2 by construction. The report
 * measures the same thing through the binding and fits an exponent
 * (2.000 / 1.000, section 2.6b); this pins it where a regression would be
 * caught by ctest rather than by a validator someone has to read.
 *
 * The consequence is a caller's level-error budget, and it is not shared:
 * a 2x level error is 4x the designed loop gain under Gardner and 2x under
 * DTTL. That asymmetry is why no single normaliser applied OUTSIDE the
 * detectors could serve both, which is the design argument the whole
 * construct-time-scale scheme rests on. */
static void
test_amplitude_law (void)
{
  /* An off-lock phase, so both outputs are non-trivial and a ratio means
     something. `mid` and the on-time step are the detector's two inputs;
     scaling BOTH is what scaling the input signal does. */
  const float mid1 = 0.37f, on1 = 0.81f, prev1 = -0.62f;

  for (int i = 0; i < 3; i++)
    {
      const float a  = (i == 0) ? 1.0f : (i == 1) ? 2.0f : 4.0f;
      const float a2 = a * a;

      double g1 = gardner_ted (mid1, on1 - prev1);
      double gA = gardner_ted (a * mid1, a * on1 - a * prev1);
      DP_CHECK (fabs (g1) > 1e-6);
      /* A^2, exactly: the ratio is the square of the scale. */
      DP_CHECK (fabs (gA - a2 * g1) <= 1e-5 * fabs (a2 * g1));

      double d1 = dttl_ted (mid1, on1, prev1);
      double dA = dttl_ted (a * mid1, a * on1, a * prev1);
      DP_CHECK (fabs (d1) > 1e-6);
      /* A^1: scaling every input scales the output by A, not A^2 --
         the hard decisions are unchanged by a positive scale, so only
         `mid` carries the amplitude through. */
      DP_CHECK (fabs (dA - (double)a * d1) <= 1e-5 * fabs ((double)a * d1));

      /* And the two laws are genuinely different, which is the whole
         point: at any scale but unity the ratio between them moves. */
      if (a != 1.0f)
        DP_CHECK (fabs (gA / g1 - dA / d1) > 0.5);
    }

  /* T5: ted_scale is the reciprocal of THIS detector's own slope against
     THIS pulse -- not merely different between the two, which is all §10
     asserted. A scale wired to the wrong detector, or to the wrong pulse,
     passes that check and fails this one. */
  const double beta    = 0.35;
  const int    teds[2] = { RATESYNC_TED_GARDNER, RATESYNC_TED_DTTL };
  const int    sym[2]  = { SYMSYNC_TED_GARDNER, SYMSYNC_TED_DTTL };
  for (int i = 0; i < 2; i++)
    {
      ratesync_state_t *s = ratesync_create (
          4.0, RATESYNC_PULSE_RRC, beta, _SPAN, 2, 1024, 0.01, 0.707, teds[i]);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      double want = symsync_ted_slope (sym[i], SYMSYNC_PULSE_RRC, beta, _SPAN);
      DP_CHECK (want > 0.0);
      DP_CHECK (fabs (1.0 / s->loop.ted_scale - want) <= 1e-9 * want);
      ratesync_destroy (s);
    }
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
  test_prime_geometry ();
  test_two_outputs_per_input ();
  test_dttl_detector ();
  test_loop_without_the_object ();
  test_ctrl_scale_is_the_terminal_rate ();
  test_clipped_reports_overdrive ();
  test_iandd_needs_m4 ();
  test_loop_state_roundtrip ();
  test_telemetry_attach_is_atomic ();
  test_configure_semantics ();
  test_max_out ();
  test_sps_equals_m_boundary ();
  test_amplitude_law ();

  DP_TEST_END ("test_ratesync_core");
}
