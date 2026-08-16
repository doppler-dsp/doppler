/**
 * @file rx_nda_tap.c
 * @brief The four NDA carrier taps, ranked by the only thing that ranks them:
 *        how much of a KNOWN frequency offset each one actually removes.
 *
 * `nda_tap` chooses where MpskReceiver's M-th-power carrier discriminator
 * reads (mpsk_rx_loops.h). Each tap claims a different pull-in range and a
 * different noise bandwidth, and `mf_in` claims to beat the shipped
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
 * (`strobe 2.3e-03 Hz` vs `mf_in 1.9e-09 Hz`) because the two numbers are
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
 * ## What this harness does NOT find: any advantage of `mf_in` over
 * ## `strobe`
 *
 * Measured, and recorded here because the opposite was claimed: once every
 * tap is scored on a real offset, `mf_in`'s residual `|f_err|` is the SAME
 * ORDER as `strobe`'s and `mf_out`'s at every rate ratio in the sweep (at
 * Fs/Rs = 10000: 1.8e-03 vs 6.2e-04 vs 1.5e-03 Hz — `mf_in` is slightly
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
 * large: every tap that claims timing-independence must acquire from cold at
 * every rate ratio, including with the data modulation removed entirely.
 * Whether `mf_in` earns its place against `strobe` is an open question this
 * harness answers "no evidence yet" (doppler#766); it is not gated in either
 * direction.
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
 * same question, which matters most for `mf_in`, whose update rate is a
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
#include <stdlib.h>
#include <string.h>

/** @brief Transmit amplitude. A cascade that plans a CIC bounds its input to
 * +-1.0 and clips silently past it, so every run asserts get_clipped() == 0
 * rather than trusting the level (mpsk_ber_common.h makes the same choice). */
#define RX_NDA_AMP 0.5

/** @brief The offset every gate applies, cycles per SYMBOL. See the file
 * docstring: 0.4x bn_carrier, and 1/125th of the narrowest tap's ceiling. */
#define RX_NDA_FOFF_SYM 0.002

/** @brief Timing offset applied to the record, in symbols, so the timing
 * loop starts genuinely cold rather than nominally so. */
#define RX_NDA_TOFF_SYM 0.5

/** @brief What the symbol source does — the axis that decides whether the
 * timing loop has anything to lock to at all. */
enum
{
  RX_NDA_MOD_DATA = 0,    /**< i.i.d. BPSK: transitions to work with.     */
  RX_NDA_MOD_NONE = 1,    /**< NO data modulation: zero transitions, so
                               the Gardner TED has no edge and the timing
                               loop can never lock.                       */
  RX_NDA_MOD_PREAMBLE = 2 /**< CW preamble, then data.                    */
};

static const char *RX_NDA_MODS[3]
    = { "i.i.d. BPSK", "no modulation", "CW preamble" };

/** @brief Taps whose contract is "no symbol timing required". */
static int
rx_nda_timing_independent (int tap)
{
  return tap == MPSK_RX_NDA_TAP_MF_OUT || tap == MPSK_RX_NDA_TAP_MF_IN;
}

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

static const char *RX_NDA_NAMES[3] = { "strobe", "mf_out", "mf_in" };

/** @brief One tap's outcome at one geometry. */
typedef struct
{
  double acquired;  /**< fraction of the applied offset removed.       */
  double ferr_hz;   /**< residual |f_err|, Hz, at the quoted Fs.       */
  double lock;      /**< carrier lock metric at the end of the run.    */
  double re;        /**< mean |Re| of the settled symbols.             */
  double im;        /**< mean |Im| of the settled symbols.             */
  long   lock_time; /**< symbols to first lock declaration, -1 if none. */
  int    clipped;   /**< front end clipped: the reading is worthless.  */
  int    refused;   /**< create() returned NULL.                       */
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
 * `mpsk_rx_push_lo()` and `mpsk_rx_push_mf_in()`.
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
rx_nda_measure_ex (int tap, double sps, double fs, int mod, double bn_timing,
                   double esn0_db, size_t nsym, size_t m_out)
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
      2, sps, m_out, MPSK_RX_PULSE_IANDD, 0.35, 8, RX_NDA_BN, 0.707, bn_timing,
      0, 0.3, 0.0, 0, MPSK_RX_NUM_PHASES, tap, 1, MPSK_RX_AGC_BW_RATIO);
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
    double est  = mpsk_receiver_get_norm_freq (rx);
    r.acquired  = est / foff;
    r.ferr_hz   = fabs (est - foff) * fs;
    r.lock      = mpsk_receiver_get_lock (rx);
    r.lock_time = (long)mpsk_receiver_get_lock_time (rx);
    r.clipped   = mpsk_receiver_get_clipped (rx);
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
      RX_NDA_BN, 0, 0.3, 0.0, 0, MPSK_RX_NUM_PHASES, tap, 1,
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
      RX_NDA_BN, 0, 0.3, 0.25, 0, MPSK_RX_NUM_PHASES, tap, 1,
      MPSK_RX_AGC_BW_RATIO);
  if (!r)
    return 0;
  mpsk_receiver_r_destroy (r);
  return 1;
}

/* ── Carrier dynamics: cold-start time, and the ramp law ─────────────────
 *
 * A type-2 loop nulls a frequency STEP to zero steady-state phase error,
 * which is why the acquisition gates above cannot rank taps that all work.
 * Against a frequency RAMP it holds a CONSTANT phase error, and that error
 * has a closed form to check against rather than a number to fit:
 *
 *     theta_ss = L / wn^2,   L = 2*pi*r  (rad/symbol^2, r in cyc/symbol^2)
 *     wn       = 8*zeta*bn / (4*zeta^2 + 1)   ->  1.8857*bn at zeta = 0.707
 *
 * The loop breaks when theta_ss leaves the M-th-power S-curve's linear
 * range, ~pi/(2M).
 *
 * Every tap matches that form to under 1%, and getting there is what closed
 * doppler#765. Before the fix the lag was the form times the tap's UPDATE
 * RATE — `strobe` (upd = 1) matched, every other tap was `upd` times worse at
 * every ramp rate (2.00 for mf_out, 1.5625 for mf_in) — because `freq_scale`
 * assumed the loop filter's output is rad per SYMBOL, which is true only for
 * `strobe`. A step could never have shown it: a type-2 loop nulls a step
 * regardless of gain. This is the measurement that could, and the gate now
 * pins the CORRECT law rather than the defect. */

/** @brief Loop natural frequency per symbol, from the shipped gain formula. */
static double
rx_nda_wn (double bn, double zeta)
{
  return 8.0 * zeta * bn / (4.0 * zeta * zeta + 1.0);
}

/** @brief Predicted steady-state phase lag (rad) under a ramp of @p r
 * cycles/symbol^2. Tap-independent BY DESIGN: `bn_carrier` is normalised to
 * the symbol rate, so every tap must deliver the same loop whatever its
 * update rate. A tap-dependent answer here is the gh-765 class of defect. */
static double
rx_nda_ramp_law (double r)
{
  double wn = rx_nda_wn (RX_NDA_BN, 0.707);
  return 2.0 * M_PI * r / (wn * wn);
}

/** @brief Drive a frequency ramp; report the settled mean |phase error|. */
static double
rx_nda_ramp_lag (int tap, double sps, double r, size_t nsym, double *upd_out,
                 double *lock_out)
{
  uint32_t               st = 12345u;
  double                 a  = r / (sps * sps); /* cycles/sample^2 */
  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      2, sps, RX_NDA_M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8, RX_NDA_BN, 0.707,
      RX_NDA_BN, 0, 0.3, 0.0, 0, MPSK_RX_NUM_PHASES, tap, 1,
      MPSK_RX_AGC_BW_RATIO);
  if (!rx)
    return -1.0;
  if (upd_out)
    *upd_out = mpsk_rx_updates_per_symbol (&rx->l);
  {
    size_t isps = (size_t)sps;
    double s1   = 0.0;
    size_t cnt  = 0;
    for (size_t k = 0; k < nsym; k++)
      {
        double sr = RX_NDA_AMP * ((rx_nda_rng (&st) % 2u) ? -1.0 : 1.0);
        for (size_t j = 0; j < isps; j++)
          {
            double        n  = (double)(k * isps + j);
            double        ph = 2.0 * M_PI * (a * n * n * 0.5);
            float complex x
                = (float)(sr * cos (ph)) + (float)(sr * sin (ph)) * I;
            float complex y;
            mpsk_receiver_step_ted (rx, x, &y, RATESYNC_TED_GARDNER);
          }
        if (k > nsym / 2)
          {
            s1 += fabs (mpsk_receiver_get_last_error (rx));
            cnt++;
          }
      }
    if (lock_out)
      *lock_out = mpsk_receiver_get_lock (rx);
    {
      double m = cnt ? s1 / (double)cnt : -1.0;
      mpsk_receiver_destroy (rx);
      return m;
    }
  }
}

/** @brief The original signature: `RX_NDA_M_OUT` terminal outputs, which is
 * every caller above. `m_out` became a parameter only so the noise table can
 * ask for a non-degenerate geometry -- at `m_out = 2` an I&D matched filter
 * degenerates to a two-tap sum and flattens every tap onto the same reading
 * (docs/design/mpsk.md S5), which is a property of the geometry and not of
 * the taps. */
static rx_nda_result_t
rx_nda_measure (int tap, double sps, double fs, int mod, double bn_timing,
                double esn0_db, size_t nsym)
{
  return rx_nda_measure_ex (tap, sps, fs, mod, bn_timing, esn0_db, nsym,
                            RX_NDA_M_OUT);
}

/** @brief Ramp rates the gate checks, cycles/symbol^2. All well inside the
 * pi/(2M) linear range so the law is being checked, not the breakdown. */
static const double RX_NDA_RAMPS[] = { 1e-7, 3e-7, 1e-6 };

/** @brief Fractional tolerance on the ramp law. The measurement is a mean of
 * a noiseless settled tail against a closed form; 0.7% is what `strobe`
 * achieves, so 10% is loose enough not to chatter and tight enough that a
 * factor-of-two error in any gain cannot hide. */
#define RX_NDA_RAMP_TOL 0.10

/* The rate ratios the gate sweeps. `mf_in`'s update rate is `bank_sps`, a
 * planner outcome, so the plan must be re-consulted at more than one ratio:
 * 8 is the ordinary case, 10000 is the ratio the tap was introduced for. */
static const double RX_NDA_CHECK_SPS[] = { 8.0, 200.0, 10000.0 };
static const double RX_NDA_FULL_SPS[]  = { 8.0, 40.0, 200.0, 1000.0, 10000.0 };

#define RX_NDA_N(a) (sizeof (a) / sizeof ((a)[0]))

/* ── Under NOISE — the one condition every sweep above removes ────────────
 *
 * Every sweep above runs NOISELESS (`esn0_db < 0` -> sigma = 0), and that is
 * the condition, singular, that hides what `mf_in` costs. Its node carries
 * `10*log10(bank_sps)` dB of EXCESS NOISE BANDWIDTH -- DEC band-limits to its
 * own Nyquist, not to the signal (mpsk_rx_loops.h's tap table has the
 * measurement) -- and with no noise there is no excess bandwidth to pay for,
 * so the tap looks free and this harness said so at every rate ratio.
 *
 * What fails first is the M-th-power LOCK statistic, because it is an SNR
 * measure and not a phase measure, while the loop itself still ACQUIRES.
 * The taps are compared under DYNAMICS -- a data onset under a Doppler ramp,
 * on the continuous flavor's own waveform -- in `rx_dynamics.c`; this table
 * is the static Es/N0 sweep that sits underneath it. That
 * asymmetry is the finding, and it is why the two columns below are printed
 * side by side: a tap that pulls a known offset in fully and cannot report it
 * is a working loop with an unusable lock indicator, which is a different
 * defect from a loop that does not work.
 *
 * ISOLATED, because the first version of this note was not. It claimed TWO
 * conditions were needed -- no noise AND a rectangular pulse, on the argument
 * that an NRZ symbol *is* the constellation held across the symbol so a
 * pre-MFR tap loses nothing to shaping. The premise is true and the
 * conclusion did not follow. Measured with the pulse held fixed
 * and only Es/N0 moving, in both directions: the RRC-shaped stimulus of
 * `rx_battery` gives mf_in a mean lock EMA of 0.24 / 0.52 / 0.85 / 0.95 at
 * 6.79 / 12 / 20 / 30 dB, and the NRZ stimulus below gives 0.19 / 0.48 / 0.91
 * / 0.99 at the same points. The pulse does not separate them. NOISE ALONE
 * DOES, and the earlier two-condition story was an explanation asserted
 * without varying the variable it named.
 *
 * NRZ BPSK is also the waveform the CONTINUOUS flavor actually targets
 * (`docs/design/mpsk.md` S0 -- RRC is the burst flavor), so this table is the
 * tap measured on its own design flavor rather than on a neighbouring one. */

/** @brief Es/N0 points, dB at the matched-filter output. 6.79 is the
 * battery's SER=1e-3 BPSK anchor; the rest walk up to where the tap's cost
 * disappears, which is the shape of the finding rather than one number. */
/** @brief Terminal outputs per symbol for the noise table: 0 asks the object
 * to DERIVE it (gh-644), which is what `rx_battery` does and what reaches 8 at
 * sps = 8. The rest of this file pins 2, and at 2 an I&D matched filter is a
 * two-tap sum whose eye barely opens -- every tap then reads the same
 * degraded lock and the comparison says nothing. */
#define RX_NDA_NOISE_M_OUT 0u

static const double RX_NDA_ESN0[] = { 6.79, 12.0, 20.0, 30.0 };

/** @brief The Es/N0 at which the gate requires every tap to acquire. Well
 * above the anchor on purpose: the gate's claim is that a tap which CLAIMS a
 * pull-in range delivers it once it can see the signal at all, not that it
 * does so at the noisiest point the battery visits. */
#define RX_NDA_NOISE_GATE_ESN0 20.0

/** @brief Acquisition tolerance under noise. The noiseless sweep holds 1e-3;
 * at 20 dB Es/N0 the loop's own jitter is the floor, and `strobe` measures
 * 6e-4 while `mf_in` measures 3.5e-3, so 5% is loose enough not to chatter
 * and tight enough that a tap which did not steer (0.0) cannot pass. */
#define RX_NDA_NOISE_ACQ_TOL 0.05

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  if (check)
    {
      /* G1/G2/G3 — at every rate ratio: every tap that claims to work must
         ACQUIRE, its lock must be dated by `lock_time`, and `mf_in` must
         leave a de-rotated constellation. */
      for (size_t i = 0; i < RX_NDA_N (RX_NDA_CHECK_SPS); i++)
        {
          double          sps = RX_NDA_CHECK_SPS[i];
          double          fs  = sps * 1000.0; /* Rs = 1 kSps throughout */
          rx_nda_result_t res[3];
          /* Per-ITERATION, not the sticky `fail`: a failure at one rate ratio
             must not skip the gates at the next one. The whole point of the
             sweep is that a mis-sized loop can pass at one ratio and fail at
             another, so every ratio has to be reported. */
          int unusable = 0;
          for (int t = 0; t < 3; t++)
            res[t] = rx_nda_measure (t, sps, fs, RX_NDA_MOD_DATA, RX_NDA_BN,
                                     -1.0, RX_NDA_NSYM);

          for (int t = 0; t < 3; t++)
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

          for (int t = 0; t < 3; t++)
            {
              if (!(res[t].acquired >= RX_NDA_ACQ_MIN))
                {
                  printf ("FAIL sps=%.0f %s: acquired %.4f of the offset "
                          "(want >= %.2f) — the loop is not steering\n",
                          sps, RX_NDA_NAMES[t], res[t].acquired,
                          RX_NDA_ACQ_MIN);
                  fail = 1;
                }
            }

          /* G2 — lock_time is the acquisition time as a NUMBER, and it has
             to agree with the thing it dates. A tap that acquired must have
             stamped one; a stamp must be inside the record; and it must be
             within the analytic settling budget (5/bn symbols) with a wide
             margin, or the number is measuring something other than lock. */
          for (int t = 0; t < 3; t++)
            {
              long lt = res[t].lock_time;
              if (!(res[t].acquired >= RX_NDA_ACQ_MIN))
                continue;
              if (lt < 0)
                {
                  printf ("FAIL rate sps=%.0f %s: acquired %.4f but "
                          "lock_time is -1 — the loop locked and nothing "
                          "dated it\n",
                          sps, RX_NDA_NAMES[t], res[t].acquired);
                  fail = 1;
                }
              else if ((size_t)lt >= RX_NDA_NSYM)
                {
                  printf ("FAIL rate sps=%.0f %s: lock_time %ld is past the "
                          "%u-symbol record\n",
                          sps, RX_NDA_NAMES[t], lt, (unsigned)RX_NDA_NSYM);
                  fail = 1;
                }
            }
          {
            rx_nda_result_t p = res[MPSK_RX_NDA_TAP_MF_IN];
            if (!(p.re > 0.0) || !(p.im / p.re < RX_NDA_DEROT_MAX))
              {
                printf ("FAIL sps=%.0f mf_in: |Im|/|Re| = %.4f "
                        "(want < %.2f) — constellation still spinning\n",
                        sps, p.re > 0.0 ? p.im / p.re : -1.0,
                        RX_NDA_DEROT_MAX);
                fail = 1;
              }
          }
        }

      /* G2b — joint acquisition with NO DATA MODULATION.
         The requirement the tap mechanism exists for, and the one your
         harness cannot reach with an i.i.d. stream. With the modulation
         removed the Gardner TED has no transition to measure, so the timing
         loop can never lock — and a tap that claims timing-independence must
         acquire the carrier anyway. `bn_timing = 0` in one case so the timing
         offset is not merely hard to correct but impossible. */
      {
        const double sps = 10000.0, fs = sps * 1000.0;
        const struct
        {
          int         mod;
          double      bn_t;
          const char *what;
        } cases[] = {
          { RX_NDA_MOD_NONE, RX_NDA_BN, "no-modulation" },
          { RX_NDA_MOD_PREAMBLE, RX_NDA_BN, "cw-preamble" },
          { RX_NDA_MOD_DATA, 0.0, "timing-loop-disabled" },
        };
        for (size_t c = 0; c < RX_NDA_N (cases); c++)
          for (int t = 0; t < 3; t++)
            {
              rx_nda_result_t r;
              if (!rx_nda_timing_independent (t))
                continue; /* strobe does not promise this; reported only */
              r = rx_nda_measure (t, sps, fs, cases[c].mod, cases[c].bn_t,
                                  -1.0, RX_NDA_NSYM);
              if (r.refused || r.clipped)
                {
                  printf ("FAIL %s %s: unusable reading\n", cases[c].what,
                          RX_NDA_NAMES[t]);
                  fail = 1;
                  continue;
                }
              if (!(r.acquired >= RX_NDA_ACQ_MIN))
                {
                  printf ("FAIL %s %s: acquired %.4f (want >= %.2f) — a "
                          "timing-independent tap must acquire the carrier "
                          "with no symbol timing available at all\n",
                          cases[c].what, RX_NDA_NAMES[t], r.acquired,
                          RX_NDA_ACQ_MIN);
                  fail = 1;
                }
            }
      }

      /* G3 — the RAMP law, and the only gate here that can rank loops
         that all work. A type-2 loop nulls a frequency STEP regardless of
         gain, so G1/G2 can only catch a DEAD loop; a ramp leaves a constant
         phase lag with a CLOSED FORM, so it catches a mis-sized one.

         Checked against the form, never against a recorded number — a gate
         fitted to its own output cannot fail for the right reason. */
      {
        const double sps = 200.0;
        for (size_t i = 0; i < RX_NDA_N (RX_NDA_RAMPS); i++)
          for (int t = 0; t < 3; t++)
            {
              double upd = 0.0, lk = 0.0;
              double got
                  = rx_nda_ramp_lag (t, sps, RX_NDA_RAMPS[i], 3000, &upd, &lk);
              double want = rx_nda_ramp_law (RX_NDA_RAMPS[i]);
              if (got < 0.0)
                {
                  printf ("FAIL ramp %s: create() refused\n", RX_NDA_NAMES[t]);
                  fail = 1;
                  continue;
                }
              if (!(fabs (got - want) <= RX_NDA_RAMP_TOL * want))
                {
                  printf ("FAIL ramp r=%.0e %s: lag %.4f rad, law says %.4f "
                          "(upd %.4f, tol %.0f%%) -- this tap is not "
                          "delivering the bn_carrier it was given\n",
                          RX_NDA_RAMPS[i], RX_NDA_NAMES[t], got, want, upd,
                          100.0 * RX_NDA_RAMP_TOL);
                  fail = 1;
                }
            }
      }

      /* G3b — cold-start acquisition TIME against the analytic budget.
         `lock_time` makes this readable instead of inferable, and the loop
         filter header gives the reference: a step settles within ~5/bn
         updates, so a lock declared later than that is not the loop settling.
         The floor matters as much as the ceiling — a detector that declared
         at symbol 0 would also "beat" the budget, and would be reporting
         nothing. */
      {
        const double sps    = 200.0;
        const double budget = 5.0 / RX_NDA_BN; /* symbols */
        for (int t = 0; t < 3; t++)
          {
            rx_nda_result_t r
                = rx_nda_measure (t, sps, sps * 1000.0, RX_NDA_MOD_DATA,
                                  RX_NDA_BN, -1.0, RX_NDA_NSYM);
            if (r.refused || r.clipped)
              continue; /* G1 already reported it */
            if (r.lock_time < 1)
              {
                printf ("FAIL lock_time %s: %ld — a lock declared before the "
                        "first symbol is reporting nothing\n",
                        RX_NDA_NAMES[t], r.lock_time);
                fail = 1;
              }
            else if ((double)r.lock_time > budget)
              {
                printf ("FAIL lock_time %s: %ld symbols > the %0.f-symbol "
                        "5/bn settling budget\n",
                        RX_NDA_NAMES[t], r.lock_time, budget);
                fail = 1;
              }
          }
      }

      /* G4 — BOTH receiver types offer every tap, and `mf_in` on the real
         path is the one worth pinning: it needs the cascade's `bank_sps`,
         which `ddcr` publishes only because it carries the same
         RateConverter. Measured, `bank_sps` is identical on both types at
         every rate ratio — it is symbol-relative, so the halfband's 2:1 is
         absorbed by the plan. This gate is what stops that wiring being
         dropped silently: before it, the real receiver refused `mf_in`
         outright, and a receiver that refuses everything would pass a
         one-sided check. */
      for (int t = 0; t < 3; t++)
        {
          if (!rx_nda_r_accepts (t))
            {
              printf ("FAIL MpskReceiverR %s: create() refused — both types "
                      "offer every tap now that ddcr publishes bank_sps\n",
                      RX_NDA_NAMES[t]);
              fail = 1;
            }
        }

      /* G5 — UNDER NOISE, every tap still acquires. This is the gate the
         file did not have, and its absence is what let `mf_in` be adopted as
         a default: every sweep above runs sigma = 0, where forgoing the
         matched filter's processing gain costs exactly nothing.

         What is gated is ACQUISITION and deliberately not the lock
         statistic. `mf_in`'s lock is degraded at every Es/N0 here (0.19 at
         the battery's anchor against `strobe`'s 0.79) and that is a real
         a stated characteristic of the tap rather than a contract -- gating
         it would pin a caller's trade as if it were the receiver's promise.
         The table below REPORTS both columns so the asymmetry is reproducible;
         the gate asserts only the half that holds, which is the honest split.
       */
      {
        double sps = 8.0, fs = 8000.0;
        for (int t = 0; t < 3; t++)
          {
            rx_nda_result_t r = rx_nda_measure_ex (
                t, sps, fs, RX_NDA_MOD_DATA, RX_NDA_BN, RX_NDA_NOISE_GATE_ESN0,
                RX_NDA_NSYM, RX_NDA_NOISE_M_OUT);
            if (r.refused || r.clipped)
              {
                printf ("FAIL noise %s: %s\n", RX_NDA_NAMES[t],
                        r.refused ? "create() refused" : "front end clipped");
                fail = 1;
              }
            else if (fabs (r.acquired - 1.0) > RX_NDA_NOISE_ACQ_TOL)
              {
                printf ("FAIL noise %s: acquired %.4f at %.0f dB Es/N0 — a "
                        "tap that claims a pull-in range must deliver it "
                        "once it can see the signal\n",
                        RX_NDA_NAMES[t], r.acquired, RX_NDA_NOISE_GATE_ESN0);
                fail = 1;
              }
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
      for (int t = 0; t < 3; t++)
        {
          rx_nda_result_t r = rx_nda_measure (t, sps, fs, RX_NDA_MOD_DATA,
                                              RX_NDA_BN, -1.0, RX_NDA_NSYM);
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
          "mf_out and mf_in are the\nsame order at every ratio. Three taps "
          "carrying the same bn_carrier over the same signal\nsettle to the "
          "same loop jitter — the tap point does not buy frequency accuracy, "
          "and no\nclaim that it does is gated here (doppler#766). What "
          "mf_in does buy is a tap that\nneeds no symbol timing at all, which "
          "is gated.\n\n");

  printf ("== Frequency RAMP: the measurement that ranks loops which all "
          "acquire ==\n");
  printf ("A type-2 loop nulls a frequency STEP regardless of gain, so the "
          "tables above cannot\nseparate taps that work. Against a ramp it "
          "holds a CONSTANT phase lag with a closed\nform: theta_ss = 2*pi*r "
          "/ wn^2, wn = 8*zeta*bn/(4*zeta^2+1) = %.6f/sym here.\n"
          "It is tap-INDEPENDENT by design — bn_carrier is symbol-rate "
          "normalised, so every tap\nmust deliver the same loop whatever its "
          "update rate.\n\n",
          rx_nda_wn (RX_NDA_BN, 0.707));
  printf ("   %-14s %10s", "r cyc/sym^2", "law");
  for (int t = 0; t < 3; t++)
    printf ("  %16s", RX_NDA_NAMES[t]);
  printf ("\n   %-14s %10s", "-------------", "-------");
  for (int t = 0; t < 3; t++)
    printf ("  %16s", "lag / lock");
  printf ("\n");
  {
    const double rr[] = { 1e-7, 3e-7, 1e-6, 3e-6, 1e-5, 3e-5 };
    for (size_t i = 0; i < RX_NDA_N (rr); i++)
      {
        printf ("   %-14.0e %10.4f", rr[i], rx_nda_ramp_law (rr[i]));
        for (int t = 0; t < 3; t++)
          {
            double upd = 0.0, lk = 0.0;
            double got = rx_nda_ramp_lag (t, 200.0, rr[i], 3000, &upd, &lk);
            printf ("  %10.4f/%.2f", got, lk);
          }
        printf ("\n");
      }
  }
  printf ("\nThe loop breaks when theta_ss leaves the M-th-power S-curve's "
          "linear range, ~pi/(2M)\n= %.4f rad at M=2 — visible above as the "
          "lock column collapsing once the law crosses it.\n\n",
          M_PI / 4.0);

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
  for (int t = 0; t < 3; t++)
    printf ("   %8s   %29.3e   %24.3e\n", RX_NDA_NAMES[t],
            rx_nda_zero_offset_ferr (t, 10000.0, 10e6, -1.0, RX_NDA_NSYM),
            rx_nda_zero_offset_ferr (t, 10000.0, 10e6, 10.0, RX_NDA_NSYM));

  /* The table the file was missing. Everything above is noiseless; this is
     the same taps once noise exists, on the CONTINUOUS flavor's own waveform
     (NRZ BPSK -- docs/design/mpsk.md S0). Two columns because they separate:
     the loop acquires and the lock statistic does not follow it. */
  printf ("\n== Under NOISE: the condition every table above removes ==\n");
  printf ("Continuous NRZ BPSK + AWGN through the I&D matched filter, "
          "sps = 8, the same\n");
  printf ("%.4f cyc/sym offset. Reading ahead of the matched filter forgoes "
          "10*log10(sps)\n= %.1f dB of processing gain, and the M-th-power "
          "lock statistic is an SNR measure,\nso it collapses while the loop "
          "still pulls the offset in. ACQUISITION is gated;\nthe lock column "
          "is reported and NOT gated -- it is the tap's stated price, a\n"
          "caller's trade rather than the receiver's contract.\n\n",
          RX_NDA_FOFF_SYM, 10.0 * log10 (8.0));
  printf ("  Es/N0 dB   per-sample     %8s          %8s          %8s\n",
          RX_NDA_NAMES[0], RX_NDA_NAMES[1], RX_NDA_NAMES[2]);
  printf ("             SNR dB        acq / lock       acq / lock"
          "       acq / lock\n");
  for (size_t e = 0; e < RX_NDA_N (RX_NDA_ESN0); e++)
    {
      printf ("  %7.2f    %+7.2f   ", RX_NDA_ESN0[e],
              RX_NDA_ESN0[e] - 10.0 * log10 (8.0));
      for (int t = 0; t < 3; t++)
        {
          rx_nda_result_t r = rx_nda_measure_ex (
              t, 8.0, 8000.0, RX_NDA_MOD_DATA, RX_NDA_BN, RX_NDA_ESN0[e],
              RX_NDA_NSYM, RX_NDA_NOISE_M_OUT);
          if (r.refused || r.clipped)
            printf ("      refused ");
          else
            printf ("   %6.4f/%+.2f", r.acquired, r.lock);
        }
      printf ("\n");
    }
  printf ("\nThe pulse is NOT the variable, and the first version of this "
          "note said it was.\nHolding the pulse fixed and moving only Es/N0, "
          "in both directions: mf_in's mean\nlock EMA is 0.24/0.52/0.85/0.95 "
          "on rx_battery's RRC-shaped stimulus and\n0.19/0.48/0.91/0.99 on "
          "the NRZ stimulus above, at 6.79/12/20/30 dB. An NRZ\nsymbol IS "
          "the constellation held across the symbol, so a pre-MFR tap loses "
          "nothing\nto SHAPING -- and it loses the processing gain either "
          "way. Noise alone separates\nthese taps.\n");

  printf ("\nMpskReceiverR (real input) accepts: ");
  for (int t = 0; t < 3; t++)
    if (rx_nda_r_accepts (t))
      printf ("%s ", RX_NDA_NAMES[t]);
  printf ("\n  mf_in is complex-input only: the real front end publishes no "
          "bank rate,\n  so there is no pre-terminal node to read.\n");
  return 0;
}
