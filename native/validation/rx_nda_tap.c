/**
 * @file rx_nda_tap.c
 * @brief The four NDA carrier taps, ranked by the only thing that ranks them:
 *        how much of a KNOWN frequency offset each one actually removes.
 *
 * `nda_tap` chooses where MpskReceiver's M-th-power carrier discriminator
 * reads (mpsk_rx_loops.h). Each tap claims a different pull-in range and a
 * different noise bandwidth, and `preterm` claims to beat the shipped
 * `strobe` default outright. Nothing ran on any tap but `strobe` until this
 * harness: every C test and every Python test constructs
 * `MPSK_RX_NDA_TAP_STROBE`, so three of the four taps were prose.
 *
 * ## Why the measurement is `acquired`, not `|f_err|`
 *
 * The obvious experiment — sit at the design centre and read
 * `mpsk_receiver_get_norm_freq()` back — **cannot rank these taps, and ranks
 * them confidently anyway.** At zero offset the correct answer IS zero, so a
 * carrier loop that never steers reports a perfect error, and reports it more
 * perfectly than a loop that works: a real loop shows its own jitter (~1e-2
 * Hz here), a dead loop shows the double-precision zero it was initialised
 * with (~1e-9 Hz). Ranked on that number the deadest loop wins by six orders
 * of magnitude and the ranking is exactly inverted.
 *
 * That is not hypothetical. It is how this tap's headline number was
 * originally produced, and the inversion survived into a commit message
 * (`strobe 2.3e-03 Hz` vs `preterm 1.9e-09 Hz`) because the two numbers are
 * genuinely what those two loops print at 0 Hz offset. Reproduced here as the
 * `foff = 0` row of the full sweep, kept deliberately so the trap is visible
 * rather than merely warned about.
 *
 * So every gate below drives a **nonzero, known** offset and scores
 *
 *     acquired = (norm_freq read back) / (offset applied)
 *
 * where 1.0 is full acquisition and 0.0 is a loop that did not move. A dead
 * loop now scores 0.0 instead of winning, and `|f_err|` becomes a meaningful
 * tie-break between taps that all actually acquired.
 *
 * ## What this harness does NOT find: any advantage of `preterm` over
 * ## `strobe`
 *
 * Measured, and recorded here because the opposite was claimed: once every
 * tap is scored on a real offset, `preterm`'s residual `|f_err|` is the SAME
 * ORDER as `strobe`'s and `mf_all`'s at every rate ratio in the sweep (at
 * Fs/Rs = 10000: 1.8e-03 vs 6.2e-04 vs 1.5e-03 Hz — `preterm` is slightly
 * WORSE than the default). That is the expected result and not a defect:
 * three taps carrying the same `bn_carrier` over the same signal settle to
 * the same loop jitter, and the tap point does not change it. The "six orders
 * of magnitude" figure was the zero-offset artifact above and nothing else.
 *
 * Timing-independence does not separate them either: `strobe` still acquires
 * fully with `bn_timing` driven to 0 against a half-sample offset, so the one
 * structural difference the design doc names is not observable in carrier
 * acquisition at these operating points.
 *
 * The claim this file therefore gates is the narrow one that IS true and IS
 * large: `preterm` acquires at rate ratios where `lo_arm` — the tap §3.3 says
 * it supersedes — cannot acquire at all. Whether `preterm` earns its place
 * against `strobe` is an open question this harness answers "no evidence yet"
 * (doppler#766); it is not gated in either direction.
 *
 * ## The offset, and why it is quoted per SYMBOL
 *
 * `RX_NDA_FOFF_SYM` = 0.002 cycles/symbol = 0.4x the loop bandwidth
 * (`bn_carrier` = 0.005/symbol), and far inside the narrowest tap's stated
 * ceiling of `Rs/(2M)` = 0.25 cycles/symbol at M = 2. Every tap can therefore
 * see it and every tap has time to pull it in, so a tap that does not is
 * failing at its own job rather than being asked for range it never claimed.
 *
 * Quoting it per symbol rather than in Hz is what makes the sps sweep mean
 * something: the same normalised offset at every rate ratio asks each tap the
 * same question, which matters most for `preterm`, whose update rate is a
 * PLANNER OUTCOME (`bank_sps`) rather than a construction constant. A tap
 * whose loop is mis-sized against its own planned rate passes at one ratio
 * and fails at another, so one geometry is not evidence.
 *
 * Usage:
 *   rx_nda_tap            full sweep, prints the tables
 *   rx_nda_tap --check    fast CI gate
 */
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "mpsk_receiver_r/mpsk_receiver_r_core.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** @brief Transmit amplitude. A cascade that plans a CIC bounds its input to
 * +-1.0 and clips silently past it, so every run asserts get_clipped() == 0
 * rather than trusting the level (mpsk_ber_common.h makes the same choice). */
#define RX_NDA_AMP 0.5

/** @brief The offset every gate applies, cycles per SYMBOL. See the file
 * docstring: 0.4x bn_carrier, and 1/125th of the narrowest tap's ceiling. */
#define RX_NDA_FOFF_SYM 0.002

/** @brief Carrier loop noise bandwidth, per symbol. */
#define RX_NDA_BN 0.005

/** @brief Symbols per run. Settling is ~5/bn = 1000 symbols; twice that
 * leaves the reading firmly in the steady state at every tap. */
#define RX_NDA_NSYM 2000u

/** @brief Terminal outputs per symbol. */
#define RX_NDA_M_OUT 2u

/** @brief Fraction of the offset a working tap must remove. */
#define RX_NDA_ACQ_MIN 0.90

/** @brief mean|Im| / mean|Re| a de-rotated BPSK constellation must beat. An
 * unacquired one sits at ~1.0 (the spin puts equal energy on both rails). */
#define RX_NDA_DEROT_MAX 0.05

static const char *RX_NDA_NAMES[4]
    = { "strobe", "mf_all", "lo_arm", "preterm" };

/** @brief One tap's outcome at one geometry. */
typedef struct
{
  double acquired; /**< fraction of the applied offset removed.       */
  double ferr_hz;  /**< residual |f_err|, Hz, at the quoted Fs.       */
  double lock;     /**< carrier lock metric at the end of the run.    */
  double re;       /**< mean |Re| of the settled symbols.             */
  double im;       /**< mean |Im| of the settled symbols.             */
  int    clipped;  /**< front end clipped: the reading is worthless.  */
  int    refused;  /**< create() returned NULL.                       */
} rx_nda_result_t;

/* xorshift32 — the symbol source, as in mpsk_ber_common.h. A plain PRNG and
 * deliberately not pn_core: an MLS contains a run of L identical bits, and at
 * 1 bit/symbol BPSK that stalls the Gardner TED. Bounded run length is what a
 * timing loop needs, and i.i.d. uniform symbols are what provide it. */
static uint32_t
rx_nda_rng (uint32_t *s)
{
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}

static double
rx_nda_uni (uint32_t *s)
{
  return ((double)rx_nda_rng (s) + 1.0) / 4294967297.0;
}

static double
rx_nda_gauss (uint32_t *s)
{
  return sqrt (-2.0 * log (rx_nda_uni (s)))
         * cos (2.0 * M_PI * rx_nda_uni (s));
}

/**
 * @brief Run one tap at one rate ratio against a known offset.
 *
 * NRZ BPSK (`MPSK_RX_PULSE_IANDD`, the rectangular integrate-and-dump) at
 * @p sps samples per symbol, offset by `RX_NDA_FOFF_SYM / sps` cycles per
 * sample, with the receiver centred at 0 so the whole offset is the loop's to
 * find. Steps sample by sample through the composition API because that is
 * the path the taps live on: `mpsk_receiver_step_ted()` is what calls
 * `mpsk_rx_push_lo()` and `mpsk_rx_push_preterm()`.
 *
 * @param tap       MPSK_RX_NDA_TAP_*.
 * @param sps       Samples per symbol at the receiver's input.
 * @param fs        Sample rate, Hz — for reporting the error in Hz only.
 * @param esn0_db   Matched-filter-output Es/N0; pass a negative value for a
 *                  noiseless run.
 * @param nsym      Symbols to transmit.
 * @return The outcome; check `.refused` before reading anything else.
 */
static rx_nda_result_t
rx_nda_measure (int tap, double sps, double fs, double esn0_db, size_t nsym)
{
  rx_nda_result_t r;
  double          foff = RX_NDA_FOFF_SYM / sps; /* cycles/sample */
  uint32_t        st   = 12345u;
  /* A rectangular symbol of amplitude A over `sps` samples through the
     length-`sps` boxcar comes out at `sps*A` while `sps` complex noise samples
     of per-quadrature variance sigma^2 sum to `sps*sigma^2`, so the
     matched-filter-output Es/N0 is `sps*A^2/(2*sigma^2)`. Same convention as
     mpsk_ber_common.h's complex path, for the same reason: an input SNR would
     mean something different at every sps and the sweep would be comparing
     operating points rather than taps. */
  double sigma
      = (esn0_db < 0.0)
            ? 0.0
            : RX_NDA_AMP * sqrt (sps / (2.0 * pow (10.0, esn0_db / 10.0)));

  memset (&r, 0, sizeof r);
  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      2, sps, RX_NDA_M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, RX_NDA_BN, 0.707,
      RX_NDA_BN, 0, 0.3, 0.0, 300, 0, MPSK_RX_NUM_PHASES, tap, 1,
      MPSK_RX_AGC_BW_RATIO);
  if (!rx)
    {
      r.refused = 1;
      return r;
    }

  {
    size_t isps = (size_t)sps;
    double sre = 0.0, sim = 0.0;
    size_t nout = 0;
    for (size_t k = 0; k < nsym; k++)
      {
        /* NRZ BPSK: one antipodal level held across the whole symbol. */
        double sr = RX_NDA_AMP * ((rx_nda_rng (&st) % 2u) ? -1.0 : 1.0);
        for (size_t j = 0; j < isps; j++)
          {
            double        ph = 2.0 * M_PI * foff * (double)(k * isps + j);
            float complex x
                = (float)(sr * cos (ph) + sigma * rx_nda_gauss (&st))
                  + (float)(sr * sin (ph) + sigma * rx_nda_gauss (&st)) * I;
            float complex y;
            /* Score only the settled half: the first half is acquisition, and
               averaging it in would report the transient as de-rotation. */
            if (mpsk_receiver_step_ted (rx, x, &y, RATESYNC_TED_GARDNER)
                && k > nsym / 2)
              {
                sre += fabs (creal (y));
                sim += fabs (cimag (y));
                nout++;
              }
          }
      }
    r.re = nout ? sre / (double)nout : 0.0;
    r.im = nout ? sim / (double)nout : 0.0;
  }

  {
    double est = mpsk_receiver_get_norm_freq (rx);
    r.acquired = est / foff;
    r.ferr_hz  = fabs (est - foff) * fs;
    r.lock     = mpsk_receiver_get_lock (rx);
    r.clipped  = mpsk_receiver_get_clipped (rx);
  }
  mpsk_receiver_destroy (rx);
  return r;
}

/** @brief `norm_freq` read back at ZERO offset — the measurement that cannot
 * rank these taps. Reported so the trap is visible; never gated on. */
static double
rx_nda_zero_offset_ferr (int tap, double sps, double fs, double esn0_db,
                         size_t nsym)
{
  uint32_t st = 12345u;
  double   sigma
      = (esn0_db < 0.0)
            ? 0.0
            : RX_NDA_AMP * sqrt (sps / (2.0 * pow (10.0, esn0_db / 10.0)));
  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      2, sps, RX_NDA_M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, RX_NDA_BN, 0.707,
      RX_NDA_BN, 0, 0.3, 0.0, 300, 0, MPSK_RX_NUM_PHASES, tap, 1,
      MPSK_RX_AGC_BW_RATIO);
  if (!rx)
    return -1.0;
  {
    size_t isps = (size_t)sps;
    for (size_t k = 0; k < nsym; k++)
      {
        double sr = RX_NDA_AMP * ((rx_nda_rng (&st) % 2u) ? -1.0 : 1.0);
        for (size_t j = 0; j < isps; j++)
          {
            float complex x = (float)(sr + sigma * rx_nda_gauss (&st))
                              + (float)(sigma * rx_nda_gauss (&st)) * I;
            float complex y;
            mpsk_receiver_step_ted (rx, x, &y, RATESYNC_TED_GARDNER);
          }
      }
  }
  {
    double f = fabs (mpsk_receiver_get_norm_freq (rx)) * fs;
    mpsk_receiver_destroy (rx);
    return f;
  }
}

/**
 * @brief Does the REAL-input receiver accept @p tap?
 *
 * Measured at the design centre `fc = 0.25`, as mpsk_receiver_r_ber.c does.
 * @return 1 if `create()` succeeded.
 */
static int
rx_nda_r_accepts (int tap)
{
  mpsk_receiver_r_state_t *r = mpsk_receiver_r_create (
      2, 8.0, RX_NDA_M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, RX_NDA_BN, 0.707,
      RX_NDA_BN, 0, 0.3, 0.25, 300, 0, MPSK_RX_NUM_PHASES, tap, 1,
      MPSK_RX_AGC_BW_RATIO);
  if (!r)
    return 0;
  mpsk_receiver_r_destroy (r);
  return 1;
}

/* The rate ratios the gate sweeps. `preterm`'s update rate is `bank_sps`, a
 * planner outcome, so the plan must be re-consulted at more than one ratio:
 * 8 is the ordinary case, 10000 is the ratio the tap was introduced for. */
static const double RX_NDA_CHECK_SPS[] = { 8.0, 200.0, 10000.0 };
static const double RX_NDA_FULL_SPS[]  = { 8.0, 40.0, 200.0, 1000.0, 10000.0 };

#define RX_NDA_N(a) (sizeof (a) / sizeof ((a)[0]))

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  if (check)
    {
      /* G1/G2/G3 — at every rate ratio: every tap that claims to work must
         ACQUIRE, `preterm` must acquire where `lo_arm` cannot, and `preterm`
         must leave a de-rotated constellation. */
      for (size_t i = 0; i < RX_NDA_N (RX_NDA_CHECK_SPS); i++)
        {
          double          sps = RX_NDA_CHECK_SPS[i];
          double          fs  = sps * 1000.0; /* Rs = 1 kSps throughout */
          rx_nda_result_t res[4];
          /* Per-ITERATION, not the sticky `fail`: a failure at one rate ratio
             must not skip the gates at the next one. The whole point of the
             sweep is that a mis-sized loop can pass at one ratio and fail at
             another, so every ratio has to be reported. */
          int unusable = 0;
          for (int t = 0; t < 4; t++)
            res[t] = rx_nda_measure (t, sps, fs, -1.0, RX_NDA_NSYM);

          for (int t = 0; t < 4; t++)
            {
              if (res[t].refused)
                {
                  printf ("FAIL sps=%.0f %s: create() refused\n", sps,
                          RX_NDA_NAMES[t]);
                  fail = unusable = 1;
                }
              else if (res[t].clipped)
                {
                  printf ("FAIL sps=%.0f %s: front end clipped\n", sps,
                          RX_NDA_NAMES[t]);
                  fail = unusable = 1;
                }
            }
          if (unusable)
            continue;

          for (int t = 0; t < 4; t++)
            {
              if (t == MPSK_RX_NDA_TAP_LO_ARM)
                continue; /* the ratchet below owns this one */
              if (!(res[t].acquired >= RX_NDA_ACQ_MIN))
                {
                  printf ("FAIL sps=%.0f %s: acquired %.4f of the offset "
                          "(want >= %.2f) — the loop is not steering\n",
                          sps, RX_NDA_NAMES[t], res[t].acquired,
                          RX_NDA_ACQ_MIN);
                  fail = 1;
                }
            }

          /* G2 — the claim the tap was actually introduced for: it is what
             `lo_arm` "approximates by hand" (docs/design/mpsk.md §3.3), so it
             must succeed where `lo_arm` fails. Deliberately NOT a comparison
             against `strobe`/`mf_all`: measured, their residuals are the same
             order as `preterm`'s, so gating one would pin a coincidence. */
          {
            double pt = res[MPSK_RX_NDA_TAP_PRETERM].acquired;
            double la = res[MPSK_RX_NDA_TAP_LO_ARM].acquired;
            if (!(pt > la + 0.5))
              {
                printf ("FAIL sps=%.0f: preterm acquired %.4f, lo_arm %.4f — "
                        "preterm must clear the tap it supersedes by 0.5\n",
                        sps, pt, la);
                fail = 1;
              }
          }
          {
            rx_nda_result_t p = res[MPSK_RX_NDA_TAP_PRETERM];
            if (!(p.re > 0.0) || !(p.im / p.re < RX_NDA_DEROT_MAX))
              {
                printf ("FAIL sps=%.0f preterm: |Im|/|Re| = %.4f "
                        "(want < %.2f) — constellation still spinning\n",
                        sps, p.re > 0.0 ? p.im / p.re : -1.0,
                        RX_NDA_DEROT_MAX);
                fail = 1;
              }
          }

          /* G4 — the existing-breakage ratchet. `lo_arm`'s integrator is
             1/upd too weak per symbol (its per-update ki scales as t^2 while
             it only gets t^-1 more updates), so it cannot pull in a frequency
             offset at any realistic rate ratio. That is a SHIPPED defect this
             harness found, not one the preterm tap introduced; it is pinned
             here so it cannot change unnoticed, and tracked upstream. If this
             line goes red because lo_arm started working, that is the fix
             landing — tighten the gate to RX_NDA_ACQ_MIN and close it. */
          if (!(res[MPSK_RX_NDA_TAP_LO_ARM].acquired < 0.5))
            {
              printf ("RATCHET sps=%.0f lo_arm: acquired %.4f — this tap was "
                      "known broken (< 0.5). If it is fixed, tighten this "
                      "gate to %.2f and close the issue.\n",
                      sps, res[MPSK_RX_NDA_TAP_LO_ARM].acquired,
                      RX_NDA_ACQ_MIN);
              fail = 1;
            }
        }

      /* G5 — the real-input receiver publishes no bank rate, so it has no
         pre-terminal node to read and must REFUSE the tap rather than
         silently fall back to `lo_sps` and mis-size the loop. The other three
         must still construct, or this gate would pass on a receiver that
         refuses everything. */
      for (int t = 0; t < 4; t++)
        {
          int got  = rx_nda_r_accepts (t);
          int want = (t != MPSK_RX_NDA_TAP_PRETERM);
          if (got != want)
            {
              printf ("FAIL MpskReceiverR %s: create() %s, want %s "
                      "(preterm needs a bank rate the real front end does "
                      "not publish)\n",
                      RX_NDA_NAMES[t], got ? "accepted" : "refused",
                      want ? "accepted" : "refused");
              fail = 1;
            }
        }

      printf (fail ? "rx_nda_tap FAILED\n" : "rx_nda_tap: OK\n");
      return fail;
    }

  /* ---------------------------------------------------------------- */
  /* Full sweep                                                        */
  /* ---------------------------------------------------------------- */
  printf ("NDA carrier taps: how much of a known offset each one removes\n");
  printf ("NRZ BPSK (I&D), M=2, m_out=%u, bn_carrier=%.3f/sym, "
          "Rs = 1 kSps, %u symbols\n",
          (unsigned)RX_NDA_M_OUT, RX_NDA_BN, (unsigned)RX_NDA_NSYM);
  printf ("Offset applied: %.4f cycles/symbol (%.2fx bn, and 1/%.0fth of "
          "strobe's Rs/(2M) ceiling)\n\n",
          RX_NDA_FOFF_SYM, RX_NDA_FOFF_SYM / RX_NDA_BN,
          0.25 / RX_NDA_FOFF_SYM);

  printf ("acquired = norm_freq / offset applied; 1.0 = fully acquired, "
          "0.0 = the loop never moved.\n\n");
  printf ("   sps        tap    acquired    |f_err| Hz     lock    "
          "|Re|     |Im|   |Im|/|Re|\n");
  printf ("  -----   --------   --------   -----------   ------   "
          "------   ------   ---------\n");
  for (size_t i = 0; i < RX_NDA_N (RX_NDA_FULL_SPS); i++)
    {
      double sps = RX_NDA_FULL_SPS[i];
      double fs  = sps * 1000.0;
      for (int t = 0; t < 4; t++)
        {
          rx_nda_result_t r = rx_nda_measure (t, sps, fs, -1.0, RX_NDA_NSYM);
          if (r.refused)
            {
              printf ("  %5.0f   %8s   refused\n", sps, RX_NDA_NAMES[t]);
              continue;
            }
          printf ("  %5.0f   %8s   %8.4f   %11.3e   %6.3f   %6.4f   %6.4f   "
                  "%9.4f\n",
                  sps, RX_NDA_NAMES[t], r.acquired, r.ferr_hz, r.lock, r.re,
                  r.im, r.re > 0.0 ? r.im / r.re : -1.0);
        }
      printf ("\n");
    }

  printf ("Read the |f_err| column across taps at a fixed sps: strobe, "
          "mf_all and preterm are the\nsame order at every ratio. Three taps "
          "carrying the same bn_carrier over the same signal\nsettle to the "
          "same loop jitter — the tap point does not buy frequency accuracy, "
          "and no\nclaim that it does is gated here (doppler#766). What "
          "preterm does buy is acquiring at\nall where lo_arm cannot, which "
          "is gated.\n\n");

  /* The trap, reproduced. These are the numbers a zero-offset experiment
     prints, and they rank the taps in very nearly the opposite order to the
     table above. Kept because a warning in a docstring is not evidence. */
  printf ("The measurement that CANNOT rank these taps: |norm_freq| read "
          "back at ZERO offset,\nsps = 10000 (Fs = 10 MSa/s). At 0 Hz the "
          "right answer is 0, so a loop that never\nsteers scores better "
          "than one that works. Compare the ordering with the 10000 row "
          "above.\n\n");
  printf ("        tap   |f_err| Hz @ 0 offset, noiseless   "
          "|f_err| Hz @ 10 dB Es/N0\n");
  printf ("   --------   -----------------------------   "
          "------------------------\n");
  for (int t = 0; t < 4; t++)
    printf ("   %8s   %29.3e   %24.3e\n", RX_NDA_NAMES[t],
            rx_nda_zero_offset_ferr (t, 10000.0, 10e6, -1.0, RX_NDA_NSYM),
            rx_nda_zero_offset_ferr (t, 10000.0, 10e6, 10.0, RX_NDA_NSYM));

  printf ("\nMpskReceiverR (real input) accepts: ");
  for (int t = 0; t < 4; t++)
    if (rx_nda_r_accepts (t))
      printf ("%s ", RX_NDA_NAMES[t]);
  printf ("\n  preterm is complex-input only: the real front end publishes no "
          "bank rate,\n  so there is no pre-terminal node to read.\n");
  return 0;
}
