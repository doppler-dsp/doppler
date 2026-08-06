/**
 * @file test_mpsk_receiver_core.c
 * @brief Unit tests for the pulse-shaped M-PSK receiver.
 *
 * Tests:
 *   1. Lifecycle / argument validation / getters / reset reproducibility
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
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
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

#define NSYM 6000
#define SPS 8.0
#define NSAMP ((size_t)(NSYM * (size_t)SPS))
/* Headroom under the CIC's +-1.0 input bound (see the file header). */
#define TX_AMP 0.5f
/* Terminal outputs per symbol: the old `n`, now the cascade's own. */
#define M_OUT 4

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

/* Uniform (0,1] from the PRBS, then a Box-Muller standard normal. */
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
      int ki           = prbs (&st) % m;
      idx[k]           = ki;
      double        th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      float complex s  = TX_AMP * ((float)cos (th) + (float)sin (th) * I);
      for (size_t j = 0; j < (size_t)SPS; j++)
        {
          size_t        n  = k * (size_t)SPS + j;
          double        ph = 2.0 * M_PI * foff * (double)n;
          float complex c  = (float)cos (ph) + (float)sin (ph) * I;
          float complex w  = (float)(sigma * gauss (&st))
                             + (float)(sigma * gauss (&st)) * I;
          tx[n]            = s * c + w;
        }
    }
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
    int acq_to_track, double lock_thresh, double init_norm_freq,
    size_t warmup_syms)
{
  return mpsk_receiver_create (m, sps, m_out, pulse, 0.35, 8, bn_carrier,
                               0.707, 0.01, acq_to_track, lock_thresh,
                               init_norm_freq, warmup_syms, 0,
                               MPSK_RX_NUM_PHASES, MPSK_RX_NDA_TAP_STROBE);
}

int
main (void)
{
  int            _fails = 0;
  float complex *tx     = malloc (NSAMP * sizeof (*tx));
  int           *idx    = malloc (NSYM * sizeof (int));
  float complex *out    = malloc (NSYM * sizeof (*out));

  /* 1. Lifecycle / validation / getters / reset reproducibility */
  {
    /* invalid args -> NULL */
    CHECK (RX (3, SPS, M_OUT, 0, 0.01, 0, 0.5, 0.0, 100) == NULL); /* bad m  */
    CHECK (RX (4, SPS, 3, 0, 0.01, 0, 0.5, 0.0, 100) == NULL); /* m_out odd  */
    CHECK (RX (4, SPS, 16, 0, 0.01, 0, 0.5, 0.0, 100) == NULL); /* m_out > 8 */
    CHECK (RX (4, 2.0, 4, 0, 0.01, 0, 0.5, 0.0, 100)
           == NULL); /* sps < m_out: the terminal stage would interpolate */
    CHECK (RX (4, 0.0, 4, 0, 0.01, 0, 0.5, 0.0, 100) == NULL); /* sps == 0  */
    CHECK (RX (4, SPS, M_OUT, 2, 0.01, 0, 0.5, 0.0, 100)
           == NULL); /* bad pulse */

    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0, 100);
    CHECK (rx != NULL);
    if (!rx)
      return 1;
    CHECK (mpsk_receiver_get_m (rx) == 4);
    CHECK (mpsk_receiver_get_sps (rx) == SPS);
    CHECK (mpsk_receiver_get_m_out (rx) == M_OUT);
    CHECK (mpsk_receiver_get_tracking (rx) == 0);
    CHECK (mpsk_receiver_get_clipped (rx) == 0); /* nothing pushed yet */

    make_mpsk (tx, idx, 4, 0.0008, 35.0, 99u);
    size_t k1 = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double f1 = mpsk_receiver_get_norm_freq (rx);
    mpsk_receiver_reset (rx);
    CHECK (mpsk_receiver_get_tracking (rx) == 0);
    size_t k2 = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    CHECK (k1 == k2);
    CHECK (mpsk_receiver_get_norm_freq (rx) == f1); /* reset is reproducible */
    mpsk_receiver_destroy (rx);
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
                                          0.005, m == 8, 0.3, fs[fi], 100);
          make_mpsk (tx, idx, m, fs[fi], 30.0, 7u + (uint32_t)(mi * 4 + fi));
          size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
          double ser = tail_ser (out, k, idx, m, phi0_for (m),
                                 dp_test_settle_syms (0.01, 0.005));
          CHECK (ser < 0.01); /* clean recovery       */
          /* The lock EMA's noise-only sd is CARRIER_NDA_LOCK_NORM_SD (0.1132)
             at EVERY m, so a threshold is meaningfully stated in sigmas. The
             shipped default lock_thresh of 0.5 is 4.42 sigma (per-look Pfa
             5e-6); assert that here rather than the old 0.15, which was only
             1.3 sigma -- a value a noise-only run reaches routinely. */
          CHECK (mpsk_receiver_get_lock (rx) > 0.5);
          mpsk_receiver_destroy (rx);
        }
  }

  /* 3. RRC matched filter locks + recovers (QPSK). A rectangular signal
   * through the RRC matched filter still acquires + recovers (the loop is
   * pulse-robust; the Python suite drives a true RRC-shaped TX). */
  {
    mpsk_receiver_state_t *rx
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_RRC, 0.005, 0, 0.5, 0.0, 200);
    CHECK (rx != NULL);
    make_mpsk (tx, idx, 4, 0.0, 30.0, 21u);
    size_t k   = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                           dp_test_settle_syms (0.01, 0.005));
    CHECK (ser < 0.02);
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 1, 0.65, 0.0, 200);
    make_mpsk (tx, idx, 4, 0.0005, 30.0, 33u);
    size_t k = mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    CHECK (mpsk_receiver_get_tracking (rx) == 1); /* handed over */
    double ser = tail_ser (out, k, idx, 4, phi0_for (4),
                           dp_test_settle_syms (0.01, 0.01));
    CHECK (ser < 0.01);

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
    CHECK (mpsk_receiver_get_tracking (rx) == 0); /* dropped back */
    mpsk_receiver_set_norm_freq (rx, 0.0005);     /* acq re-seed */
    make_mpsk (tx, idx, 4, 0.0005, 30.0, 45u);
    (void)mpsk_receiver_steps (rx, tx, NSAMP, out, NSYM);
    CHECK (mpsk_receiver_get_tracking (rx) == 1); /* re-declared */
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
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0, 100);
    mpsk_receiver_state_t *b
        = RX (4, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0, 100);
    CHECK (a != NULL && b != NULL);
    (void)mpsk_receiver_steps (a, tx, 256, out, 32);
    DP_STATE_ROUNDTRIP_TEST (mpsk_receiver, a, b);
    CHECK (b->l.sym_count == a->l.sym_count);
    /* the timing loop's strobe phase is the child that must resume */
    CHECK (b->l.timing.out_count == a->l.timing.out_count);
    CHECK (b->l.timing.prime_left == a->l.timing.prime_left);
    CHECK (b->l.sym_rot == a->l.sym_rot);
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
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0, 100);
    CHECK (tlm != NULL && a != NULL);
    CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx", 1) == DP_OK);
    CHECK (dp_tlm_probe_id (tlm, "rx.lock") == a->l.tlm.id_lock);
    CHECK (dp_tlm_probe_id (tlm, "rx.tracking") == a->l.tlm.id_tracking);
    CHECK (dp_tlm_probe_id (tlm, "rx.car.e") == a->l.tlm.id_e);
    CHECK (dp_tlm_probe_id (tlm, "rx.car.freq") == a->l.tlm.id_freq);
    CHECK (dp_tlm_probe_id (tlm, "rx.car.locked") == a->l.tlm.id_locked);
    CHECK (dp_tlm_probe_id (tlm, "rx.sync.e") == a->l.timing.tlm.id_e);
    CHECK (dp_tlm_probe_id (tlm, "rx.sync.locked")
           == a->l.timing.tlm.id_locked);
    CHECK (dp_tlm_probe_id (tlm, "rx.sync.mu") == a->l.timing.tlm.id_mu);
    CHECK (dp_tlm_probe_count (tlm) == 11);

    size_t n_sym = mpsk_receiver_steps (a, tx, 512, out, 80);
    CHECK (n_sym > 0);
    dp_tlm_rec_t recs[2048];
    size_t       n_rec = dp_tlm_read (tlm, 2048, recs, 2048);
    /* lock + tracking + car(e,freq,locked) + sync(e,ctrl,rate,lock,locked,mu):
     * eleven records per recovered symbol, all flushed at the strobe. The arm
     * AGC is not attached -- it is an internal normaliser on the
     * discriminator's input, not a receiver diagnostic. */
    CHECK (n_rec == 11 * n_sym);

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
            CHECK (v >= 0.0 && v < 1.0);
            mn = v < mn ? v : mn;
            mx = v > mx ? v : mx;
            n_mu++;
          }
      CHECK (n_mu == (int)n_sym);
      CHECK (mx > mn);
    }

    /* Detach cascades to both embedded loops (and the AGC). */
    CHECK (mpsk_receiver_set_telemetry (a, NULL, "rx", 1) == DP_OK);
    CHECK (a->l.tlm.ctx == NULL && a->l.timing.tlm.ctx == NULL);
    (void)mpsk_receiver_steps (a, tx, 512, out, 80);
    CHECK (dp_tlm_read (tlm, 2048, recs, 2048) == 0);

    /* bits() flushes telemetry too (the guarded in-loop path). */
    CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx2", 1) == DP_OK);
    uint8_t bit_out[128];
    size_t  n_bits = mpsk_receiver_bits (a, tx, 512, bit_out, 128);
    CHECK (n_bits > 0);
    CHECK (dp_tlm_read (tlm, 2048, recs, 2048) > 0);

    /* A full probe table fails the attach whole (receiver detached). */
    char pname[DP_TLM_NAME_MAX];
    for (size_t i = 0; dp_tlm_probe_count (tlm) < DP_TLM_MAX_PROBES; i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm, pname, 1);
      }
    mpsk_receiver_state_t *b
        = RX (2, SPS, M_OUT, MPSK_RX_PULSE_IANDD, 0.01, 0, 0.5, 0.0, 100);
    CHECK (b != NULL);
    CHECK (mpsk_receiver_set_telemetry (b, tlm, "full", 1) == DP_ERR_INVALID);
    CHECK (b->l.tlm.ctx == NULL);

    /* Partial registration failure unwinds: leave exactly six slots — the
     * receiver's own five probes fit, the six-probe timing forward cannot,
     * and the whole attach fails with everything detached again. */
    dp_tlm_t *tlm2 = dp_tlm_create (256);
    CHECK (tlm2 != NULL);
    for (size_t i = 0;
         dp_tlm_probe_count (tlm2) < (size_t)(DP_TLM_MAX_PROBES - 6); i++)
      {
        (void)snprintf (pname, sizeof (pname), "fill%zu", i);
        (void)dp_tlm_probe (tlm2, pname, 1);
      }
    CHECK (mpsk_receiver_set_telemetry (b, tlm2, "uw", 1) == DP_ERR_INVALID);
    CHECK (b->l.tlm.ctx == NULL && b->l.timing.tlm.ctx == NULL);
    dp_tlm_destroy (tlm2);

    mpsk_receiver_destroy (b);
    mpsk_receiver_destroy (a);
    dp_tlm_destroy (tlm);
  }

  if (_fails)
    {
      fprintf (stderr, "test_mpsk_receiver_core FAILED (%d)\n", _fails);
      return 1;
    }

  printf ("test_mpsk_receiver_core PASSED\n");
  return 0;
}
