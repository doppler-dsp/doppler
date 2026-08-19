/**
 * @file test_mpsk_receiver_core.c
 * @brief Unit tests for the pulse-shaped M-PSK receiver.
 *
 * Tests:
 *   1. Lifecycle / argument validation / getters / reset reproducibility
 *   1b. Zero means derive: the five derived parameters and their readbacks
 *   2. Locks + recovers symbols under a carrier offset (I&D), every M -> SER 0
 *   3. RRC matched filter locks + recovers
 *   4. The estimate the NDA steer builds survives a lock edge, both ways
 *   ...
 *   15-21. The same object through its REAL-input constructor
 *   22. Telemetry reaches the real front end's AGC
 *   23. The LO runs at HALF the input rate (the ramp, and the readback)
 *
 * **One object, two faces, one test home.** Sections 1-14 drive the complex
 * front end and 15-23 the real one, because they are the same receiver behind
 * a different matched front end (docs/design/mpsk.md §12). That is not
 * an organisational choice: the shared header mpsk_rx_loops.h has no test
 * file of its own, so while the two faces were two types its claims were
 * pinned only where one of the two tests happened to reach them -- and the
 * two did not overlap. Section 23 is the claim that reached neither.
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

/* The REAL face's geometry (sections 15-23). `sps` must clear 2*m_out
 * STRICTLY there -- the cascade behind the R2C halfband runs at twice the
 * overall rate -- so 16 rather than the complex face's 8. RFC is the design
 * centre: the halfband's +fs/4 shift makes fs/4 the symmetric,
 * best-rejection point of that front end. */
#define RSPS 16.0
#define RM_OUT 4u
#define RNSAMP ((size_t)(NSYM * (size_t)RSPS))
#define RFC 0.25

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
  /* +-60, not +-40: the real face's R2C halfband adds group delay of its own,
     and one function serving both faces is the point of the fold. Widening
     cannot manufacture a pass -- a wrong lag scores ~1-1/m with a standard
     error under 0.01 over a settled window, so the minimum over 60*m tries
     still sits two orders of magnitude above the 0.01 gate. */
  for (int lag = -60; lag <= 60; lag++)
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

/* Every construction in this file varies the same eight things and leaves the
 * rest at their documented defaults; spelling out fifteen positional arguments
 * each time buried which ones actually differ. */
static mpsk_receiver_state_t *
RX (int m, double sps, size_t m_out, int pulse, double bn_carrier,
    double lock_thresh, double init_norm_freq)
{
  /* The shipped defaults: the front-end AGC on, at the default ratio. */
  return mpsk_receiver_create (m, sps, m_out, pulse, 0.35, 8, bn_carrier,
                               0.707, 0.01, lock_thresh, init_norm_freq, 0,
                               MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
}

/* Build a REAL rectangular-pulse M-PSK IF at `fc` cycles/sample with AWGN.
 * The passband signal is Re{s(t) e^{j2pi fc n}} -- taking the real part is the
 * whole difference from make_mpsk()'s stimulus, and it is what an ADC
 * downstream of an analogue mixer actually delivers.
 *
 * `sps` is a PARAMETER, not the RSPS macro: section 19 builds at a different
 * oversampling than the rest, and a hardcoded macro here silently hands the
 * receiver a signal at the wrong symbol rate (a 1.6x clock error, which
 * measures as ~-10 dB EVM and reads exactly like a front-end fault). Writes
 * `nsym * sps` samples -- the caller sizes the buffer. */
static void
make_mpsk_real (float *tx, int *idx, int m, double sps, size_t nsym, double fc,
                double snr_db, uint32_t seed, double phi0)
{
  uint32_t st    = seed;
  double   sigma = TX_AMP * sqrt (0.5 / pow (10.0, snr_db / 10.0));
  size_t   isps  = (size_t)sps;
  for (size_t k = 0; k < nsym; k++)
    {
      int ki    = (int)(dp_xs32 (&st) & 0xFFFFu) % m;
      idx[k]    = ki;
      double th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      double sr = TX_AMP * cos (th);
      double si = TX_AMP * sin (th);
      for (size_t j = 0; j < isps; j++)
        {
          size_t n  = k * isps + j;
          double ph = 2.0 * M_PI * fc * (double)n;
          /* Re{(sr + j si) e^{j ph}} = sr cos(ph) - si sin(ph) */
          tx[n] = (float)(sr * cos (ph) - si * sin (ph)
                          + sigma * dp_gauss (&st));
        }
    }
}

/* RX()'s real-input twin: the same eight knobs through the other
 * constructor, so a section that runs on both faces differs in one letter. */
static mpsk_receiver_state_t *
RXR (int m, double sps, size_t m_out, int pulse, double bn_carrier,
     double lock_thresh, double init_norm_freq)
{
  return mpsk_receiver_create_real (
      m, sps, m_out, pulse, 0.35, 8, bn_carrier, 0.707, 0.01, lock_thresh,
      init_norm_freq, 0, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
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
    DP_CHECK (RX (3, SPS, M_OUT, 0, 0.01, 0.5, 0.0) == NULL); /* bad m  */
    DP_CHECK (RX (4, SPS, 3, 0, 0.01, 0.5, 0.0) == NULL);     /* m_out odd  */
    DP_CHECK (RX (4, SPS, 16, 0, 0.01, 0.5, 0.0) == NULL);    /* m_out > 8 */
    DP_CHECK (RX (4, 2.0, 4, 0, 0.01, 0.5, 0.0)
              == NULL); /* sps < m_out: the terminal stage would interpolate */
    DP_CHECK (RX (4, 0.0, 4, 0, 0.01, 0.5, 0.0) == NULL);     /* sps == 0  */
    DP_CHECK (RX (4, SPS, M_OUT, 2, 0.01, 0.5, 0.0) == NULL); /* bad pulse */

    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    if (!rx)
      return 1;
    DP_CHECK (mpsk_receiver_get_m (rx) == 4);
    DP_CHECK (mpsk_receiver_get_sps (rx) == SPS);
    DP_CHECK (mpsk_receiver_get_m_out (rx) == M_OUT);

    DP_CHECK (mpsk_receiver_get_clipped (rx) == 0); /* nothing pushed yet */

    make_mpsk (tx, idx, 4, 0.0008, 35.0, 99u);
    size_t k1 = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double f1 = mpsk_receiver_get_norm_freq (rx);
    mpsk_receiver_reset (rx);

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
    mpsk_receiver_state_t *d
        = mpsk_receiver_create (4, SPS, 0u, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.0, 0.01, 0.0, 0.0, 0, 0u, 1, 0.0);
    DP_CHECK (d != NULL);
    if (d)
      {
        DP_CHECK (mpsk_receiver_get_m_out (d) == 8u);
        DP_CHECK (
            dp_near (mpsk_receiver_get_zeta (d), 0.70710678118654752, 1e-15));
        DP_CHECK (mpsk_receiver_get_num_phases (d) == 64u);
        DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (d), 0.4999, 1e-15));
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (d), 0.05, 1e-15));
        /* The DROP side is the declare side times the hysteresis, asserted
           as that RELATIONSHIP rather than as 0.39992: a literal would still
           pass if the constant moved and the two stopped being a pair. */
        DP_CHECK (dp_near (
            mpsk_receiver_get_lock_drop_thresh (d),
            MPSK_RX_LOCK_DOWN * mpsk_receiver_get_lock_thresh (d), 1e-15));
        /* The timing loop's pair is NOT the carrier's -- a different
           statistic, sized by symsync's own (rolloff, esno_min, pfa, pd)
           geometry and stepped on a different clock. That they DIFFER is
           why both are exposed, so it is what gets asserted. */
        DP_CHECK (mpsk_receiver_get_sync_lock_thresh (d)
                  != mpsk_receiver_get_lock_thresh (d));
        DP_CHECK (mpsk_receiver_get_sync_lock_thresh (d) > 0.0);
        /* Equal by design: the timing decision carries no LEVEL hysteresis,
           its hysteresis living in the verify counts instead. */
        DP_CHECK (dp_near (mpsk_receiver_get_sync_lock_drop_thresh (d),
                           mpsk_receiver_get_sync_lock_thresh (d), 1e-15));
        mpsk_receiver_destroy (d);
      }
    /* A supplied value still wins -- the derivation is a fallback, not a
       policy that overrides the caller. */
    mpsk_receiver_state_t *p
        = mpsk_receiver_create (4, SPS, 4u, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.9, 0.01, 0.6, 0.0, 0, 128u, 1, 0.02);
    DP_CHECK (p != NULL);
    if (p)
      {
        DP_CHECK (mpsk_receiver_get_m_out (p) == 4u);
        DP_CHECK (dp_near (mpsk_receiver_get_zeta (p), 0.9, 1e-15));
        DP_CHECK (mpsk_receiver_get_num_phases (p) == 128u);
        DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (p), 0.6, 1e-15));
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (p), 0.02, 1e-15));
        /* The drop side follows a SUPPLIED declare threshold too, which is
           what makes it a readback of the pair IN USE rather than of the
           derivation: 0.8 x 0.6, not 0.8 x 0.4999. */
        DP_CHECK (dp_near (mpsk_receiver_get_lock_drop_thresh (p),
                           MPSK_RX_LOCK_DOWN * 0.6, 1e-15));
        mpsk_receiver_destroy (p);
      }
  }

  /* 1c. ONE discriminator, and nothing gates it.
     docs/design/mpsk.md §3.3. There used to be a second, decision-directed
     arm behind `acq_to_track`, and a `ContinuousMpskReceiver` whose whole
     purpose was to pin it off; both are gone (doppler#877), so "the receiver
     never hands over" is now a property of the TYPE and there is no
     construction that could falsify it.

     What is still falsifiable, and is what actually matters, is that the one
     discriminator is UNGATED: the NDA steer runs from the first strobe, not
     from the instant some detector declares. That claim has a runtime
     failure mode -- an `if (locked)` in front of the steer -- so it is
     checked the way it could break. The receiver is stepped over a PREFIX
     short enough that the carrier lock detector has not declared, and the
     tracked frequency must ALREADY have moved most of the way to the true
     offset. A gated steer leaves it at zero there and still passes every
     end-of-run assertion below, which is why the prefix is the check and the
     final lock is not. */
  {
    const double           FOFF = 0.0008;
    mpsk_receiver_state_t *c
        = RX (2, SPS, 8, MPSK_RX_PULSE_IANDD, 0.02, 0.5, 0.0);
    DP_CHECK (c != NULL);
    if (c)
      {
        /* The discriminator's clock IS the symbol clock. §2.1 lists three
           defects that follow from a tap faster than Rs; they cannot occur
           at an update rate of exactly 1, and this asserts the rate rather
           than the tap name that used to imply it. */
        DP_CHECK (dp_near (mpsk_rx_updates_per_symbol (&c->l), 1.0, 1e-15));
        DP_CHECK (c->fe.c->rc->agc != NULL); /* the AGC is not optional */
        DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (c), 0.05, 1e-15));

        make_mpsk (tx, idx, 2, FOFF, 30.0, 21u);

        /* Step one symbol at a time and keep the LAST reading taken while the
           indicator still said 0. Taking it at a fixed prefix would only ask
           whether that prefix happened to fall before the declaration; this
           asks the actual question, whichever symbol the declaration lands
           on. */
        const size_t chunk1       = (size_t)SPS;
        double       f_undeclared = 0.0;
        int          saw_declare  = 0;
        for (size_t i = 0; i + chunk1 <= NSAMP; i += chunk1)
          {
            (void)mpsk_receiver_steps (c, tx + i, chunk1, out, NSYM);
            if (mpsk_receiver_get_locked (c))
              {
                saw_declare = 1;
                break;
              }
            f_undeclared = mpsk_receiver_get_norm_freq (c);
          }
        DP_CHECK (saw_declare); /* vacuous if it never declares */
        /* Measured 0.376 of the offset at the last undeclared symbol. The
           bound is 0.25 rather than something tighter because the number
           being defended is DISTANCE FROM ZERO: a steer gated on the
           indicator reads exactly 0.0 here, and that is the only thing this
           compares against. Nothing is asserted about the SIZE of the
           fraction. `car_lock` is a threshold test with hysteresis on the
           M-th-power lock EMA, which measures phase coherence rather than
           frequency error, so the declaration instant carries no
           convergence information and 0.376 is not "early". */
        DP_CHECK (fabs (f_undeclared) > 0.25 * FOFF);
        DP_CHECK (f_undeclared * FOFF > 0.0); /* toward it, not away */

        /* And the whole record locks and demodulates. */
        mpsk_receiver_reset (c);
        make_mpsk (tx, idx, 2, FOFF, 30.0, 21u);
        size_t n = mpsk_receiver_steps (c, tx, NSAMP, out, NSYM);
        DP_CHECK (n > 0);
        DP_CHECK (mpsk_receiver_get_lock (c) > 0.5);
        DP_CHECK (mpsk_receiver_get_locked (c) == 1);
        DP_CHECK (tail_ser (out, n, idx, 2, phi0_for (2),
                            dp_test_settle_syms (0.02, 0.01))
                  < 0.01);
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
                                    8, 0.01, 0.707, 0.01, 0.5, 0.0, 0, 3u, 1,
                                    0.05)
              == NULL); /* 3 is not a power of two */
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0.5, 0.0, 0, 1u, 1,
                                    0.05)
              == NULL); /* 1 is a power of two but below the floor of 2 */
    /* bn_agc_ratio: strictly inside (0, 1). At 1 the AGC is exactly as fast
       as a loop it feeds; past that it is faster, and two level-correcting
       loops at the same speed integrate against each other. */
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0.5, 0.0, 0, 64u, 1,
                                    1.0)
              == NULL);
    DP_CHECK (mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35,
                                    8, 0.01, 0.707, 0.01, 0.5, 0.0, 0, 64u, 1,
                                    -0.05)
              == NULL);
    /* Non-vacuity: the SAME call with only the offending argument made legal
       must construct, or every line above passes for the wrong reason. */
    mpsk_receiver_state_t *ok
        = mpsk_receiver_create (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8,
                                0.01, 0.707, 0.01, 0.5, 0.0, 0, 64u, 1, 0.05);
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
    const double bn      = 0.01;
    /* Seeded at the bound, not past it: 0.0005 cyc/sample stood here, which
       at this sps is u = 3.5 -- inside the region where acquisition is a coin
       flip, so the irrational-rate claim was riding on the dice. */
    const double foff = dp_test_freq_offset_inside_bw (bn, 4, 1.0) / sps_odd;
    size_t       nsym = (size_t)((double)NSAMP / sps_odd) - 4;
    size_t n = make_mpsk_sps (tx, idx, 4, sps_odd, nsym, foff, 30.0, 77u);
    mpsk_receiver_state_t *rx
        = RX (4, sps_odd, M_OUT, MPSK_RX_PULSE_IANDD, bn, 0.5, 0.0);
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
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
    int          ms[3] = { 2, 4, 8 };
    const double bn    = 0.005;
    for (int mi = 0; mi < 3; mi++)
      for (int fi = 0; fi < 2; fi++)
        {
          int m = ms[mi];
          /* The offset case is seeded at the bound and the receiver is NOT
             told it -- `init_norm_freq` stays 0 in both cases. It used to be
             `fs[fi]`, i.e. the answer, so the loop started on truth and never
             left its initial state: the "under a carrier offset" half of this
             case measured nothing about the carrier loop at either value.
             The bound carries the `m` because the discriminator is an M-th
             power, so the same cycles/sample literal is a different question
             at each order -- the old 0.001 was u = 3.2 at BPSK and u = 12.8
             at 8PSK, both past the measured collapse. */
          double fs[2]
              = { 0.0, dp_test_freq_offset_inside_bw (bn, m, 1.0) / SPS };
          /* 8PSK hands the carrier over to the decision-directed loop; the
             other orders stay in NDA the whole way. Its decision margin is
             only +-pi/8, so the M-th-power discriminator's own phase jitter
             is the dominant error term -- the same call the BER validation
             (mpsk_receiver_ber.c) and the Python suite both make. */
          mpsk_receiver_state_t *rx
              = RX (m, SPS, M_OUT, MPSK_RX_PULSE_IANDD, bn, 0.3, 0.0);
          make_mpsk (tx, idx, m, fs[fi], 30.0, 7u + (uint32_t)(mi * 4 + fi));
          size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
          double ser = tail_ser (out, k, idx, m, phi0_for (m),
                                 dp_test_settle_syms (0.01, bn));
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_RRC, 0.005, 0.5, 0.0);
    DP_CHECK (rx != NULL);
    make_mpsk (tx, idx, 4, 0.0, 30.0, 21u);
    size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                           dp_test_settle_syms (0.01, 0.005));
    DP_CHECK (ser < 0.02);
    mpsk_receiver_destroy (rx);
  }

  /* 4. The carrier estimate the NDA steer builds is the one the loop keeps.

     Sections 4 and 4b used to live here and both were about the handover:
     `acq_to_track` flipping NDA -> decision-directed and back, and the shared
     loop filter carrying the frequency estimate across each flip. Neither
     claim exists any more (doppler#877) -- there is one discriminator, so
     there is no transition to carry anything across.

     What is left of 4b's question is still worth asking, and is asked here on
     the surviving mechanism: the estimate must be BUILT by the steer and then
     HELD, rather than rebuilt from nothing whenever the lock indicator
     changes its mind. The indicator is stepped on the same statistic the
     steer reads, so a receiver that (wrongly) cleared its filter on a lock
     edge would still reach lock and still demodulate -- it would just pay
     pull-in twice. Nothing in a pass/fail sense distinguishes that, which is
     why the estimate is sampled either side of the lock edge rather than at
     block boundaries. */
  {
    const double bn   = 0.01;
    const double foff = dp_test_freq_offset_inside_bw (bn, 4, 1.0) / SPS;
    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, bn, 0.65, 0.0);
    make_mpsk (tx, idx, 4, foff, 30.0, 33u);

    const size_t chunk = (size_t)SPS; /* one symbol */
    double       f_pre = 0.0, f_post = 0.0;
    int          declared = 0;
    for (size_t i = 0; i + chunk <= NSAMP; i += chunk)
      {
        int    was    = mpsk_receiver_get_locked (rx);
        double before = mpsk_receiver_get_norm_freq (rx);
        (void)mpsk_receiver_steps (rx, tx + i, chunk, out, NSYM);
        if (!was && mpsk_receiver_get_locked (rx))
          {
            f_pre    = before;
            f_post   = mpsk_receiver_get_norm_freq (rx);
            declared = 1;
            break;
          }
      }
    DP_CHECK (declared); /* vacuous if the indicator never declares */

    /* The estimate is already the offset BEFORE the declaration -- that is
       what the NDA steer is for, and it is why the indicator gates nothing --
       and the declaration does not disturb it. Both halves matter: the first
       says the loop had something worth holding, the second says a lock edge
       did not cost it. */
    DP_CHECK (fabs (f_pre - foff) < 0.25 * foff);
    DP_CHECK (fabs (f_post - f_pre) < 0.05 * foff);

    /* And the same across a DROP. A noise burst collapses the lock EMA
       through the 0.8x drop threshold for 32 straight symbols; the steer runs
       throughout (it is ungated), so the estimate drifts on noise -- bounded,
       and nowhere near the distance back to zero, which is what a cleared
       filter would read. */
    make_mpsk (tx, idx, 4, foff, -10.0, 44u);
    double f_drop_pre = 0.0, f_drop_post = 0.0;
    int    dropped = 0;
    for (size_t i = 0; i + chunk <= NSAMP / 10; i += chunk)
      {
        int    was    = mpsk_receiver_get_locked (rx);
        double before = mpsk_receiver_get_norm_freq (rx);
        (void)mpsk_receiver_steps (rx, tx + i, chunk, out, NSYM);
        if (was && !mpsk_receiver_get_locked (rx))
          {
            f_drop_pre  = before;
            f_drop_post = mpsk_receiver_get_norm_freq (rx);
            dropped     = 1;
            break;
          }
      }
    DP_CHECK (dropped);
    DP_CHECK (fabs (f_drop_post - f_drop_pre) < 0.05 * foff);

    /* It still demodulates the clean record afterwards, from the estimate it
       held rather than from a cold start. */
    mpsk_receiver_set_norm_freq (rx, foff);
    make_mpsk (tx, idx, 4, foff, 30.0, 45u);
    size_t k = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    DP_CHECK (mpsk_receiver_get_locked (rx) == 1);
    DP_CHECK (
        tail_ser (out, k, idx, 4, phi0_for (4), dp_test_settle_syms (0.01, bn))
        < 0.01);

    printf ("    freq_est across the lock edge: fwd %.6f -> %.6f   "
            "rev %.6f -> %.6f\n",
            f_pre, f_post, f_drop_pre, f_drop_post);
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
    mpsk_receiver_state_t *b
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
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

  /* telemetry attach — the receiver's lock probe + the forwarded carrier
   * (incl. its arm AGC) and symsync probes: nine records per emitted symbol
   * plus the AGC's amortized gain records; detach cascades. */
  {
    float complex tx[512], out[80];
    for (int i = 0; i < 512; i++)
      tx[i] = ((i / 8) % 2 ? 1.0f : -1.0f) + 0.0f * I; /* BPSK, sps=8 */
    dp_tlm_t              *tlm = dp_tlm_create (4096);
    mpsk_receiver_state_t *a
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
    DP_CHECK (tlm != NULL && a != NULL);
    DP_CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx", 1) == DP_OK);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.lock") == a->l.tlm.id_lock);
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
    /* 15 with the AGC pair: lock, car(e, freq, nco, locked),
       sync(e, ctrl, rate, lock, locked, mu), agc(gain_db, level_db),
       sym(i, q). `car.nco` is the SUM driving the LO; `car.freq` is the
       integrator alone. Both are published because on a ramp they differ by
       the proportional term, and only the sum is the applied frequency.

       `sym.i`/`sym.q` are the recovered SYMBOL, split because a telemetry
       record carries one float and a complex value cannot be one probe.
       Without them a capture holds every internal and not the output they
       exist to produce -- no constellation, and no error rate recomputable
       from the filed evidence (doppler#846). */
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.car.nco") >= 0);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.sym.i") >= 0);
    DP_CHECK (dp_tlm_probe_id (tlm, "rx.sym.q") >= 0);
    DP_CHECK (dp_tlm_probe_count (tlm) == 15);

    size_t n_sym = mpsk_receiver_steps (a, tx, 512, out, 80);
    DP_CHECK (n_sym > 0);
    dp_tlm_rec_t recs[2048];
    size_t       n_rec = dp_tlm_read (tlm, 2048, recs, 2048);
    /* lock + car(e,freq,nco,locked) + sync(e,ctrl,rate,lock,
     * locked,mu) + sym(i,q): THIRTEEN records per recovered symbol, all
     * flushed at the strobe -- which is also the rule the symbol pair had to
     * respect to be addable at all: every probe here fires once per symbol
     * or less, never per input sample, so the pair lands on the same index
     * as the loop state that produced it. The arm
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
    DP_CHECK (n_rec == 13 * n_sym + n_agc);

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
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
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
        2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 0.5,
        0.0, 0, MPSK_RX_NUM_PHASES, 0, MPSK_RX_AGC_BW_RATIO);
    dp_tlm_t *tlm4 = dp_tlm_create (4096);
    DP_CHECK (noagc != NULL && tlm4 != NULL);
    if (noagc && tlm4)
      {
        DP_CHECK (mpsk_receiver_set_telemetry (noagc, tlm4, "rx", 1) == DP_OK);
        /* 15 less the AGC pair. */
        DP_CHECK (dp_tlm_probe_count (tlm4) == 13);
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
            4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, bn_c, 0.707, bn_t,
            0.5, 0.0, 0, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
        DP_CHECK (rx != NULL);
        if (!rx)
          continue;
        /* The receiver's ONE AGC, in the front-end cascade; bn is per symbol
           on both sides of the comparison. */
        double bn_agc = rx->fe.c->rc->agc_bn_sym;
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
    DP_CHECK (a != NULL);
    if (a)
      {
        /* Enough input for the loop to move off unity, nowhere near
         * enough for it to settle. */
        float complex y[512];
        size_t        n_pre = (size_t)SPS * 200u;
        (void)mpsk_receiver_steps (a, tx, n_pre, y, 512);
        DP_CHECK (a->fe.c->rc->agc != NULL);
        /* Non-vacuous: the gain is genuinely mid-flight at the split. */
        DP_CHECK (mpsk_receiver_get_agc_gain_db (a) != 0.0);

        size_t   nb   = mpsk_receiver_state_bytes (a);
        uint8_t *blob = malloc (nb);
        mpsk_receiver_get_state (a, blob);

        mpsk_receiver_state_t *b
            = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
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
                0.5, 0.0, 0, MPSK_RX_NUM_PHASES, use_agc,
                MPSK_RX_AGC_BW_RATIO);
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

    /* Tighter than "it moved": the gain is the exact RECIPROCAL of the
       level. Each 4x step in amplitude must move it by 20*log10(4), and
       measured it does -- 12.0412 dB and 12.0412 dB against the analytic
       12.0412. That is the difference between a TREND and an absolute
       level estimate, and it is what lets a caller read
       get_agc_gain_db() as "the input is this far from the level the
       cascade was built for" rather than only as "something changed".
       Carried back from the validation report's §2.9, where the same law
       holds to under 0.01 dB across a 32x span; the report is evidence
       and this is what keeps it true. */
    const double step_db = 20.0 * log10 (4.0);
    DP_CHECK (fabs ((gain[0] - gain[1]) - step_db) < 0.01);
    DP_CHECK (fabs ((gain[1] - gain[2]) - step_db) < 0.01);
  }

  /* Sections 12 and 13 own their buffers: the file's shared tx/idx/out are
     freed after section 4, and reaching past that free is a use-after-free
     that segfaults rather than failing an assert. */
  {
    float complex *ftx = malloc (NSAMP * sizeof (*ftx));
    int           *fid = malloc (NSYM * sizeof (int));

    /* 12. The verify counts are TIME hysteresis, and the defaults are the
       header's. gh-814: "both directions are verify-counted (8 symbols up /
       32 down)" was documented and tested nowhere, and carrier_nda's own
       certification found the analogous count mattered a great deal (its
       n_up = 8 false-declared 18/60 at one geometry), so an unmeasured count
       here is not a safe assumption.

       This used to drive the counts through `configure_lock` on the
       handover's detector and compare declare instants between two settings.
       Both are gone (doppler#877), so the same claim is checked on the
       detector that survives -- the carrier lock indicator -- and against the
       header's CONSTANT rather than against a second configuration:
       `MPSK_RX_LOCK_N_UP` consecutive above-threshold symbols are required,
       so the declaration must land at least that many symbols after the EMA
       first crosses. A count wired to nothing declares on the first crossing
       and fails by N_UP - 1 symbols.

       Stepped one SYMBOL at a time, because the counter counts symbols: the
       EMA is sampled at the same cadence the detector sees it, so "the first
       crossing" means the same thing to the test and to the code. */
    {
      const double bn = 0.01;
      make_mpsk (ftx, fid, 4, dp_test_freq_offset_inside_bw (bn, 4, 1.0) / SPS,
                 30.0, 72u);
      mpsk_receiver_state_t *rx
          = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, bn, 0.65, 0.0);
      DP_CHECK (rx != NULL);
      const double  thr = mpsk_receiver_get_lock_thresh (rx);
      float complex y;
      int64_t       first_cross = -1, declared = -1;
      int64_t       sym = 0;
      for (size_t i = 0; i < NSAMP && declared < 0; i++)
        {
          if (!mpsk_receiver_step_ted (rx, ftx[i], &y, RATESYNC_TED_GARDNER))
            continue;
          sym++;
          /* A crossing only starts a run if the run is unbroken; reset the
             mark whenever the statistic falls back under. */
          if (mpsk_receiver_get_lock (rx) > thr)
            {
              if (first_cross < 0)
                first_cross = sym;
            }
          else
            first_cross = -1;
          if (mpsk_receiver_get_locked (rx))
            declared = sym;
        }
      DP_CHECK (first_cross > 0); /* the statistic did cross */
      DP_CHECK (declared > 0);    /* and the detector did declare */
      /* The whole claim, as one inequality: the declaration is N_UP symbols
         of unbroken evidence after the run began, never the first sample of
         it. */
      DP_CHECK (declared - first_cross >= (int64_t)MPSK_RX_LOCK_N_UP - 1);
      printf ("    carrier lock: EMA crossed at symbol %lld, declared at "
              "%lld (n_up = %u)\n",
              (long long)first_cross, (long long)declared,
              (unsigned)MPSK_RX_LOCK_N_UP);
      mpsk_receiver_destroy (rx);
    }

    /* 13. `bn_carrier` is normalised to the SYMBOL rate, not the input rate.
       gh-814, and the @warning's own headline: "bn_carrier also changed units:
       it is now normalised to the symbol rate, like bn_timing, rather than to
       the input sample rate -- at the old default sps = 8 the same number is
       now an 8x wider loop." Nothing measured it, so a regression to
       input-rate normalisation would look correct at sps = 8 -- the rate
       every other test in this file uses -- and be wrong everywhere else.

       Measured as SETTLING TIME IN SYMBOLS at one `bn` across a 4x span of
       `sps`, with the carrier offset held constant in SYMBOL-RATE units so
       the loop is asked the same question each time. If `bn` means cycles per
       symbol, the answer does not depend on `sps`; if it means cycles per
       input sample, settling in symbols scales WITH `sps` -- 4x across this
       span, which is what makes the invariance the discriminator rather than
       a coincidence.

       Read off get_norm_freq rather than the lock detector: the statistic has
       its own EMA and verify counts, so it measures the DETECTOR's settling
       as much as the loop's, and it was too noisy across this axis to carry
       the claim (955 symbols at sps = 4 against 51 at 16, in an earlier
       attempt through `lock_time`). */
    {
      const double BN = 0.01;
      /* The offset is held constant in cycles per SYMBOL across the sweep --
         which is what the helper gives, since `bn/(m*sps)` cycles per SAMPLE
         IS `bn/m` per symbol at every rate. It used to be a literal 0.004
         cyc/sym, i.e. u = 1.6 once the M-th power is counted. */
      double settle_sym[3];
      size_t ns = 0;
      for (double sps = 8.0; sps <= 32.0; sps *= 2.0, ns++)
        {
          float complex *vtx = malloc (NSAMP * sizeof (*vtx));
          int           *vid = malloc (NSYM * sizeof (int));
          double foff = dp_test_freq_offset_inside_bw (BN, 4, 1.0) / sps;
          size_t nsym = (size_t)((double)NSAMP / sps) - 4;
          size_t n = make_mpsk_sps (vtx, vid, 4, sps, nsym, foff, 30.0, 81u);
          mpsk_receiver_state_t *rx
              = RX (4, sps, M_OUT, MPSK_RX_PULSE_IANDD, BN, 0.5, 0.0);
          DP_CHECK (rx != NULL);
          float complex y;
          size_t        at = 0;
          for (size_t i = 0; i < n; i++)
            {
              (void)mpsk_receiver_step_ted (rx, vtx[i], &y,
                                            RATESYNC_TED_GARDNER);
              if (fabs (mpsk_receiver_get_norm_freq (rx) - foff)
                  < 0.1 * fabs (foff))
                {
                  at = i + 1;
                  break;
                }
            }
          DP_CHECK (at > 0); /* it settled at all, at every rate */
          settle_sym[ns] = (double)at / sps;
          mpsk_receiver_destroy (rx);
          free (vtx);
          free (vid);
        }
      /* One bn, three rates, one answer in symbols. The bound is generous
         against the 4x an input-rate `bn` would produce: anything under 2x
         cannot be that mistake, and the measured spread is far tighter. */
      double lo = settle_sym[0], hi = settle_sym[0];
      for (size_t i = 1; i < 3; i++)
        {
          lo = (settle_sym[i] < lo) ? settle_sym[i] : lo;
          hi = (settle_sym[i] > hi) ? settle_sym[i] : hi;
        }
      DP_CHECK (lo > 0.0 && hi / lo < 2.0);
      /* And it is a real settling time rather than the first sample: at
         bn = 0.01 a second-order loop needs O(1/bn) symbols, so a test that
         passed at ~1 symbol would be measuring nothing. */
      DP_CHECK (lo > 10.0);
    }

    /* 14. Never pair `m_out = 2` with MPSK_RX_PULSE_IANDD. gh-814.
       The header says "never", explains why -- "the filter degenerates to a
       two-tap sum, the eye barely opens and acquisition itself fails about
       half the time" -- and construction permits it anyway, so the failure
       was a caller's to discover.

       Pinned as the DEGENERACY rather than as the acquisition failure. "Fails
       about half the time" is a statement about a distribution over seeds,
       which a unit test cannot assert without becoming a Monte Carlo; the
       mechanism behind it is a filter that cannot open the eye, and that is
       deterministic and cheap. Measured at 20 dB Es/N0, BPSK, where nothing
       else is marginal.

       The `lock` reading is asserted too, and in the direction that surprises:
       it stays healthy while the eye collapses. That is the third instance in
       this file of the receiver's lock statistic moving last, and it is why
       the header's "never" cannot be left to a caller noticing at runtime. */
    {
      float complex *dtx = malloc (NSAMP * sizeof (*dtx));
      int           *did = malloc (NSYM * sizeof (int));
      /* Its own output buffer: the shared `out` is freed after section 4. */
      float complex *dou = malloc (NSYM * sizeof (*dou));
      double         excess[2];
      double         lk[2];
      const size_t   mo[2] = { M_OUT * 2, 2 }; /* 8 (derived-equivalent), 2 */
      for (int c = 0; c < 2; c++)
        {
          size_t n
              = make_mpsk_sps (dtx, did, 2, SPS, NSYM - 4, 0.0, 20.0, 91u);
          mpsk_receiver_state_t *rx
              = RX (2, SPS, mo[c], MPSK_RX_PULSE_IANDD, 0.01, 0.5, 0.0);
          DP_CHECK (rx != NULL);
          size_t k = mpsk_receiver_steps (rx, dtx, n, dou, NSYM);
          DP_CHECK (k > 0);
          size_t settle = dp_test_settle_syms (0.01, 0.01);
          excess[c]     = dp_test_evm_db_hard_range (dou, settle, k, 2) + 20.0;
          lk[c]         = mpsk_receiver_get_lock (rx);
          mpsk_receiver_destroy (rx);
        }
      /* m_out = 8 sits close to the matched-filter bound; m_out = 2 is many dB
         off it. The gap is the two-tap sum, and it is not subtle. */
      DP_CHECK (excess[0] < 2.0);
      DP_CHECK (excess[1] > excess[0] + 5.0);
      /* And the lock statistic does NOT report the collapse: it stays above
         the shipped declare threshold at BOTH geometries, so a caller who
         pairs 2 with I&D and watches `lock` sees nothing wrong. */
      DP_CHECK (lk[0] > 0.5 && lk[1] > 0.5);
      free (dtx);
      free (did);
      free (dou);
    }
    free (ftx);
    free (fid);
  }

  /* ================================================================== *
   * The REAL face — mpsk_receiver_create_real()
   *
   * Sections 15-22 exercise the same object through its other constructor.
   * They were `test_mpsk_receiver_r_core.c` until the two receivers became
   * one (docs/design/mpsk.md §12); folding them in is not tidying. The
   * shared header mpsk_rx_loops.h had NO test file of its own, so every
   * claim it makes was pinned only where one of the two receivers' tests
   * happened to reach it -- and the two did not overlap. `set_telemetry` was
   * asserted seven times on the complex side and zero on the real one; "the
   * LO runs at half the input rate" was asserted by NEITHER, which is where
   * the gh-765 `freq_scale` defect lived. Section 22 is that missing claim.
   *
   * **Everything is measured at the design centre `fc = 0.25` unless the
   * section is specifically about placement.** The R2C halfband bakes in a
   * +fs/4 shift, so fs/4 is where the front end is symmetric and its image
   * rejection is best (past -100 dB across roughly 0.06..0.44, but only -7 dB
   * at 0.01). Measuring off-centre and blaming the receiver is a mistake this
   * project has already made and retracted -- section 19 pins the real
   * behaviour so it is not repeated.
   * ================================================================== */
  {
    /* 15. Lifecycle / validation / getters / reset reproducibility. */
    float         *rtx = malloc (RNSAMP * sizeof (*rtx));
    int           *rid = malloc (NSYM * sizeof (int));
    float complex *rou = malloc (NSYM * sizeof (*rou));
    DP_CHECK (rtx && rid && rou);
    if (rtx && rid && rou)
      {
        {
          mpsk_receiver_state_t *rx
              = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          DP_CHECK (rx != NULL);
          if (rx)
            {
              DP_CHECK (mpsk_receiver_get_m (rx) == 4);
              DP_CHECK (fabs (mpsk_receiver_get_sps (rx) - RSPS) < 1e-12);
              DP_CHECK (mpsk_receiver_get_m_out (rx) == RM_OUT);

              DP_CHECK (mpsk_receiver_get_clipped (rx) == 0);
              mpsk_receiver_destroy (rx);
            }

          /* An invalid order is rejected, not silently accepted. */
          mpsk_receiver_state_t *bad
              = RXR (3, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          DP_CHECK (bad == NULL);
          mpsk_receiver_destroy (bad);

          /* lock_time is the acquisition time as a NUMBER, and it has to
             agree with the flag it dates. Cold it is -1; after a record the
             receiver locks on it is a symbol index inside that record; and
             reset() puts it back to -1, because a reset receiver has not
             locked. Checking it against `locked` is the point -- a lock_time
             that disagreed with the detector reporting it would be a second,
             competing answer. */
          make_mpsk_real (rtx, rid, 4, RSPS, NSYM, RFC, 30.0, 11u,
                          phi0_for (4));
          mpsk_receiver_state_t *lt
              = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          if (lt)
            {
              DP_CHECK (mpsk_receiver_get_lock_time (lt) == -1);
              DP_CHECK (mpsk_receiver_get_locked (lt) == 0);
              size_t n = mpsk_receiver_steps_real (lt, rtx, RNSAMP, rou, NSYM);
              int64_t at = mpsk_receiver_get_lock_time (lt);
              DP_CHECK (mpsk_receiver_get_locked (lt) == 1);
              DP_CHECK (at >= 0);
              DP_CHECK ((size_t)at < n);
              mpsk_receiver_reset (lt);
              DP_CHECK (mpsk_receiver_get_lock_time (lt) == -1);
              /* And it is the FIRST declaration, not the latest. Re-running
                 the record and comparing is NOT enough -- a stamp rewritten
                 on every locked symbol is equally reproducible, it just lands
                 at the END of the record. What separates them is WHERE it
                 lands: acquisition finishes early, so a first-declaration
                 stamp sits in the opening part of the record and a restamped
                 one sits at the last symbol. (Verified by sabotage: dropping
                 the `lock_time < 0` guard leaves every other assertion here
                 passing.) */
              DP_CHECK ((size_t)at < n / 2);
              (void)mpsk_receiver_steps_real (lt, rtx, RNSAMP, rou, NSYM);
              DP_CHECK (mpsk_receiver_get_lock_time (lt) == at);
              mpsk_receiver_destroy (lt);
            }
        }

        {
          /* reset() returns the receiver to a cold start: the same input
             twice across a reset must give byte-identical symbols. */
          make_mpsk_real (rtx, rid, 4, RSPS, NSYM, RFC, 30.0, 11u,
                          phi0_for (4));
          mpsk_receiver_state_t *a = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          if (a)
            {
              size_t k1 = mpsk_receiver_steps_real (a, rtx, RNSAMP, rou, NSYM);
              double f1 = mpsk_receiver_get_norm_freq (a);
              float complex first = rou[k1 / 2];
              mpsk_receiver_reset (a);
              size_t k2 = mpsk_receiver_steps_real (a, rtx, RNSAMP, rou, NSYM);
              DP_CHECK (k1 == k2);
              DP_CHECK (rou[k2 / 2] == first);
              DP_CHECK (mpsk_receiver_get_norm_freq (a) == f1);
              mpsk_receiver_destroy (a);
            }
        }

        /* 16. Locks + recovers from a real IF at the design centre, every M.
         */
        {
          int ms[3] = { 2, 4, 8 };
          for (int mi = 0; mi < 3; mi++)
            {
              int m = ms[mi];
              /* Seeded ON the centre, the loop tracking the residual around
                 it. 8PSK hands over to the decision-directed loop for the
                 same reason the complex face does: its decision margin is
                 only +-pi/8, so the M-th-power discriminator's own jitter
                 dominates. */
              mpsk_receiver_state_t *rx
                  = RXR (m, RSPS, RM_OUT, 0, 0.005, 0.3, RFC);
              DP_CHECK (rx != NULL);
              if (!rx)
                continue;
              make_mpsk_real (rtx, rid, m, RSPS, NSYM, RFC, 30.0,
                              7u + (uint32_t)mi, phi0_for (m));
              size_t k = mpsk_receiver_steps_real (rx, rtx, RNSAMP, rou, NSYM);
              size_t settle = dp_test_settle_syms (0.01, 0.005);
              double ser    = tail_ser (rou, k, rid, m, phi0_for (m), settle);
              DP_CHECK (ser < 0.01);
              /* The lock EMA's noise-only sd is CARRIER_NDA_LOCK_NORM_SD
                 (0.1132) at every m, so state the threshold in sigmas: 0.5 is
                 4.42 sigma, i.e. the shipped default's per-look Pfa of 5e-6.
               */
              DP_CHECK (mpsk_receiver_get_lock (rx) > 0.5);
              /* Truth-free corroboration -- a BER alone can false-pass
                 through its own lag/rotation search (see dp_sym_test.h).

                 TWO assertions, because one of them used to be vacuous. This
                 read `evm < -12.0` for every M until 2026-07-27, and the 8PSK
                 scatter floor is **-12.9 dB**
                 (dp_test_evm_scatter_floor_db) -- so a constellation with no
                 carrier recovery whatsoever satisfied it, and the check had
                 no discriminating power at M = 8 at all.

                   - the absolute gate is the quality bar (measured -18.1 /
                     -17.7 / -18.2 dB at M = 2 / 4 / 8, so ~2 dB of margin);
                   - the floor-relative gate is what makes the absolute one
                     provably non-vacuous, and it is the one that fires first
                     if another M is ever added -- at M = 16 the floor rises
                     to -19.0 dB and a fixed -16.0 would silently go vacuous
                     again.

                 Note how little room there is at M = 8: 5.3 dB between a
                 healthy receiver and pure noise. The self-referenced EVM
                 cannot carry this verdict alone at high M, which is why
                 `ser < 0.01` above is the primary check and this is
                 corroboration. */
              if (k > settle)
                {
                  double evm
                      = dp_test_evm_db_hard_m (rou + settle, k - settle, m);
                  double flr = dp_test_evm_scatter_floor_db (m);
                  printf ("  real M=%d: evm=%6.1f dB (scatter floor %5.1f, "
                          "margin %4.1f dB)\n",
                          m, evm, flr, flr - evm);
                  DP_CHECK (evm < -16.0);
                  DP_CHECK (evm < flr - 3.0);
                }
              DP_CHECK (mpsk_receiver_get_clipped (rx) == 0);
              mpsk_receiver_destroy (rx);
            }
        }

        /* 17. `sps > 2 * m_out` is enforced on the real face.
         *
         * The cascade behind the R2C halfband runs at twice the overall rate,
         * so the terminal stage needs rate = m_out/sps < 0.5. This is the one
         * constraint the real path has that the complex path does not (which
         * needs only sps >= m_out), and a documented-but-unenforced
         * constraint is how a caller gets a silently wrong receiver instead
         * of an error. */
        {
          /* sps == 2 * m_out exactly: rejected (strictly greater required). */
          mpsk_receiver_state_t *eq = RXR (4, 8.0, 4, 0, 0.005, 0.5, 0.0);
          DP_CHECK (eq == NULL);
          mpsk_receiver_destroy (eq);

          /* Below it: rejected. */
          mpsk_receiver_state_t *lo = RXR (4, 6.0, 4, 0, 0.005, 0.5, 0.0);
          DP_CHECK (lo == NULL);
          mpsk_receiver_destroy (lo);

          /* Just above it: accepted. */
          mpsk_receiver_state_t *ok = RXR (4, 8.5, 4, 0, 0.005, 0.5, 0.0);
          DP_CHECK (ok != NULL);
          mpsk_receiver_destroy (ok);

          /* And the SAME geometry the complex face accepts: `sps == m_out`
             is legal there and refused here. Asserting both halves is what
             makes this a difference between the faces rather than a bound
             that happens to be true of both. */
          mpsk_receiver_state_t *cx = RX (4, 8.0, 8, 0, 0.005, 0.5, 0.0);
          DP_CHECK (cx != NULL);
          mpsk_receiver_destroy (cx);
          DP_CHECK (RXR (4, 8.0, 8, 0, 0.005, 0.5, 0.0) == NULL);
        }

        /* 18. The real face acquires an offset and declares lock on it,
         * exactly as the complex face does.
         *
         * This asserted the handover flag on the real face until
         * doppler#877; with one discriminator the observable is the lock
         * INDICATOR, and the pairing with the complex face is the point --
         * one core, so the two faces answer the same question the same way.
         */
        {
          /* lock_thresh 0.65 is 5.74 sigma -- deliberately above the 0.5
             default so the declare is unambiguous, and matching the complex
             face's case so the two measure the same operating point. */
          const double           RBN = 0.01;
          mpsk_receiver_state_t *rx = RXR (4, RSPS, RM_OUT, 0, RBN, 0.65, RFC);
          DP_CHECK (rx != NULL);
          if (rx)
            {
              /* RFC + the bound, not RFC + 0.0005: at RSPS = 16 that literal
                 was u = 3.2, so this case was seeded past the point where
                 acquisition is repeatable. */
              make_mpsk_real (
                  rtx, rid, 4, RSPS, NSYM,
                  RFC + dp_test_freq_offset_inside_bw (RBN, 4, 1.0) / RSPS,
                  30.0, 33u, phi0_for (4));
              size_t k = mpsk_receiver_steps_real (rx, rtx, RNSAMP, rou, NSYM);
              DP_CHECK (mpsk_receiver_get_locked (rx) == 1);
              DP_CHECK (mpsk_receiver_get_lock (rx) > 0.65);
              DP_CHECK (mpsk_receiver_get_lock_time (rx) >= 0);
              double ser = tail_ser (rou, k, rid, 4, phi0_for (4),
                                     dp_test_settle_syms (0.01, 0.01));
              DP_CHECK (ser < 0.01);
              mpsk_receiver_destroy (rx);
            }
        }

        /* 19. The usable band constrains the OCCUPIED band, not the centre.
         *
         * The R2C halfband's image rejection collapses at the band edges, and
         * a rectangular pulse spans fc +- 1/sps to its first null. So a
         * centre that looks comfortably inside the band can still have its
         * skirt reach an edge, where the folded image lands on the wanted
         * signal. THIS is what a "receiver bug at low oversampling" actually
         * is; pinning both halves is what stops it being misdiagnosed
         * again. */
        {
          /* sps = 10 -> the pulse is +-0.1 wide, so an IF at 0.10 puts its
             lower skirt on DC, where rejection is only about -7 dB. */
          const double sps_edge   = 10.0;
          size_t       nsamp_edge = (size_t)(NSYM * (size_t)sps_edge);
          size_t       settle     = dp_test_settle_syms (0.01, 0.005);

          mpsk_receiver_state_t *edge
              = RXR (4, sps_edge, RM_OUT, 0, 0.005, 0.5, 0.10);
          mpsk_receiver_state_t *ctr
              = RXR (4, sps_edge, RM_OUT, 0, 0.005, 0.5, RFC);
          DP_CHECK (edge != NULL && ctr != NULL);
          if (edge && ctr)
            {
              /* Effectively noiseless (50 dB), because this section isolates
                 PLACEMENT: at 30 dB the AWGN swamps the very effect being
                 measured and both cases read the same -10 dB. Enough noise
                 remains to break the measure-zero unstable equilibrium an
                 M-th-power loop would sit at on a perfectly clean,
                 zero-offset input. The Python twin
                 (test_usable_band_is_the_input_constraint) is fully noiseless
                 for the same reason. */
              /* phi0 = 0, NOT the pi/4 QPSK convention, and the choice is
                 load-bearing. The leaked image is the signal's CONJUGATE, so
                 how much it hurts depends on whether each symbol's conjugate
                 is itself or a different symbol. Unrotated QPSK {0, pi/2, pi,
                 3pi/2} pairs two symbols with themselves and the image adds
                 coherently; the pi/4-rotated set maps every symbol onto a
                 DIFFERENT one and the damage largely averages out. Measured
                 at this geometry: -4.4 dB EVM unrotated vs -20.1 dB rotated
                 -- a 16 dB difference from the constellation phase alone.
                 Unrotated is both the worst case and what the Python twin
                 uses, so the two pin the same thing. */
              make_mpsk_real (rtx, rid, 4, sps_edge, NSYM, 0.10, 50.0, 51u,
                              0.0);
              size_t ke = mpsk_receiver_steps_real (edge, rtx, nsamp_edge, rou,
                                                    NSYM);
              double evm_edge
                  = (ke > settle)
                        ? dp_test_evm_db_hard_m (rou + settle, ke - settle, 4)
                        : 0.0;

              make_mpsk_real (rtx, rid, 4, sps_edge, NSYM, RFC, 50.0, 51u,
                              0.0);
              size_t kc
                  = mpsk_receiver_steps_real (ctr, rtx, nsamp_edge, rou, NSYM);
              double evm_ctr
                  = (kc > settle)
                        ? dp_test_evm_db_hard_m (rou + settle, kc - settle, 4)
                        : 0.0;

              /* The SAME geometry -- only the placement differs. The centre
                 must be clean and the edge visibly degraded.

                 The MAGNITUDE here is deliberately weak (2 dB), and the
                 reason is NOT the receiver. The leaked image is the signal's
                 own conjugate, so the resulting ISI is a deterministic
                 function of the symbol SEQUENCE, and the penalty varies
                 enormously with it. Measured over 8 seeds at this exact
                 geometry, noiseless, identical receiver:

                     symbol source              min   median    max
                     pn_core MLS, length 64     2.7     11.9   18.5   dB
                     numpy PCG64              -11.1      2.8   18.6   dB
                     this file's xorshift32      --       2.9     --   dB

                 So NO symbol source reliably excites it, and a test asserting
                 a large penalty on one sequence is asserting a property of
                 that sequence. An earlier version of this comment claimed the
                 xorshift source under-excited the impairment by 15 dB
                 relative to PCG64; that compared ONE seed against ONE seed
                 and the PCG64 draw happened to be favourable -- on medians
                 PCG64 is the worse source. Corrected.

                 Likely mechanism, if this is ever tightened: an m-sequence is
                 white to SECOND order but its higher-order joint statistics
                 are constrained by the linear recurrence, and this impairment
                 depends on symbol PAIRS -- so second-order whiteness is not
                 the property that matters, which is why "uniform,
                 decorrelated at lag 1" says nothing here.

                 This section therefore pins the always-true form: the centre
                 is clean, the edge is worse. **The Python twin
                 (test_usable_band_is_the_input_constraint) asserts 10 dB on a
                 single seed and is fragile for exactly this reason** -- it
                 should average over seeds. If the edge ever matches the
                 centre, the halfband's edge behaviour changed and the
                 documented input constraint needs re-measuring, not
                 deleting. */
              DP_CHECK (evm_ctr < -15.0);
              DP_CHECK (evm_edge > evm_ctr + 2.0);
              printf ("  real usable band: EVM %.1f dB at fc=0.10 vs "
                      "%.1f dB at fs/4\n",
                      evm_edge, evm_ctr);
              mpsk_receiver_destroy (edge);
              mpsk_receiver_destroy (ctr);
            }
        }

        /* 20. Serialized state resumes bit-for-bit; a bad envelope rejects;
         * and a blob from the OTHER face is refused by name.
         *
         * The cross-face reject is what one object buys and one type could
         * not: the two faces now share `mpsk_receiver_set_state`, so the only
         * thing standing between a DDC blob and a DDCR's cascade is the
         * envelope magic being keyed on the face. Reinterpreting one as the
         * other would restore a plausible-looking receiver with the wrong
         * front-end memory -- the exact failure dp_state.h's validate exists
         * to make impossible. */
        {
          make_mpsk_real (rtx, rid, 4, RSPS, NSYM, RFC, 30.0, 71u,
                          phi0_for (4));
          size_t half = RNSAMP / 2;

          mpsk_receiver_state_t *ref
              = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          mpsk_receiver_state_t *src
              = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          mpsk_receiver_state_t *dst
              = RXR (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
          DP_CHECK (ref && src && dst);
          if (ref && src && dst)
            {
              float complex *ref_out = malloc (NSYM * sizeof (*ref_out));
              if (ref_out)
                {
                  /* Reference: both halves through one instance. */
                  (void)mpsk_receiver_steps_real (ref, rtx, half, ref_out,
                                                  NSYM);
                  size_t rn = mpsk_receiver_steps_real (
                      ref, rtx + half, RNSAMP - half, ref_out, NSYM);

                  /* Split: first half through `src`, hand its state to `dst`,
                     finish there. The second halves must be identical. */
                  (void)mpsk_receiver_steps_real (src, rtx, half, rou, NSYM);
                  size_t nb   = mpsk_receiver_state_bytes (src);
                  void  *blob = malloc (nb);
                  DP_CHECK (nb > 0 && blob != NULL);
                  if (blob)
                    {
                      mpsk_receiver_get_state (src, blob);
                      DP_CHECK (mpsk_receiver_set_state (dst, blob) == DP_OK);
                      size_t dn = mpsk_receiver_steps_real (
                          dst, rtx + half, RNSAMP - half, rou, NSYM);
                      DP_CHECK (dn == rn);
                      int same = (dn == rn);
                      for (size_t i = 0; same && i < dn; i++)
                        if (rou[i] != ref_out[i])
                          same = 0;
                      DP_CHECK (same); /* bit-for-bit resume */

                      /* A COMPLEX receiver must refuse this real blob, and
                         the refusal must be the envelope's -- not a size
                         accident. Built at the same sps/m_out so the two
                         disagree about the face and nothing else. */
                      mpsk_receiver_state_t *cx
                          = RX (4, RSPS, RM_OUT, 0, 0.005, 0.5, RFC);
                      DP_CHECK (cx != NULL);
                      if (cx)
                        {
                          DP_CHECK (mpsk_receiver_set_state (cx, blob)
                                    == DP_ERR_INVALID);
                          /* And the other direction, so neither face is
                             merely lucky about its blob size. */
                          size_t cnb = mpsk_receiver_state_bytes (cx);
                          void  *cbl = malloc (cnb);
                          if (cbl)
                            {
                              mpsk_receiver_get_state (cx, cbl);
                              DP_CHECK (mpsk_receiver_set_state (dst, cbl)
                                        == DP_ERR_INVALID);
                              free (cbl);
                            }
                          mpsk_receiver_destroy (cx);
                        }

                      /* A clobbered envelope must be REJECTED, never
                         reinterpreted. */
                      ((unsigned char *)blob)[0] ^= 0xFFu;
                      DP_CHECK (mpsk_receiver_set_state (dst, blob)
                                == DP_ERR_INVALID);
                      free (blob);
                    }
                  free (ref_out);
                }
              mpsk_receiver_destroy (ref);
              mpsk_receiver_destroy (src);
              mpsk_receiver_destroy (dst);
            }
        }

        /* 21. The real face's front-end AGC: level-invariant, slower than
         * both loops, and the five derived parameters read back.
         *
         * Section 11 carries the reasoning for the complex face. This pins
         * that the wedge actually reached the OTHER front end -- a different
         * cascade, behind the R2C halfband. Nothing in section 11 would fail
         * if `agc` were quietly ignored here. */
        {
          static const double amps[]  = { 0.25, 1.0, 4.0 };
          double              gain[3] = { 0, 0, 0 };
          size_t              nsym[3] = { 0, 0, 0 };
          for (size_t a = 0; a < 3; a++)
            {
              make_mpsk_real (rtx, rid, 4, RSPS, NSYM, RFC, 30.0, 5u,
                              phi0_for (4));
              for (size_t i = 0; i < RNSAMP; i++)
                rtx[i] *= (float)amps[a];

              mpsk_receiver_state_t *rx
                  = RXR (4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0.5, RFC);
              DP_CHECK (rx != NULL);
              if (rx)
                {
                  size_t k
                      = mpsk_receiver_steps_real (rx, rtx, RNSAMP, rou, NSYM);
                  DP_CHECK (k > 0);
                  gain[a] = mpsk_receiver_get_agc_gain_db (rx);
                  nsym[a] = k;
                  mpsk_receiver_destroy (rx);
                }

              /* agc=0 is the bisect handle here too: no gain, ever. */
              mpsk_receiver_state_t *off = mpsk_receiver_create_real (
                  4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707,
                  0.01, 0.5, RFC, 0, MPSK_RX_NUM_PHASES, 0,
                  MPSK_RX_AGC_BW_RATIO);
              DP_CHECK (off != NULL);
              if (off)
                {
                  (void)mpsk_receiver_steps_real (off, rtx, RNSAMP, rou, NSYM);
                  DP_CHECK (mpsk_receiver_get_agc_gain_db (off) == 0.0);
                  mpsk_receiver_destroy (off);
                }
            }
          DP_CHECK (nsym[0] == nsym[1] && nsym[1] == nsym[2]);
          /* Non-vacuous: the gain tracked the input across the full 24 dB. */
          DP_CHECK (gain[0] - gain[1] > 10.0 && gain[1] - gain[2] > 10.0);

          /* The AGC is slower than BOTH loops on this face too, and by the
             declared ratio off the slowest -- the same claim section 9 makes
             through the complex front end's cascade, reached through the
             other one. */
          {
            const double bns[][2] = {
              { 0.01, 0.01 },
              { 0.05, 0.001 },
              { 0.001, 0.05 },
            };
            for (size_t i = 0; i < sizeof (bns) / sizeof (bns[0]); i++)
              {
                double                 bn_c = bns[i][0], bn_t = bns[i][1];
                mpsk_receiver_state_t *rx = mpsk_receiver_create_real (
                    4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, bn_c, 0.707,
                    bn_t, 0.5, RFC, 0, MPSK_RX_NUM_PHASES, 1,
                    MPSK_RX_AGC_BW_RATIO);
                DP_CHECK (rx != NULL);
                if (!rx)
                  continue;
                double bn_agc  = rx->fe.r->rc->agc_bn_sym;
                double slowest = bn_c < bn_t ? bn_c : bn_t;
                DP_CHECK (bn_agc < bn_c && bn_agc < bn_t);
                DP_CHECK (fabs (bn_agc - MPSK_RX_AGC_BW_RATIO * slowest)
                          < 1e-15 * slowest + 1e-18);
                mpsk_receiver_destroy (rx);
              }
          }

          /* The ratio is refused at or above 1, where the AGC would be as
             fast as the loop it feeds. */
          DP_CHECK (mpsk_receiver_create_real (
                        4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                        0.707, 0.01, 0.5, RFC, 0, MPSK_RX_NUM_PHASES, 1, 1.0)
                    == NULL);
          /* Zero is no longer a rejection: it asks for the derived ratio
             (design/mpsk.md §8.1). The invariant it used to guard is
             unchanged and still checked above at 1.0 -- what moved is only
             the meaning of 0, which previously could not construct anything
             and so had no caller to break. Assert the DERIVED value rather
             than merely that it builds, or this reads as a weaker version of
             the reject it replaced. */
          {
            mpsk_receiver_state_t *d = mpsk_receiver_create_real (
                4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707,
                0.01, 0.5, RFC, 0, MPSK_RX_NUM_PHASES, 1, 0.0);
            DP_CHECK (d != NULL);
            DP_CHECK (dp_near (mpsk_receiver_get_bn_agc_ratio (d),
                               MPSK_RX_AGC_RATIO_DEFAULT, 1e-12));
            if (d)
              mpsk_receiver_destroy (d);
          }
          /* Negative is still refused -- a ratio below zero is not a slower
             AGC. */
          DP_CHECK (mpsk_receiver_create_real (
                        4, RSPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                        0.707, 0.01, 0.5, RFC, 0, MPSK_RX_NUM_PHASES, 1, -0.05)
                    == NULL);

          /* All five derived at once, read back. The real face's ONE
             difference is the rate the m_out rule sees: this cascade sits
             behind the R2C halfband so the bound is sps/2, STRICTLY (Ddcr
             needs a ratio below 0.5). At sps = 16 that is a bound of 8
             exclusive, which lands on 6 -- not the 8 the complex face
             reaches at sps = 8, and not the 8 that min(8, 2*floor(sps/4))
             used to claim and mpsk_receiver_create_real() rejects. Literals,
             so the expectation does not agree with the rule by
             construction. */
          {
            mpsk_receiver_state_t *d = mpsk_receiver_create_real (
                4, RSPS, 0u, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.0, 0.01,
                0.0, RFC, 0, 0u, 1, 0.0);
            DP_CHECK (d != NULL);
            if (d)
              {
                DP_CHECK (mpsk_receiver_get_m_out (d) == 6u);
                DP_CHECK (dp_near (mpsk_receiver_get_zeta (d),
                                   0.70710678118654752, 1e-15));
                DP_CHECK (mpsk_receiver_get_num_phases (d) == 64u);
                DP_CHECK (dp_near (mpsk_receiver_get_lock_thresh (d), 0.4999,
                                   1e-15));
                DP_CHECK (
                    dp_near (mpsk_receiver_get_bn_agc_ratio (d), 0.05, 1e-15));
                mpsk_receiver_destroy (d);
              }
          }

          /* The rule REFUSES rather than clamps when the bound cannot carry
             even two outputs per symbol: behind the halfband, sps = 4 leaves
             a strict bound of 2, so there is no even m_out >= 2 below it and
             create() has to return NULL. A clamp here would hand back a
             receiver whose detector has nothing to detect with, which is the
             failure mode deriving exists to remove -- so the refusal is the
             behaviour, not an edge case. */
          DP_CHECK (mpsk_receiver_create_real (4, 4.0, 0u, MPSK_RX_PULSE_IANDD,
                                               0.35, 8, 0.01, 0.0, 0.01, 0.0,
                                               RFC, 0, 0u, 1, 0.0)
                    == NULL);
        }
      }
    free (rtx);
    free (rid);
    free (rou);
  }

  /* 22. Telemetry reaches the REAL front end's AGC.
   *
   * §2 of docs/design/mpsk.md §12: `set_telemetry` was asserted seven
   * times against the complex front end and ZERO times against the real one,
   * so the forward into `ddcr_set_telemetry()` was carried by nothing. The
   * loops' half is shared code and section 8 covers it; what is unique here
   * is the third attachment, and the assertion that separates them is the
   * AGC pair being on the CASCADE's grid rather than the symbol grid --
   * which is only observable if the forward actually happened. */
  {
    float         *ttx = malloc (RNSAMP * sizeof (*ttx));
    int           *tid = malloc (NSYM * sizeof (int));
    float complex *tou = malloc (NSYM * sizeof (*tou));
    dp_tlm_t      *tlm = dp_tlm_create (1 << 16);
    DP_CHECK (ttx && tid && tou && tlm);
    if (ttx && tid && tou && tlm)
      {
        mpsk_receiver_state_t *rx = RXR (4, RSPS, RM_OUT, 0, 0.01, 0.5, RFC);
        DP_CHECK (rx != NULL);
        if (rx)
          {
            DP_CHECK (mpsk_receiver_set_telemetry (rx, tlm, "rr", 1) == DP_OK);
            /* Fifteen: the receiver's lock, the carrier loop's four, the
               timing loop's six, the front end's AGC pair, and the recovered
               symbol as a real/imag pair. The REAL face publishes the same
               set as the complex one, which is the claim -- one core, and a
               capture from either reconstructs the same constellation. */
            DP_CHECK (dp_tlm_probe_count (tlm) == 15);
            int id_car = dp_tlm_probe_id (tlm, "rr.car.e");
            int id_syn = dp_tlm_probe_id (tlm, "rr.sync.e");
            int id_agc = dp_tlm_probe_id (tlm, "rr.agc.gain_db");
            int id_lvl = dp_tlm_probe_id (tlm, "rr.agc.level_db");
            DP_CHECK (id_car >= 0 && id_syn >= 0 && id_agc >= 0
                      && id_lvl >= 0);

            make_mpsk_real (ttx, tid, 4, RSPS, 1024u, RFC, 30.0, 17u,
                            phi0_for (4));
            size_t n = mpsk_receiver_steps_real (rx, ttx, 1024u * (size_t)RSPS,
                                                 tou, NSYM);
            DP_CHECK (n > 0);

            dp_tlm_rec_t *recs = malloc (65536 * sizeof (*recs));
            DP_CHECK (recs != NULL);
            if (recs)
              {
                size_t n_rec = dp_tlm_read (tlm, 65536, recs, 65536);
                DP_CHECK (dp_tlm_dropped (tlm) == 0);
                size_t n_car = 0, n_syn = 0, n_agc = 0;
                for (size_t i = 0; i < n_rec; i++)
                  {
                    if (recs[i].probe == (uint16_t)id_car)
                      n_car++;
                    else if (recs[i].probe == (uint16_t)id_syn)
                      n_syn++;
                    else if (recs[i].probe == (uint16_t)id_agc
                             || recs[i].probe == (uint16_t)id_lvl)
                      n_agc++;
                  }
                /* The loops are on the symbol grid and agree with each
                   other. */
                DP_CHECK (n_car == n && n_syn == n);
                /* The AGC forward reached ddcr_set_telemetry(), and its pair
                   is on the cascade's own grid -- both probes always
                   together, and NOT one per symbol. A forward that silently
                   did nothing reads n_agc == 0; one wired to the symbol
                   strobe reads n_agc / 2 == n. */
                DP_CHECK (n_agc > 0);
                DP_CHECK (n_agc % 2 == 0);
                DP_CHECK (n_agc / 2 != n);
                free (recs);
              }
            /* Detach reaches everything, including the front end. */
            DP_CHECK (mpsk_receiver_set_telemetry (rx, NULL, "rr", 1)
                      == DP_OK);
            DP_CHECK (rx->l.tlm.ctx == NULL && rx->l.timing.tlm.ctx == NULL);
            mpsk_receiver_destroy (rx);
          }
      }
    dp_tlm_destroy (tlm);
    free (ttx);
    free (tid);
    free (tou);
  }

  /* 23. THE LO RUNS AT HALF THE INPUT RATE.
   *
   * The claim mpsk_rx_loops.h makes that NOTHING in this repository asserted
   * (docs/design/mpsk.md §12.3, last row): the R2C halfband decimates
   * 2:1 before the mix, so `lo_sps = sps/2` and every caller-facing frequency
   * is converted back to the input rate on the way out. It is where the
   * gh-765 `freq_scale` defect lived, and it went through every step test in
   * the tree because **a type-2 loop nulls a frequency STEP to zero
   * steady-state error regardless of gain**. Only a RAMP separates a loop
   * from a mis-scaled one.
   *
   * Two halves, because `lo_sps` enters in two places:
   *
   *   (a) the LOOP GAIN, through `freq_scale = (1/2pi) * upd / lo_sps`.
   *       Measured against the closed form, which is face-independent BY
   *       DESIGN -- `bn_carrier` is normalised to the symbol rate, so both
   *       faces must hold the same lag under the same ramp in cycles per
   *       SYMBOL squared:
   *
   *           theta_ss = 2*pi*r / wn^2,   wn = loop_filter_wn(bn, zeta)
   *
   *       Sabotage target: pass `sps` instead of `0.5 * sps` for `lo_sps` in
   *       mpsk_rx_create_impl(). `freq_scale` then halves, the loop gain
   *       halves with it and the lag DOUBLES -- measured 0.0429 and 0.1424
   *       rad against a law of 0.0212 and 0.0707, i.e. 2.00x at both ramp
   *       rates, while the complex face and the readback below stay green.
   *
   *   (b) the READBACK, through mpsk_rx_lo_to_input(). A residual of `d`
   *       cycles/sample at the intermediate rate is `d/2` at the real input
   *       rate. Sabotage target: return 1.0 there. `norm_freq` then reads
   *       twice the residual -- measured 0.25008 against a true 0.25004, an
   *       error of exactly `df` and five times this tolerance -- while EVERY
   *       other assertion in this file, including (a) above, stays green.
   *       Nothing else moves: the receiver still locks, still demodulates,
   *       still reports a healthy statistic. It just lies about where the
   *       carrier is. */
  {
    const double LO_BN   = 0.005;
    const double LO_SPS  = 32.0; /* > 2*m_out on the real face      */
    const size_t LO_NSYM = 3000u;
    const double wn      = loop_filter_wn (LO_BN, 0.707);

    /* (a) The ramp law, on both faces, against one prediction. */
    static const double ramps[] = { 3e-7, 1e-6 };
    for (size_t ri = 0; ri < sizeof (ramps) / sizeof (ramps[0]); ri++)
      {
        double r    = ramps[ri];             /* cycles/symbol^2      */
        double a    = r / (LO_SPS * LO_SPS); /* cycles/sample^2      */
        double want = 2.0 * M_PI * r / (wn * wn);
        double got[2];
        for (int real = 0; real < 2; real++)
          {
            mpsk_receiver_state_t *rx
                = real ? mpsk_receiver_create_real (
                             2, LO_SPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8,
                             LO_BN, 0.707, LO_BN, 0.3, RFC, 0,
                             MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO)
                       : mpsk_receiver_create (
                             2, LO_SPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8,
                             LO_BN, 0.707, LO_BN, 0.3, 0.0, 0,
                             MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
            DP_CHECK (rx != NULL);
            if (!rx)
              continue;
            uint32_t st   = 12345u;
            size_t   isps = (size_t)LO_SPS;
            double   s1   = 0.0;
            size_t   cnt  = 0;
            for (size_t k = 0; k < LO_NSYM; k++)
              {
                double sr = TX_AMP * ((dp_xs32 (&st) % 2u) ? -1.0 : 1.0);
                for (size_t j = 0; j < isps; j++)
                  {
                    double n = (double)(k * isps + j);
                    /* The ramp, plus the real face's IF centre. Integrating
                       f(n) = fc + a*n gives the instantaneous phase. */
                    double ph = 2.0 * M_PI
                                * ((real ? RFC * n : 0.0) + a * n * n * 0.5);
                    float complex y;
                    if (real)
                      (void)mpsk_receiver_step_real_ted (
                          rx, (float)(sr * cos (ph)), &y,
                          RATESYNC_TED_GARDNER);
                    else
                      (void)mpsk_receiver_step_ted (
                          rx,
                          (float)(sr * cos (ph)) + (float)(sr * sin (ph)) * I,
                          &y, RATESYNC_TED_GARDNER);
                  }
                if (k > LO_NSYM / 2)
                  {
                    /* SIGNED, then |mean| -- not mean|e|. Under a ramp the
                       lag is a CONSTANT offset the loop is holding, so the
                       signed mean estimates it and the loop's own jitter
                       averages out; mean|e| adds a positive bias that grows
                       as the lag approaches the jitter, and at r = 3e-7 on
                       this face that bias alone read 44% high. */
                    s1 += mpsk_receiver_get_last_error (rx);
                    cnt++;
                  }
              }
            got[real] = cnt ? fabs (s1 / (double)cnt) : -1.0;
            mpsk_receiver_destroy (rx);
          }
        printf ("  lo_sps ramp r=%.0e: law %.4f rad, complex %.4f, "
                "real %.4f\n",
                ramps[ri], want, got[0], got[1]);
        /* 8% on BOTH faces. Measured 0.5% / 1.4% (complex) and 1.4% / 0.4%
           (real) across the two ramps, so this is ~5x the observed spread and
           an order of magnitude inside the factor of TWO a wrong `lo_sps`
           costs -- tight enough to be a gate, loose enough to survive another
           toolchain's floating point. */
        DP_CHECK (fabs (got[0] - want) <= 0.08 * want);
        DP_CHECK (fabs (got[1] - want) <= 0.08 * want);
      }

    /* (b) The readback converts the intermediate rate back to the input
       rate. A STEP is the right stimulus here -- what is being measured is
       the reported number, not the settling -- and the offset is held well
       inside the seeding bound bn/(M*sps) so the loop pulls it in. */
    {
      const double   df  = 4.0e-5; /* cycles/sample at the REAL input rate */
      float         *ltx = malloc ((size_t)(6000.0 * LO_SPS) * sizeof (*ltx));
      int           *lid = malloc (6000 * sizeof (int));
      float complex *lou = malloc (6000 * sizeof (*lou));
      DP_CHECK (ltx && lid && lou);
      if (ltx && lid && lou)
        {
          size_t n = (size_t)(6000.0 * LO_SPS);
          make_mpsk_real (ltx, lid, 2, LO_SPS, 6000u, RFC + df, 30.0, 23u,
                          0.0);
          mpsk_receiver_state_t *rx = mpsk_receiver_create_real (
              2, LO_SPS, RM_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, LO_BN, 0.707,
              0.01, 0.3, RFC, 0, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO);
          DP_CHECK (rx != NULL);
          if (rx)
            {
              (void)mpsk_receiver_steps_real (rx, ltx, n, lou, 6000);
              double got = mpsk_receiver_get_norm_freq (rx);
              printf ("  lo_sps readback: norm_freq %.8f, true %.8f "
                      "(err %.2f%% of df)\n",
                      got, RFC + df, 100.0 * fabs (got - (RFC + df)) / df);
              DP_CHECK (mpsk_receiver_get_lock (rx) > 0.5);
              /* Within 20% of the offset itself. Dropping the 0.5 makes this
                 read RFC + 2*df -- an error of exactly df, five times this
                 bound. */
              DP_CHECK (fabs (got - (RFC + df)) < 0.2 * df);
              mpsk_receiver_destroy (rx);
            }
        }
      free (ltx);
      free (lid);
      free (lou);
    }
  }

  DP_TEST_END ("test_mpsk_receiver_core");
}
