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

#include "doppler/detection/detection_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/ratesync/ratesync_core.h"
#include "dp_test.h"

#include "dp_sym_test.h" /* EVM / M2M4 / settling — the shared primitives */
#include "dp_tx_test.h"  /* the shaped symbol stream — the shared stimulus */
#include "ratesync_settle_test.h" /* settle_time, measured; the sweep's too */

#include "doppler/dp_complex.h"
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
 * CIC's input word budgets for explicitly (its headroom), so this
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
static float _Complex *
_tx_amp (double sps, double tau, double amp, size_t *n_out)
{
  dp_tx_cfg_t c = _tx_cfg (sps, tau);
  c.amp         = amp;
  return dp_tx_make (&c, NULL, n_out);
}

/* The same at the loop's CONTRACTED symbol amplitude of 1.0. */
static float _Complex *
_tx (double sps, double tau, size_t *n_out)
{
  return _tx_amp (sps, tau, 1.0, n_out);
}

/* Rectangular NRZ at `sps` samples/symbol — the pulse RATESYNC_PULSE_IANDD is
 * matched to, and the stream the m >= 4 guidance is stated against. */
static float _Complex *
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
_evm_db_bn (const float _Complex *y, size_t n, double bn)
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
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 1.5, 8, 2, 1024, 0.01,
                                0.707, RATESYNC_TED_GARDNER)
            == NULL); /* beta > 1 */
  /* The loop filter's own domain: the private check this replaced took an
     infinite bn or zeta (doppler#2103). */
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                INFINITY, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                0.01, INFINITY, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, -0.1, 8, 2, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 0, 2, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL); /* span 0 */
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 1, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL); /* m < 2: no half-symbol gate exists */
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 3, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL); /* m odd: the gate would not land on m/2 */
  DP_CHECK (dp_ratesync_create (40.0, RATESYNC_PULSE_RRC, 0.35, 8,
                                RATESYNC_MAX_M + 2u, 1024, 0.01, 0.707,
                                RATESYNC_TED_GARDNER)
            == NULL); /* m beyond the in-struct ring */
  /* The m domain is one public predicate, which create refuses through and
     a composition checks before building a chain (doppler#2421). Both
     edges, and each side of each. */
  DP_CHECK (!dp_ratesync_m_ok (0) && !dp_ratesync_m_ok (1));
  DP_CHECK (dp_ratesync_m_ok (2) && !dp_ratesync_m_ok (3));
  DP_CHECK (dp_ratesync_m_ok (RATESYNC_MAX_M));
  DP_CHECK (!dp_ratesync_m_ok (RATESYNC_MAX_M + 1u));
  DP_CHECK (!dp_ratesync_m_ok (RATESYNC_MAX_M + 2u));
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1000,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL); /* num_phases not a power of two */
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                -0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                0.01, 0.0, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, 7, 0.35, 8, 2, 1024, 0.01, 0.707,
                                RATESYNC_TED_GARDNER)
            == NULL); /* unknown pulse */
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                0.01, 0.707, 9)
            == NULL); /* unknown TED */
  /* sps < m would make the terminal stage interpolate, and one input could
     then complete several strobes — which the single-symbol step() contract
     cannot express. */
  DP_CHECK (dp_ratesync_create (1.5, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  /* NaN must be rejected, not accepted by a comparison that happens to be
     false. */
  DP_CHECK (dp_ratesync_create (NAN, RATESYNC_PULSE_RRC, 0.35, 8, 2, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  DP_CHECK (dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, NAN, 8, 2, 1024, 0.01,
                                0.707, RATESYNC_TED_GARDNER)
            == NULL);
  dp_ratesync_destroy (NULL); /* documented no-op */
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
      dp_ratesync_state_t *s
          = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      int last = s->mf->n_stages - 1;
      DP_CHECK (s->mf->stage_types[last] == RC_STAGE_RESAMP);
      DP_CHECK (dp_RateConverter_stage_label (s->mf, last, buf, sizeof buf));
      DP_CHECK (strstr (buf, "rrc") != NULL);
      /* The control is referenced to the TERMINAL stage's rate, not the
         cascade's; getting that wrong under-drives the loop by the whole
         integer decimation in front. */
      DP_CHECK (
          s->loop.term_rate
          == dp_resamp_get_rate ((resamp_state_t *)s->mf->stage_ptrs[last]));
      /* The bank is sized by the post-decimation rate, so a 16x span of input
         rates leaves it the same size. */
      DP_CHECK (dp_resamp_get_num_taps (
                    (const resamp_state_t *)s->mf->stage_ptrs[last])
                < 4u * _SPAN * 2u + 16u);
      dp_ratesync_destroy (s);
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
      size_t          n = 0;
      float _Complex *x = _tx (sps, taus[t], &n);
      float _Complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          return;
        }
      dp_ratesync_state_t *s
          = dp_ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          size_t ns = dp_ratesync_steps (s, x, n, y, n);
          double ev = _evm_db_bn (y, ns, 0.01);
          if (ev > worst)
            worst = ev;
          /* lock_stat, not EVM, is the lock decision: a single acquisition
             cycle slip drags a windowed EVM by 20 dB with the eye wide
             open. */
          if (dp_ratesync_get_lock_stat (s) > 0.55)
            locked++;
          DP_CHECK (dp_ratesync_get_clipped (s)
                    == 0); /* drive stayed in range */
          dp_ratesync_destroy (s);
        }
      free (x);
      free (y);
    }
  DP_CHECK (locked == 8);
  DP_CHECK (worst < evm_max_db);
  if (locked != 8 || worst >= evm_max_db)
    DP_TEST_ERR ("  %s: %d/8 locked, worst EVM %.1f dB (limit %.1f)\n", label,
                 locked, worst, evm_max_db);
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
      size_t          n = 0;
      float _Complex *x = _tx (actual[i], 0.2, &n);
      float _Complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      dp_ratesync_state_t *s
          = dp_ratesync_create (nominal, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.005, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          (void)dp_ratesync_steps (s, x, n, y, n);
          double est = dp_ratesync_get_rate (s);
          DP_CHECK (fabs (est - actual[i]) < 0.01);
          if (!(fabs (est - actual[i]) < 0.01))
            DP_TEST_ERR ("  clock offset: true %.4f, est %.4f\n", actual[i],
                         est);
          dp_ratesync_destroy (s);
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
  size_t          n = 0;
  float _Complex *x = _tx (17.333333333, 0.3, &n);
  float _Complex *a = calloc (n, sizeof *a);
  float _Complex *b = calloc (n, sizeof *b);
  float _Complex *c = calloc (n, sizeof *c);
  DP_CHECK (x && a && b && c);
  if (!x || !a || !b || !c)
    {
      free (x);
      free (a);
      free (b);
      free (c);
      return;
    }

  dp_ratesync_state_t *s1
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_ratesync_state_t *s2
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_ratesync_state_t *s3
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s1 && s2 && s3);
  if (s1 && s2 && s3)
    {
      size_t na = dp_ratesync_steps (s1, x, n, a, n);

      size_t nb = 0;
      for (size_t i = 0; i < n; i++)
        if (ratesync_step (s2, x[i], &b[nb]))
          nb++;

      size_t cut = n / 3;
      size_t nc  = dp_ratesync_steps (s3, x, cut, c, n);
      nc += dp_ratesync_steps (s3, x + cut, n - cut, c + nc, n - nc);

      DP_CHECK (na == nb && na == nc);
      DP_CHECK (na > 0);
      DP_CHECK (memcmp (a, b, na * sizeof (float _Complex)) == 0);
      DP_CHECK (memcmp (a, c, na * sizeof (float _Complex)) == 0);
    }
  dp_ratesync_destroy (s1);
  dp_ratesync_destroy (s2);
  dp_ratesync_destroy (s3);
  free (x);
  free (a);
  free (b);
  free (c);
}

static void
test_reset (void)
{
  size_t          n = 0;
  float _Complex *x = _tx (4.0, 0.4, &n);
  float _Complex *a = calloc (n, sizeof *a);
  float _Complex *b = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }
  dp_ratesync_state_t *s
      = dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (s)
    {
      size_t na = dp_ratesync_steps (s, x, n, a, n);
      dp_ratesync_reset (s);
      /* Post-reset the object must behave exactly as freshly created — the
         prime countdown re-arms along with the cascade's delay lines. */
      DP_CHECK (dp_ratesync_get_ctrl (s) == 0.0);
      DP_CHECK (dp_ratesync_get_locked (s) == 0);
      size_t nb = dp_ratesync_steps (s, x, n, b, n);
      DP_CHECK (na == nb);
      DP_CHECK (na > 0 && memcmp (a, b, na * sizeof (float _Complex)) == 0);
      dp_ratesync_destroy (s);
    }
  free (x);
  free (a);
  free (b);
}

/* A refused loop blob changes nothing (doppler#2104). The loop wrote its own
 * fields before its loop-filter child could refuse, so a refused restore
 * left them holding the blob's values. A blob from an earlier point, with
 * the loop filter's envelope clobbered, is refused by the SAME loop after it
 * has advanced, and the loop then gives the advanced blob back byte for
 * byte. The premise, that the advance moved the loop's own fields, is
 * asserted. */
static void
test_loop_refused_blob_changes_nothing (void)
{
  size_t               n = 0;
  float _Complex      *x = _tx (17.333333333, 0.2, &n);
  float _Complex      *y = calloc (n, sizeof *y);
  dp_ratesync_state_t *s
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  const size_t   cb  = s ? dp_ratesync_loop_state_bytes (&s->loop) : 0;
  unsigned char *old = malloc (cb), *cur = malloc (cb), *now = malloc (cb);
  DP_CHECK (x && y && s && old && cur && now);
  if (x && y && s && old && cur && now)
    {
      const size_t lfb   = dp_loop_filter_state_bytes (&s->loop.lf);
      const size_t own   = cb - lfb; /* envelope + the loop's own fields */
      const size_t third = n / 3;
      (void)dp_ratesync_steps (s, x, third, y, n);
      dp_ratesync_loop_get_state (&s->loop, old);
      (void)dp_ratesync_steps (s, x + third, third, y, n);
      dp_ratesync_loop_get_state (&s->loop, cur);
      DP_CHECK (memcmp (old + sizeof (dp_state_hdr_t),
                        cur + sizeof (dp_state_hdr_t),
                        own - sizeof (dp_state_hdr_t))
                != 0);

      old[own] ^= 0xFFu; /* the loop filter's envelope magic */
      DP_CHECK (dp_ratesync_loop_set_state (&s->loop, old) == DP_ERR_INVALID);
      dp_ratesync_loop_get_state (&s->loop, now);
      DP_CHECK (memcmp (now, cur, cb) == 0);
    }
  dp_ratesync_destroy (s);
  free (old);
  free (cur);
  free (now);
  free (x);
  free (y);
}

static void
test_state_roundtrip (void)
{
  size_t          n = 0;
  float _Complex *x = _tx (17.333333333, 0.2, &n);
  float _Complex *a = calloc (n, sizeof *a);
  float _Complex *b = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }
  dp_ratesync_state_t *s1
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_ratesync_state_t *s2
      = dp_ratesync_create (17.333333333, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                            1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s1 && s2);
  if (s1 && s2)
    {
      size_t cut = n / 2;
      (void)dp_ratesync_steps (s1, x, cut, a, n);

      size_t sb   = dp_ratesync_state_bytes (s1);
      void  *blob = malloc (sb);
      DP_CHECK (blob != NULL);
      if (blob)
        {
          dp_ratesync_get_state (s1, blob);
          DP_CHECK (dp_ratesync_set_state (s2, blob) == DP_OK);

          /* Compare the BLOBS too, not just the symbols: a size miscount
             leaves uninitialised trailing bytes that a symbol comparison
             happily ignores — the bug the state matrix caught in the first
             RateSync prototype. */
          void *blob2 = malloc (sb);
          DP_CHECK (blob2 != NULL);
          if (blob2)
            {
              dp_ratesync_get_state (s2, blob2);
              DP_CHECK (memcmp (blob, blob2, sb) == 0);
              free (blob2);
            }

          size_t na = dp_ratesync_steps (s1, x + cut, n - cut, a, n);
          size_t nb = dp_ratesync_steps (s2, x + cut, n - cut, b, n);
          DP_CHECK (na == nb);
          DP_CHECK (na > 0
                    && memcmp (a, b, na * sizeof (float _Complex)) == 0);

          /* Standard envelope: a clobbered blob is rejected, never
             reinterpreted. */
          ((char *)blob)[0] ^= (char)0xFF;
          DP_CHECK (dp_ratesync_set_state (s2, blob) == DP_ERR_INVALID);
          free (blob);
        }
    }
  dp_ratesync_destroy (s1);
  dp_ratesync_destroy (s2);
  free (x);
  free (a);
  free (b);
}

/* ── §8 — the prime countdown, and where its length comes from ───────────
 *
 * C8: "the loop stays open until the cascade is primed ...
 * dp_ratesync_create() computes the prime length from the terminal bank's own
 * geometry." C25: "the loop discards `prime_taps + 1` outputs." C20: `term` is
 * NULL when the geometry was bound by hand.
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
      dp_ratesync_state_t *s
          = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      int    last = s->mf->n_stages - 1;
      size_t taps = dp_resamp_get_num_taps (
          (const resamp_state_t *)s->mf->stage_ptrs[last]);
      /* The prime length IS the terminal bank's geometry, not a constant. */
      DP_CHECK (s->loop.prime_taps == taps);
      DP_CHECK (taps > 0);
      DP_CHECK (s->loop.prime_left == taps + 1u);
      /* A cascade-bound loop keeps the stage for the `mu` probe. */
      DP_CHECK (s->loop.term
                == (const resamp_state_t *)s->mf->stage_ptrs[last]);
      /* reset() re-arms the countdown along with the delay lines. */
      float _Complex y;
      for (size_t k = 0; k < 64; k++)
        (void)ratesync_step (s, 0.1f, &y);
      DP_CHECK (s->loop.prime_left < taps + 1u); /* it really counted down */
      dp_ratesync_reset (s);
      DP_CHECK (s->loop.prime_left == taps + 1u);
      dp_ratesync_destroy (s);
    }

  /* Geometry given by hand: prime_left follows the stated tap count, and the
     telemetry pointer is dropped so the probe cannot report another object's
     phase. */
  ratesync_loop_t l;
  dp_ratesync_loop_init (&l, 8.0, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (l.term == NULL); /* nothing bound yet */
  DP_CHECK (l.ted_scale == 1.0);
  dp_ratesync_loop_set_cascade (&l, 0.5, 99);
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
      size_t          n = 0;
      float _Complex *x = _tx (sps[i], 0.3, &n);
      DP_CHECK (x != NULL);
      if (!x)
        continue;
      dp_ratesync_state_t *s
          = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          size_t doubles = 0, most = 0;
          for (size_t k = 0; k < n; k++)
            {
              float _Complex ys[4];
              size_t got = dp_RateConverter_execute_ctrl_push (
                  s->mf, x[k], s->loop.ctrl, ys, 4);
              if (got > most)
                most = got;
              if (got >= 2)
                doubles++;
              for (size_t oi = 0; oi < got; oi++)
                {
                  float _Complex yo;
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
                DP_TEST_ERR (
                    "  sps=%g: terminal rate %.4f never emitted two\n", sps[i],
                    s->loop.term_rate);
            }
          else
            DP_CHECK (doubles == 0);
          dp_ratesync_destroy (s);
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
      size_t          n = 0;
      float _Complex *x = _tx (sps[i], 0.3, &n);
      float _Complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      float _Complex *yg = calloc (n, sizeof *yg);
      DP_CHECK (yg != NULL);
      dp_ratesync_state_t *d
          = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_DTTL);
      dp_ratesync_state_t *g
          = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (d && g && yg);
      if (d && g && yg)
        {
          size_t ns = dp_ratesync_steps (d, x, n, y, n);
          double ev = _evm_db_bn (y, ns, 0.01);
          DP_CHECK (dp_ratesync_get_lock_stat (d) > 0.55);
          DP_CHECK (ev < -35.0);
          if (!(dp_ratesync_get_lock_stat (d) > 0.55) || !(ev < -35.0))
            DP_TEST_ERR ("  DTTL sps=%g: lock %.3f EVM %.1f dB\n", sps[i],
                         dp_ratesync_get_lock_stat (d), ev);
          /* The two detectors have different slopes against the same pulse,
             so the construct-time reciprocal must differ between them. */
          d_scale = d->loop.ted_scale;
          g_scale = g->loop.ted_scale;
          DP_CHECK (d_scale > 0.0 && g_scale > 0.0);
          DP_CHECK (d_scale != g_scale);

          size_t ng = dp_ratesync_steps (g, x, n, yg, n);
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
          dp_ratesync_loop_init (&p, sps[i], 2, 0.01, 0.707,
                                 RATESYNC_TED_GARDNER);
          dp_ratesync_loop_init (&q, sps[i], 2, 0.01, 0.707,
                                 RATESYNC_TED_GARDNER);
          dp_ratesync_loop_bind_cascade (&p, d->mf);
          dp_ratesync_loop_bind_cascade (&q, d->mf);
          DP_CHECK (p.ted_scale == q.ted_scale);
          int differed = 0;
          for (size_t k = 0; k < 64; k++)
            {
              /* A deterministic non-trivial complex sequence: both
                 detectors see exactly these outputs. */
              float _Complex v = (float)cos (0.7 * (double)k)
                                 + (float)sin (0.31 * (double)k + 0.4) * I;
              float _Complex yp, yq;
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
            DP_TEST_ERR (
                "  sps=%g: GARDNER and DTTL produced the SAME error — "
                "the take_output dispatch is not reaching both bodies\n",
                sps[i]);
        }
      dp_ratesync_destroy (d);
      dp_ratesync_destroy (g);
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
 * RateConverter through dp_ratesync_loop_init/bind_cascade/take_output and the
 * symbols must equal RateSync's own, sample for sample. If the object ever
 * grows a second copy of the loop, this is what turns red. */
static void
test_loop_without_the_object (void)
{
  const double    sps = 17.333333333;
  size_t          n   = 0;
  float _Complex *x   = _tx (sps, 0.3, &n);
  float _Complex *a   = calloc (n, sizeof *a);
  float _Complex *b   = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }

  dp_ratesync_state_t *s
      = dp_ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  size_t na = s ? dp_ratesync_steps (s, x, n, a, n) : 0;

  /* The same cascade RateSync builds, owned here instead. */
  dp_RateConverter_state_t *rc = dp_RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  DP_CHECK (rc != NULL);
  ratesync_loop_t l;
  dp_ratesync_loop_init (&l, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_ratesync_loop_bind_cascade (&l, rc);
  /* bind_cascade reads the geometry off the stage rather than being told. */
  DP_CHECK (l.term_rate
            == dp_resamp_get_rate (
                (resamp_state_t *)rc->stage_ptrs[rc->n_stages - 1]));
  DP_CHECK (l.prime_taps > 0 && l.prime_left == l.prime_taps + 1u);
  DP_CHECK (l.term != NULL);

  size_t nb = 0;
  if (rc)
    for (size_t k = 0; k < n; k++)
      {
        float _Complex ys[4];
        size_t got
            = dp_RateConverter_execute_ctrl_push (rc, x[k], l.ctrl, ys, 4);
        for (size_t oi = 0; oi < got; oi++)
          if (ratesync_loop_take_output (&l, ys[oi], &b[nb],
                                         RATESYNC_TED_GARDNER))
            nb++;
      }
  DP_CHECK (na > 0 && na == nb);
  DP_CHECK (na && memcmp (a, b, na * sizeof (float _Complex)) == 0);
  if (s)
    {
      DP_CHECK (l.lock_stat == dp_ratesync_get_lock_stat (s));
      DP_CHECK (l.rate_est == dp_ratesync_get_rate (s));
    }

  dp_RateConverter_destroy (rc);
  dp_ratesync_destroy (s);
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
  const double    sps = 64.0; /* CIC(32) + Resampler(1.0): a 32x decimation */
  size_t          n   = 0;
  float _Complex *x   = _tx (sps, 0.3, &n);
  float _Complex *y   = calloc (n, sizeof *y);
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
      dp_RateConverter_state_t *rc = dp_RateConverter_create_matched (
          2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
      DP_CHECK (rc != NULL);
      if (!rc)
        continue;
      ratesync_loop_t l;
      dp_ratesync_loop_init (&l, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
      dp_ratesync_loop_bind_cascade (&l, rc);
      if (wrong) /* the cascade rate, m/sps — 32x too under4 */
        dp_ratesync_loop_set_cascade (&l, 2.0 / sps, l.prime_taps);
      size_t ns = 0;
      for (size_t k = 0; k < n; k++)
        {
          float _Complex ys[4];
          size_t got
              = dp_RateConverter_execute_ctrl_push (rc, x[k], l.ctrl, ys, 4);
          for (size_t oi = 0; oi < got; oi++)
            if (ratesync_loop_take_output (&l, ys[oi], &y[ns],
                                           RATESYNC_TED_GARDNER))
              ns++;
        }
      lock[wrong] = l.lock_stat;
      evm[wrong]  = _evm_db_bn (y, ns, 0.01);
      dp_RateConverter_destroy (rc);
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
    DP_TEST_ERR (
        "  ctrl scale: terminal lock %.3f EVM %.1f; cascade lock %.3f "
        "EVM %.1f\n",
        lock[0], evm[0], lock[1], evm[1]);
  free (x);
  free (y);
}

/* ── §13 — `clipped` reports over-drive, and only where a CIC exists ─────
 *
 * C36: "Over-driving ... IS reported, by dp_ratesync_get_clipped(): a CIC
 * bounds its input to +-1.0." C57: "Always 0 when the plan has no CIC stage."
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
        size_t          n = 0;
        float _Complex *x = _tx_amp (sps[i], 0.2, over ? 4.0 : 1.0, &n);
        float _Complex *y = calloc (n, sizeof *y);
        DP_CHECK (x && y);
        if (!x || !y)
          {
            free (x);
            free (y);
            continue;
          }
        dp_ratesync_state_t *s
            = dp_ratesync_create (sps[i], RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                  1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
        DP_CHECK (s != NULL);
        if (s)
          {
            (void)dp_ratesync_steps (s, x, n, y, n);
            int c = dp_ratesync_get_clipped (s);
            /* Unit amplitude never clips on any plan — the pulse's 1.582x
               PAPR is inside the CIC's budgeted headroom. */
            int want = over && has_cic[i];
            DP_CHECK (c == want);
            if (c != want)
              DP_TEST_ERR ("  clipped: sps=%g over=%d got %d want %d\n",
                           sps[i], over, c, want);
            /* reset() clears the flag along with everything else. */
            if (c)
              {
                dp_ratesync_reset (s);
                DP_CHECK (dp_ratesync_get_clipped (s) == 0);
              }
            dp_ratesync_destroy (s);
          }
        free (x);
        free (y);
      }
}

/* ── §14 — the rectangle locks at m = 2 as it does at m = 4 ──────────────
 *
 * C39: "m = 2 is enough for either pulse". The header used to say the
 * opposite ("use m >= 4 with RATESYNC_PULSE_IANDD ... the eye barely
 * opens"), and this test used to pin that: lock statistic under the 0.311
 * declare threshold at m = 2. That was the matched bank's dead zone, not the
 * rectangle (doppler#3362): its arms were the rectangle sampled on the
 * input grid, so the loop could not see a timing error inside one input
 * interval. With arms that follow the fraction both counts lock, and on the
 * sampled arms the m = 2 half of this goes red again (measured: 0.955 now,
 * under 0.311 then). */
static void
test_iandd_locks_at_m2 (void)
{
  for (size_t m = 2; m <= 4; m += 2)
    {
      size_t          n = 0;
      float _Complex *x = _tx_nrz (4.0, &n);
      float _Complex *y = calloc (n, sizeof *y);
      DP_CHECK (x && y);
      if (!x || !y)
        {
          free (x);
          free (y);
          continue;
        }
      dp_ratesync_state_t *s
          = dp_ratesync_create (4.0, RATESYNC_PULSE_IANDD, 0.0, 1, m, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (s != NULL);
      if (s)
        {
          (void)dp_ratesync_steps (s, x, n, y, n);
          double lock = dp_ratesync_get_lock_stat (s);
          DP_CHECK (lock > 0.55);
          DP_CHECK (dp_ratesync_get_locked (s) == 1);
          if (!(lock > 0.55))
            DP_TEST_ERR ("  IANDD: lock_stat %.3f at m=%zu\n", lock, m);
          dp_ratesync_destroy (s);
        }
      free (x);
      free (y);
    }

  /* And at 2 INPUT samples a symbol, where the dead zone was half a symbol
     wide and the loop did not hold lock at all. */
  size_t               n = 0;
  float _Complex      *x = _tx_nrz (2.0, &n);
  float _Complex      *y = calloc (n ? n : 1, sizeof *y);
  dp_ratesync_state_t *s
      = dp_ratesync_create (2.0, RATESYNC_PULSE_IANDD, 0.0, 1, 2, 1024, 0.01,
                            0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (x && y && s);
  if (x && y && s)
    {
      (void)dp_ratesync_steps (s, x, n, y, n);
      double lock = dp_ratesync_get_lock_stat (s);
      DP_CHECK (lock > 0.55);
      DP_CHECK (dp_ratesync_get_locked (s) == 1);
      if (!(lock > 0.55))
        DP_TEST_ERR ("  IANDD: lock_stat %.3f at sps=2, m=2\n", lock);
    }
  dp_ratesync_destroy (s);
  free (x);
  free (y);
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
  const double    sps = 17.333333333;
  size_t          n   = 0;
  float _Complex *x   = _tx (sps, 0.25, &n);
  float _Complex *a   = calloc (n, sizeof *a);
  float _Complex *b   = calloc (n, sizeof *b);
  DP_CHECK (x && a && b);
  if (!x || !a || !b)
    {
      free (x);
      free (a);
      free (b);
      return;
    }

  dp_RateConverter_state_t *rc1 = dp_RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  dp_RateConverter_state_t *rc2 = dp_RateConverter_create_matched (
      2.0 / sps, 1, RC_PULSE_RRC, _BETA, _SPAN, 2.0, 1024);
  ratesync_loop_t l1, l2;
  dp_ratesync_loop_init (&l1, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_ratesync_loop_init (&l2, sps, 2, 0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (rc1 && rc2);
  if (rc1 && rc2)
    {
      dp_ratesync_loop_bind_cascade (&l1, rc1);
      dp_ratesync_loop_bind_cascade (&l2, rc2);

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
          float _Complex ys[4];
          size_t got
              = dp_RateConverter_execute_ctrl_push (rc1, x[k], l1.ctrl, ys, 4);
          for (size_t oi = 0; oi < got; oi++)
            if (ratesync_loop_take_output (&l1, ys[oi], &a[na],
                                           RATESYNC_TED_GARDNER))
              na++;
        }
      DP_CHECK (cut > 0);
      DP_CHECK ((int)(l1.out_count & 1u) == parity);

      size_t sb   = dp_ratesync_loop_state_bytes (&l1);
      void  *blob = malloc (sb);
      DP_CHECK (blob != NULL);
      if (blob)
        {
          dp_ratesync_loop_get_state (&l1, blob);
          DP_CHECK (dp_ratesync_loop_set_state (&l2, blob) == DP_OK);
          void *blob2 = malloc (sb);
          DP_CHECK (blob2 != NULL);
          if (blob2)
            {
              dp_ratesync_loop_get_state (&l2, blob2);
              DP_CHECK (memcmp (blob, blob2, sb) == 0);
              free (blob2);
            }
          /* The cascade is the loop's peer, not its child: carry it across
             by hand so the resumed pair is genuinely identical. */
          size_t rb    = dp_RateConverter_state_bytes (rc1);
          void  *rblob = malloc (rb);
          DP_CHECK (rblob != NULL);
          if (rblob)
            {
              dp_RateConverter_get_state (rc1, rblob);
              DP_CHECK (dp_RateConverter_set_state (rc2, rblob) == DP_OK);
              free (rblob);
            }

          size_t ka = na, kb = na;
          for (size_t k = cut; k < n; k++)
            {
              float _Complex ys[4];
              size_t got = dp_RateConverter_execute_ctrl_push (rc1, x[k],
                                                               l1.ctrl, ys, 4);
              for (size_t oi = 0; oi < got; oi++)
                if (ratesync_loop_take_output (&l1, ys[oi], &a[ka],
                                               RATESYNC_TED_GARDNER))
                  ka++;
              got = dp_RateConverter_execute_ctrl_push (rc2, x[k], l2.ctrl, ys,
                                                        4);
              for (size_t oi = 0; oi < got; oi++)
                if (ratesync_loop_take_output (&l2, ys[oi], &b[kb],
                                               RATESYNC_TED_GARDNER))
                  kb++;
            }
          DP_CHECK (ka == kb && ka > na);
          DP_CHECK (
              memcmp (a + na, b + na, (ka - na) * sizeof (float _Complex))
              == 0);
          if (ka != kb
              || memcmp (a + na, b + na, (ka - na) * sizeof (float _Complex))
                     != 0)
            DP_TEST_ERR ("  loop blob: parity %d resumed %zu vs %zu\n", parity,
                         ka - na, kb - na);

          /* Its own envelope rejects, independently of the parent's. */
          ((char *)blob)[0] ^= (char)0xFF;
          DP_CHECK (dp_ratesync_loop_set_state (&l2, blob) == DP_ERR_INVALID);
          free (blob);
        }
    }
  dp_RateConverter_destroy (rc1);
  dp_RateConverter_destroy (rc2);
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
  dp_ratesync_state_t *s
      = dp_ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  dp_tlm_t *t = dp_tlm_create (1 << 12);
  DP_CHECK (s && t);
  if (s && t)
    {
      DP_CHECK (dp_ratesync_set_telemetry (s, t, "ok", 1) == DP_OK);
      DP_CHECK (dp_tlm_probe_count (t) == 6);
      DP_CHECK (s->loop.tlm.ctx == t);

      /* NULL detaches, and detaching cannot fail. */
      DP_CHECK (dp_ratesync_set_telemetry (s, NULL, NULL, 1) == DP_OK);
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
      DP_CHECK (dp_ratesync_set_telemetry (s, t, "nope", 1) == DP_ERR_INVALID);
      DP_CHECK (s->loop.tlm.ctx == NULL); /* the attach failed WHOLE */
    }
  dp_tlm_destroy (t);
  dp_ratesync_destroy (s);
}

/* ── §17 — retune keeps the lock; a lock retune drops it ─────────────────
 *
 * C28: dp_ratesync_configure "preserves the integrator (and so the lock)".
 * C29/C58: configure_lock_raw "clears the in-flight block sum and drops the
 * lock", and avgs is "clamped >= 1".
 *
 * Both are pinned by .pyi doctests on the Python face only. A doctest is not
 * a gate a C-side regression would ever reach. */
static void
test_configure_semantics (void)
{
  size_t          n = 0;
  float _Complex *x = _tx (8.0, 0.2, &n);
  float _Complex *y = calloc (n, sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    {
      free (x);
      free (y);
      return;
    }
  dp_ratesync_state_t *s
      = dp_ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (s)
    {
      (void)dp_ratesync_steps (s, x, n, y, n);
      DP_CHECK (dp_ratesync_get_locked (s) == 1);
      double integ = s->loop.lf.integ, rate = dp_ratesync_get_rate (s);

      DP_CHECK (dp_ratesync_configure (s, 0.002, 0.707) == DP_OK);
      DP_CHECK (dp_ratesync_get_bn (s) == 0.002);
      DP_CHECK (s->loop.lf.integ == integ); /* the rate memory survives */
      DP_CHECK (dp_ratesync_get_rate (s) == rate);
      DP_CHECK (dp_ratesync_get_locked (s) == 1); /* and so does the lock */

      /* An invalid retune is refused, with a status (#2646), rather than
         applied destructively. */
      DP_CHECK (dp_ratesync_configure (s, -1.0, 0.707) == DP_ERR_INVALID);
      DP_CHECK (dp_ratesync_get_bn (s) == 0.002);
      DP_CHECK (dp_ratesync_configure (s, 0.002, 0.0) == DP_ERR_INVALID);
      DP_CHECK (dp_ratesync_set_bn (s, NAN) == DP_ERR_INVALID);
      DP_CHECK (dp_ratesync_loop_configure (&s->loop, -1.0, 0.707)
                == DP_ERR_INVALID);
      DP_CHECK (dp_ratesync_get_bn (s) == 0.002);
      /* So is an infinite bn or zeta, which the private check this call
         used to keep took (doppler#2103): nothing is written. */
      const double kp = s->loop.lf.kp, ki = s->loop.lf.ki;
      dp_ratesync_configure (s, INFINITY, 0.707);
      dp_ratesync_configure (s, 0.002, INFINITY);
      DP_CHECK (dp_ratesync_get_bn (s) == 0.002 && s->loop.zeta == 0.707);
      DP_CHECK (s->loop.lf.kp == kp && s->loop.lf.ki == ki);

      /* set_bn is the property face of the same call. */
      DP_CHECK (dp_ratesync_set_bn (s, 0.004) == DP_OK);
      DP_CHECK (dp_ratesync_get_bn (s) == 0.004);
      DP_CHECK (dp_ratesync_get_locked (s) == 1);

      /* A lock retune drops the decision and the in-flight block. */
      dp_ratesync_configure_lock_raw (s, 0, 0.5, 0.4, 2, 4);
      DP_CHECK (s->loop.avgs == 1); /* clamped up from 0 */
      DP_CHECK (dp_ratesync_get_locked (s) == 0);
      DP_CHECK (dp_ratesync_get_lock_stat (s) == 0.0);
      DP_CHECK (s->loop.lock_count == 0);
      /* but not the timing estimate */
      DP_CHECK (dp_ratesync_get_rate (s) == rate);
      dp_ratesync_destroy (s);
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
  size_t          n = 0;
  float _Complex *x = _tx (4.0, 0.2, &n);
  float _Complex *y = calloc (n, sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    {
      free (x);
      free (y);
      return;
    }
  dp_ratesync_state_t *s
      = dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (s)
    {
      DP_CHECK (dp_ratesync_steps_max_out (s) == 0);
      size_t full = dp_ratesync_steps (s, x, n, y, n);
      DP_CHECK (full > 0);
      DP_CHECK (full
                <= n); /* symbols can never exceed inputs: sps >= m >= 2 */
      dp_ratesync_reset (s);
      DP_CHECK (dp_ratesync_steps (s, x, n, y, 10) == 10);
      dp_ratesync_reset (s);
      DP_CHECK (dp_ratesync_steps (s, x, n, y, 0) == 0);
      dp_ratesync_destroy (s);
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
  dp_ratesync_state_t *s
      = dp_ratesync_create (2.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL); /* rate = m/sps = 1.0 exactly: allowed */
  if (s)
    {
      DP_CHECK (s->loop.term_rate <= 1.0);
      dp_ratesync_destroy (s);
    }
  /* One ulp below is not. */
  DP_CHECK (dp_ratesync_create (1.999, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER)
            == NULL);
  /* The same edge at the other supported m. */
  s = dp_ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 8, 1024, 0.01,
                          0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  dp_ratesync_destroy (s);
  DP_CHECK (dp_ratesync_create (7.999, RATESYNC_PULSE_RRC, _BETA, _SPAN, 8,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER)
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
 *   - ted_scale is `1 / dp_symsync_ted_slope(ted, pulse, beta, span)`, a
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
      dp_ratesync_state_t *s = dp_ratesync_create (
          4.0, RATESYNC_PULSE_RRC, beta, _SPAN, 2, 1024, 0.01, 0.707, teds[i]);
      DP_CHECK (s != NULL);
      if (!s)
        continue;
      double want
          = dp_symsync_ted_slope (sym[i], SYMSYNC_PULSE_RRC, beta, _SPAN);
      DP_CHECK (want > 0.0);
      DP_CHECK (fabs (1.0 / s->loop.ted_scale - want) <= 1e-9 * want);
      dp_ratesync_destroy (s);
    }
}

/* ── §21 — the lock's time hysteresis (#3133) ──
   The default verify counts are the ones
   dp_det_verify_count() gives at the sizing rates and the per-decision
   budget the header states, and a declare takes n_up consecutive passing
   decisions, so a single one never declares. Negative control: a detector
   left at the n_up of 1 it had declares on a single pass and fails the same
   predicate. */
static int
declares_on_single_pass (dp_lockdet_state_t *lk)
{
  /* pass, fail, pass, fail, ...: never two passing decisions in a row */
  for (int i = 0; i < 64; i++)
    {
      (void)dp_lockdet_step (lk, (i & 1) ? 0.0 : 1.0);
      if (lk->locked)
        return 1;
    }
  return 0;
}

static void
test_lock_time_hysteresis (void)
{
  dp_ratesync_state_t *s
      = dp_ratesync_create (8.0, RATESYNC_PULSE_IANDD, 0.35, 8, 4, 32, 0.01,
                            0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (!s)
    return;
  const dp_lockdet_state_t *lk = &s->loop.lock;
  const uint32_t up   = (uint32_t)dp_det_verify_count (RATESYNC_LOCK_PFA,
                                                       RATESYNC_LOCK_BUDGET);
  const uint32_t down = (uint32_t)dp_det_verify_count (1.0 - RATESYNC_LOCK_PD,
                                                       RATESYNC_LOCK_BUDGET);
  DP_CHECK_MSG (lk->n_up == up && lk->n_down == down,
                "the verify counts are the budget's");
  DP_CHECK_MSG (lk->n_up > 1, "a declare needs more than one decision");
  /* The rule as built: a single passing decision never declares, and two in
     a row do. */
  dp_lockdet_state_t d = *lk;
  dp_lockdet_reset (&d);
  DP_CHECK_MSG (!declares_on_single_pass (&d),
                "a single passing decision never declares");
  dp_lockdet_reset (&d);
  for (uint32_t i = 0; i < d.n_up; i++)
    (void)dp_lockdet_step (&d, 1.0);
  DP_CHECK_MSG (d.locked, "n_up passing decisions in a row declare");
  /* Negative control: the old default, n_up 1. */
  dp_lockdet_state_t old;
  dp_lockdet_init (&old, lk->up_thresh, lk->down_thresh, 1u, lk->n_down);
  DP_CHECK_MSG (declares_on_single_pass (&old),
                "control: n_up 1 declares on a single passing decision");
  dp_ratesync_destroy (s);
}

/* settle_time is the loop filter's figure at the loop's own bn and zeta,
   and follows a retune. Sabotage, each red here: the getter reading a
   literal damping, and one reading a literal bandwidth. */
static void
test_settle_time (void)
{
  dp_ratesync_state_t *s
      = dp_ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (s != NULL);
  if (!s)
    return;
  DP_CHECK (
      dp_ratesync_get_settle_time (s)
      == dp_loop_filter_settle_time (0.01, 0.707, DP_LOOP_FILTER_SETTLE_EPS));
  DP_CHECK_NEAR (dp_ratesync_get_settle_time (s), 259.49, 0.01);
  DP_CHECK (dp_ratesync_configure (s, 0.004, 1.0) == DP_OK);
  DP_CHECK (
      dp_ratesync_get_settle_time (s)
      == dp_loop_filter_settle_time (0.004, 1.0, DP_LOOP_FILTER_SETTLE_EPS));
  DP_CHECK (dp_ratesync_get_settle_time (s)
            == dp_ratesync_loop_settle_time (&s->loop));
  /* A loop that does not steer never settles. */
  DP_CHECK (dp_ratesync_set_bn (s, 0.0) == DP_OK);
  DP_CHECK (isinf (dp_ratesync_get_settle_time (s)));
  dp_ratesync_destroy (s);
}

/* settle_time held against the object itself, where the header claims it:
   RRC, Gardner, a starting error of 0.25 symbol or less. At every symbol
   from ceil(settle_time) the mean error over data patterns is at or under
   2% of the start, plus RS_SETTLE_K standard errors of that mean
   (ratesync_settle_test.h says what is measured and why it is a mean).

   Why five standard errors. The bound is checked at every symbol from
   settle_time (260) to 449, 190 of them, on each of five rows: 950
   comparisons. For a loop that meets the claim, the mean passes its bound
   by k standard errors at one comparison with the one-sided probability
   of a t with 99 degrees of freedom, since the standard error is itself
   estimated from the 100 patterns:

       k    per comparison    expected false fails in 950    at least one
       3    1.7e-3            1.6                            80%
       4    6.1e-5            0.058                          at most 6%
       5    1.2e-6            1.2e-3                         at most 0.12%

   The last column at 3 takes the comparisons as independent; neighboring
   symbols are not, so it is an overestimate. The others are the union
   bound, which holds whatever the correlation. All are the worst case, a
   true residual right at 2%, and that case is real: the 0.25 symbol row
   sits there, so 6% is not small enough.

   Further starts and the rectangle are the sweep's,
   native/validation/ratesync_settle.c.

   Sabotage, on these data: the loop built at half its bandwidth or at 0.8
   of it, red at every row; at 0.9 of it, red at two. */
#define _SETTLE_SEEDS 100

static void
test_settle_time_from_an_offset (void)
{
  /* offsets whose start is 0.06 to 0.25 symbol, both signs */
  static const double taus[] = { 0.25, 0.375, 0.4375, 0.625, 0.6875 };
  for (size_t i = 0; i < sizeof taus / sizeof *taus; i++)
    {
      rs_settle_t r;
      const int   ran = rs_settle_measure (RATESYNC_PULSE_RRC, 4.0, 2, taus[i],
                                           _SETTLE_SEEDS, &r);
      DP_CHECK (ran);
      if (!ran)
        return;
      printf ("  settle: start %+.4f symbol, worst residual from symbol %d "
              "%.1f%% of it, %d symbols over the bound\n",
              r.start, r.from, 100.0 * r.worst / fabs (r.start), r.over);
      DP_CHECK (r.from < RS_SETTLE_END);
      /* inside the range the claim is made over, and a start at all */
      DP_CHECK (fabs (r.start) > 0.05 && fabs (r.start) <= 0.25);
      DP_CHECK (r.over == 0);
    }
}

/* mean (|Re y|)^2 / mean (Re y ^2) over y[a..b): 1 when every symbol is
   read at its full height, lower as the reading instant leaves the eye. */
static double
_eye (const float _Complex *y, size_t a, size_t b)
{
  double sa = 0.0, sp = 0.0;
  for (size_t i = a; i < b; i++)
    {
      double v = (double)crealf (y[i]);
      sa += fabs (v);
      sp += v * v;
    }
  double n = (double)(b - a);
  return (b > a && sp > 0.0) ? (sa / n) * (sa / n) / (sp / n) : 0.0;
}

/* Symbol periods the cascade's fill takes: its outputs are dropped, not
   emitted, so a count of symbols is short by this. */
static double
_primed (const dp_ratesync_state_t *s)
{
  return (double)(s->loop.prime_taps + 1u) / (double)s->loop.m;
}

/* The seed: where the next output reads, the caller's call, and the call
   after the rate has been steered.
     Sabotages, each shown red: the whole inputs owed left out of
   dp_resamp_get_ctrl_lead() -> (1), every plan under unity; the group
   delay left out of the seed -> (2); the seed's control first order
   (`r / m` for `r / (m + r)`) -> (2); its sign -> (2); the on-time outputs
   picked one early -> (2); the next output placed by the count of outputs
   and not by the accumulator -> (3), the second seed. */
static void
test_seed (void)
{
  static const struct
  {
    double sps;
    size_t m;
  } plan[]
      = { { 4.0, 2 }, { 5.0, 2 }, { 8.0, 4 }, { 17.333, 4 }, { 67.3, 2 } };
  static const double taus[]
      = { 0.0, 0.13, 0.25, 0.37, 0.5, 0.63, 0.75, 0.87 };

  /* (1) Unsteered, the accumulator and the count of outputs say the same
     thing at every input: decim * (term_in + lead) is outputs / rate. */
  for (size_t pi = 0; pi < sizeof plan / sizeof *plan; pi++)
    {
      const double         sps = plan[pi].sps;
      dp_ratesync_state_t *s   = dp_ratesync_create (
          sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, plan[pi].m, 1024, 0.0, 0.707,
          RATESYNC_TED_GARDNER);
      size_t          n = 0;
      float _Complex *x = _tx (sps, 0.3, &n);
      DP_CHECK (s != NULL && x != NULL);
      if (!s || !x)
        {
          dp_ratesync_destroy (s);
          free (x);
          continue;
        }
      double worst = 0.0;
      size_t lim   = n < 6000 ? n : 6000;
      for (size_t i = 0; i < lim; i++)
        {
          float _Complex y;
          (void)ratesync_step (s, x[i], &y);
          double outs = (double)(s->loop.out_count + s->loop.prime_taps + 1u
                                 - s->loop.prime_left);
          double here = s->decim
                        * ((double)s->term_in
                           + dp_resamp_get_ctrl_lead (s->loop.term));
          double d    = fabs (here - outs * sps / (double)plan[pi].m);
          if (d > worst)
            worst = d;
        }
      printf ("  seed: sps %.3f m %zu, next output placed within %.2e "
              "inputs of outputs / rate\n",
              sps, plan[pi].m, worst);
      DP_CHECK (s->in_count == lim);
      DP_CHECK (worst < 1e-4);
      dp_ratesync_destroy (s);
      free (x);
    }

  /* (2) The caller's seed, on a loop that does not steer (bn = 0), so
     where the symbols are read is the seed's doing alone. Seeded, the eye
     is open at every offset; unseeded, the same stream is read wherever
     the count happens to fall, and at some offset that is not the eye. */
  for (size_t pi = 0; pi < sizeof plan / sizeof *plan; pi++)
    {
      const double sps        = plan[pi].sps;
      double       worst_seed = 1.0, worst_none = 1.0;
      for (size_t ti = 0; ti < sizeof taus / sizeof *taus; ti++)
        for (int seeded = 0; seeded < 2; seeded++)
          {
            dp_ratesync_state_t *s = dp_ratesync_create (
                sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, plan[pi].m, 1024, 0.0,
                0.707, RATESYNC_TED_GARDNER);
            size_t          n = 0;
            float _Complex *x = _tx (sps, taus[ti], &n);
            float _Complex *y = malloc ((n + 8) * sizeof *y);
            DP_CHECK (s != NULL && x != NULL && y != NULL);
            if (!s || !x || !y)
              {
                dp_ratesync_destroy (s);
                free (x);
                free (y);
                continue;
              }
            /* symbol k's center, on the input's timeline */
            const double eye = ((double)_SPAN + taus[ti]) * sps;
            /* at half the offsets the seed comes before the first
               input, while the cascade has still to fill */
            const size_t at = (ti & 1u) ? 0u : (size_t)(300.0 * sps);
            size_t       ny = 0, y_at = 0;
            for (size_t i = 0; i < n; i++)
              {
                if (i == at)
                  {
                    y_at = ny;
                    if (seeded)
                      DP_CHECK (dp_ratesync_seed (s, eye, 0.0) == DP_OK);
                  }
                ny += (size_t)ratesync_step (s, x[i], &y[ny]);
              }
            /* one symbol a symbol period, seed or no seed */
            DP_CHECK (fabs ((double)ny + _primed (s) - (double)n / sps) < 4.0);
            double e = _eye (y, y_at + 4, ny - 4);
            if (seeded)
              {
                DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_TRACKING);
                if (e < worst_seed)
                  worst_seed = e;
              }
            else if (e < worst_none)
              worst_none = e;
            dp_ratesync_destroy (s);
            free (x);
            free (y);
          }
      printf ("  seed: sps %.3f m %zu, worst eye over %zu offsets: seeded "
              "%.3f, unseeded %.3f\n",
              sps, plan[pi].m, sizeof taus / sizeof *taus, worst_seed,
              worst_none);
      DP_CHECK (worst_seed > 0.97);
      DP_CHECK (worst_none < 0.90);
    }

  /* (3) After the rate has been steered. Symbols 0.2% long; the first
     seed gives the loop that rate, which it holds (bn = 0), and a second
     seed 1200 symbols on names an eye center of the same stream. The
     outputs' count no longer says where an output reads; the eye has to
     stay open through both. */
  {
    const double         sps = 8.0, stretch = 1.002;
    dp_ratesync_state_t *s
        = dp_ratesync_create (sps, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                              0.0, 0.707, RATESYNC_TED_GARDNER);
    dp_tx_cfg_t c     = _tx_cfg (sps, 0.37);
    c.rate            = stretch;
    size_t          n = 0;
    float _Complex *x = dp_tx_make (&c, NULL, &n);
    float _Complex *y = malloc ((n + 8) * sizeof *y);
    DP_CHECK (s != NULL && x != NULL && y != NULL);
    if (s && x && y)
      {
        const double sym  = sps * stretch;
        const double eye  = (double)_SPAN * sym + 0.37 * sps;
        const double rate = 1.0 / stretch - 1.0;
        const size_t at1 = (size_t)(300.0 * sym), at2 = (size_t)(1500.0 * sym);
        size_t       ny = 0, y1 = 0, y2 = 0;
        for (size_t i = 0; i < n; i++)
          {
            if (i == at1)
              {
                y1 = ny;
                DP_CHECK (dp_ratesync_seed (s, eye, rate) == DP_OK);
              }
            if (i == at2)
              {
                y2 = ny;
                /* an eye center 2000 symbols on: any one will do */
                DP_CHECK (dp_ratesync_seed (s, eye + 2000.0 * sym, rate)
                          == DP_OK);
              }
            ny += (size_t)ratesync_step (s, x[i], &y[ny]);
          }
        double e1 = _eye (y, y1 + 4, y2), e2 = _eye (y, y2 + 4, ny - 4);
        printf ("  seed: symbols 0.2%% long, eye %.3f after the first seed, "
                "%.3f after the second\n",
                e1, e2);
        DP_CHECK (e1 > 0.97);
        DP_CHECK (e2 > 0.97);
        DP_CHECK (s->loop.lf.integ == rate);
      }
    dp_ratesync_destroy (s);
    free (x);
    free (y);
  }

  /* Refusals change nothing: an eye that is not a number, a rate of half
     a symbol a symbol. */
  {
    dp_ratesync_state_t *s
        = dp_ratesync_create (8.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                              0.01, 0.707, RATESYNC_TED_GARDNER);
    DP_CHECK (s != NULL);
    if (s)
      {
        size_t          n = 0;
        float _Complex *x = _tx (8.0, 0.0, &n);
        float _Complex y;
        for (size_t i = 0; x && i < 2000; i++)
          (void)ratesync_step (s, x[i], &y);
        size_t nb = dp_ratesync_state_bytes (s);
        char  *b0 = malloc (nb), *b1 = malloc (nb);
        DP_CHECK (b0 != NULL && b1 != NULL);
        if (b0 && b1)
          {
            dp_ratesync_get_state (s, b0);
            DP_CHECK (dp_ratesync_seed (s, NAN, 0.0) == DP_ERR_INVALID);
            DP_CHECK (dp_ratesync_seed (s, INFINITY, 0.0) == DP_ERR_INVALID);
            DP_CHECK (dp_ratesync_seed (s, 3.0, 0.5) == DP_ERR_INVALID);
            DP_CHECK (dp_ratesync_seed (s, 3.0, NAN) == DP_ERR_INVALID);
            dp_ratesync_get_state (s, b1);
            DP_CHECK (memcmp (b0, b1, nb) == 0);
            DP_CHECK (dp_ratesync_seed (s, 3.0, 0.0) == DP_OK);
          }
        free (b0);
        free (b1);
        free (x);
      }
    dp_ratesync_destroy (s);
  }
}

/* A rectangle stream in three parts: `a` symbols of signal, `gap` symbols
   of nothing, `b` symbols of signal whose symbol edges sit `shift` samples
   off the first part's. Symbols `stretch` of nominal long. */
static float _Complex *
_tx_gap (double sps, double stretch, size_t a, size_t gap, size_t b,
         size_t shift, size_t *n_out)
{
  dp_tx_cfg_t c     = _tx_cfg (sps, 0.0);
  c.pulse           = DP_TX_NRZ;
  c.rate            = stretch;
  c.nsym            = a + gap + b + 8;
  size_t          n = 0;
  float _Complex *x = dp_tx_make (&c, NULL, &n);
  if (!x)
    return NULL;
  const double    sym = sps * stretch;
  size_t          g0  = (size_t)((double)(a + _SPAN) * sym);
  size_t          g1  = g0 + (size_t)((double)gap * sym);
  float _Complex *o   = calloc (n, sizeof *o);
  if (o)
    {
      for (size_t i = 0; i < g0 && i < n; i++)
        o[i] = x[i];
      for (size_t i = g1; i + shift < n; i++)
        o[i] = x[i + shift];
      *n_out = n - shift;
    }
  free (x);
  return o;
}

/* The three states. Sabotages, each shown red: a loop whose owner does
   not acquire leaving tracking -> the first block; the estimator not
   reset at a new acquisition -> the reacquisition's eye; presence not
   read -> the coast; the coast's rate zeroed -> the held rate; the rate
   kept when acquiring -> the zero rate; the miss run not counted -> the
   never-declared return; the acquisition left out of settle_time. */
static void
test_lifecycle (void)
{
  const double sps = 8.0;
  const size_t L   = 128;

  /* No acquisition: tracking from reset to the end, through a loss of
     signal, and `valid` is the lock flag. */
  {
    dp_ratesync_state_t *s
        = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4, 1024,
                              0.01, 0.707, RATESYNC_TED_GARDNER);
    size_t          n = 0;
    float _Complex *x = _tx_gap (sps, 1.0, 1200, 1200, 400, 0, &n);
    DP_CHECK (s != NULL && x != NULL);
    int was_valid = 0, dropped = 0;
    for (size_t i = 0; s && x && i < n; i++)
      {
        float _Complex y;
        (void)ratesync_step (s, x[i], &y);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_TRACKING);
        DP_CHECK (dp_ratesync_get_valid (s) == dp_ratesync_get_locked (s));
        /* and nothing of the machine has run: no decision was read, no
           acquisition started, no seed made */
        DP_CHECK (!s->loop.had_lock && !s->acq_run && !s->loop.seed_left);
        if (dp_ratesync_get_valid (s))
          was_valid = 1;
        else if (was_valid)
          dropped = 1;
      }
    DP_CHECK (was_valid && dropped);
    dp_ratesync_destroy (s);
    free (x);
  }

  /* The domain of the setting. */
  {
    dp_ratesync_state_t *s
        = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4, 1024,
                              0.01, 0.707, RATESYNC_TED_GARDNER);
    dp_ratesync_state_t *under4
        = dp_ratesync_create (3.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, 2, 1024,
                              0.01, 0.707, RATESYNC_TED_GARDNER);
    DP_CHECK (s != NULL && under4 != NULL);
    if (s && under4)
      {
        DP_CHECK (under4->est == NULL && s->est != NULL);
        DP_CHECK (dp_ratesync_configure_acquisition (under4, L, 0.05)
                  == DP_ERR_INVALID);
        DP_CHECK (dp_ratesync_configure_acquisition (under4, 0, 0.05)
                  == DP_OK);
        DP_CHECK (dp_ratesync_configure_acquisition (s, L, 0.0)
                  == DP_ERR_INVALID);
        DP_CHECK (dp_ratesync_configure_acquisition (s, L, 1.0)
                  == DP_ERR_INVALID);
        DP_CHECK (dp_ratesync_configure_acquisition (s, L, NAN)
                  == DP_ERR_INVALID);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_TRACKING);
        const double t0 = dp_ratesync_get_settle_time (s);
        DP_CHECK (dp_ratesync_configure_acquisition (s, L, 0.05) == DP_OK);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_ACQUIRING);
        /* one attempt ahead of the loop */
        DP_CHECK (dp_ratesync_get_settle_time (s)
                  == t0
                         + ((double)s->est->lead / TIMING_EST_MATCHED_SPS
                            + (double)L + 2.0));
        DP_CHECK (s->est->lead > 0);
        dp_ratesync_reset (s);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_ACQUIRING);
        DP_CHECK (dp_ratesync_configure_acquisition (s, 0, 0.0) == DP_OK);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_TRACKING);
        DP_CHECK (dp_ratesync_get_settle_time (s) == t0);
      }
    dp_ratesync_destroy (s);
    dp_ratesync_destroy (under4);
  }

  /* Signal, nothing, signal again with its symbol edges 3 samples off and
     its symbols 0.1% long; `present` is the owner's presence input. */
  for (int present = 0; present < 3; present++)
    {
      const double         stretch = 1.001;
      dp_ratesync_state_t *s
          = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4,
                                1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
      size_t          n = 0;
      float _Complex *x = _tx_gap (sps, stretch, 1500, 1200, 1500, 3, &n);
      float _Complex *y = malloc ((n + 8) * sizeof *y);
      DP_CHECK (s != NULL && x != NULL && y != NULL);
      if (!s || !x || !y)
        {
          dp_ratesync_destroy (s);
          free (x);
          free (y);
          continue;
        }
      DP_CHECK (dp_ratesync_configure_acquisition (s, L, 0.05) == DP_OK);
      DP_CHECK (dp_ratesync_get_presence (s) == 0);
      dp_ratesync_set_presence (s, present != 0);
      DP_CHECK (dp_ratesync_get_presence (s) == (present != 0));

      const double sym  = sps * stretch;
      const size_t g0   = (size_t)((double)(1500 + _SPAN) * sym);
      const size_t g1   = g0 + (size_t)(1200.0 * sym);
      const double want = 1.0 / stretch - 1.0; /* the loop's rate state */
      size_t       ny = 0, first_track = 0, first_valid = 0, left = 0;
      size_t       back = 0, valid2 = 0, y_back = 0;
      int          left_to      = -1;
      double       rate_at_drop = 0.0, rate_after = 0.0, rate_seeded = 0.0;
      int          prev = dp_ratesync_get_lifecycle (s);
      for (size_t i = 0; i < n; i++)
        {
          /* case 2: presence drops in the middle of the coast */
          if (present == 2 && i == g0 + (size_t)(1000.0 * sym))
            {
              DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_COASTING);
              dp_ratesync_set_presence (s, 0);
              DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_ACQUIRING);
              DP_CHECK (s->loop.lf.integ == 0.0);
              DP_CHECK (dp_ratesync_get_ctrl (s) == 0.0);
            }
          const double integ_before = s->loop.lf.integ;
          ny += (size_t)ratesync_step (s, x[i], &y[ny]);
          int now = dp_ratesync_get_lifecycle (s);
          if (now != prev)
            {
              if (prev == RATESYNC_ACQUIRING && !first_track)
                first_track = ny;
              else if (prev == RATESYNC_TRACKING && !left)
                {
                  left         = i;
                  left_to      = now;
                  rate_at_drop = integ_before;
                  rate_after   = s->loop.lf.integ;
                  DP_CHECK (dp_ratesync_get_ctrl (s)
                            == s->loop.lf.integ * s->loop.term_rate);
                }
              else if (now == RATESYNC_TRACKING && left && !back)
                {
                  back        = i;
                  y_back      = ny;
                  rate_seeded = s->loop.lf.integ;
                }
              prev = now;
            }
          if (dp_ratesync_get_valid (s))
            {
              if (!first_valid)
                first_valid = ny;
              if (back && !valid2)
                valid2 = ny;
            }
          /* valid is never up outside tracking */
          DP_CHECK (!dp_ratesync_get_valid (s) || now == RATESYNC_TRACKING);
        }
      printf ("  lifecycle (presence %d): tracking at symbol %zu, valid at "
              "%zu, left tracking at input %zu for state %d (gap %zu..%zu), "
              "back at input %zu, valid again %zu symbols on; rate %.6f at "
              "the drop, %.6f after, %.6f at the reseed (signal's %.6f)\n",
              present, first_track, first_valid, left, left_to, g0, g1, back,
              valid2 - y_back, rate_at_drop, rate_after, rate_seeded, want);
      /* acquired by count: the estimator's lead-in, one block, the seed */
      DP_CHECK (first_track > L
                && (double)first_track
                       < (double)s->est->lead / TIMING_EST_MATCHED_SPS
                             + (double)L + 8.0);
      DP_CHECK (first_valid > first_track);
      /* the lock drops n_down decisions into the gap, not before */
      DP_CHECK (left > g0 && left < g1);
      DP_CHECK (left_to == (present ? RATESYNC_COASTING : RATESYNC_ACQUIRING));
      DP_CHECK (fabs (rate_at_drop) < 5e-3);
      if (present)
        DP_CHECK (rate_after == rate_at_drop); /* held */
      else
        DP_CHECK (rate_after == 0.0);
      /* signal again: reseeded within one block of its return */
      DP_CHECK (back > g1 && (double)(back - g1) < ((double)L + 40.0) * sym);
      if (present == 1)
        DP_CHECK (rate_seeded == rate_at_drop); /* the held rate, kept */
      else
        DP_CHECK (rate_seeded == 0.0);
      DP_CHECK (valid2 > y_back);
      /* one symbol a symbol period, in every state */
      DP_CHECK (fabs ((double)ny + _primed (s) - (double)n / sym) < 4.0);
      /* and the reacquired stream is read in its eye from the seed on,
         before the loop could have pulled it there: the 60 symbols after
         the seed's two */
      DP_CHECK (_eye (y, y_back + 4, y_back + 64) > 0.97);
      DP_CHECK (_eye (y, y_back + 8, ny - 4) > 0.97);
      dp_ratesync_destroy (s);
      free (x);
      free (y);
    }

  /* A seed on nothing: tracking that never declares goes back to acquiring
     after n_down decisions that were not hits, and not one sooner. */
  {
    dp_ratesync_state_t *s
        = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4, 1024,
                              0.01, 0.707, RATESYNC_TED_GARDNER);
    DP_CHECK (s != NULL);
    if (s)
      {
        DP_CHECK (dp_ratesync_configure_acquisition (s, L, 0.05) == DP_OK);
        float _Complex y;
        for (size_t i = 0; i < 400; i++)
          (void)ratesync_step (s, 0.0f, &y);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_ACQUIRING);
        DP_CHECK (dp_ratesync_seed (s, 0.0, 0.0) == DP_OK);
        DP_CHECK (dp_ratesync_get_lifecycle (s) == RATESYNC_TRACKING);
        size_t syms = 0, went = 0;
        for (size_t i = 0; i < (size_t)(1200.0 * sps) && !went; i++)
          {
            syms += (size_t)ratesync_step (s, 0.0f, &y);
            if (dp_ratesync_get_lifecycle (s) == RATESYNC_ACQUIRING)
              went = syms;
          }
        const size_t decisions = s->loop.lock.n_down * s->loop.avgs;
        printf ("  lifecycle: a seed on nothing returns to acquiring %zu "
                "symbols on (%u decisions of %zu)\n",
                went, s->loop.lock.n_down, s->loop.avgs);
        /* the seed's two open symbols, then the decisions */
        DP_CHECK (went >= decisions && went <= decisions + 4);
      }
    dp_ratesync_destroy (s);
  }

  /* State: a split in the middle of an acquisition and in the middle of a
     coast resumes bit for bit on an object that was never configured; and
     blobs this object does not reach are refused, changing nothing. */
  {
    size_t          n = 0;
    float _Complex *x = _tx_gap (sps, 1.001, 1500, 1200, 600, 3, &n);
    DP_CHECK (x != NULL);
    static const size_t cuts[] = { 517, 19000, 0 };
    for (size_t ci = 0; x && cuts[ci]; ci++)
      {
        dp_ratesync_state_t *a
            = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4,
                                  1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
        dp_ratesync_state_t *b
            = dp_ratesync_create (sps, RATESYNC_PULSE_IANDD, _BETA, _SPAN, 4,
                                  1024, 0.01, 0.707, RATESYNC_TED_GARDNER);
        DP_CHECK (a != NULL && b != NULL);
        if (a && b)
          {
            DP_CHECK (dp_ratesync_configure_acquisition (a, L, 0.05) == DP_OK);
            dp_ratesync_set_presence (a, 1);
            float _Complex ya, yb;
            for (size_t i = 0; i < cuts[ci]; i++)
              (void)ratesync_step (a, x[i], &ya);
            DP_CHECK (dp_ratesync_get_lifecycle (a)
                      == (ci == 0 ? RATESYNC_ACQUIRING : RATESYNC_COASTING));
            size_t nb   = dp_ratesync_state_bytes (a);
            char  *blob = malloc (nb), *keep = malloc (nb);
            DP_CHECK (blob != NULL && keep != NULL
                      && nb == dp_ratesync_state_bytes (b));
            if (blob && keep)
              {
                dp_ratesync_get_state (a, blob);
                /* refusals first, on the untouched twin */
                dp_ratesync_get_state (b, keep);
                {
                  dp_ratesync_state_t bad = *a;
                  char               *bb  = malloc (nb);
                  DP_CHECK (bb != NULL);
                  if (bb)
                    {
                      bad.loop.state = 7; /* no such state */
                      dp_ratesync_get_state (&bad, bb);
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      bad             = *a;
                      bad.acq_quality = 2.0; /* outside the setting */
                      dp_ratesync_get_state (&bad, bb);
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      bad             = *a;
                      bad.acq_symbols = 0; /* not tracking, no acquisition */
                      bad.acq_run     = 0;
                      dp_ratesync_get_state (&bad, bb);
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      bad         = *a;
                      bad.term_in = bad.in_count + 1u;
                      dp_ratesync_get_state (&bad, bb);
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      bad                = *a;
                      bad.loop.seed_left = 2u * bad.loop.m + 1u;
                      dp_ratesync_get_state (&bad, bb);
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      /* The case the restore of the loop exists for: the
                         LAST child refuses, the estimator, after the
                         cascade and the loop have both taken the blob.
                         `b` is fresh and `a` is far along, so a loop left
                         holding the blob's state would not match `keep`.
                         Sabotage, shown red here: the loop not put back. */
                      memcpy (bb, blob, nb);
                      bb[nb - dp_timing_est_state_bytes (a->est)] ^= 0x5a;
                      DP_CHECK (dp_ratesync_set_state (b, bb)
                                == DP_ERR_INVALID);
                      dp_ratesync_get_state (b, bb);
                      DP_CHECK (memcmp (bb, keep, nb) == 0);
                      DP_CHECK (b->loop.acquires == 0);
                    }
                  free (bb);
                }
                DP_CHECK (dp_ratesync_set_state (b, blob) == DP_OK);
                DP_CHECK (b->acq_symbols == L && b->loop.acquires == 1);
                int same = 1;
                for (size_t i = cuts[ci]; i < n; i++)
                  {
                    int ra = ratesync_step (a, x[i], &ya);
                    int rb = ratesync_step (b, x[i], &yb);
                    if (ra != rb || (ra && memcmp (&ya, &yb, sizeof ya) != 0)
                        || dp_ratesync_get_lifecycle (a)
                               != dp_ratesync_get_lifecycle (b))
                      same = 0;
                  }
                DP_CHECK (same);
                DP_CHECK (dp_ratesync_get_lifecycle (a) == RATESYNC_TRACKING);
              }
            free (blob);
            free (keep);
          }
        dp_ratesync_destroy (a);
        dp_ratesync_destroy (b);
      }
    free (x);
  }
}

/* One strobe per input (doppler#3289). A strobe is every m-th terminal
 * output, and two need m + 1 outputs from one input, so the converter holds
 * the steered rate (rate + ctrl) at or under m and one input yields at most
 * m. Forced past it, the loop's control is held there. Both sides of the
 * edge: forced to exactly m, the bound is reached and no input holds two
 * strobes; forced to m + 0.5, an input still yields at most m (clamped),
 * and none holds two. Sabotage: the bound removed (ctrl_rate_max left at
 * +INFINITY) -> at m + 0.5 an input yields m + 1 and two strobes share it,
 * one symbol lost, red. */
static void
test_a_forced_control_never_shares_an_input (void)
{
  const size_t m       = 2;
  const double over[2] = { 0.0, 0.5 }; /* steered rate m + over */
  for (size_t v = 0; v < 2; v++)
    {
      size_t               n = 0;
      float _Complex      *x = _tx (4.0, 0.0, &n);
      dp_ratesync_state_t *s
          = dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, m, 1024,
                                0.01, 0.707, RATESYNC_TED_GARDNER);
      DP_CHECK (x && s);
      if (!x || !s)
        {
          free (x);
          dp_ratesync_destroy (s);
          return;
        }
      DP_CHECK (s->loop.term_rate < (double)m); /* only a transient meets it */
      const double ctrl = (double)m + over[v] - s->loop.term_rate;
      size_t       most = 0, shared = 0;
      for (size_t i = 0; i < n; i++)
        {
          s->loop.ctrl    = ctrl; /* forced: the loop's own is overwritten */
          const size_t o0 = s->loop.out_count;
          float _Complex y;
          (void)ratesync_step (s, x[i], &y);
          const size_t o1 = s->loop.out_count, outs = o1 - o0;
          size_t       strobes = 0;
          for (size_t k = o0; k < o1; k++)
            strobes += k % m == 0;
          most = outs > most ? outs : most;
          shared += strobes > 1;
        }
      DP_CHECK_MSG (most == m, "the bound is reached, and never passed");
      DP_CHECK_MSG (shared == 0, "no input holds two strobes");
      if (most != m || shared)
        DP_TEST_ERR ("  steered rate m + %.1f: an input yielded up to %zu "
                     "outputs, %zu inputs held two strobes\n",
                     over[v], most, shared);
      dp_ratesync_destroy (s);
      free (x);
    }
}

/* The bound is never reached while tracking (doppler#3289): on a clean
 * stream at a clock offset, the steered rate stays under m once locked, so
 * the clamp costs tracking nothing. */
static void
test_tracking_stays_under_the_rate_bound (void)
{
  const size_t         m = 2;
  size_t               n = 0;
  float _Complex      *x = _tx (4.0, 0.37, &n);
  dp_ratesync_state_t *s
      = dp_ratesync_create (4.0, RATESYNC_PULSE_RRC, _BETA, _SPAN, m, 1024,
                            0.01, 0.707, RATESYNC_TED_GARDNER);
  DP_CHECK (x && s);
  if (!x || !s)
    {
      free (x);
      dp_ratesync_destroy (s);
      return;
    }
  double worst = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      float _Complex y;
      (void)ratesync_step (s, x[i], &y);
      const double steered = s->loop.term_rate + s->loop.ctrl;
      worst                = steered > worst ? steered : worst;
    }
  DP_CHECK_MSG (worst < (double)m, "tracking never reaches the rate bound");
  if (!(worst < (double)m))
    DP_TEST_ERR ("  steered rate reached %.4f against m %zu\n", worst, m);
  dp_ratesync_destroy (s);
  free (x);
}

int
main (void)
{
  test_invalid_params ();
  test_a_forced_control_never_shares_an_input ();
  test_tracking_stays_under_the_rate_bound ();
  test_owns_a_matched_cascade ();
  test_locks_across_planned_cascades ();
  test_tracks_a_clock_offset ();
  test_step_equals_steps ();
  test_reset ();
  test_state_roundtrip ();
  test_loop_refused_blob_changes_nothing ();
  test_prime_geometry ();
  test_two_outputs_per_input ();
  test_dttl_detector ();
  test_loop_without_the_object ();
  test_ctrl_scale_is_the_terminal_rate ();
  test_clipped_reports_overdrive ();
  test_iandd_locks_at_m2 ();
  test_loop_state_roundtrip ();
  test_telemetry_attach_is_atomic ();
  test_configure_semantics ();
  test_max_out ();
  test_sps_equals_m_boundary ();
  test_amplitude_law ();
  test_lock_time_hysteresis ();
  test_settle_time ();
  test_settle_time_from_an_offset ();
  test_seed ();
  test_lifecycle ();

  DP_TEST_END ("test_ratesync_core");
}
