/**
 * @file test_mpsk_receiver_core.c
 * @brief Unit tests for the pulse-shaped M-PSK receiver.
 *
 * Tests:
 *   1. Lifecycle / argument validation / getters / reset reproducibility
 *   1b. Zero means derive: the five derived parameters and their readbacks
 *   2. Locks + recovers symbols under a carrier offset (I&D), every M -> SER 0
 *   3. RRC matched filter locks + recovers
 *   4. acq_to_track flips the loop from NDA acquisition to decision tracking
 *
 * The receiver is a matched DDC plus two loops now, so nothing here pins an
 * exact output: the matched filter is a polyphase bank rather than a dense FIR
 * and the interpolator is a bank arm rather than a Farrow. What IS pinned is
 * every property that must survive the rebuild -- symbol error rate, lock,
 * handover, reset reproducibility and the state round-trip.
 *
 * Amplitude is deliberately 0.5, not 1.0. A cascade that plans a CIC bounds
 * its input to +-1.0 and clips silently past it, costing ~25 dB of EVM that no
 * lock metric reveals; a unit-amplitude constellation plus noise sits right on
 * that edge. See mpsk_receiver_get_clipped().
 */
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define NSYM 6000
#define SPS 8.0
#define NSAMP ((size_t)(NSYM * (size_t)SPS))
/* Headroom under the CIC's +-1.0 input bound (see the file header). */
#define TX_AMP 0.5f
/* Terminal outputs per symbol: the old `n`, now the cascade's own. */
#define M_OUT 4

/* Constellation phase offset matching the mpsk convention (pi/4 for QPSK). */
static double
phi0_for (int m)
{
  return (m == 4) ? (M_PI / 4.0) : 0.0;
}

/* Build a rectangular-pulse (I&D-matched) M-PSK signal at SPS samples/symbol
 * with carrier offset `foff` (cycles/sample) and AWGN at `snr_db` (per-sample
 * Es/N0); fill tx[] (n=NSYM*SPS) and the per-symbol indices idx[]. Light noise
 * is realistic and breaks the measure-zero unstable-equilibrium the M-th-power
 * loop would otherwise sit at for a perfectly noiseless, zero-offset signal.
 */
static void
make_mpsk (float complex *tx, int *idx, int m, double foff, double snr_db,
           uint32_t seed)
{
  uint32_t st   = seed;
  double   phi0 = phi0_for (m);
  double   sigma
      = TX_AMP * sqrt (0.5 / pow (10.0, snr_db / 10.0)); /* per quad. */
  for (size_t k = 0; k < NSYM; k++)
    {
      int ki           = (int)(dp_xs32 (&st) & 0xFFFFu) % m;
      idx[k]           = ki;
      double        th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      float complex s  = TX_AMP * ((float)cos (th) + (float)sin (th) * I);
      for (size_t j = 0; j < (size_t)SPS; j++)
        {
          size_t        n  = k * (size_t)SPS + j;
          double        ph = 2.0 * M_PI * foff * (double)n;
          float complex c  = (float)cos (ph) + (float)sin (ph) * I;
          /* Sequenced: indeterminately-sequenced calls in one
             expression, and gcc and clang pick opposite orders. gcc's is
             pinned. */
          double        wi = sigma * dp_gauss (&st);
          double        wr = sigma * dp_gauss (&st);
          float complex w  = (float)wr + (float)wi * I;
          tx[n]            = s * c + w;
        }
    }
}

/* As make_mpsk(), but at an ARBITRARY `sps` -- including an irrational one.
 * Separate from make_mpsk() rather than a parameter on it because the symbol
 * boundary stops landing on the sample grid: the symbol index is
 * `floor(n / sps)`, so transitions fall BETWEEN samples and no resampling is
 * involved on the stimulus side. That is the whole point of the claim it
 * exists to test -- a generator that rounded sps to an integer would test
 * nothing. Writes `nsym * sps` samples (floor), and returns that count. */
static size_t
make_mpsk_sps (float complex *tx, int *idx, int m, double sps, size_t nsym,
               double foff, double snr_db, uint32_t seed)
{
  uint32_t st    = seed;
  double   phi0  = phi0_for (m);
  double   sigma = TX_AMP * sqrt (0.5 / pow (10.0, snr_db / 10.0));
  size_t   n     = (size_t)((double)nsym * sps);
  for (size_t k = 0; k < nsym; k++)
    idx[k] = (int)(dp_xs32 (&st) & 0xFFFFu) % m;
  for (size_t i = 0; i < n; i++)
    {
      size_t k = (size_t)((double)i / sps);
      if (k >= nsym)
        k = nsym - 1;
      double        th = 2.0 * M_PI * (double)idx[k] / (double)m + phi0;
      float complex s  = TX_AMP * ((float)cos (th) + (float)sin (th) * I);
      double        ph = 2.0 * M_PI * foff * (double)i;
      float complex c  = (float)cos (ph) + (float)sin (ph) * I;
      double        wi = sigma * dp_gauss (&st);
      double        wr = sigma * dp_gauss (&st);
      tx[i]            = s * c + ((float)wr + (float)wi * I);
    }
  return n;
}

/* Decide the constellation index of a recovered symbol (mpsk convention). */
static int
decide (float complex y, int m, double phi0)
{
  double th = atan2 ((double)cimagf (y), (double)crealf (y)) - phi0;
  long   k  = lround (th * (double)m / (2.0 * M_PI));
  return (int)((k % m + m) % m);
}

/* Symbol error rate over a locked middle window, tolerant of the unknown
 * M-fold rotation and a small symbol lag (acquisition transient + filter
 * delay). */
/* @p settle is the symbol at which the measurement may start --
 * dp_test_settle_syms() of the two loop bandwidths in use, NOT a fraction of
 * the record. A fraction is what this used to be (`nout / 3`), and at the
 * bandwidths section 2 runs (bn_timing 0.01, bn_carrier 0.005 -> a 3000-symbol
 * budget) `nout / 3` = 2000 began 1000 symbols INSIDE the joint acquisition
 * transient, scoring unsettled symbols against a steady-state threshold. */
static double
tail_ser (const float complex *out, size_t nout, const int *idx, int m,
          double phi0, size_t settle)
{
  if (settle + 400 >= nout)
    return 1.0; /* record too short to hold a settled window */
  size_t lo = settle, hi = nout - nout / 8;
  double best = 1.0;
  for (int lag = -40; lag <= 40; lag++)
    {
      for (int rot = 0; rot < m; rot++)
        {
          int err = 0, cnt = 0;
          for (size_t i = lo; i < hi; i++)
            {
              long j = (long)i + lag;
              if (j < 0 || j >= (long)NSYM)
                continue;
              int d = decide (out[i], m, phi0);
              if (((d - idx[j] - rot) % m + m) % m != 0)
                err++;
              cnt++;
            }
          if (cnt < 200)
            continue;
          double s = (double)err / cnt;
          if (s < best)
            best = s;
        }
    }
  return best;
}

/* Every construction in this file varies the same nine things and leaves the
 * rest at their documented defaults; spelling out fifteen positional arguments
 * each time buried which ones actually differ. */
static mpsk_receiver_state_t *
RX (int m, double sps, size_t m_out, int pulse, double bn_carrier,
    int acq_to_track, double lock_thresh, double init_norm_freq)
{
  /* The shipped defaults: the front-end AGC on, at the default ratio. */
  return mpsk_receiver_create (
      m, sps, m_out, pulse, 0.35, 8, bn_carrier, 0.707, 0.01, acq_to_track,
      lock_thresh, init_norm_freq, 0, MPSK_RX_NUM_PHASES,
      MPSK_RX_NDA_TAP_STROBE, 1, MPSK_RX_AGC_BW_RATIO);
}

int
main (void)
{
  float complex *tx  = malloc (NSAMP * sizeof (*tx));
  int           *idx = malloc (NSYM * sizeof (int));
  float complex *out = malloc (NSYM * sizeof (*out));

  /* 1. Lifecycle / validation / getters / reset reproducibility */
  {
    /* invalid args -> NULL */
    DP_CHECK (RX (3, SPS, M_OUT, 0, 0.01, 0, 0.5, 0.0) == NULL); /* bad m  */
    DP_CHECK (RX (4, SPS, 3, 0, 0.01, 0, 0.5, 0.0) == NULL);  /* m_out odd  */
    DP_CHECK (RX (4, SPS, 16, 0, 0.01, 0, 0.5, 0.0) == NULL); /* m_out > 8 */
    DP_CHECK (RX (4, 2.0, 4, 0, 0.01, 0, 0.5, 0.0)
              == NULL); /* sps < m_out: the terminal stage would interpolate */
    DP_CHECK (RX (4, 0.0, 4, 0, 0.01, 0, 0.5, 0.0) == NULL); /* sps == 0  */
    DP_CHECK (RX (4, SPS, M_OUT, 2, 0.01, 0, 0.5, 0.0)
              == NULL); /* bad pulse */

    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    if (!rx)
      return 1;
    DP_CHECK (mpsk_receiver_get_m (rx) == 4);
    DP_CHECK (mpsk_receiver_get_sps (rx) == SPS);
    DP_CHECK (mpsk_receiver_get_m_out (rx) == M_OUT);
    DP_CHECK (mpsk_receiver_get_tracking (rx) == 0);
    DP_CHECK (mpsk_receiver_get_clipped (rx) == 0); /* nothing pushed yet */

    make_mpsk (tx, idx, 4, 0.0008, 35.0, 99u);
    size_t k1 = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double f1 = mpsk_receiver_get_norm_freq (rx);
    mpsk_receiver_reset (rx);
    DP_CHECK (mpsk_receiver_get_tracking (rx) == 0);
    size_t k2 = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    DP_CHECK (k1 == k2);
    DP_CHECK (mpsk_receiver_get_norm_freq (rx)
              == f1); /* reset is reproducible */
    mpsk_receiver_destroy (rx);
  }

  /* 1b. Zero means derive, and every derived value is READ BACK.
     design/mpsk.md §8.1. That the object CONSTRUCTS proves nothing on its
     own: zero used to be a rejection, so "it built" is equally satisfied by a
     receiver that quietly kept the zero. The five readbacks are the whole
     mechanism by which a caller can check what was chosen, so the test that
     they exist has to be the test that they are right.

     The expected values are written as literals, not as a second call to
     mpsk_rx_derive_m_out() or a repeat of the MPSK_RX_*_DEFAULT arithmetic --
     an expectation computed by the code under test agrees with it by
     construction. At sps = 8 with an inclusive bound the rule reaches its
     cap. */
  {
    mpsk_receiver_state_t *d = mpsk_receiver_create (
        4, SPS, 0u, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.0, 0.01, 0, 0.0, 0.0,
        0, 0u, MPSK_RX_NDA_TAP_STROBE, 1, 0.0);
    DP_CHECK (d != NULL);
    if (d)
      {
        DP_CHECK (mpsk_receiver_get_m_out (d) == 8u);
        DP_CHECK (
            dp_near (mpsk_receiver_get_zeta (d), 0.70710678118654752, 1e-15));
        DP_CHECK (mpsk_receiver_get_num_phases (d) == 64u);
        DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (d), 0.4999, 1e-15));
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (d), 0.05, 1e-15));
        mpsk_receiver_destroy (d);
      }
    /* A supplied value still wins -- the derivation is a fallback, not a
       policy that overrides the caller. */
    mpsk_receiver_state_t *p = mpsk_receiver_create (
        4, SPS, 4u, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.9, 0.01, 0, 0.6, 0.0,
        0, 128u, MPSK_RX_NDA_TAP_STROBE, 1, 0.02);
    DP_CHECK (p != NULL);
    if (p)
      {
        DP_CHECK (mpsk_receiver_get_m_out (p) == 4u);
        DP_CHECK (dp_near (mpsk_receiver_get_zeta (p), 0.9, 1e-15));
        DP_CHECK (mpsk_receiver_get_num_phases (p) == 128u);
        DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (p), 0.6, 1e-15));
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (p), 0.02, 1e-15));
        mpsk_receiver_destroy (p);
      }
  }

  /* 1c. The continuous flavor pins the gating, and it STAYS pinned.
     docs/design/mpsk.md §2.1. Asserting the construct-time values is only
     half of it: `acq_to_track` is the handover, so the claim "there is no
     handover" is a claim about a receiver that has RUN, on a signal good
     enough that a handover-enabled twin would have taken it. Both are
     checked below, and the twin is the control -- without it, `tracking == 0`
     is equally satisfied by a signal that simply never locked. */
  {
    mpsk_receiver_state_t *c = mpsk_receiver_create_continuous (
        2, SPS, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.02, 0.01, 0.0, 1);
    DP_CHECK (c != NULL);
    if (c)
      {
        /* Pinned at construction. */
        DP_CHECK (c->l.acq_to_track == 0);
        DP_CHECK (c->l.nda_tap == MPSK_RX_NDA_TAP_STROBE);
        DP_CHECK (c->l.tap_timed == 1); /* strobe is the one timed tap */
        /* THE reason the tap is pinned here, asserted as the property rather
           than as the enum: the discriminator's clock IS the symbol clock.
           docs/design/mpsk.md §2.1 lists three defects that follow from a
           tap faster than Rs -- a lock EMA whose alpha is per-update, and two
           lockdets carrying the same verify counts on different clocks -- and
           claims Mode 1 cannot have them. That is only true at an update rate
           of exactly 1. This flavor shipped pinning `mf_in` (bank_sps) and
           the second defect duly arrived: the lock statistic settled below a
           threshold derived per-M, so the one metric a gate-free receiver
           still reports read as a permanent no-lock (doppler#791).
           An enum check alone would survive a tap being swapped for another
           fast one; this does not. */
        DP_CHECK (dp_near (mpsk_rx_updates_per_symbol (&c->l), 1.0, 1e-15));
        DP_CHECK (c->fe->rc->agc != NULL); /* the AGC is not optional */
        /* Derived, not defaulted -- the same five §8.1 rows as 1b. */
        DP_CHECK (mpsk_receiver_get_m_out (c) == 8u);
        DP_CHECK (
            dp_near (mpsk_receiver_get_zeta (c), 0.70710678118654752, 1e-15));
        DP_CHECK (mpsk_receiver_get_num_phases (c) == 64u);
        DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (c), 0.4999, 1e-15));
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (c), 0.05, 1e-15));

        /* It runs, it locks, and it never hands over. */
        make_mpsk (tx, idx, 2, 0.0008, 30.0, 21u);
        size_t n = mpsk_receiver_steps (c, tx, NSAMP, out, NSYM);
        DP_CHECK (n > 0);
        DP_CHECK (mpsk_receiver_get_lock (c) > 0.5);    /* it did lock */
        DP_CHECK (mpsk_receiver_get_tracking (c) == 0); /* and never flipped */
        DP_CHECK (tail_ser (out, n, idx, 2, phi0_for (2),
                            dp_test_settle_syms (0.02, 0.01))
                  < 0.01);

        /* The control: the SAME stimulus through a handover-enabled receiver
           DOES flip. Without this the assertion above is vacuous -- it would
           pass on a receiver that never locked, and on one whose handover was
           simply broken. */
        mpsk_receiver_state_t *h
            = RX (2, SPS, 8, MPSK_RX_PULSE_IANDD, 0.02, 1, 0.65, 0.0);
        DP_CHECK (h != NULL);
        if (h)
          {
            (void)mpsk_receiver_steps (h, tx, NSAMP, out, NSYM);
            DP_CHECK (mpsk_receiver_get_tracking (h) == 1);
            mpsk_receiver_destroy (h);
          }
        mpsk_receiver_destroy (c);
      }
  }

  /* 1d. The validation the header states and nothing checked.
     `num_phases` "a power of two" and `bn_agc_ratio` "must be in (0, 1);
     construction refuses 1 or above rather than warning". Both were prose
     here -- the real twin pins the ratio, this one did not, which is the
     asymmetry a claim inventory is for. Zero is deliberately NOT in this
     list: for both of these it is the derive request (§8.1), which 1b
     covers, and asserting a reject for it would contradict that.

     These pin the CONTRACT, and the two halves are not equally deep --
     established by sabotage rather than assumed:

       - deleting `mpsk_receiver_create`'s `bn_agc_ratio` guard takes both
         ratio lines below RED. That guard is the sole enforcer.
       - deleting its `num_phases` power-of-two guard changes NOTHING: these
         still refuse, because `RateConverter_core.c:830` carries the same
         check and the front end is built before the loops. The receiver's
         copy is fail-fast (it produces the named `create_error_message`
         instead of a bare NULL from a composed core), not the enforcement.

     Worth writing down because a reject test is exactly where a redundant
     guard hides: the assertion is true either way, so nothing distinguishes
     "this guard works" from "something else catches it" without the
     sabotage. */
  {
    /* num_phases: a power of two, >= 2. */
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 0, 3u,
                                    MPSK_RX_NDA_TAP_STROBE, 1, 0.05)
              == NULL); /* 3 is not a power of two */
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 0, 1u,
                                    MPSK_RX_NDA_TAP_STROBE, 1, 0.05)
              == NULL); /* 1 is a power of two but below the floor of 2 */
    /* bn_agc_ratio: strictly inside (0, 1). At 1 the AGC is exactly as fast
       as a loop it feeds; past that it is faster, and two level-correcting
       loops at the same speed integrate against each other. */
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 0, 64u,
                                    MPSK_RX_NDA_TAP_STROBE, 1, 1.0)
              == NULL);
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 0, 64u,
                                    MPSK_RX_NDA_TAP_STROBE, 1, -0.05)
              == NULL);
    /* Non-vacuity: the SAME call with only the offending argument made legal
       must construct, or every line above passes for the wrong reason. */
    mpsk_receiver_state_t *ok = mpsk_receiver_create (
        4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 0, 0.5,
        0.0, 0, 64u, MPSK_RX_NDA_TAP_STROBE, 1, 0.05);
    DP_CHECK (ok != NULL);
    mpsk_receiver_destroy (ok);
  }

  /* 1e. An IRRATIONAL sps is no harder than an integer one.
     The header's headline claim -- "a complete inline modem ... at **any**
     input rate", "17.33389 is equally valid", "because the terminal
     accumulator is a double and the loop only has to steer the strobe" --
     and every test in this file ran at sps = 8.0, so nothing had ever
     exercised a symbol boundary that does not land on the sample grid.
     17.33389 is the header's own example, used deliberately. */
  {
    const double sps_odd = 17.33389;
    size_t       nsym    = (size_t)((double)NSAMP / sps_odd) - 4;
    size_t n = make_mpsk_sps (tx, idx, 4, sps_odd, nsym, 0.0005, 30.0, 77u);
    mpsk_receiver_state_t *rx
        = RX (4, sps_odd, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    if (rx)
      {
        size_t k = mpsk_receiver_steps (rx, tx, n, out, NSYM);
        /* The output count is the integral of the rate, not a rounded sps:
           an implementation that truncated sps to 17 would emit ~2% more
           symbols over this record, which is hundreds. */
        double expect = (double)n / sps_odd;
        DP_CHECK (fabs ((double)k - expect) < 0.02 * expect);
        DP_CHECK (mpsk_receiver_get_sps (rx) == sps_odd); /* stored exactly */
        DP_CHECK (mpsk_receiver_get_lock (rx) > 0.5);
        DP_CHECK (tail_ser (out, k, idx, 4, phi0_for (4),
                            dp_test_settle_syms (0.01, 0.01))
                  < 0.02);
        mpsk_receiver_destroy (rx);
      }
  }

  /* 1f. The stable FALSE lock at `df = k*F/M`, pinned as behaviour.
     design/mpsk.md §2.1 calls this Mode 1's "one quiet failure" and §3.5
     measures it: F/M is exactly where an M-th power at update rate F aliases
     onto zero, so the loop sits still and reports a HEALTHY statistic on a
     stationary constellation that no self-referenced metric can flag.

     This is a documented defect, not a bug to fix here, so the test pins the
     behaviour rather than asserting it away -- if it ever changes, that is a
     result worth seeing rather than a silent one. The value of pinning it is
     that `lock` alone is proven insufficient: the assertion is that a high
     lock statistic COEXISTS with a completely wrong frequency. */
  {
    /* Strobe tap: F = Rs, so the alias sits at Rs/M = 1/(M*sps) cyc/sample.
       QPSK at sps = 8 -> 0.03125. */
    const double alias = 1.0 / (4.0 * SPS);
    make_mpsk (tx, idx, 4, alias, 40.0, 5u);
    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    if (rx)
      {
        (void)mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
        double f  = mpsk_receiver_get_norm_freq (rx);
        double lk = mpsk_receiver_get_lock (rx);
        /* It did NOT find the true offset -- it is parked at the alias. */
        DP_CHECK (fabs (f - alias) > 0.5 * alias);
        /* ...while reporting a lock statistic a caller would trust. THIS is
           the finding: the two together are what no self-referenced metric
           can separate. */
        DP_CHECK (lk > 0.5);
        mpsk_receiver_destroy (rx);
      }
  }

  /* 2. Lock + recover under a carrier offset (I&D), every M -> SER 0 */
  {
    int    ms[3] = { 2, 4, 8 };
    double fs[2] = { 0.0, 0.001 };
    for (int mi = 0; mi < 3; mi++)
      for (int fi = 0; fi < 2; fi++)
        {
          int m = ms[mi];
          /* 8PSK hands the carrier over to the decision-directed loop; the
             other orders stay in NDA the whole way. Its decision margin is
             only +-pi/8, so the M-th-power discriminator's own phase jitter
             is the dominant error term -- the same call the BER validation
             (mpsk_receiver_ber.c) and the Python suite both make. */
          mpsk_receiver_state_t *rx = RX (m, SPS, M_OUT, MPSK_RX_PULSE_IANDD,
                                          0.005, m == 8, 0.3, fs[fi]);
          make_mpsk (tx, idx, m, fs[fi], 30.0, 7u + (uint32_t)(mi * 4 + fi));
          size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
          double ser = tail_ser (out, k, idx, m, phi0_for (m),
                                 dp_test_settle_syms (0.01, 0.005));
          DP_CHECK (ser < 0.01); /* clean recovery       */
          /* The lock EMA's noise-only sd is CARRIER_NDA_LOCK_NORM_SD (0.1132)
             at EVERY m, so a threshold is meaningfully stated in sigmas. The
             shipped default lock_thresh of 0.5 is 4.42 sigma (per-look Pfa
             5e-6); assert that here rather than the old 0.15, which was only
             1.3 sigma -- a value a noise-only run reaches routinely. */
          DP_CHECK (mpsk_receiver_get_lock (rx) > 0.5);
          mpsk_receiver_destroy (rx);
        }
  }

  /* 3. RRC matched filter locks + recovers (QPSK). A rectangular signal
   * through the RRC matched filter still acquires + recovers (the loop is
   * pulse-robust; the Python suite drives a true RRC-shaped TX). */
  {
    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_RRC, 0.005, 0, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    make_mpsk (tx, idx, 4, 0.0, 30.0, 21u);
    size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                           dp_test_settle_syms (0.01, 0.005));
    DP_CHECK (ser < 0.02);
    mpsk_receiver_destroy (rx);
  }

  /* 4. acq_to_track flips NDA acquisition -> decision-directed tracking */
  {
    /* bn_carrier 0.01, not 0.005: this case seeds the LO at 0 and makes the
       loop ACQUIRE 0.0005 cyc/sample (0.004*Rs), and carrier pull-in range
       scales with the loop bandwidth. At 0.005 the loop reaches the right
       frequency but slips -- lock still reads a healthy +0.64 while EVM sits
       at -8.1 dB against the -18.3 dB the wider loops give, which is exactly
       why a lock statistic is never read on its own here. */
    mpsk_receiver_state_t *rx
        /* 0.65, not the 0.4 this used before the lock statistic was
           normalised: the statistic now reads ~1.0 at lock for EVERY M
           instead of the old per-M 1/0.619/0.412, so a QPSK threshold of
           0.4 used to mean 0.4/0.619 = 65% of the achievable ceiling and
           would now mean 40% of it. Rescaling here keeps this test at the
           same OPERATING POINT so it still measures the handover rather
           than the units change. */
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 1, 0.65, 0.0);
    make_mpsk (tx, idx, 4, 0.0005, 30.0, 33u);
    size_t k = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    DP_CHECK (mpsk_receiver_get_tracking (rx) == 1); /* handed over */
    double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                           dp_test_settle_syms (0.01, 0.01));
    DP_CHECK (ser < 0.01);

    /* two-way: a sustained lock loss (noise-dominated input collapses the
       lock EMA below the 0.8x drop threshold for 32 straight symbols)
       falls back to the NDA acquisition steer, and a returning signal
       hands over again. During the outage both discriminators see only
       noise, so the NCO random-walks — possibly beyond the NDA pull-in
       range onto an M-th-power alias grid point. Recovering from THAT is
       acquisition's job, so the test does what a real receiver does on a
       drop-back: re-seed the carrier from the (still valid) acquisition
       estimate before the signal returns. */
    make_mpsk (tx, idx, 4, 0.0005, -10.0, 44u);
    (void)mpsk_receiver_steps (rx, tx, (NSAMP / 10), out, NSYM);
    DP_CHECK (mpsk_receiver_get_tracking (rx) == 0); /* dropped back */
    mpsk_receiver_set_norm_freq (rx, 0.0005);        /* acq re-seed */
    make_mpsk (tx, idx, 4, 0.0005, 30.0, 45u);
    (void)mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    DP_CHECK (mpsk_receiver_get_tracking (rx) == 1); /* re-declared */
    mpsk_receiver_destroy (rx);
  }

  free (tx);
  free (idx);
  free (out);
  /* serializable state — carrier_nda + symsync + MF children resume.
   * (Moved above the final _fails check: this block used to sit after it,
   * so its own failures could never fail the test.) */
  {
    float complex tx[256], out[32];
    for (int i = 0; i < 256; i++)
      tx[i] = (float)(i % 4) - 2.0f + 0.1f * I;
    mpsk_receiver_state_t *a
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    mpsk_receiver_state_t *b
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (a != NULL && b != NULL);
    (void)mpsk_receiver_steps (a, tx, 256, out, 32);
    DP_STATE_ROUNDTRIP_TEST (mpsk_receiver, a, b);
    DP_CHECK (b->l.sym_count == a->l.sym_count);
    /* the timing loop's strobe phase is the child that must resume */
    DP_CHECK (b->l.timing.out_count == a->l.timing.out_count);
    DP_CHECK (b->l.timing.prime_left == a->l.timing.prime_left);
    DP_CHECK (b->l.sym_rot == a->l.sym_rot);
    mpsk_receiver_destroy (a);
    mpsk_receiver_destroy (b);
  }

  /* telemetry attach — the receiver's lock + tracking probes + the
   * forwarded carrier (incl. its arm AGC) and symsync probes: ten
   * records per emitted symbol plus the AGC's amortized gain records;
   * detach cascades. */
  {
    float complex tx[512], out[80];
    for (int i = 0; i < 512; i++)
      tx[i] = ((i / 8) % 2 ? 1.0f : -1.0f) + 0.0f * I; /* BPSK, sps=8 */
    dp_tlm_t              *tlm = dp_tlm_create (4096);
    mpsk_receiver_state_t *a
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (tlm != NULL && a != NULL);
    DP_CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.lock") == a->l.tlm.id_lock);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.tracking") == a->l.tlm.id_tracking);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.car.e") == a->l.tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.car.freq") == a->l.tlm.id_freq);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.car.locked") == a->l.tlm.id_locked);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.sync.e") == a->l.timing.tlm.id_e);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.sync.locked")
              == a->l.timing.tlm.id_locked);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.sync.mu") == a->l.timing.tlm.id_mu);
    /* The front end's AGC is the third loop, forwarded under "rx.agc". It was
       the only one of the three emitting nothing, which made its settling the
       one thing a caller had to infer -- and by mpsk_rx_agc_bn() it is the
       SLOWEST of the three, so it is what sets the receiver's warmup. */
    int id_agc_gain = dp_tlm_probe_id (tlm, "rx.agc.gain_db");
    int id_agc_lvl  = dp_tlm_probe_id (tlm, "rx.agc.level_db");
    DP_CHECK (id_agc_gain >= 0 && id_agc_lvl >= 0);
    /* 14 with the AGC pair: lock, tracking, car(e, freq, nco, locked),
       sync(e, ctrl, rate, lock, locked, mu), agc(gain_db, level_db).
       `car.nco` is the SUM driving the LO; `car.freq` is the integrator
       alone. Both are published because on a ramp they differ by the
       proportional term, and only the sum is the applied frequency. */
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.car.nco") >= 0);
    DP_CHECK (dp_tlm_probe_count (tlm) == 14);

    size_t n_sym = mpsk_receiver_steps (a, tx, 512, out, 80);
    DP_CHECK (n_sym > 0);
    dp_tlm_rec_t recs[2048];
    size_t       n_rec = dp_tlm_read (tlm, 2048, recs, 2048);
    /* lock + tracking + car(e,freq,nco,locked) + sync(e,ctrl,rate,lock,
     * locked,mu): twelve records per recovered symbol, all flushed at the
     * strobe. The arm
     * AGC is not attached -- it is an internal normaliser on the
     * discriminator's input, not a receiver diagnostic. The FRONT-END AGC is,
     * and it is deliberately NOT on the symbol grid: it sits pre-terminal in
     * the cascade and emits per gain-update event, so its two probes
     * contribute a count of their own. */
    size_t n_agc = 0;
    for (size_t i = 0; i < n_rec; i++)
      if (recs[i].probe == (uint16_t)id_agc_gain
          || recs[i].probe == (uint16_t)id_agc_lvl)
        n_agc++;
    DP_CHECK (n_agc > 0);          /* the forward actually reaches the AGC */
    DP_CHECK (n_agc % 2 == 0);     /* both probes emit together, always    */
    DP_CHECK (n_agc / 2 != n_sym); /* a cascade grid, not the symbol grid  */
    DP_CHECK (n_rec == 12 * n_sym + n_agc);

    /* `mu` is the timing NCO's phase, so it is a FRACTION: every record must
       land in [0, 1) whatever the loop is doing, and it must actually vary
       (a frozen mu would mean the steering never reached the accumulator). */
    {
      int    n_mu = 0;
      double mn = 2.0, mx = -1.0;
      for (size_t i = 0; i < n_rec; i++)
        if (recs[i].probe == (uint16_t)a->l.timing.tlm.id_mu)
          {
            double v = (double)recs[i].value;
            DP_CHECK (v >= 0.0 && v < 1.0);
            mn = v < mn ? v : mn;
            mx = v > mx ? v : mx;
            n_mu++;
          }
      DP_CHECK (n_mu == (int)n_sym);
      DP_CHECK (mx > mn);
    }

    /* Detach cascades to both embedded loops (and the AGC). */
    DP_CHECK (mpsk_receiver_set_telemetry (a, NULL, "rx", 1) == DP_OK);
    DP_CHECK (a->l.tlm.ctx == NULL && a->l.timing.tlm.ctx == NULL);
    (void)mpsk_receiver_steps (a, tx, 512, out, 80);
    DP_CHECK (dp_tlm_read (tlm, 2048, recs, 2048) == 0);

    /* bits() flushes telemetry too (the guarded in-loop path). */
    DP_CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx2", 1) == DP_OK);
    uint8_t bit_out[128];
    size_t  n_bits = mpsk_receiver_bits (a, tx, 512, bit_out, 128);
    DP_CHECK (n_bits > 0);
    DP_CHECK (dp_tlm_read (tlm, 2048, recs, 2048) > 0);

    /* A full probe table fails the attach whole (receiver detached). */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    mpsk_receiver_state_t *b
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (b != NULL);
    DP_CHECK (mpsk_receiver_set_telemetry (b, tlm, "full", 1)
              == DP_ERR_INVALID);
    DP_CHECK (b->l.tlm.ctx == NULL);

    /* Partial registration failure unwinds: leave exactly six slots — the
     * receiver's own five probes fit, the six-probe timing forward cannot,
     * and the whole attach fails with everything detached again. */
    dp_tlm_t *tlm2 = dp_tlm_create (256);
    DP_CHECK (tlm2 != NULL);
    for (size_t i = 0;
         dp_tlm_probe_count (tlm2) < (size_t)(DP_TLM_MAX_PROBES - 6); i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm2, pname, 1);
      }
    DP_CHECK (mpsk_receiver_set_telemetry (b, tlm2, "uw", 1)
              == DP_ERR_INVALID);
    DP_CHECK (b->l.tlm.ctx == NULL && b->l.timing.tlm.ctx == NULL);
    dp_tlm_destroy (tlm2);

    /* The unwind one step further out: leave exactly ELEVEN slots, so both
       loops attach and only the AGC forward cannot. "Fails whole" has to mean
       the loops are rolled back too, or a caller who checked the return value
       would still be quietly emitting eleven of thirteen probes. */
    dp_tlm_t *tlm3 = dp_tlm_create (256);
    DP_CHECK (tlm3 != NULL);
    for (size_t i = 0;
         dp_tlm_probe_count (tlm3) < (size_t)(DP_TLM_MAX_PROBES - 11); i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm3, pname, 1);
      }
    DP_CHECK (mpsk_receiver_set_telemetry (b, tlm3, "uw2", 1)
              == DP_ERR_INVALID);
    DP_CHECK (b->l.tlm.ctx == NULL && b->l.timing.tlm.ctx == NULL);
    /* And nothing emits: the rollback is real, not just a flag. */
    (void)mpsk_receiver_steps (b, tx, 512, out, 80);
    DP_CHECK (dp_tlm_read (tlm3, 2048, recs, 2048) == 0);
    dp_tlm_destroy (tlm3);

    /* With agc = 0 there is no third loop to attach: eleven probes, and the
       attach still succeeds -- a caller should not have to know how the
       receiver was constructed to avoid an error. */
    mpsk_receiver_state_t *noagc = mpsk_receiver_create (
        2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 0, 0.5,
        0.0, 0, MPSK_RX_NUM_PHASES, MPSK_RX_NDA_TAP_STROBE, 0,
        MPSK_RX_AGC_BW_RATIO);
    dp_tlm_t *tlm4 = dp_tlm_create (4096);
    DP_CHECK (noagc != NULL && tlm4 != NULL);
    if (noagc && tlm4)
      {
        DP_CHECK (mpsk_receiver_set_telemetry (noagc, tlm4, "rx", 1) == DP_OK);
        DP_CHECK (dp_tlm_probe_count (tlm4) == 12); /* 14 less the AGC pair */
        DP_CHECK (dp_tlm_probe_id (tlm4, "rx.agc.gain_db") < 0);
      }
    dp_tlm_destroy (tlm4);
    mpsk_receiver_destroy (noagc);

    mpsk_receiver_destroy (b);
    mpsk_receiver_destroy (a);
    dp_tlm_destroy (tlm);
  }

  /* 9. The one AGC is slower than every loop it feeds, at any configuration.
   *
   * Not a style rule: an AGC divides out the amplitude the discriminators are
   * built around, so one running near a loop's bandwidth corrects excursions
   * that loop is itself producing and the two integrate against each other.
   * The MINIMUM of the two bandwidths is the load-bearing part — a receiver
   * with bn_timing far below bn_carrier is exactly the case a carrier-only
   * ratio gets wrong. */
  {
    const double bns[][2] = {
      { 0.01, 0.01 },  /* the defaults                             */
      { 0.05, 0.001 }, /* timing far slower — the case that bites  */
      /* Sharp: a carrier-only ratio gives 0.01*0.05 = 5e-4, which is FASTER
         than this bn_timing — so the `< both` check below is the thing that
         fires, not merely the exact-value one. */
      { 0.05, 0.0004 },
      { 0.001, 0.05 }, /* carrier far slower                       */
      { 0.02, 0.02 },
    };
    for (size_t i = 0; i < sizeof (bns) / sizeof (bns[0]); i++)
      {
        double                 bn_c = bns[i][0], bn_t = bns[i][1];
        mpsk_receiver_state_t *rx = mpsk_receiver_create (
            4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, bn_c, 0.707, bn_t, 0,
            0.5, 0.0, 0, MPSK_RX_NUM_PHASES, MPSK_RX_NDA_TAP_STROBE, 1,
            MPSK_RX_AGC_BW_RATIO);
        DP_CHECK (rx != NULL);
        if (!rx)
          continue;
        /* The receiver's ONE AGC, in the front-end cascade; bn is per symbol
           on both sides of the comparison. */
        double bn_agc = rx->fe->rc->agc_bn_sym;
        DP_CHECK (bn_agc < bn_c && bn_agc < bn_t);
        /* And it is the ratio, off the slowest — not merely "smaller". */
        double slowest = bn_c < bn_t ? bn_c : bn_t;
        DP_CHECK (fabs (bn_agc - MPSK_RX_AGC_BW_RATIO * slowest)
                  < 1e-15 * slowest + 1e-18);
        mpsk_receiver_destroy (rx);
      }
  }

  /* 10. A blob taken while the front-end AGC is still converging resumes
   * with that convergence intact.
   *
   * The AGC's gain integrator and detector EMA only look different from
   * their settled values while the loop is walking, so a split at n/2 --
   * what every other round-trip here does -- would resume a converged loop
   * and prove very little. This one cuts early, and reaches that state
   * through the whole nesting: receiver -> ddc -> RateConverter -> agc. */
  {
    /* Own buffers: tx/idx/out above are already freed by this point. */
    float complex *stx  = malloc (NSAMP * sizeof (*stx));
    int           *sidx = malloc (NSYM * sizeof (int));
    make_mpsk (stx, sidx, 4, 0.0, 30.0, 11u);
    float complex         *tx = stx; /* keep the body reading naturally */
    mpsk_receiver_state_t *a
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
    DP_CHECK (a != NULL);
    if (a)
      {
        /* Enough input for the loop to move off unity, nowhere near
         * enough for it to settle. */
        float complex y[512];
        size_t        n_pre = (size_t)SPS * 200u;
        (void)mpsk_receiver_steps (a, tx, n_pre, y, 512);
        DP_CHECK (a->fe->rc->agc != NULL);
        /* Non-vacuous: the gain is genuinely mid-flight at the split. */
        DP_CHECK (mpsk_receiver_get_agc_gain_db (a) != 0.0);

        size_t   nb   = mpsk_receiver_state_bytes (a);
        uint8_t *blob = malloc (nb);
        mpsk_receiver_get_state (a, blob);

        mpsk_receiver_state_t *b
            = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0);
        DP_CHECK (b != NULL && mpsk_receiver_set_state (b, blob) == DP_OK);

        /* Resume both on the same remainder; every symbol must match bit for
         * bit, which it cannot if the in-flight seed mean was lost. */
        float complex ya[512], yb[512];
        size_t        na
            = mpsk_receiver_steps (a, tx + n_pre, NSAMP - n_pre, ya, 512);
        size_t nc
            = mpsk_receiver_steps (b, tx + n_pre, NSAMP - n_pre, yb, 512);
        DP_CHECK (na == nc && na > 0);
        int same = 1;
        for (size_t i = 0; i < na && i < nc; i++)
          if (memcmp (&ya[i], &yb[i], sizeof ya[i]) != 0)
            same = 0;
        DP_CHECK (same);

        free (blob);
        mpsk_receiver_destroy (b);
        mpsk_receiver_destroy (a);
      }
    free (stx);
    free (sidx);
  }

  /* 11. The receiver is LEVEL-INVARIANT with the AGC on, and demonstrably
   * not without it. This is the whole point of the change, so it is asserted
   * on the thing a user cares about (does it acquire) rather than on a gain.
   *
   * Both halves matter. The `agc=1` half is the claim; the `agc=0` half keeps
   * it honest -- if the receiver were level-invariant anyway (say the
   * discriminator's own |z|^M normalisation were carrying it), the first
   * assertion would pass for a reason that has nothing to do with the AGC,
   * and this gate would be measuring nothing. */
  {
    static const double amps[]  = { 0.25, 1.0, 4.0 };
    double              gain[3] = { 0, 0, 0 };
    size_t              nsym[3] = { 0, 0, 0 };
    for (int use_agc = 1; use_agc >= 0; use_agc--)
      {
        for (size_t a = 0; a < 3; a++)
          {
            float complex *sx = malloc (NSAMP * sizeof (*sx));
            int           *si = malloc (NSYM * sizeof (int));
            float complex *so = malloc (NSYM * sizeof (*so));
            make_mpsk (sx, si, 4, 0.0, 20.0, 3u);
            for (size_t i = 0; i < NSAMP; i++)
              sx[i] *= (float)amps[a];

            mpsk_receiver_state_t *rx = mpsk_receiver_create (
                4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01,
                0, 0.5, 0.0, 0, MPSK_RX_NUM_PHASES, MPSK_RX_NDA_TAP_STROBE,
                use_agc, MPSK_RX_AGC_BW_RATIO);
            DP_CHECK (rx != NULL);
            if (rx)
              {
                size_t n = mpsk_receiver_steps (rx, sx, NSAMP, so, NSYM);
                DP_CHECK (n > 0);
                if (use_agc)
                  {
                    gain[a] = mpsk_receiver_get_agc_gain_db (rx);
                    nsym[a] = n;
                    /* SER 0 at every level, not merely "it ran". */
                    DP_CHECK (tail_ser (so, n, si, 4, phi0_for (4), 400)
                              == 0.0);
                  }
                else
                  {
                    /* agc=0 is the bisect handle: no gain, ever. */
                    DP_CHECK (mpsk_receiver_get_agc_gain_db (rx) == 0.0);
                  }
                mpsk_receiver_destroy (rx);
              }
            free (sx);
            free (si);
            free (so);
          }
      }
    /* Same symbol count at every level -- the receiver did the same work. */
    DP_CHECK (nsym[0] == nsym[1] && nsym[1] == nsym[2]);
    /* And the AGC is what made the levels agree: its gain tracked the input
       across the full 24 dB, which is the non-vacuous half. */
    DP_CHECK (gain[0] - gain[1] > 10.0 && gain[1] - gain[2] > 10.0);
  }

  DP_TEST_END ("test_mpsk_receiver_core");
}
