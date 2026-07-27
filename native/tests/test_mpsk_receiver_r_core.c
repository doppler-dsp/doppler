/**
 * @file test_mpsk_receiver_r_core.c
 * @brief Unit tests for the real-IF M-PSK receiver.
 *
 * Tests:
 *   1. Lifecycle / argument validation / getters / reset reproducibility
 *   2. Locks + recovers symbols from a real IF at the design centre, every M
 *   3. The `sps > 2 * m_out` constraint is enforced, not merely documented
 *   4. acq_to_track flips NDA acquisition -> decision-directed tracking
 *   5. The usable-band constraint is on the OCCUPIED band, not the centre
 *   6. Serialized state resumes bit-for-bit, and a clobbered envelope rejects
 *
 * This is the twin of test_mpsk_receiver_core.c and shares its reasoning: the
 * receiver is a matched DDCR plus two loops, so nothing here pins an exact
 * output -- what IS pinned is every property that must hold (symbol error
 * rate, lock, handover, reset reproducibility, the state round-trip).
 *
 * **Everything is measured at the design centre `fc = 0.25` unless the test is
 * specifically about placement.** The R2C halfband bakes in a +fs/4 shift, so
 * fs/4 is where the front end is symmetric and its image rejection is best
 * (past -100 dB across roughly 0.06..0.44, but only -7 dB at 0.01). Measuring
 * off-centre and blaming the receiver is a mistake this project has already
 * made and retracted -- test 5 pins the real behaviour so it is not repeated.
 *
 * Amplitude is 0.5, not 1.0: a cascade that plans a CIC bounds its input to
 * +-1.0 and clips silently past it, costing ~25 dB of EVM that no lock metric
 * reveals. See mpsk_receiver_r_get_clipped().
 */
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "mpsk_receiver_r/mpsk_receiver_r_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

/* 6000 symbols at sps = 16 is 96000 real samples. The settling budget alone is
 * 3000 symbols at the bandwidths used below, so a shorter record would leave
 * no settled window to judge -- see dp_test_settle_syms(). */
#define NSYM 6000u
#define SPS 16.0
#define M_OUT 4u
#define NSAMP ((size_t)(NSYM * (size_t)SPS))
#define TX_AMP 0.5
/* The design centre: the R2C halfband's +fs/4 shift makes this the symmetric,
 * best-rejection point of the front end. */
#define FC_CENTRE 0.25

static int
prbs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return (int)(x & 0xFFFFu);
}

static double
uni (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return ((double)x + 1.0) / 4294967297.0;
}

static double
gauss (uint32_t *st)
{
  double u1 = uni (st), u2 = uni (st);
  return sqrt (-2.0 * log (u1)) * cos (2.0 * M_PI * u2);
}

static double
phi0_for (int m)
{
  return (m == 4) ? (M_PI / 4.0) : 0.0;
}

/* Build a REAL rectangular-pulse M-PSK IF at `fc` cycles/sample with AWGN.
 * The passband signal is Re{s(t) e^{j2pi fc n}} -- taking the real part is the
 * whole difference from the complex twin's stimulus, and it is what an ADC
 * downstream of an analogue mixer actually delivers.
 *
 * `sps` is a PARAMETER, not the SPS macro: test 5 builds at a different
 * oversampling than the rest of the file, and a hardcoded macro here silently
 * hands the receiver a signal at the wrong symbol rate (a 1.6x clock error,
 * which measures as ~-10 dB EVM and reads exactly like a front-end fault).
 * Writes `nsym * sps` samples -- the caller sizes the buffer. */
static void
make_mpsk_real (float *tx, int *idx, int m, double sps, size_t nsym, double fc,
                double snr_db, uint32_t seed, double phi0)
{
  uint32_t st    = seed;
  double   sigma = TX_AMP * sqrt (0.5 / pow (10.0, snr_db / 10.0));
  size_t   isps  = (size_t)sps;
  for (size_t k = 0; k < nsym; k++)
    {
      int ki    = prbs (&st) % m;
      idx[k]    = ki;
      double th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      double sr = TX_AMP * cos (th);
      double si = TX_AMP * sin (th);
      for (size_t j = 0; j < isps; j++)
        {
          size_t n  = k * isps + j;
          double ph = 2.0 * M_PI * fc * (double)n;
          /* Re{(sr + j si) e^{j ph}} = sr cos(ph) - si sin(ph) */
          tx[n] = (float)(sr * cos (ph) - si * sin (ph) + sigma * gauss (&st));
        }
    }
}

static int
decide (float complex y, int m, double phi0)
{
  double th = atan2 ((double)cimagf (y), (double)crealf (y)) - phi0;
  long   k  = lround (th * (double)m / (2.0 * M_PI));
  return (int)((k % m + m) % m);
}

/* Symbol error rate over a SETTLED window, tolerant of the unknown M-fold
 * rotation and of a symbol lag (the cascade's group delay is not a round
 * number of symbols). `settle` comes from dp_test_settle_syms(), never from a
 * fraction of the record -- see that helper for what a fraction costs. */
static double
tail_ser (const float complex *out, size_t nout, const int *idx, int m,
          double phi0, size_t settle)
{
  if (settle + 400 >= nout)
    return 1.0; /* record too short to hold a settled window */
  size_t lo = settle, hi = nout - nout / 8;
  double best = 1.0;
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
              if (((decide (out[i], m, phi0) - idx[j] - rot) % m + m) % m != 0)
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

/* Vary the nine things these tests actually vary; leave the rest at their
 * documented defaults rather than spelling out sixteen positional arguments.
 */
static mpsk_receiver_r_state_t *
RXR (int m, double sps, size_t m_out, int pulse, double bn_carrier,
     int acq_to_track, double lock_thresh, double init_norm_freq,
     size_t warmup_syms)
{
  return mpsk_receiver_r_create (m, sps, m_out, pulse, 0.35, 8, bn_carrier,
                                 0.707, 0.01, acq_to_track, lock_thresh,
                                 init_norm_freq, warmup_syms, 0,
                                 MPSK_RX_NUM_PHASES, MPSK_RX_NDA_TAP_STROBE);
}

int
main (void)
{
  int            _fails = 0;
  float         *tx     = malloc (NSAMP * sizeof (*tx));
  int           *idx    = malloc (NSYM * sizeof (int));
  float complex *out    = malloc (NSYM * sizeof (*out));
  if (!tx || !idx || !out)
    return 1;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle / validation / getters / reset reproducibility
   * ---------------------------------------------------------------- */
  {
    mpsk_receiver_r_state_t *rx
        = RXR (4, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    CHECK (rx != NULL);
    if (rx)
      {
        CHECK (mpsk_receiver_r_get_m (rx) == 4);
        CHECK (fabs (mpsk_receiver_r_get_sps (rx) - SPS) < 1e-12);
        CHECK (mpsk_receiver_r_get_m_out (rx) == M_OUT);
        CHECK (mpsk_receiver_r_get_tracking (rx) == 0);
        CHECK (mpsk_receiver_r_get_clipped (rx) == 0);
        mpsk_receiver_r_destroy (rx);
      }

    /* An invalid order is rejected, not silently accepted. */
    mpsk_receiver_r_state_t *bad
        = RXR (3, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    CHECK (bad == NULL);
    mpsk_receiver_r_destroy (bad);

    /* reset() returns the receiver to a cold start: the same input twice
       across a reset must give byte-identical symbols. */
    make_mpsk_real (tx, idx, 4, SPS, NSYM, FC_CENTRE, 30.0, 11u, phi0_for (4));
    mpsk_receiver_r_state_t *a
        = RXR (4, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    if (a)
      {
        size_t        k1    = mpsk_receiver_r_steps (a, tx, NSAMP, out, NSYM);
        double        f1    = mpsk_receiver_r_get_norm_freq (a);
        float complex first = out[k1 / 2];
        mpsk_receiver_r_reset (a);
        size_t k2 = mpsk_receiver_r_steps (a, tx, NSAMP, out, NSYM);
        CHECK (k1 == k2);
        CHECK (out[k2 / 2] == first);
        CHECK (mpsk_receiver_r_get_norm_freq (a) == f1);
        mpsk_receiver_r_destroy (a);
      }
  }

  /* ---------------------------------------------------------------- *
   * 2. Locks + recovers from a real IF at the design centre, every M
   * ---------------------------------------------------------------- */
  {
    int ms[3] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        int m = ms[mi];
        /* Seeded ON the centre, the loop tracking the residual around it.
           8PSK hands over to the decision-directed loop for the same reason
           the complex twin does: its decision margin is only +-pi/8, so the
           M-th-power discriminator's own jitter dominates. */
        mpsk_receiver_r_state_t *rx
            = RXR (m, SPS, M_OUT, 0, 0.005, m == 8, 0.3, FC_CENTRE, 100);
        CHECK (rx != NULL);
        if (!rx)
          continue;
        make_mpsk_real (tx, idx, m, SPS, NSYM, FC_CENTRE, 30.0,
                        7u + (uint32_t)mi, phi0_for (m));
        size_t k      = mpsk_receiver_r_steps (rx, tx, NSAMP, out, NSYM);
        size_t settle = dp_test_settle_syms (0.01, 0.005);
        double ser    = tail_ser (out, k, idx, m, phi0_for (m), settle);
        CHECK (ser < 0.01);
        /* The lock EMA's noise-only sd is CARRIER_NDA_LOCK_NORM_SD (0.1132) at
           every m, so state the threshold in sigmas: 0.5 is 4.42 sigma, i.e.
           the shipped default's per-look Pfa of 5e-6. */
        CHECK (mpsk_receiver_r_get_lock (rx) > 0.5);
        /* Truth-free corroboration -- a BER alone can false-pass through its
           own lag/rotation search (see dp_sym_test.h).

           TWO assertions, because one of them used to be vacuous. This read
           `evm < -12.0` for every M until 2026-07-27, and the 8PSK scatter
           floor is **-12.9 dB** (dp_test_evm_scatter_floor_db) -- so a
           constellation with no carrier recovery whatsoever satisfied it, and
           the check had no discriminating power at M = 8 at all.

             - the absolute gate is the quality bar (measured -18.1 / -17.7 /
               -18.2 dB at M = 2 / 4 / 8, so ~2 dB of margin);
             - the floor-relative gate is what makes the absolute one provably
               non-vacuous, and it is the one that fires first if another M is
               ever added -- at M = 16 the floor rises to -19.0 dB and a fixed
               -16.0 would silently go vacuous again.

           Note how little room there is at M = 8: 5.3 dB between a healthy
           receiver and pure noise. The self-referenced EVM cannot carry this
           verdict alone at high M, which is why `ser < 0.01` above is the
           primary check and this is corroboration. */
        if (k > settle)
          {
            double evm = dp_test_evm_db_hard_m (out + settle, k - settle, m);
            double flr = dp_test_evm_scatter_floor_db (m);
            printf ("  M=%d: evm=%6.1f dB (scatter floor %5.1f, "
                    "margin %4.1f dB)\n",
                    m, evm, flr, flr - evm);
            CHECK (evm < -16.0);
            CHECK (evm < flr - 3.0);
          }
        CHECK (mpsk_receiver_r_get_clipped (rx) == 0);
        mpsk_receiver_r_destroy (rx);
      }
  }

  /* ---------------------------------------------------------------- *
   * 3. `sps > 2 * m_out` is enforced
   *
   * The cascade behind the R2C halfband runs at twice the overall rate, so the
   * terminal stage needs rate = m_out/sps < 0.5. This is the one constraint
   * the real path has that the complex path does not (which needs only
   * sps >= m_out), and a documented-but-unenforced constraint is how a caller
   * gets a silently wrong receiver instead of an error.
   * ---------------------------------------------------------------- */
  {
    /* sps == 2 * m_out exactly: rejected (strictly greater is required). */
    mpsk_receiver_r_state_t *eq = RXR (4, 8.0, 4, 0, 0.005, 0, 0.5, 0.0, 100);
    CHECK (eq == NULL);
    mpsk_receiver_r_destroy (eq);

    /* Below it: rejected. */
    mpsk_receiver_r_state_t *lo = RXR (4, 6.0, 4, 0, 0.005, 0, 0.5, 0.0, 100);
    CHECK (lo == NULL);
    mpsk_receiver_r_destroy (lo);

    /* Just above it: accepted. */
    mpsk_receiver_r_state_t *ok = RXR (4, 8.5, 4, 0, 0.005, 0, 0.5, 0.0, 100);
    CHECK (ok != NULL);
    mpsk_receiver_r_destroy (ok);
  }

  /* ---------------------------------------------------------------- *
   * 4. acq_to_track flips NDA acquisition -> decision-directed tracking
   * ---------------------------------------------------------------- */
  {
    /* lock_thresh 0.65 is 5.74 sigma -- deliberately above the 0.5 default so
       the declare is unambiguous, and matching the complex twin's handover
       case so the two measure the same operating point. */
    mpsk_receiver_r_state_t *rx
        = RXR (4, SPS, M_OUT, 0, 0.01, 1, 0.65, FC_CENTRE, 200);
    CHECK (rx != NULL);
    if (rx)
      {
        make_mpsk_real (tx, idx, 4, SPS, NSYM, FC_CENTRE + 0.0005, 30.0, 33u,
                        phi0_for (4));
        size_t k = mpsk_receiver_r_steps (rx, tx, NSAMP, out, NSYM);
        CHECK (mpsk_receiver_r_get_tracking (rx) == 1);
        double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                               dp_test_settle_syms (0.01, 0.01));
        CHECK (ser < 0.01);
        mpsk_receiver_r_destroy (rx);
      }
  }

  /* ---------------------------------------------------------------- *
   * 5. The usable band constrains the OCCUPIED band, not the centre
   *
   * The R2C halfband's image rejection collapses at the band edges, and a
   * rectangular pulse spans fc +- 1/sps to its first null. So a centre that
   * looks comfortably inside the band can still have its skirt reach an edge,
   * where the folded image lands on the wanted signal. THIS is what a
   * "receiver bug at low oversampling" actually is; pinning both halves is
   * what stops it being misdiagnosed again.
   * ---------------------------------------------------------------- */
  {
    /* sps = 10 -> the pulse is +-0.1 wide, so an IF at 0.10 puts its lower
       skirt on DC, where rejection is only about -7 dB. */
    const double sps_edge   = 10.0;
    size_t       nsamp_edge = (size_t)(NSYM * (size_t)sps_edge);
    size_t       settle     = dp_test_settle_syms (0.01, 0.005);

    mpsk_receiver_r_state_t *edge
        = RXR (4, sps_edge, M_OUT, 0, 0.005, 0, 0.5, 0.10, 100);
    mpsk_receiver_r_state_t *ctr
        = RXR (4, sps_edge, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    CHECK (edge != NULL && ctr != NULL);
    if (edge && ctr)
      {
        /* Effectively noiseless (50 dB), because this test isolates
           PLACEMENT: at 30 dB the AWGN swamps the very effect being measured
           and both cases read the same -10 dB. Enough noise remains to break
           the measure-zero unstable equilibrium an M-th-power loop would sit
           at on a perfectly clean, zero-offset input. The Python twin
           (test_usable_band_is_the_input_constraint) is fully noiseless for
           the same reason. */
        /* phi0 = 0, NOT the pi/4 QPSK convention, and the choice is
           load-bearing. The leaked image is the signal's CONJUGATE, so how
           much it hurts depends on whether each symbol's conjugate is itself
           or a different symbol. Unrotated QPSK {0, pi/2, pi, 3pi/2} pairs
           two symbols with themselves and the image adds coherently; the
           pi/4-rotated set maps every symbol onto a DIFFERENT one and the
           damage largely averages out. Measured at this geometry: -4.4 dB EVM
           unrotated vs -20.1 dB rotated -- a 16 dB difference from the
           constellation phase alone. Unrotated is both the worst case and what
           the Python twin uses, so the two pin the same thing. */
        make_mpsk_real (tx, idx, 4, sps_edge, NSYM, 0.10, 50.0, 51u, 0.0);
        size_t ke = mpsk_receiver_r_steps (edge, tx, nsamp_edge, out, NSYM);
        double evm_edge
            = (ke > settle)
                  ? dp_test_evm_db_hard_m (out + settle, ke - settle, 4)
                  : 0.0;

        make_mpsk_real (tx, idx, 4, sps_edge, NSYM, FC_CENTRE, 50.0, 51u, 0.0);
        size_t kc = mpsk_receiver_r_steps (ctr, tx, nsamp_edge, out, NSYM);
        double evm_ctr = (kc > settle) ? dp_test_evm_db_hard_m (out + settle,
                                                                kc - settle, 4)
                                       : 0.0;

        /* The SAME geometry -- only the placement differs. The centre must be
           clean and the edge visibly degraded.

           The MAGNITUDE here is deliberately weak (2 dB), and the reason is
           NOT the receiver. The leaked image is the signal's own conjugate, so
           the resulting ISI is a deterministic function of the symbol
           SEQUENCE, and the penalty varies enormously with it. Measured over
           8 seeds at this exact geometry, noiseless, identical receiver:

               symbol source              min   median    max
               pn_core MLS, length 64     2.7     11.9   18.5   dB
               numpy PCG64              -11.1      2.8   18.6   dB
               this file's xorshift32      --       2.9     --   dB

           So NO symbol source reliably excites it, and a test asserting a
           large penalty on one sequence is asserting a property of that
           sequence. An earlier version of this comment claimed the xorshift
           source under-excited the impairment by 15 dB relative to PCG64;
           that compared ONE seed against ONE seed and the PCG64 draw happened
           to be favourable -- on medians PCG64 is the worse source. Corrected.

           Likely mechanism, if this is ever tightened: an m-sequence is white
           to SECOND order but its higher-order joint statistics are
           constrained by the linear recurrence, and this impairment depends on
           symbol PAIRS -- so second-order whiteness is not the property that
           matters, which is why "uniform, decorrelated at lag 1" says nothing
           here.

           This test therefore pins the always-true form: the centre is clean,
           the edge is worse. **The Python twin
           (test_usable_band_is_the_input_constraint) asserts 10 dB on a single
           seed and is fragile for exactly this reason** -- it should average
           over seeds. If the edge ever matches the centre, the halfband's edge
           behaviour changed and the documented input constraint needs
           re-measuring, not deleting. */
        CHECK (evm_ctr < -15.0);
        CHECK (evm_edge > evm_ctr + 2.0);
        printf ("  usable band: EVM %.1f dB at fc=0.10 vs %.1f dB at fs/4\n",
                evm_edge, evm_ctr);
        mpsk_receiver_r_destroy (edge);
        mpsk_receiver_r_destroy (ctr);
      }
  }

  /* ---------------------------------------------------------------- *
   * 6. Serialized state resumes bit-for-bit; a bad envelope rejects
   * ---------------------------------------------------------------- */
  {
    make_mpsk_real (tx, idx, 4, SPS, NSYM, FC_CENTRE, 30.0, 71u, phi0_for (4));
    size_t half = NSAMP / 2;

    mpsk_receiver_r_state_t *ref
        = RXR (4, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    mpsk_receiver_r_state_t *src
        = RXR (4, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    mpsk_receiver_r_state_t *dst
        = RXR (4, SPS, M_OUT, 0, 0.005, 0, 0.5, FC_CENTRE, 100);
    CHECK (ref && src && dst);
    if (ref && src && dst)
      {
        float complex *ref_out = malloc (NSYM * sizeof (*ref_out));
        if (ref_out)
          {
            /* Reference: both halves through one instance. */
            (void)mpsk_receiver_r_steps (ref, tx, half, ref_out, NSYM);
            size_t rn = mpsk_receiver_r_steps (ref, tx + half, NSAMP - half,
                                               ref_out, NSYM);

            /* Split: first half through `src`, hand its state to `dst`, finish
               there. The second halves must be identical. */
            (void)mpsk_receiver_r_steps (src, tx, half, out, NSYM);
            size_t nb   = mpsk_receiver_r_state_bytes (src);
            void  *blob = malloc (nb);
            CHECK (nb > 0 && blob != NULL);
            if (blob)
              {
                mpsk_receiver_r_get_state (src, blob);
                CHECK (mpsk_receiver_r_set_state (dst, blob) == DP_OK);
                size_t dn = mpsk_receiver_r_steps (dst, tx + half,
                                                   NSAMP - half, out, NSYM);
                CHECK (dn == rn);
                int same = (dn == rn);
                for (size_t i = 0; same && i < dn; i++)
                  if (out[i] != ref_out[i])
                    same = 0;
                CHECK (same); /* bit-for-bit resume */

                /* A clobbered envelope must be REJECTED, never
                   reinterpreted. */
                ((unsigned char *)blob)[0] ^= 0xFFu;
                CHECK (mpsk_receiver_r_set_state (dst, blob)
                       == DP_ERR_INVALID);
                free (blob);
              }
            free (ref_out);
          }
        mpsk_receiver_r_destroy (ref);
        mpsk_receiver_r_destroy (src);
        mpsk_receiver_r_destroy (dst);
      }
  }

  free (tx);
  free (idx);
  free (out);
  if (_fails)
    {
      fprintf (stderr, "test_mpsk_receiver_r_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_mpsk_receiver_r_core PASSED\n");
  return 0;
}
