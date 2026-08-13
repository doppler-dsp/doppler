/**
 * @file test_symsync_core.c
 * @brief Unit tests for the symbol-timing synchronizer (Gardner + DTTL TEDs).
 *
 * Tests:
 *   1. Lifecycle / order / init parity / reset reproducibility
 *   2. Lock across a range of static timing offsets -> zero BER
 *   3. Clock-rate (asynchronous) tracking -> zero BER + recovered rate
 *   4. All three interpolator orders lock
 *   5. Both TEDs (Gardner, DTTL) lock on a BPSK stream
 */
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include "symsync/symsync_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSYM 2000
#define SPS 4

/* Nyquist raised-cosine pulse (matched-filtered), unit symbol period T. */
static double
rc (double t, double beta, double T)
{
  double x = t / T;
  double s = (fabs (x) < 1e-9) ? 1.0 : sin (M_PI * x) / (M_PI * x);
  double d = 1.0 - (2.0 * beta * x) * (2.0 * beta * x);
  double c = (fabs (d) < 1e-9) ? M_PI / 4.0 : cos (M_PI * beta * x) / d;
  return s * c;
}

/* Build an RC-shaped BPSK signal at SPS samples/symbol with a fractional
 * timing `offset` (samples) and a clock-rate scale `rate`.  Fills bits[]. */
static size_t
make_signal (float complex *rx, int *bits, size_t nsym, double offset,
             double rate, uint32_t seed)
{
  size_t   n    = nsym * SPS;
  uint32_t st   = seed;
  double   beta = 0.35, T = SPS, span = 8 * SPS;
  for (size_t i = 0; i < n; i++)
    rx[i] = 0.0f;
  for (size_t k = 0; k < nsym; k++)
    {
      int b    = dp_bit (&st);
      bits[k]  = b; /* fill every bit so the BER tail is always valid */
      double c = (double)k * SPS * rate + offset;
      if (c + span >= (double)n)
        continue; /* signal ran out, but keep populating bits[] */
      long lo = (long)(c - span), hi = (long)(c + span);
      if (lo < 0)
        lo = 0;
      for (long i = lo; i <= hi && i < (long)n; i++)
        rx[i] += (float)(b * rc ((double)i - c, beta, T));
    }
  return n;
}

/* Ambiguity- and lag-tolerant BER over a clean, fully-locked middle window
 * (avoids the acquisition transient and the low-signal tail). */
static double
tail_ber (const float complex *sym, size_t nsym, const int *bits, size_t nbits)
{
  size_t lo_i = nsym / 4, hi_i = nsym - nsym / 4; /* middle half */
  double best = 1.0;
  for (int lag = -80; lag <= 80; lag++)
    {
      int err = 0, cnt = 0;
      for (size_t i = lo_i; i < hi_i; i++)
        {
          long j = (long)i + lag;
          if (j < 0 || j >= (long)nbits)
            continue;
          int dec = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
          err += (dec != bits[j]);
          cnt++;
        }
      if (cnt < 100)
        continue;
      double b = (double)err / cnt;
      if (b > 0.5)
        b = 1.0 - b; /* global inversion is don't-care */
      if (b < best)
        best = b;
    }
  return best;
}

int
main (void)
{
  float complex *rx   = malloc (NSYM * SPS * sizeof (*rx));
  int           *bits = malloc (NSYM * sizeof (int));
  float complex *sym  = malloc (NSYM * sizeof (*sym));

  /* 1. Lifecycle / order / reset reproducibility */
  {
    symsync_state_t *s
        = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (s != NULL);
    if (!s)
      return 1;
    DP_CHECK (s->farrow.order == FARROW_CUBIC);
    DP_CHECK (s->ted == SYMSYNC_TED_GARDNER);
    DP_CHECK (fabs (symsync_get_bn (s) - 0.01) < 1e-12);
    make_signal (rx, bits, NSYM, 1.3, 1.0, 3u);
    size_t k1 = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
    double r1 = symsync_get_rate (s);
    symsync_reset (s);
    size_t k2 = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
    DP_CHECK (k1 == k2);
    DP_CHECK (symsync_get_rate (s) == r1);
    symsync_destroy (s);
  }

  /* 2. Lock across static timing offsets */
  {
    for (int oi = 0; oi < 8; oi++)
      {
        double           off = oi * (double)SPS / 8.0;
        symsync_state_t *s   = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC,
                                               SYMSYNC_TED_GARDNER);
        make_signal (rx, bits, NSYM, off, 1.0, 7u);
        size_t k   = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        double ber = tail_ber (sym, k, bits, NSYM);
        DP_CHECK (ber == 0.0);
        symsync_destroy (s);
      }
  }

  /* 3. Clock-rate (asynchronous) tracking */
  {
    double rates[3] = { 1.0, 1.005, 0.995 };
    for (int ri = 0; ri < 3; ri++)
      {
        symsync_state_t *s = symsync_create (SPS, 0.005, 0.707, FARROW_CUBIC,
                                             SYMSYNC_TED_GARDNER);
        make_signal (rx, bits, NSYM, 1.3, rates[ri], 11u);
        size_t k   = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        double ber = tail_ber (sym, k, bits, NSYM);
        DP_CHECK (ber == 0.0);
        /* recovered samples/symbol tracks the true clock rate to ~1% */
        DP_CHECK (fabs (symsync_get_rate (s) - SPS * rates[ri]) < 0.05);
        symsync_destroy (s);
      }
  }

  /* 4. All three interpolator orders lock */
  {
    for (int order = 0; order <= 2; order++)
      {
        symsync_state_t *s
            = symsync_create (SPS, 0.01, 0.707, order, SYMSYNC_TED_GARDNER);
        make_signal (rx, bits, NSYM, 1.7, 1.0, 13u);
        size_t k = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        DP_CHECK (tail_ber (sym, k, bits, NSYM) == 0.0);
        symsync_destroy (s);
      }
  }

  /* 5. Both TEDs lock on a BPSK stream. DTTL's sign() decision device is
   * only valid for BPSK/QPSK-like independent I/Q rails; make_signal()'s
   * stream is real-valued BPSK so both TEDs are in their valid domain here.
   * DTTL's decision-directed detector gain differs from Gardner's smooth
   * product, so the same nominal bn does not yield the same loop bandwidth
   * across TEDs -- this only asserts lock (zero BER), not a bandwidth
   * target. */
  {
    int teds[2] = { SYMSYNC_TED_GARDNER, SYMSYNC_TED_DTTL };
    for (int ti = 0; ti < 2; ti++)
      {
        symsync_state_t *s
            = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, teds[ti]);
        make_signal (rx, bits, NSYM, 2.1, 1.0, 17u);
        size_t k = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        DP_CHECK (tail_ber (sym, k, bits, NSYM) == 0.0);
        symsync_destroy (s);
      }
  }

  /* symsync_init() (by-value, in place) produces a state byte-for-byte
   * identical to symsync_create()'s calloc + init — including a stack-embedded
   * target with arbitrary prior contents (symsync_init memsets first). The
   * whole-struct memcmp IS the init==create contract: identical state implies
   * identical behaviour. (A per-sample stream compare would be fragile here —
   * the compiler inlines symsync_step separately for the heap and stack
   * instances and may contract FMAs differently between the two, ~1 ULP, which
   * is a codegen artifact, not a state difference.) */
  {
    symsync_state_t *c
        = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_DTTL);
    /* poison the target so memset-or-not is actually exercised */
    symsync_state_t v;
    memset (&v, 0xFF, sizeof v);
    symsync_init (&v, SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_DTTL);
    DP_CHECK (memcmp (c, &v, sizeof *c)
              == 0); /* init == create, byte-for-byte */
    symsync_destroy (c);
  }

  free (rx);
  free (bits);
  free (sym);
  /* serializable state — pointer-free composition resumes whole-struct.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    float complex rx[256], sym[32];
    for (int i = 0; i < 256; i++)
      rx[i] = (float)(i % 8) - 4.0f + 0.3f * I;
    symsync_state_t *a
        = symsync_create (8, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_DTTL);
    symsync_state_t *b
        = symsync_create (8, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_DTTL);
    DP_CHECK (a != NULL && b != NULL);
    (void)symsync_steps (a, rx, 256, sym, 32);
    DP_STATE_ROUNDTRIP_TEST (symsync, a, b);
    DP_CHECK (b->timing.phase == a->timing.phase); /* nco child */
    DP_CHECK (b->last_error == a->last_error);
    DP_CHECK (b->ted == a->ted);
    symsync_destroy (a);
    symsync_destroy (b);
  }

  /* telemetry attach — five probes per recovered symbol; blobs stay
   * deterministic (attachment zeroed); a live attachment survives
   * set_state; detach reverts to the no-op path. */
  {
    float complex trx[512], tsym[160];
    for (int i = 0; i < 512; i++)
      trx[i] = ((i / 4) % 2 ? 1.0f : -1.0f) + 0.0f * I; /* BPSK, sps=4 */
    dp_tlm_t        *tlm = dp_tlm_create (1024);
    symsync_state_t *a
        = symsync_create (4, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (tlm != NULL && a != NULL);
    DP_CHECK (symsync_set_telemetry (a, tlm, "sync", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "sync.e") == a->tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "sync.freq") == a->tlm.id_freq);
    DP_CHECK (dp_tlm_probe_id (tlm, "sync.rate") == a->tlm.id_rate);
    DP_CHECK (dp_tlm_probe_id (tlm, "sync.lock") == a->tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "sync.locked") == a->tlm.id_locked);

    size_t n_sym = symsync_steps (a, trx, 512, tsym, 160);
    DP_CHECK (n_sym > 0);
    dp_tlm_rec_t recs[1024];
    size_t       n_rec = dp_tlm_read (tlm, 1024, recs, 1024);
    DP_CHECK (n_rec == 5 * n_sym); /* e + freq + rate + lock + locked */
    DP_CHECK (recs[n_rec - 1].probe == (uint16_t)a->tlm.id_locked);
    DP_CHECK (recs[n_rec - 1].value == (float)a->lock.locked);

    /* Blob determinism: attached vs detached serialize identically. */
    symsync_state_t *d
        = symsync_create (4, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (d != NULL);
    *d          = *a;
    d->tlm.ctx  = NULL;
    d->tlm.id_e = d->tlm.id_freq = d->tlm.id_rate = d->tlm.id_lock
        = d->tlm.id_locked                        = 0;
    uint8_t blob_a[sizeof (dp_state_hdr_t) + sizeof (symsync_state_t)];
    uint8_t blob_d[sizeof (blob_a)];
    DP_CHECK (symsync_state_bytes (a) == sizeof (blob_a));
    symsync_get_state (a, blob_a);
    symsync_get_state (d, blob_d);
    DP_CHECK (memcmp (blob_a, blob_d, sizeof (blob_a)) == 0);

    /* Restore into an attached instance keeps the live attachment. */
    dp_tlm_t        *tlm2 = dp_tlm_create (1024);
    symsync_state_t *b
        = symsync_create (4, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (tlm2 != NULL && b != NULL);
    DP_CHECK (symsync_set_telemetry (b, tlm2, "rx.sync", 1) == DP_OK);
    DP_CHECK (symsync_set_state (b, blob_a) == DP_OK);
    DP_CHECK (b->rate_est == a->rate_est);
    DP_CHECK (b->tlm.ctx == tlm2);

    /* Detach: no further records. */
    DP_CHECK (symsync_set_telemetry (a, NULL, "sync", 1) == DP_OK);
    (void)symsync_steps (a, trx, 512, tsym, 160);
    DP_CHECK (dp_tlm_read (tlm, 1024, recs, 1024) == 0);

    symsync_destroy (d);
    symsync_destroy (b);
    symsync_destroy (a);
    dp_tlm_destroy (tlm2);
    dp_tlm_destroy (tlm);
  }

  /* telemetry edge paths: the public per-sample step flushes when
   * attached; the attached-DTTL block loop emits; a full probe table
   * fails the attach whole. */
  {
    float complex trx2[64], tsym2[32];
    for (int i = 0; i < 64; i++)
      trx2[i] = ((i / 4) % 2 ? 1.0f : -1.0f) + 0.0f * I;
    dp_tlm_t        *tlm = dp_tlm_create (1024);
    symsync_state_t *a
        = symsync_create (4, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_DTTL);
    DP_CHECK (tlm != NULL && a != NULL);
    DP_CHECK (symsync_set_telemetry (a, tlm, "s", 1) == DP_OK);

    /* Attached DTTL block loop. */
    size_t       n_sym = symsync_steps (a, trx2, 64, tsym2, 32);
    dp_tlm_rec_t recs[256];
    DP_CHECK (dp_tlm_read (tlm, 256, recs, 256) == 5 * n_sym);

    /* Public single-sample step (dispatches + flushes when attached). */
    float complex y;
    size_t        n_step_sym = 0;
    for (int i = 0; i < 64; i++)
      if (symsync_step (a, trx2[i], &y))
        n_step_sym++;
    DP_CHECK (n_step_sym > 0);
    DP_CHECK (dp_tlm_read (tlm, 256, recs, 256) == 5 * n_step_sym);

    /* Fill the probe table; a fresh attach must fail whole and leave the
     * object detached. */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    symsync_state_t *c
        = symsync_create (4, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (c != NULL);
    DP_CHECK (symsync_set_telemetry (c, tlm, "full", 1) == DP_ERR_INVALID);
    DP_CHECK (c->tlm.ctx == NULL);

    symsync_destroy (c);
    symsync_destroy (a);
    dp_tlm_destroy (tlm);
  }

  /* 6. Lock detector: locks on a clean matched-filtered eye, does not
   * false-lock on noise. make_signal()'s pulse is truncated near the very
   * end of its buffer (no room left for the tail once c+span >= n), so
   * -- like tail_ber() above -- only feed a prefix well clear of that
   * truncated region; reading final state after processing the whole
   * (partly-silent) buffer would read a corrupted, not steady-state,
   * value. */
  {
    size_t nsym    = 4000;
    size_t measure = nsym - 100; /* well clear of the ~8-symbol
                                     (span/SPS) truncated tail */
    float complex *lrx   = malloc (nsym * SPS * sizeof (*lrx));
    int           *lbits = malloc (nsym * sizeof (*lbits));
    float complex *lsym  = malloc (nsym * sizeof (*lsym));

    make_signal (lrx, lbits, nsym, 1.3, 1.0, 13u);
    symsync_state_t *s
        = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    (void)symsync_steps (s, lrx, measure * SPS, lsym, nsym);
    DP_CHECK (symsync_get_locked (s) == 1);
    DP_CHECK (symsync_get_lock_stat (s) > 0.5); /* default threshold ~0.24 */
    symsync_destroy (s);

    uint32_t st = 9090u;
    for (size_t i = 0; i < nsym * SPS; i++)
      lrx[i] = dp_cgauss (&st);
    symsync_state_t *n
        = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    (void)symsync_steps (n, lrx, nsym * SPS, lsym, nsym);
    DP_CHECK (symsync_get_locked (n) == 0);
    symsync_destroy (n);

    free (lrx);
    free (lbits);
    free (lsym);
  }

  /* 7. configure_lock() derives (avgs, threshold) from (rolloff, esno_min,
   * pfa, pd); configure_lock_raw() is the escape hatch for direct control;
   * bad pfa/pd are rejected. */
  {
    symsync_state_t *s
        = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC, SYMSYNC_TED_GARDNER);
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 1e-3, 0.9) == DP_OK);
    DP_CHECK (s->avgs > 0);
    /* Pin the DERIVED VALUES, not merely that they are positive. `avgs > 0`
       passes on any formula at all, including a wrong one -- and this pair is
       the whole output of the sizing chain. At the defaults (rolloff 0.35,
       Es/N0 10 dB, pfa 1e-3, pd 0.9) the per-look mean is 0.4399539 and
       SYMSYNC_LOCK_STAT_VARIANCE is 1.343, giving: */
    DP_CHECK (s->avgs == 133);
    DP_CHECK (fabs (s->lock.up_thresh - 0.310985128970) < 1e-9);
    /* Sensitivity, so the pin above cannot be satisfied by constants: a
       stricter budget costs looks and raises the bar, a stronger signal
       (higher Es/N0 -> larger mean) buys both back. */
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 1e-6, 0.99) == DP_OK);
    DP_CHECK (s->avgs > 133);
    DP_CHECK (symsync_configure_lock (s, 0.35, 20.0, 1e-3, 0.9) == DP_OK);
    DP_CHECK (s->avgs < 133);
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 1e-3, 0.9) == DP_OK);
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 0.0, 0.9)
              == DP_ERR_INVALID);
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 1.0, 0.9)
              == DP_ERR_INVALID);
    DP_CHECK (symsync_configure_lock (s, 0.35, 10.0, 0.9, 0.9)
              == DP_ERR_INVALID); /* pd must exceed pfa */

    /* raw: an unreachable threshold never locks even on a strong signal. */
    size_t         nsym  = 4000;
    float complex *lrx   = malloc (nsym * SPS * sizeof (*lrx));
    int           *lbits = malloc (nsym * sizeof (*lbits));
    float complex *lsym  = malloc (nsym * sizeof (*lsym));
    make_signal (lrx, lbits, nsym, 1.3, 1.0, 13u);
    symsync_configure_lock_raw (s, 20, 100.0, 100.0, 1, 1);
    DP_CHECK (s->avgs == 20);
    (void)symsync_steps (s, lrx, (nsym - 100) * SPS, lsym, nsym);
    DP_CHECK (symsync_get_locked (s) == 0);
    symsync_destroy (s);
    free (lrx);
    free (lbits);
    free (lsym);
  }

  DP_TEST_END ("test_symsync_core");
}
