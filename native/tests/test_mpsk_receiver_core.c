/**
 * @file test_mpsk_receiver_core.c
 * @brief Unit tests for the pulse-shaped M-PSK receiver.
 *
 * Tests:
 *   1. Lifecycle / argument validation / getters / reset reproducibility
 *   2. Locks + recovers symbols under a carrier offset (I&D), every M -> SER 0
 *   3. RRC matched filter locks + recovers
 *   4. acq_to_track flips the loop from NDA acquisition to decision tracking
 */
#include "dp_state_test.h"
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
#define SPS 8

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
  uint32_t st    = seed;
  double   phi0  = phi0_for (m);
  double   sigma = sqrt (0.5 / pow (10.0, snr_db / 10.0)); /* per quadrature */
  for (size_t k = 0; k < NSYM; k++)
    {
      int ki           = prbs (&st) % m;
      idx[k]           = ki;
      double        th = 2.0 * M_PI * (double)ki / (double)m + phi0;
      float complex s  = (float)cos (th) + (float)sin (th) * I;
      for (size_t j = 0; j < SPS; j++)
        {
          size_t        n  = k * SPS + j;
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
static double
tail_ser (const float complex *out, size_t nout, const int *idx, int m,
          double phi0)
{
  size_t lo = nout / 3, hi = 2 * nout / 3;
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

int
main (void)
{
  int            _fails = 0;
  float complex *tx     = malloc (NSYM * SPS * sizeof (*tx));
  int           *idx    = malloc (NSYM * sizeof (int));
  float complex *out    = malloc (NSYM * sizeof (*out));

  /* 1. Lifecycle / validation / getters / reset reproducibility */
  {
    /* invalid args -> NULL */
    CHECK (mpsk_receiver_create (3, 8, 4, 0, 0.35, 8, 0.01, 0.707, 0.01, 0,
                                 0.5, 0.0, 100, 0)
           == NULL); /* bad m */
    CHECK (mpsk_receiver_create (4, 8, 3, 0, 0.35, 8, 0.01, 0.707, 0.01, 0,
                                 0.5, 0.0, 100, 0)
           == NULL); /* sps % n != 0 */
    CHECK (mpsk_receiver_create (4, 0, 4, 0, 0.35, 8, 0.01, 0.707, 0.01, 0,
                                 0.5, 0.0, 100, 0)
           == NULL); /* sps == 0 */
    CHECK (mpsk_receiver_create (4, 8, 4, 2, 0.35, 8, 0.01, 0.707, 0.01, 0,
                                 0.5, 0.0, 100, 0)
           == NULL); /* bad pulse */

    mpsk_receiver_state_t *rx
        = mpsk_receiver_create (4, SPS, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.707, 0.01, 0, 0.5, 0.0, 100, 0);
    CHECK (rx != NULL);
    if (!rx)
      return 1;
    CHECK (mpsk_receiver_get_m (rx) == 4);
    CHECK (mpsk_receiver_get_sps (rx) == SPS);
    CHECK (mpsk_receiver_get_n (rx) == 4);
    CHECK (mpsk_receiver_get_tracking (rx) == 0);

    make_mpsk (tx, idx, 4, 0.0008, 35.0, 99u);
    size_t k1 = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
    double f1 = mpsk_receiver_get_norm_freq (rx);
    mpsk_receiver_reset (rx);
    CHECK (mpsk_receiver_get_tracking (rx) == 0);
    size_t k2 = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
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
          int                    m  = ms[mi];
          mpsk_receiver_state_t *rx = mpsk_receiver_create (
              m, SPS, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.02, 0.707, 0.01, 0,
              0.5, fs[fi], 100, 0);
          make_mpsk (tx, idx, m, fs[fi], 30.0, 7u + (uint32_t)(mi * 4 + fi));
          size_t k   = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
          double ser = tail_ser (out, k, idx, m, phi0_for (m));
          CHECK (ser < 0.01); /* clean recovery       */
          CHECK (mpsk_receiver_get_lock (rx) > 0.15); /* locked, lock > 0 */
          mpsk_receiver_destroy (rx);
        }
  }

  /* 3. RRC matched filter locks + recovers (QPSK). A rectangular signal
   * through the RRC matched filter still acquires + recovers (the loop is
   * pulse-robust; the Python suite drives a true RRC-shaped TX). */
  {
    mpsk_receiver_state_t *rx
        = mpsk_receiver_create (4, SPS, 4, MPSK_RX_PULSE_RRC, 0.35, 8, 0.02,
                                0.707, 0.005, 0, 0.5, 0.0, 200, 0);
    CHECK (rx != NULL);
    make_mpsk (tx, idx, 4, 0.0, 30.0, 21u);
    size_t k   = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
    double ser = tail_ser (out, k, idx, 4, phi0_for (4));
    CHECK (ser < 0.02);
    mpsk_receiver_destroy (rx);
  }

  /* 4. acq_to_track flips NDA acquisition -> decision-directed tracking */
  {
    mpsk_receiver_state_t *rx
        = mpsk_receiver_create (4, SPS, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.03,
                                0.707, 0.01, 1, 0.4, 0.0, 200, 0);
    make_mpsk (tx, idx, 4, 0.0005, 30.0, 33u);
    size_t k = mpsk_receiver_steps (rx, tx, NSYM * SPS, out, NSYM);
    CHECK (mpsk_receiver_get_tracking (rx) == 1); /* handed over */
    double ser = tail_ser (out, k, idx, 4, phi0_for (4));
    CHECK (ser < 0.01);
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
        = mpsk_receiver_create (4, 8, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.707, 0.01, 0, 0.5, 0.0, 100, 0);
    mpsk_receiver_state_t *b
        = mpsk_receiver_create (4, 8, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.707, 0.01, 0, 0.5, 0.0, 100, 0);
    CHECK (a != NULL && b != NULL);
    (void)mpsk_receiver_steps (a, tx, 256, out, 32);
    DP_STATE_ROUNDTRIP_TEST (mpsk_receiver, a, b);
    CHECK (b->sym_count == a->sym_count);
    CHECK (b->sync.timing.phase == a->sync.timing.phase); /* symsync child */
    CHECK (b->sym_rot == a->sym_rot);
    mpsk_receiver_destroy (a);
    mpsk_receiver_destroy (b);
  }

  /* telemetry attach — the receiver's lock probe + the forwarded symsync
   * probes: four records per emitted symbol; detach cascades. */
  {
    float complex tx[512], out[80];
    for (int i = 0; i < 512; i++)
      tx[i] = ((i / 8) % 2 ? 1.0f : -1.0f) + 0.0f * I; /* BPSK, sps=8 */
    dp_tlm_t              *tlm = dp_tlm_create (4096);
    mpsk_receiver_state_t *a
        = mpsk_receiver_create (2, 8, 4, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01,
                                0.707, 0.01, 0, 0.5, 0.0, 100, 0);
    CHECK (tlm != NULL && a != NULL);
    CHECK (mpsk_receiver_set_telemetry (a, tlm, "rx", 1) == DP_OK);
    CHECK (dp_tlm_lookup (tlm, "rx.lock") == a->tlm_id_lock);
    CHECK (dp_tlm_lookup (tlm, "rx.sync.e") == a->sync.tlm.id_e);
    CHECK (dp_tlm_probe_count (tlm) == 4);

    size_t n_sym = mpsk_receiver_steps (a, tx, 512, out, 80);
    CHECK (n_sym > 0);
    dp_tlm_rec_t recs[1024];
    size_t       n_rec = dp_tlm_read (tlm, recs, 1024);
    CHECK (n_rec == 4 * n_sym); /* lock + e + freq + rate per symbol */

    /* Detach cascades to the embedded timing loop. */
    CHECK (mpsk_receiver_set_telemetry (a, NULL, "rx", 1) == DP_OK);
    CHECK (a->tlm_ctx == NULL && a->sync.tlm.ctx == NULL);
    (void)mpsk_receiver_steps (a, tx, 512, out, 80);
    CHECK (dp_tlm_read (tlm, recs, 1024) == 0);

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
