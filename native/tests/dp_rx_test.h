/**
 * @file dp_rx_test.h
 * @brief The receiver instrument: one struct, one run, every receiver.
 *
 * `docs/design/rx-test.md` goal 6 asks for "one harness, every receiver,
 * parameterised by operating point, not forked per object, so two receivers
 * are comparable by construction rather than by hoping two harnesses agree".
 * This is that. It owns **no capability** — no pulse, no level convention, no
 * estimator, and no random number generator — it composes pieces that already
 * ship, exactly as `native/validation/rx_frame_fer.c` proved the §8 sequence
 * can be composed.
 *
 * ## The one idea
 *
 * **The measurements are not different methods. They are the same method at
 * different operating points.** Cold acquisition, the ramp law and the
 * timing-transient disturbance need different STIMULUS, not different code, so
 * giving each its own entry point is what fragments a harness into pieces that
 * cannot be compared. Here a point carries its own impairment, `dp_rx_run()`
 * measures it, and the battery is the named point set.
 *
 * That is why there is one `dp_rx_result_t` rather than a record per
 * measurement: goal 4 asks for the metrics together because "they fail
 * differently, and the disagreement is the diagnostic", and the same argument
 * applies to the loop numbers sitting beside the trio.
 *
 * ## What composes
 *
 * | piece                  | supplies                                     |
 * | ---------------------- | -------------------------------------------- |
 * | `dp_frame_test.h`      | the named frames — what is transmitted       |
 * | `wfm_frame_bits()`     | the frame materialised as bits               |
 * | `wfm_synth`            | symbols, pulse, oversampling, carrier, AWGN  |
 * | `doppler_channel`      | Doppler offset and rate — one coupled clock  |
 * | the receiver           | via `dp_rx_iface_t`, the only forked part    |
 * | `dp_ber_test.h`        | settling, detection, the trio, the gates     |
 * | `frame_meter`          | FER and the sync-miss rate, exact intervals  |
 *
 * **`doppler_channel` is not a convenience.** A carrier offset and a sample
 * clock error are the same physical parameter: a real Doppler shift dilates
 * the whole received time base, so every clock moves together. A harness that
 * ramps the carrier without dilating the clock — or dilates the clock without
 * moving the carrier — is measuring a signal no receiver will ever see. Both
 * mistakes were made in the hand-rolled harness this replaces.
 *
 * ## What it refuses
 *
 * Goal 1: "the harness never returns a plausible number from an untrustworthy
 * state". `refused` is a FIELD, not an exception, and a refusal names itself.
 * Alignment that did not detect, a window that never settled, a frame with no
 * payload to demodulate — each is a refusal to report, not a number with a
 * caveat.
 *
 * ## Where it lives, and why not in the library
 *
 * `native/tests/`, deliberately — the same call §7.4 made for the named frame
 * set. Goal 9 argues anything reachable only from here is exercised by nobody
 * but us, and that is the right test for a CAPABILITY. This is a composition
 * of shipped capabilities plus a handful of named configurations that exist so
 * OUR measurements are comparable; shipping them would make our choice of
 * `sps = 8` an API to keep stable. A caller measuring their own receiver wants
 * `BerMeter`, `FrameMeter`, `wfm_frame_t` and `doppler_channel` — all of which
 * already ship — not our operating points.
 */
#ifndef DP_RX_TEST_H
#define DP_RX_TEST_H

#include "dp_ber_test.h"
#include "dp_frame_test.h"
#include "dp_test.h"

#include "ber/ber_core.h"
#include "doppler_channel/doppler_channel_core.h"
#include "frame_meter/frame_meter_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include "wfm/wfm_dsp.h"
#include "wfm/wfm_frame.h"
#include "wfm_synth/wfm_synth_core.h"

#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 1. The receiver under test ─────────────────────────────────────────── */

/**
 * @brief What the receiver's front end accepts.
 *
 * This is not cosmetic. A real front end takes `Re{}` of the complex
 * waveform, which halves the signal energy AND the noise variance — but the
 * real path's convention counts the real noise against the halved `Es`, which
 * is 3 dB less noise. So the stimulus must be generated 3 dB hot for a real
 * receiver to see the Es/N0 it was asked for. The instrument does that from
 * this field; a caller who gets it wrong measures a receiver 3 dB better than
 * it is and has nothing to tell them.
 */
typedef enum
{
  DP_RX_IN_COMPLEX = 0, /**< complex baseband (MpskReceiver)   */
  DP_RX_IN_REAL    = 1  /**< real IF (MpskReceiverR)           */
} dp_rx_domain_t;

struct dp_rx_point;

/**
 * @brief The only part of the instrument that forks per receiver design.
 *
 * Every entry is something both shipped M-PSK receivers already expose under
 * the same name modulo an `_r` infix, which is what makes the adapter thin
 * enough to trust. A design that cannot fill one of these in is telling you
 * something real about its observability, not about the harness.
 *
 * `last_error` is here because the timing-transient disturbance is only
 * visible at the DISCRIMINATOR: `norm_freq` is the integrator downstream of
 * the loop filter and buries it. That was learned by measuring the wrong one
 * first.
 *
 * `zeta` is here rather than on the point for the same reason `m_out` is left
 * 0 there: the adapter asks the receiver to DERIVE its damping, so the only
 * honest source for it is the constructed object. The ramp law is written in
 * `wn = loop_filter_wn(bn, zeta)`, and reading it against a damping the
 * receiver was not built at is a silent ~30% error — restating the default as
 * a harness constant would have been a second copy of exactly the number the
 * receiver is free to change.
 */
typedef struct
{
  const char    *name;   /**< printed with every number it produces      */
  dp_rx_domain_t domain; /**< drives the step call AND the Es/N0 offset  */

  /** Construct at an operating point, or NULL if the point is unbuildable. */
  void *(*create) (const struct dp_rx_point *);
  void (*destroy) (void *);

  /** One input sample in, 0 or more terminal outputs out. A real receiver
   *  takes `crealf(x)`; the instrument does that conversion, not the caller.
   */
  int (*step) (void *, float complex x, float complex *y);

  double (*norm_freq) (const void *);  /**< tracked carrier, cycles/sample */
  double (*last_error) (const void *); /**< discriminator output, radians  */
  double (*lock) (const void *);       /**< normalised lock statistic      */
  int (*locked) (const void *);        /**< lock declared                  */
  long (*lock_time) (const void *);    /**< symbols to first lock, -1 none */
  int (*clipped) (const void *);       /**< front end clipped: reading dead */
  double (*zeta) (const void *);       /**< carrier damping, as BUILT       */
} dp_rx_iface_t;

/* ── 2. The operating point ─────────────────────────────────────────────── */

/**
 * @brief One named operating point: everything a run needs to be reproducible.
 *
 * §8.1 lists stage 1's gap as "no frame descriptor; no named operating
 * points". The frame half landed as `dp_frame_test.h`; this is the other half,
 * and it follows the same convention for the same reason — a point per test is
 * how a convention goes wrong silently, so the set is named and a result says
 * which point produced it.
 *
 * Goal 7: "a run is reproducible from its description". A point plus a seed
 * fully determines a record — no stored truth arrays, no ambient state.
 */
typedef struct dp_rx_point
{
  const char     *name;  /**< printed with every number                   */
  dp_frame_name_t frame; /**< what is transmitted                         */

  int    m;     /**< constellation order, 2/4/8                           */
  double sps;   /**< samples per symbol at the receiver's input           */
  size_t m_out; /**< terminal outputs per symbol                          */

  double beta; /**< RRC excess bandwidth                                  */
  int    span; /**< RRC span in symbols                                   */

  double fc;   /**< carrier the receiver is centred on, cycles/sample     */
  double foff; /**< offset it must ACQUIRE, cycles/symbol                 */

  double bn_timing;  /**< timing loop noise bandwidth, per symbol       */
  double bn_carrier; /**< carrier loop noise bandwidth, per symbol      */

  double esn0_db; /**< matched-filter-output Es/N0 the stimulus carries   */

  /* Impairment — `doppler_channel`, so the carrier and every clock move
     TOGETHER. `doppler_ppm` alone is a static offset plus a clock error;
     `doppler_rate_ppm_s` adds the ramp. Leave both 0 for an unimpaired run. */
  double fs_hz;              /**< receive sample rate, Hz                 */
  double carrier_hz;         /**< RF carrier — converts ppm to Hz         */
  double doppler_ppm;        /**< d0, ppm of nominal                      */
  double doppler_rate_ppm_s; /**< d-dot, ppm/s                            */

  /** Transmit level in dBFS (<= 0), the same unit `wfm_compose` states a
      source's level in. It is part of the OPERATING POINT rather than a
      harness constant because the level a geometry can carry is a property of
      the geometry: a cascade that plans a CIC bounds its input to
      `CIC_PAPR_HEADROOM`, and the stimulus RMS grows as `sqrt(sps)` at fixed
      matched-filter Es/N0 — per-sample noise does — so one constant cannot
      serve every rate. Measured: RMS 1.63 at sps=8 against 5.27 at sps=128.
      0 means unit power and no gain. `clipped` is the check, and it is
      asserted rather than trusted. */
  double level_dbfs;

  uint32_t seed; /**< the only randomness, and it belongs to wfm_synth    */
} dp_rx_point_t;

/* ── 3. The standard record ─────────────────────────────────────────────── */

/**
 * @brief Everything needed to defend every number from one run.
 *
 * §8.1 lists stage 10's gap as "no standard record". This is it, and the
 * ordering is the argument: the trio first because they fail differently, the
 * frame statistics next because FER is the only truth-free metric that sees a
 * false lock, and the loop numbers last because they explain the others.
 */
typedef struct
{
  const dp_rx_point_t *point; /**< which point produced this             */
  const dp_rx_iface_t *rx;    /**< which receiver                        */

  dp_ber_report_t rep; /**< SER/BER + EVM + M2M4 + theory + the gates    */

  /* Frame statistics — `frame_meter`, exact intervals, absent when the
     point's frame carries no CRC (reported as `framed == 0`, never as 0.0). */
  int            framed;
  int            frame_enough; /**< the FRAME error target was reached     */
  size_t         frames, sync_detected, crc_passed;
  size_t         prot_bits; /**< bits the CRC protects (payload + CRC)     */
  ber_interval_t fer, sync_miss;
  /* The FER anchor. A rate is only defensible against a closed form, and
     FER's is the measured BER: if payload bit errors were independent, a
     frame protecting `L` bits fails with probability `1-(1-p)^L`. Errors that
     CLUSTER hit fewer frames for the same `p`, so that is an UPPER bound and
     the gate built on it is one-sided. */
  double crc_fail;      /**< CRC failures among DETECTED frames            */
  double crc_fail_pred; /**< `1-(1-BER)^prot_bits`                         */
  double fer_pred;      /**< what the miss rate and that term imply        */

  /* Loop behaviour, from the SAME record as the trio. */
  double acq_frac;     /**< fraction of `foff` removed; 1.0 is acquired    */
  double acq_time_bl;  /**< symbols to lock, in units of 1/bn_carrier —
                            the loop's own clock, so the number is
                            comparable across every point in the set      */
  double ramp_lag_rad; /**< settled phase lag under a Doppler RATE       */
  double ramp_law_rad; /**< what `2*pi*r/wn^2` says it should be         */
  double disturb_peak_rad; /**< peak |discriminator| excursion           */
  double disturb_rms_rad;  /**< and its RMS                              */

  /* The lock indicators as DUTY CYCLES over the scored window, which is what
     makes them evidence rather than a sample. The binary flag is thresholded
     (`lock_thresh`, sized against the no-signal distribution alone) while
     `lock_stat_duty` is the share where the statistic was merely positive —
     so the two together say whether a low duty means the loops are struggling
     or only that the threshold is not meant for this Es/N0. doppler#835.

     REPORTED, not gated, and deliberately: `dp_ber_settle` already requires
     the flag to hold 90 % over 200 symbols before a window opens, so a
     receiver whose duty is poor refuses the point and `dp_rx_tally_check`
     catches a receiver that refuses everything. A `lock_duty >= 0.9` gate
     was written here and REMOVED after sabotage — raising the threshold and
     dropping lock mid-record both redden the tally first, and neither could
     make the duty gate fire on its own. A gate that cannot fail
     independently is one nobody can trust the day it goes green. */
  double      lock_duty;      /**< share where `locked` was asserted        */
  double      lock_stat_duty; /**< share where the lock statistic was > 0   */
  unsigned    bursts;     /**< bursts consumed — the headroom, see below    */
  int         clipped;    /**< the front end clipped: nothing here is real  */
  int         unsettled;  /**< bursts whose window never settled             */
  int         unaligned;  /**< bursts settled, marker never detected         */
  size_t      frame_bits; /**< bits in one frame                             */
  const char *refused;    /**< non-NULL: not measurable, and WHY            */
} dp_rx_result_t;

/* ── 4. The named points — the battery ──────────────────────────────────── */

/**
 * @brief The standard battery. A result names the point that produced it.
 *
 * Each point exists to make one thing observable, and says so. They are not
 * five code paths — they are one struct with different values, exactly as the
 * named frame set is.
 */
typedef enum
{
  DP_RX_ANCHOR = 0,  /**< SER=1e-3 anchor, unimpaired: the reference the
                          others are read against                        */
  DP_RX_ACQUIRE,     /**< a static offset at the edge of the design
                          envelope; scores `acq_frac` and `acq_time_bl`  */
  DP_RX_DOPPLER,     /**< a Doppler RATE: the ramp law, with the carrier
                          and the clocks moving together as they must    */
  DP_RX_RUNBURST,    /**< a transition-starved payload: the timing loop
                          coasts, then slews, and the question is whether
                          that reaches the carrier loop                  */
  DP_RX_OVERSAMPLED, /**< Fs/Rs = 10000 — the §8.3 step-7 geometry, where
                          a planner outcome replaces a construction
                          constant                                       */
  DP_RX_QPSK,        /**< the anchor at M = 4, at ITS OWN SER=1e-3 Es/N0 —
                          10.35 dB, not BPSK's 6.79. One Es/N0 across M
                          compares constellations, not receivers         */
  DP_RX_PSK8,        /**< the anchor at M = 8 (15.68 dB). The M-th-power
                          squaring loss grows with M and the decision
                          margin falls to +-pi/8, so this is where the
                          NDA path is worst and the number matters most  */
  DP_RX_IRRATIONAL,  /**< sps = 17.33389 — the header's own example of an
                          input rate with no integer relationship to the
                          symbol clock. The symbol boundary falls BETWEEN
                          samples, which the terminal accumulator's being
                          a double is what makes free                    */
  DP_RX_RATE_ODD,    /**< sps = 31.7 — high AND irrational together. This
                          point exists because a Python harness measured
                          it as a receiver defect when its own alignment
                          had failed; the instrument either defends a
                          number here or refuses, and either answers it  */
  DP_RX_POINT_COUNT
} dp_rx_point_name_t;

/** @brief The named point, or NULL if @p name is out of range. */
/* dp_rx_point() is defined below. */

/* ── 5. The methods ─────────────────────────────────────────────────────── */

/* The three methods are defined at the bottom of this header: the dp_*.h
   family is header-only `static inline`, so a declaration here would be a
   second, non-static one. */

/* ── 6. Implementation ──────────────────────────────────────────────────── */

#define DP_RX_NSYM 40000u
#define DP_RX_BETA 0.35
#define DP_RX_SPAN 8
/** @brief Amplitude for a point's level, as a linear gain on a unit-power
 * stream — the same `10^(dBFS/20)` `wfm_compose` applies to a source. */
static inline double
dp_rx_amp (const dp_rx_point_t *pt)
{
  return pow (10.0, pt->level_dbfs / 20.0);
}
#define DP_RX_MAX_BURSTS 60
#define DP_RX_TARGET_FRAME_ERRORS 50u

/** @brief Per-frame sync confirmation half-width, symbols.
 *
 * A tracking receiver does not re-acquire every frame; it looks in a narrow
 * window where the frame is due. That is also what makes the question fair —
 * a Bonferroni correction over 25 lags is a very different bar from one over
 * 401, and quoting a sync miss rate from a full re-acquisition would measure
 * the SEARCH rather than the sync word. Bounded below by `ber_align_detect`'s
 * CFAR, which needs 8 reference cells outside its 3-lag guard band. */
#define DP_RX_SYNC_SPAN 12L

/** @brief How far the measured FER's LOWER limit may sit above the predicted
 * FER before the measurement is disbelieved.
 *
 * Small on purpose. The tolerance is not absorbing counting noise — the lower
 * limit already does that — only the CRC-16 alias (2^-16) and the prediction
 * resting on a BER point estimate. At 1.5 the gate was measured to still PASS
 * with the CRC check sabotaged to always fail, which is a gate that cannot
 * fail for the reason it exists. */
#define DP_RX_FER_TOL 1.15

/** @brief Below this share of the predicted FER coming from CRC failures, the
 * anchor is testing the sync miss rate it was handed and nothing else. It is
 * then SKIPPED with the reason printed, rather than passed vacuously. */
#define DP_RX_FER_ANCHOR_SHARE 0.5

/**
 * @brief The frequency RAMP a point presents to the carrier loop, in cycles
 * per symbol squared.
 *
 * `doppler_channel` states the impairment as a dilation of the whole time
 * base, in ppm and ppm/s, because that is what a Doppler shift physically is.
 * The ramp law is written in the CARRIER loop's own units, so the two have to
 * be reconciled exactly once, here.
 *
 * The channel puts the carrier at `carrier_hz * (d0 + d_dot*t) * 1e-6` Hz, so
 * the ramp is `carrier_hz * d_dot * 1e-6` Hz/s; dividing by the symbol rate
 * squared converts Hz per second into cycles per symbol squared. `d0` does not
 * appear: a type-2 loop nulls a frequency STEP regardless of gain, which is
 * exactly why the ramp is the disturbance a gain can be checked against.
 */
static inline double
dp_rx_ramp_rate (const dp_rx_point_t *pt)
{
  double rs;
  if (pt->doppler_rate_ppm_s == 0.0 || pt->fs_hz <= 0.0 || pt->sps <= 0.0)
    return 0.0;
  rs = pt->fs_hz / pt->sps;
  return pt->carrier_hz * pt->doppler_rate_ppm_s * 1e-6 / (rs * rs);
}

/**
 * @brief The steady-state phase lag that ramp implies, radians.
 *
 * `2*pi*r / wn^2`, with `wn` from `loop_filter_wn()` — the library's own
 * formula, so the harness is not carrying a sixth copy of it. Zero when the
 * point sets no rate: there is then no lag to predict, and a point that
 * measured one would be measuring its own noise.
 *
 * @param pt    The point — supplies the ramp and `bn_carrier`.
 * @param zeta  The damping the receiver was BUILT at, read back from it.
 */
static inline double
dp_rx_ramp_law (const dp_rx_point_t *pt, double zeta)
{
  double r  = dp_rx_ramp_rate (pt);
  double wn = (zeta > 0.0) ? loop_filter_wn (pt->bn_carrier, zeta) : 0.0;
  return (r != 0.0 && wn > 0.0) ? 2.0 * M_PI * r / (wn * wn) : 0.0;
}

/** @brief Fractional tolerance on the ramp law — the same bar
 * the retired tap sweep set, and for the same reason: the settled mean of a
 * noiseless tail lands within 1% of the closed form there, so 10% is loose
 * enough not to chatter and tight enough that a factor-of-two error in any
 * gain on the path cannot hide inside it. Here the tail is NOT noiseless, so
 * the width also has to cover the mean's own standard error — measured at
 * roughly 2% of the lag over a settled window, which fits with room to
 * spare. */
#define DP_RX_RAMP_TOL 0.10

/**
 * @brief Share of `[lo, hi)` where a per-symbol flag is set.
 *
 * A duty cycle, not a lock decision. Both of the receiver's lock indicators
 * are per-symbol flags, and a rate over the SCORED window is the only honest
 * summary of one: a boolean sampled once says whether the last symbol was
 * lucky, and `dp_ber_settle`'s answer says when a sustained run began, which
 * is a different question from how much of the record it covered.
 *
 * It is here rather than in a harness because the number it produces is
 * evidence about a THRESHOLD (doppler#835), and a threshold claim measured
 * two different ways in two harnesses is a claim about neither.
 */
static inline double
dp_rx_duty (const unsigned char *flags, size_t lo, size_t hi)
{
  if (flags == NULL || hi <= lo)
    return 0.0;
  size_t set = 0;
  for (size_t i = lo; i < hi; i++)
    set += (flags[i] != 0);
  return (double)set / (double)(hi - lo);
}

/**
 * @brief Generate one impaired burst and run it through the receiver.
 *
 * Stages 2-4 of the §8 sequence. Every sample the receiver sees comes from
 * `wfm_synth` and, when the point asks, `doppler_channel` — this function owns
 * no pulse, no level convention and no RNG.
 *
 * Steps sample by sample rather than through the block API because the
 * settling gate needs the receiver's own lock indicators PER SYMBOL, and the
 * block API exposes only their final value.
 */
static inline size_t
dp_rx_burst (const dp_rx_iface_t *rx, const dp_rx_point_t *pt,
             const uint8_t *bits, size_t nbits, uint32_t seed, size_t nsym,
             float complex *out, unsigned char *lock_c, unsigned char *track,
             double *err, double *nf_out, long *lt_out, double *zeta_out,
             int *clipped)
{
  int                      isps  = (int)pt->sps;
  double                   beta  = pt->beta > 0.0 ? pt->beta : DP_RX_BETA;
  int                      span  = pt->span > 0 ? pt->span : DP_RX_SPAN;
  size_t                   ntaps = wfm_rrc_ntaps (isps, span);
  size_t                   nsamp = nsym * (size_t)isps;
  float                   *taps  = (float *)malloc (ntaps * sizeof *taps);
  float complex           *x     = (float complex *)malloc (nsamp * sizeof *x);
  float complex           *imp   = NULL;
  wfm_synth_state_t       *tx    = NULL;
  doppler_channel_state_t *ch    = NULL;
  void                    *r     = NULL;
  size_t                   nout = 0, navail = nsamp;
  /* A real front end takes Re{}, halving signal AND noise, but its convention
     counts the real noise against the halved Es — 3 dB less noise. Asking the
     complex generator for 3 dB more delivers what was requested (§8.4). */
  double esn0 = pt->esn0_db + (rx->domain == DP_RX_IN_REAL ? 3.0 : 0.0);
  double amp  = dp_rx_amp (pt);

  *clipped = 0;
  if (!taps || !x)
    goto done;
  wfm_rrc_taps (beta, isps, span, taps);

  /* snr_mode 3 is Es/N0 at the MATCHED-FILTER OUTPUT, verified to 0.04 dB
     against the library's own estimator. Read at the sample stream instead it
     appears 10*log10(sps) low, which is what makes a wrong convention here
     look like a plausible receiver result. */
  tx = wfm_synth_create (WFM_SYNTH_BITS, 1.0, pt->fc, esn0, 3, seed, isps, 7,
                         0, 0, 0.0);
  if (!tx)
    goto done;
  if (wfm_synth_set_bits (tx, bits, nbits, mpsk_bps (pt->m)) != 0
      || wfm_synth_set_rrc (tx, taps, ntaps) != 0)
    goto done;
  wfm_synth_steps (tx, x, nsamp); /* the pattern CYCLES: many frames, one
                                     descriptor */

  /* Stage 3 — IMPAIR. One parameter moves the carrier AND every clock,
     because a Doppler shift dilates the whole received time base. */
  if (pt->doppler_ppm != 0.0 || pt->doppler_rate_ppm_s != 0.0)
    {
      ch  = doppler_channel_create (pt->fs_hz > 0.0 ? pt->fs_hz : 1.0,
                                    pt->carrier_hz, pt->doppler_ppm,
                                    pt->doppler_rate_ppm_s);
      imp = (float complex *)malloc (nsamp * sizeof *imp);
      if (!ch || !imp)
        goto done;
      navail = doppler_channel_execute (ch, x, nsamp, imp, nsamp);
    }

  r = rx->create (pt);
  if (!r)
    goto done;
  {
    const float complex *src = imp ? imp : x;
    for (size_t n = 0; n < navail; n++)
      {
        float complex in = src[n] * (float)amp;
        float complex y;
        if (rx->domain == DP_RX_IN_REAL)
          in = crealf (in) + 0.0f * I;
        if (rx->step (r, in, &y) && nout < nsym)
          {
            out[nout]    = y;
            lock_c[nout] = (unsigned char)rx->locked (r);
            track[nout]  = (unsigned char)(rx->lock (r) > 0.0);
            /* The DISCRIMINATOR, not the frequency estimate: a timing
               transient enters here, and norm_freq is the integrator
               downstream of the loop filter and buries it.

               Kept as a SERIES rather than reduced here, because the window
               it has to be scored over is not known yet: dp_ber_settle()
               computes it from these very lock flags after the burst
               returns. Reducing early scored the acquisition transient and
               read a flat 1.0000 at every point — the maximum the
               discriminator can output, which is what a peak taken across a
               cold start always finds.

               SIGNED, and that is load-bearing. The disturbance numbers want
               the magnitude and take it themselves, but the ramp lag is a
               MEAN, and at these Es/N0 the discriminator's own noise dwarfs
               the lag being measured: at the anchor, |e| has an RMS of 0.54
               against a lag of order 0.1, so `mean |e|` reads the noise and
               is nearly identical at every point in the battery — which is
               precisely how the Doppler point came to be indistinguishable
               from the anchor. The mean of the SIGNED series is not: the
               loop's own integrator forces it to the value that sustains the
               ramp, so the noise averages out of it and what is left is the
               lag. Reducing to |e| here would throw that away before the
               window is even known. */
            err[nout] = rx->last_error (r);
            nout++;
          }
      }
    *clipped = rx->clipped (r);
    if (nf_out)
      *nf_out = rx->norm_freq (r);
    if (lt_out)
      *lt_out = rx->lock_time (r);
    if (zeta_out)
      *zeta_out = rx->zeta (r);
  }

done:
  if (r)
    rx->destroy (r);
  if (ch)
    doppler_channel_destroy (ch);
  if (tx)
    wfm_synth_destroy (tx);
  free (taps);
  free (x);
  free (imp);
  return nout;
}

/**
 * @brief Name the first entry an adapter left NULL, or NULL if it is complete.
 *
 * Every entry is mandatory — the instrument calls all twelve unconditionally.
 * A positional initializer that stops short therefore SEGFAULTS at the first
 * point, before a line is printed, and says nothing about which entry is
 * missing. That is not hypothetical: `RX_CONT` was written when the interface
 * had eleven entries, `zeta` was appended as the twelfth, and the two met in a
 * rebase rather than in either branch's CI.
 *
 * @param rx  The adapter to check.
 * @return The missing entry's name, or NULL if every entry is filled.
 */
static inline const char *
dp_rx_iface_missing (const dp_rx_iface_t *rx)
{
  if (!rx->name)
    return "name";
  if (!rx->create)
    return "create";
  if (!rx->destroy)
    return "destroy";
  if (!rx->step)
    return "step";
  if (!rx->norm_freq)
    return "norm_freq";
  if (!rx->last_error)
    return "last_error";
  if (!rx->lock)
    return "lock";
  if (!rx->locked)
    return "locked";
  if (!rx->lock_time)
    return "lock_time";
  if (!rx->clipped)
    return "clipped";
  if (!rx->zeta)
    return "zeta";
  return NULL;
}

/**
 * @brief Score every whole frame in the settled window into @p fm.
 *
 * The truth-free half of goal 4. A CRC-checked frame needs no payload truth —
 * it either checks or it does not — so this is the one outcome that survives
 * on a real capture and still catches a stable false lock, which EVM and M2M4
 * cannot see and BER can only see with truth AND an alignment.
 *
 * Two detections are in play and they are deliberately different. The RECORD
 * alignment (`lag`, `phase`, from `dp_ber_measure`) is an ACQUISITION over
 * +-DP_BER_LAG_SPAN. The per-frame sync detection here is a CONFIRMATION over
 * +-DP_RX_SYNC_SPAN, because a tracking receiver looks where the frame is due
 * rather than re-acquiring. Accumulating the second is what turns "is this
 * sync word long enough at this Es/N0" into a number.
 *
 * @param m      Constellation order.
 * @param f      The frame descriptor — the SAME one the transmitter built
 *               from, which is the entire point: the layout, the CRC's
 *               position and its bit order are stated once.
 * @param l      Its layout.
 * @param out    Recovered symbols.
 * @param n      How many.
 * @param truth  Transmitted symbol indices, the frame bits cycled.
 * @param nsym   How many.
 * @param lag    The RECORD alignment; every frame's position follows from it.
 * @param phase  The record's residual constellation rotation, radians.
 * @param lo     First symbol of the scored window.
 * @param fm     The accumulator.
 * @param rxbits Scratch, at least `l->total_bits` bytes.
 */
static inline void
dp_rx_score_frames (int m, const wfm_frame_t *f, const wfm_frame_layout_t *l,
                    const float complex *out, size_t n, const uint8_t *truth,
                    size_t nsym, long lag, double phase, size_t lo,
                    frame_meter_state_t *fm, uint8_t *rxbits)
{
  /* Shift whichever array needs it so the residual lag is zero, because
     ber_align_detect searches around lag 0 and has no centre argument. After
     this, `rxa[i]` carries `tra[i]`. */
  size_t               rx_skip = (lag < 0) ? (size_t)(-lag) : 0;
  size_t               tr_skip = (lag > 0) ? (size_t)(lag) : 0;
  const float complex *rxa     = out + rx_skip;
  size_t               n_rxa   = (n > rx_skip) ? n - rx_skip : 0;
  const uint8_t       *tra     = truth + tr_skip;
  size_t               n_tra   = (nsym > tr_skip) ? nsym - tr_skip : 0;
  size_t               nbits   = l->total_bits;
  /* The frame layout is stated in BITS; `out` and `truth` hold SYMBOLS. At
     BPSK the two coincide, which is why this arithmetic survived unnoticed in
     `rx_frame_fer.c` — its geometry struct says "BPSK only for now" and every
     point it runs is M = 2. Lifting it here dropped that constraint: this
     function takes `m`, slices with `m`, and `dp_rx_run()` may hand it a point
     carrying M = 4 or 8. So the conversion is explicit, and it now matches
     what `dp_rx_run()` already does for the RECORD marker (`sync_off / bps`,
     `sync_bits / bps`, `nbits / bps`) — the two disagreeing was the tell. */
  size_t bps  = (size_t)mpsk_bps (m);
  size_t fsym = bps ? nbits / bps : 0;        /* symbols per frame */
  size_t soff = bps ? l->sync_off / bps : 0;  /* sync, in symbols  */
  size_t slen = bps ? l->sync_bits / bps : 0; /* ditto, length     */
  /* One rotation for the whole record, from the marker — not re-estimated per
     frame, which would be a per-frame minimisation over the answer. */
  float complex derot = (float)cos (-phase) + (float)sin (-phase) * I;
  size_t        k;

  /* A geometry whose fields do not land on symbol boundaries cannot be scored
     per frame at all, and guessing would produce confident garbage that is
     self-consistent with the FER computed from it — the failure mode the
     witness exists to catch, in a place the witness does not look. Score
     NOTHING instead: the caller's `frames > 0 && sync_detected > 0 &&
     crc_passed > 0` gate then fails loudly rather than reporting an invented
     rate. */
  if (!bps || !fsym || nbits % bps || l->sync_off % bps || l->sync_bits % bps)
    return;

  for (k = 0; (k + 1) * fsym <= nsym; k++)
    {
      size_t          t_start = k * fsym; /* truth index of the frame */
      long            i0      = (long)t_start - lag; /* its index in `out` */
      size_t          t0_a, a0, s, t;
      dp_ber_marker_t m1;
      dp_ber_sync_t   s1;
      int             crc;

      if (i0 < (long)lo || (size_t)i0 + fsym > n)
        continue;
      if (t_start < tr_skip || t_start + fsym > nsym)
        continue;
      t0_a = t_start + soff - tr_skip; /* marker, aligned coords */
      a0   = (size_t)i0 - rx_skip;
      if (t0_a + slen > n_tra || a0 + fsym > n_rxa)
        continue;

      /* Sync: the DETECTOR's own decision, in a tracking-mode window. Never a
         threshold applied afterwards to a statistic — that is what
         `frame_meter_add` asks for and what makes the miss rate a measurement
         of the sync word rather than of our post-processing. */
      m1.sym    = NULL;
      m1.n      = slen;
      m1.t0     = t0_a;
      m1.period = 0;
      m1.reps   = 0;
      s1        = dp_ber_sync (rxa, n_rxa, tra, n_tra, &m1, m, DP_RX_SYNC_SPAN,
                               DP_BER_SYNC_PFA);

      /* CRC: hard decisions over the frame, checked against nothing but the
         frame's own trailer. No payload truth is consulted here, which is the
         property that makes this usable on a real capture.
         Each symbol carries `bps` bits MSB-first — the same packing
         `dp_rx_run()` builds `truth` with, and the same one
         `wfm_synth_bit_symbol()` transmits, so the two halves of the
         measurement cannot disagree about bit order. */
      for (s = 0; s < fsym; s++)
        {
          float complex ahat;
          unsigned      lab
              = (unsigned)mpsk_slice (out[(size_t)i0 + s] * derot, m, &ahat);
          for (t = 0; t < bps; t++)
            rxbits[s * bps + t] = (uint8_t)((lab >> (bps - 1u - t)) & 1u);
        }
      crc = wfm_frame_crc_ok (f, rxbits);
      frame_meter_add (fm, s1.ok, crc);
    }
}

/**
 * @brief Run one receiver at one operating point — stages 5-10 of §8.
 *
 * The two REFUSE paths are intact and they name themselves: "the loops never
 * locked" and "the marker never detected" call for different repairs, and one
 * counter would say neither.
 *
 * An incomplete adapter is neither of those: it is a harness defect, so it
 * FAILS the record rather than refusing it — a refusal is the instrument
 * declining a number it cannot defend, and this is the instrument being unable
 * to run at all.
 */
static inline dp_rx_result_t
dp_rx_run (const dp_rx_iface_t *rx, const dp_rx_point_t *pt)
{
  dp_rx_result_t       r;
  wfm_frame_t          f = dp_frame_named (pt->frame);
  wfm_frame_layout_t   l;
  dp_ber_t             acc;
  size_t               nbits = wfm_frame_nbits (&f), nsym;
  size_t               bps   = (size_t)mpsk_bps (pt->m);
  uint8_t             *bits = NULL, *truth = NULL, *rxbits = NULL;
  float complex       *out = NULL;
  unsigned char       *lc = NULL, *tk = NULL;
  double              *err = NULL;
  frame_meter_state_t *fm  = NULL;
  size_t               lo = 0, hi = 0, settle = 0;
  int                  settled = 0;

  memset (&r, 0, sizeof r);
  r.point    = pt;
  r.rx       = rx;
  r.acq_frac = r.acq_time_bl = -1.0;
  /* 0, not -1: an unimpaired point HAS no ramp, and the law for it is exactly
     zero rather than absent. `ramp_law_rad > 0` is therefore the honest test
     for "this point measures the ramp", and the gate uses it. */
  r.ramp_lag_rad = r.ramp_law_rad = 0.0;

  {
    const char *gap = dp_rx_iface_missing (rx);
    if (gap)
      {
        static char msg[96];
        snprintf (msg, sizeof msg, "adapter entry '%s' is NULL", gap);
        r.rep.ok  = 0;
        r.rep.why = msg;
        return r;
      }
  }

  if (nbits == 0 || wfm_frame_layout (&f, &l) != 0)
    {
      r.refused = "frame geometry is invalid";
      return r;
    }
  /* A precondition, not a verdict: every metric scores DATA symbols, and a
     preamble-only frame has none. Running it would end in a *settling*
     verdict, the wrong diagnosis for a frame never meant to demodulate. */
  if (l.payload_bits == 0)
    {
      r.refused = "frame carries no payload — nothing to demodulate";
      return r;
    }
  if (nbits % bps != 0)
    {
      r.refused = "frame bits do not divide into whole symbols at this m";
      return r;
    }

  nsym = DP_RX_NSYM;
  /* Frame statistics need BOTH halves: a sync word to detect and a CRC to
     check. Without either there is no truth-free outcome, and reporting one
     anyway is the failure this instrument exists to refuse — which is why
     `framed == 0` prints n/a rather than an FER of 0.0. */
  r.framed     = (l.sync_bits >= 8 && l.crc_bits > 0);
  r.prot_bits  = l.payload_bits + l.crc_bits;
  r.frame_bits = nbits;

  fm     = frame_meter_create (DP_RX_TARGET_FRAME_ERRORS, DP_BER_CONF);
  rxbits = (uint8_t *)malloc (nbits);
  bits   = (uint8_t *)malloc (nbits);
  truth  = (uint8_t *)malloc (nsym);
  out    = (float complex *)malloc (nsym * sizeof *out);
  lc     = (unsigned char *)malloc (nsym);
  tk     = (unsigned char *)malloc (nsym);
  err    = (double *)malloc (nsym * sizeof *err);
  dp_ber_init (&acc, pt->m, DP_BER_TARGET_ERRORS);
  if (!bits || !truth || !out || !lc || !tk || !err || !rxbits || !fm
      || !acc.meter || wfm_frame_bits (&f, bits, nbits) != nbits)
    {
      r.refused = "allocation failed";
      goto done;
    }

  /* Truth is the frame's bits packed into symbol labels MSB-first — the same
     packing wfm_synth_bit_symbol() uses, which is what makes the two halves of
     the measurement agree. The pattern cycles, so the truth does too. */
  {
    size_t i, t;
    for (i = 0; i < nsym; i++)
      {
        unsigned g = 0u;
        for (t = 0; t < bps; t++)
          g = (g << 1) | (unsigned)bits[(i * bps + t) % nbits];
        truth[i] = (uint8_t)g;
      }
  }

  {
    unsigned burst;
    /* Both accumulators have to be satisfied, not just the symbol one: the
       frame meter shares `ber_confidence` and therefore shares its STOPPING
       RULE, so an FER interval quoted from a handful of frame errors is a
       number whose width is set by luck. An unframed point has no second
       target and stops on the first. */
    for (burst = 0; burst < DP_RX_MAX_BURSTS
                    && !(dp_ber_enough (&acc)
                         && (!r.framed || frame_meter_get_enough (fm)));
         burst++)
      {
        dp_ber_marker_t mk;
        double          nf      = 0.0;
        double          zeta    = 0.0;
        long            lt      = -1;
        int             clipped = 0, ok = 0;
        size_t n = dp_rx_burst (rx, pt, bits, nbits, pt->seed + burst, nsym,
                                out, lc, tk, err, &nf, &lt, &zeta, &clipped);
        if (n == 0)
          {
            r.refused = "burst produced no output";
            goto done;
          }
        if (clipped)
          {
            r.clipped = 1;
            r.refused = "front end clipped — the reading is worthless";
            goto done;
          }
        settle
            = dp_ber_settle (pt->bn_timing, pt->bn_carrier, NULL, lc, n, &ok);
        if (!ok
            || settle + (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS + 500 >= n)
          {
            r.unsettled++;
            continue;
          }
        settled = 1;

        /* Over the SCORED window, so it answers "was the receiver's own
           indicator asserted while these numbers were being taken" rather
           than "did it ever assert". Accumulated across bursts by the same
           mean the trio uses. */
        r.lock_duty += dp_rx_duty (lc, settle, n);
        r.lock_stat_duty += dp_rx_duty (tk, settle, n);

        if (l.sync_bits >= 8)
          {
            /* The sync word IS the marker, repeating with the frame period. */
            size_t per     = nbits / bps;
            size_t floor_t = settle + (size_t)DP_BER_LAG_SPAN;
            size_t t0      = l.sync_off / bps;
            if (t0 < floor_t)
              t0 += per * ((floor_t - t0 + per - 1) / per);
            mk.sym    = NULL;
            mk.n      = l.sync_bits / bps;
            mk.t0     = t0;
            mk.period = per;
            mk.reps   = 0;
          }
        else
          {
            mk.sym    = NULL;
            mk.n      = DP_BER_SYNC_SYMS;
            mk.t0     = settle + (size_t)DP_BER_LAG_SPAN;
            mk.period = 0;
            mk.reps   = 0;
          }
        if (mk.t0 + mk.n > nsym)
          {
            r.unaligned++;
            continue;
          }

        /* dp_ber_measure() is the sanctioned one-call path: it "wires the
           three gates together in the only order that is correct, and it
           places the marker so the alignment is fixed on symbols DISJOINT
           from the ones scored". This harness hand-rolled sync -> lag -> lo
           -> score -> report by copying rx_frame_fer.c's inline version,
           which is a second copy of a subtle ordering — including the
           `lo` computation that has to respect BOTH the settled point and
           the end of the marker. What stays here is only the marker itself,
           because that comes from the FRAME layout and nothing in the
           library knows about frames. */
        r.rep = dp_ber_measure (&acc, out, n, truth, nsym, pt->esn0_db, settle,
                                ok, &mk);
        if (!r.rep.aligned)
          {
            r.unaligned++;
            continue;
          }
        lo       = r.rep.window_lo;
        hi       = r.rep.window_hi;
        r.bursts = burst + 1u;

        /* The fourth metric, over the SAME record — goal 4 asks for the four
           together because they fail differently, and FER is the only one of
           them that is truth-free AND sees a false lock. */
        if (r.framed)
          dp_rx_score_frames (pt->m, &f, &l, out, n, truth, nsym, r.rep.lag,
                              r.rep.phase, lo, fm, rxbits);

        /* The loop numbers, over the SAME settled window the trio used —
           which the report hands back rather than the caller recomputing. */
        {
          double s1 = 0.0, s2 = 0.0, pk = 0.0;
          size_t k, cnt = 0;
          for (k = r.rep.window_lo; k < r.rep.window_hi && k < n; k++)
            {
              double e = err[k];
              if (fabs (e) > pk)
                pk = fabs (e);
              s1 += e; /* SIGNED — the lag; see dp_rx_burst()       */
              s2 += e * e;
              cnt++;
            }
          if (pk > r.disturb_peak_rad)
            r.disturb_peak_rad = pk;
          r.disturb_rms_rad = cnt ? sqrt (s2 / (double)cnt) : -1.0;
          /* The ramp lag, and only where a ramp exists. The magnitude is
             taken at the END, of the MEAN — the sign is the direction the
             Doppler happens to run and says nothing about the loop, but
             taking it per sample first is what turns the measurement into a
             reading of the discriminator's noise. */
          r.ramp_law_rad = dp_rx_ramp_law (pt, zeta);
          if (r.ramp_law_rad > 0.0 && cnt)
            r.ramp_lag_rad = fabs (s1 / (double)cnt);
        }
        /* Acquisition, reported only where the answer is not trivially zero:
           at foff = 0 the correct answer IS zero, so a loop that never steers
           scores perfectly and a working one shows its own jitter — the
           inversion this file's design notes open with. */
        if (pt->foff != 0.0)
          {
            double want = pt->foff / pt->sps; /* cycles/sample */
            r.acq_frac  = (nf - (pt->fc - want)) / want;
          }
        if (lt >= 0)
          r.acq_time_bl = (double)lt * pt->bn_carrier;
      }
  }

  /* Read the REPORT, not a local sync result: dp_ber_measure() owns the
     alignment now, so a stale `sy` here stamped "never detected" on every
     record including the ones that aligned. */
  if (!settled)
    r.refused = "no burst settled — the loops never locked";
  else if (!r.rep.aligned)
    r.refused = "no burst aligned — the marker never detected";

  r.frames        = frame_meter_get_frames (fm);
  r.sync_detected = frame_meter_get_sync_detected (fm);
  r.crc_passed    = frame_meter_get_crc_passed (fm);
  r.frame_enough  = frame_meter_get_enough (fm);
  r.fer           = frame_meter_fer (fm);
  r.sync_miss     = frame_meter_sync_miss (fm);

  /* Accumulated per burst above; a mean over the bursts that produced
     numbers, so it is a rate over exactly the record the trio scored. */
  if (r.bursts)
    {
      r.lock_duty /= (double)r.bursts;
      r.lock_stat_duty /= (double)r.bursts;
    }

done:
  /* The anchor's two terms. A frame is delivered when its sync was found AND
     its CRC checked, so the FER those imply is the miss rate plus what the bit
     errors do to the frames that WERE found. Both halves are needed: gating on
     the CRC term alone would let a sync word that misses most frames pass. */
  r.crc_fail = r.sync_detected ? (double)(r.sync_detected - r.crc_passed)
                                     / (double)r.sync_detected
                               : NAN;
  r.crc_fail_pred
      = (r.rep.ber.symbols && r.prot_bits)
            ? 1.0 - pow (1.0 - r.rep.ber.p_hat, (double)r.prot_bits)
            : NAN;
  r.fer_pred = r.sync_miss.p_hat + (1.0 - r.sync_miss.p_hat) * r.crc_fail_pred;

  dp_ber_free (&acc);
  frame_meter_destroy (fm);
  free (bits);
  free (truth);
  free (out);
  free (lc);
  free (tk);
  free (err);
  free (rxbits);
  return r;
}

/**
 * @brief The named operating points — the battery itself.
 *
 * Values of one struct, not five code paths, exactly as the named frame set
 * is. A result carries the pointer, so a number can always name the point that
 * produced it. The anchor is where `validate_rx_frame_fer` already measures,
 * so the instrument's first job is reproducing a number that exists.
 */
static inline const dp_rx_point_t *
dp_rx_point (dp_rx_point_name_t name)
{
  static const dp_rx_point_t pts[DP_RX_POINT_COUNT] = {
    /* ANCHOR — unimpaired; the reference the others are read against. */
    { "anchor", RX_FRAME_CONT, 2, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0,
      0.01, 0.005, 6.79, 0.0, 0.0, 0.0, 0.0, -10.0, 7u },
    /* ACQUIRE — a static offset at half the design envelope B_l/M. Inside it
       the loop is linear and settles; beyond it pull-in is nonlinear and
       depends on initial conditions, which is why nothing asks for more. */
    { "acquire", RX_FRAME_CONT, 2, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0025,
      0.01, 0.005, 6.79, 0.0, 0.0, 0.0, 0.0, -10.0, 7u },
    /* DOPPLER — a RATE, not an offset: a type-2 loop nulls a step regardless
       of gain, so only a ramp leaves a constant lag with a closed form. It
       comes through doppler_channel, so the carrier and every clock move
       together as they physically must.

       The RATE IS THE POINT, and it is sized from the answer it has to make
       observable rather than from a plausible-looking geometry. At 0.02 ppm/s
       — where this sat until the lag was first computed rather than assumed —
       the law predicts 2.2e-4 rad against a linear range of pi/4, so the
       measurement was three and a half decades below anything the loop does,
       and every number this point produced was byte-identical to `anchor`'s.
       A point that reproduces the reference is not measuring the thing it was
       named for. 9.2 ppm/s puts the predicted lag at ~0.1 rad: a comfortable
       eighth of the range, so the S-curve is still linear and the LAW rather
       than its breakdown is what is being checked, and two decades clear of
       the settled mean's own standard error. The static `d0` stays where it
       is — a type-2 loop nulls it, `acquire` is the point that scores an
       offset, and moving it here would only blur which disturbance the lag
       came from. */
    { "doppler", RX_FRAME_CONT, 2, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0,
      0.01, 0.005, 6.79, 1.0e6, 2.4e9, 0.02, 9.2, -10.0, 7u },
    /* RUNBURST — the timing loop coasts through a transition-starved stretch
       and then slews. The question is whether that reaches the CARRIER loop,
       which is the whole reason a pre-terminal tap exists. */
    { "runburst", RX_FRAME_BURST, 2, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0,
      0.01, 0.005, 6.79, 1.0e6, 2.4e9, 0.02, 0.0, -10.0, 7u },
    /* OVERSAMPLED — where m_out and the bank rate stop being construction
       constants and become planner outcomes. */
    { "oversampled", RX_FRAME_CONT, 2, 64.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0,
      0.0, 0.01, 0.005, 6.79, 0.0, 0.0, 0.0, 0.0, -18.0, 7u },
    /* QPSK — the anchor at M = 4, at ITS OWN SER=1e-3 Es/N0. Holding one
       Es/N0 across M would compare constellations rather than receivers:
       the same 6.79 dB that anchors BPSK at 1e-3 leaves QPSK at ~4e-2, so
       every M is read at the Es/N0 where it means the same thing.
       ber_esn0_db_for_ser(4, 1e-3) = 10.3453. */
    { "qpsk", RX_FRAME_CONT, 4, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0, 0.01,
      0.005, 10.3453, 0.0, 0.0, 0.0, 0.0, -10.0, 7u },
    /* PSK8 — ber_esn0_db_for_ser(8, 1e-3) = 15.6782. The worst case for
       an NDA path: the M-th-power squaring loss grows with M while the
       decision margin shrinks to +-pi/8. m_out is left DERIVED, which the
       header says reaches 8 and calls non-optional at M = 8. */
    { "psk8", RX_FRAME_CONT, 8, 8.0, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0, 0.01,
      0.005, 15.6782, 0.0, 0.0, 0.0, 0.0, -10.0, 7u },
    /* IRRATIONAL — the header's own 17.33389. Not a round rate and not a
       ratio of small integers, so the symbol boundary lands between
       samples on almost every symbol. */
    { "irrational", RX_FRAME_CONT, 2, 17.33389, 0, DP_RX_BETA, DP_RX_SPAN, 0.0,
      0.0, 0.01, 0.005, 6.79, 0.0, 0.0, 0.0, 0.0, -10.0, 7u },
    /* RATE_ODD — high and irrational at once. Kept as its own point
       because it is where a hand-rolled Python estimator reported a
       receiver defect while its OWN alignment had failed (align_ok = 0,
       margin -3.4 dB, a lag outside the window it searched). The
       instrument's four gates either defend a number here or refuse, and
       a refusal is a result rather than a failure. */
    { "rate_odd", RX_FRAME_CONT, 2, 31.7, 0, DP_RX_BETA, DP_RX_SPAN, 0.0, 0.0,
      0.01, 0.005, 6.79, 0.0, 0.0, 0.0, 0.0, -14.0, 7u },
  };
  if ((int)name < 0 || (int)name >= DP_RX_POINT_COUNT)
    return NULL;
  return &pts[name];
}

/**
 * @brief Apply the gates to a record.
 *
 * A REFUSAL is not a failure: it is the harness declining to report a number
 * it cannot defend, which is goal 1 working. It is printed and returns 0.
 * What fails is a record that claims to be a measurement and is not.
 */
static inline int
dp_rx_check (const dp_rx_result_t *r)
{
  if (r->refused)
    {
      printf ("  %-12s %-11s REFUSED — %s\n", r->rx->name, r->point->name,
              r->refused);
      return 0;
    }
  if (!r->rep.ok)
    {
      printf ("FAIL %s @ %s: %s\n", r->rx->name, r->point->name,
              r->rep.why ? r->rep.why : "a gate failed");
      return 1;
    }
  /* The ramp law, at any point that presents a ramp — the ONE closed form a
     tracking loop's gain can be checked against end to end, because a type-2
     loop nulls a frequency step regardless of gain and therefore cannot tell
     a correct gain from a wrong one. Everything on the path is inside it: the
     channel's ppm-to-Hz conversion, the discriminator, the loop filter's
     gains, and whatever turns the filter's output into the LO's control port.
     A gain error anywhere in that chain moves the lag by its own factor, and
     that is the gh-765 class of defect.

     Measured, both ways: scaling `freq_scale` by 2 reads +101.4% and fails,
     by 1.25 reads -19.4% and fails, and the trio stays GREEN through both —
     SER 1.09e-3, EVM -7.35 dB, unmoved. A carrier loop running at twice its
     stated bandwidth is invisible to every other number this instrument
     produces, which is the whole argument for the point.

     WHAT IT DOES NOT COVER, and the reason is structural rather than a
     tolerance. gh-765 itself was `freq_scale` missing its `* upd` factor —
     the loop filter's output taken as radians per UPDATE rather than per
     symbol. Every point in this battery runs `nda_tap = 0` (STROBE), whose
     update rate is exactly 1, so that factor IS 1 and removing it changes
     nothing: applied here it leaves this gate byte-identical and green.
     `rx_nda_tap.c` is what catches it, because it sweeps the taps whose
     update rate is not 1 — under the same sabotage it fails six ramp rows on
     `mf_out` (upd 2.0) and `mf_in` (upd 1.5625) and none on `strobe`. So
     this gate pins the gain of the loop the battery actually runs; it does
     not pin the tap-rate conversion, and a battery point at a non-unity tap
     is what would.

     That is a SETTLED boundary, not a gap awaiting a decision. The battery is
     STROBE-only on purpose: `ContinuousMpskReceiver` was measured pinning a
     tap whose update rate is not 1 and its lock statistic no longer meant
     what its threshold said, so the flavor moved to `strobe` too
     (doppler#791). The tap-rate conversion is therefore `rx_nda_tap.c`'s to
     pin, permanently, and that file sweeps all three taps precisely because
     nothing else does. Do not add a fast-tap point here to close the gap —
     it would import a lock statistic this harness's settling gate cannot
     read, which is the defect that started it. */
  if (r->ramp_law_rad > 0.0
      && !(fabs (r->ramp_lag_rad - r->ramp_law_rad)
           <= DP_RX_RAMP_TOL * r->ramp_law_rad))
    {
      printf ("FAIL %s @ %s: settled lag %.4g rad against %.4g predicted by "
              "2*pi*r/wn^2 (r %.4g cyc/sym^2, bn %.4g, tol %.0f%%) — the "
              "carrier loop's gain is not what its bandwidth says\n",
              r->rx->name, r->point->name, r->ramp_lag_rad, r->ramp_law_rad,
              dp_rx_ramp_rate (r->point), r->point->bn_carrier,
              100.0 * DP_RX_RAMP_TOL);
      return 1;
    }
  /* An unframed point has no truth-free frame outcome and says so; there is
     nothing here to gate and pretending otherwise would invent a number. */
  if (!r->framed)
    return 0;
  if (!r->frame_enough)
    {
      printf ("FAIL %s @ %s: only %zu of %u frame errors in %zu frames\n",
              r->rx->name, r->point->name, r->frames - r->crc_passed,
              DP_RX_TARGET_FRAME_ERRORS, r->frames);
      return 1;
    }
  /* Whatever the anchor goes on to say, the machinery must have RUN: frames
     attempted, frames detected, frames checked. Without this a configuration
     that produced no frames at all reaches the OK line, and an invented miss
     rate is still self-consistent with the FER computed from it. */
  if (!(r->frames > 0 && r->sync_detected > 0 && r->crc_passed > 0))
    {
      printf ("FAIL %s @ %s: %zu frames, %zu synced, %zu crc-ok — the frame "
              "path did not run\n",
              r->rx->name, r->point->name, r->frames, r->sync_detected,
              r->crc_passed);
      return 1;
    }
  /* The FER anchor, on the interval's LOWER limit so counting noise cannot
     flake it, and one-sided because clustered errors hit FEWER frames for the
     same BER — the independent-bit expression is an upper bound. */
  {
    /* How much of the predicted FER the CRC term actually accounts for. When
       most of it is sync miss, the "prediction" is dominated by a number the
       harness MEASURED and handed to itself, so comparing against it tests
       nothing — say that instead of passing. */
    double crc_share
        = (r->fer_pred > 0.0)
              ? (1.0 - r->sync_miss.p_hat) * r->crc_fail_pred / r->fer_pred
              : 0.0;
    if (crc_share < DP_RX_FER_ANCHOR_SHARE)
      printf ("  %-12s %-11s NOTE: FER anchor skipped — only %.0f%% of the "
              "predicted FER is CRC failure, the rest is a sync miss of %.4g "
              "that this gate would merely be handing back to itself\n",
              r->rx->name, r->point->name, 100.0 * crc_share,
              r->sync_miss.p_hat);
    else if (!(r->fer.lo <= r->fer_pred * DP_RX_FER_TOL))
      {
        printf ("FAIL %s @ %s: FER lower limit %.4g exceeds %.4g predicted "
                "from sync miss %.4g and BER over %zu protected bits "
                "(x%.2f tolerance)\n",
                r->rx->name, r->point->name, r->fer.lo, r->fer_pred,
                r->sync_miss.p_hat, r->prot_bits, DP_RX_FER_TOL);
        return 1;
      }
  }
  return 0;
}

/* ── 7. Was the sync detector actually ASKED? ───────────────────────────── */

/**
 * @brief Run-level witness that the per-frame sync detection is real.
 *
 * Measured, not reasoned. Hard-wiring `sync_ok = 1` — a harness that stops
 * consulting the detector and asserts every frame was found — leaves EVERY
 * per-point gate in `dp_rx_check()` green, because an invented miss rate is
 * still perfectly self-consistent with the FER computed from it. The FER, the
 * prediction and the anchor all move together and agree.
 *
 * No single point can catch that, and that is the whole reason this is
 * separate: a point may legitimately miss nothing (a PN-127 sync word) or
 * almost everything (Barker-13 at an Es/N0 floor), so neither outcome is
 * evidence on its own. The RUN can. Across the battery the detector must be
 * observed both to ACCEPT and to REFUSE, and the standard point set supplies
 * both by construction — which is part of why `runburst` carries the short
 * sync word.
 */
typedef struct
{
  size_t detected; /**< frames whose sync the detector accepted */
  size_t missed;   /**< frames whose sync it refused            */
} dp_rx_witness_t;

/** @brief Fold one record into the witness. Refused and unframed points
 * contribute nothing, because neither ran the detector. */
static inline void
dp_rx_witness_add (dp_rx_witness_t *w, const dp_rx_result_t *r)
{
  if (r->refused || !r->framed)
    return;
  w->detected += r->sync_detected;
  w->missed += r->frames - r->sync_detected;
}

/** @brief Apply the witness. @return 0 pass, 1 fail. */
static inline int
dp_rx_witness_check (const dp_rx_witness_t *w)
{
  if (w->detected && w->missed)
    return 0;
  printf ("FAIL: the per-frame sync detector was never observed to %s "
          "(%zu detected, %zu missed) — a detector that refused nothing, or "
          "accepted nothing, is not a measurement\n",
          w->missed ? "accept" : "refuse", w->detected, w->missed);
  return 1;
}

/**
 * @brief Did this receiver produce ANY defensible record?
 *
 * The second run-level gate, and it exists for the same reason as
 * `dp_rx_witness_t`: a property that is true of every point individually and
 * false of the run.
 *
 * `dp_rx_check()` returns 0 for a refusal, deliberately — a refusal is the
 * harness declining to defend a number, which is goal 1 working, and a
 * `qpsk`/`psk8` frame-geometry refusal is the design doing its job. But
 * **`--check` therefore exits 0 when a receiver refuses EVERY point**, and
 * that is how `ContinuousMpskReceiver` shipped pinning `nda_tap = mf_in`:
 * nine `no burst settled` lines, exit 0, on a receiver whose EVM is on the
 * matched-filter bound (doppler#791). Each line was individually correct.
 * The aggregate was a receiver that does not work.
 *
 * Nine refusals is not nine refusals. It is one result.
 *
 * No threshold and no per-point allowlist, because none is needed: zero
 * defensible records is never legitimate for something in the standard
 * battery, so there is nothing to tune. A receiver that scores even one point
 * is measured, and its refusals stay uncounted exactly as the header argues.
 */
typedef struct
{
  const char *name;    /**< the receiver, for the message          */
  unsigned    scored;  /**< points that produced a record          */
  unsigned    refused; /**< points that declined to                */
} dp_rx_tally_t;

/** @brief Fold one record into the tally. */
static inline void
dp_rx_tally_add (dp_rx_tally_t *t, const dp_rx_result_t *r)
{
  if (!t->name)
    t->name = r->rx->name;
  if (r->refused)
    t->refused++;
  else
    t->scored++;
}

/** @brief Apply the tally. @return 0 pass, 1 fail. */
static inline int
dp_rx_tally_check (const dp_rx_tally_t *t)
{
  if (t->scored > 0 || t->refused == 0)
    return 0;
  printf ("FAIL %s: refused every point (%u/%u) — a receiver in the standard "
          "battery that scores nothing does not work\n",
          t->name ? t->name : "(receiver)", t->refused,
          t->refused + t->scored);
  return 1;
}

/** @brief Print the standard record — one point, every number, one block. */
static inline void
dp_rx_print (const dp_rx_result_t *r)
{
  if (r->refused)
    {
      printf ("  %-12s %-11s REFUSED — %s\n", r->rx->name, r->point->name,
              r->refused);
      return;
    }
  printf ("  %-12s %-11s SER %.3e  EVM %6.2f dB  M2M4 %5.2f dB  loss %5.2f dB"
          "  |e| pk %.3f rms %.4f  acq %.3f t %.2f/Bl  lock %3.0f%%/%3.0f%%"
          "  %s\n",
          r->rx->name, r->point->name, r->rep.ser.p_hat, r->rep.evm_db,
          r->rep.m2m4_db, r->rep.loss_db, r->disturb_peak_rad,
          r->disturb_rms_rad, r->acq_frac, r->acq_time_bl,
          100.0 * r->lock_duty, 100.0 * r->lock_stat_duty,
          r->rep.ok ? "ok" : (r->rep.why ? r->rep.why : "not ok"));
  /* Printed only where a ramp exists. An unimpaired point has a law of
     exactly zero, and a row of zeros beside a gate that cannot fire reads as
     a measurement when it is an absence. */
  if (r->ramp_law_rad > 0.0)
    printf ("  %-12s %-11s   ramp lag %.4g rad vs %.4g predicted "
            "(%+.1f%%, tol %.0f%%)  r %.4g cyc/sym^2, %.3f of the pi/%d "
            "linear range\n",
            r->rx->name, r->point->name, r->ramp_lag_rad, r->ramp_law_rad,
            100.0 * (r->ramp_lag_rad - r->ramp_law_rad) / r->ramp_law_rad,
            100.0 * DP_RX_RAMP_TOL, dp_rx_ramp_rate (r->point),
            r->ramp_lag_rad / (M_PI / (2.0 * r->point->m)), 2 * r->point->m);
  /* n/a, never 0.0: an unprotected stream having no truth-free error detector
     is the gap a frame closes, and printing a zero would hide the only thing
     the baseline has to say. */
  if (!r->framed)
    printf ("  %-12s %-11s   FER n/a — no sync word and no CRC: no "
            "truth-free frame outcome\n",
            r->rx->name, r->point->name);
  else
    printf ("  %-12s %-11s   FER %.4g [%.4g, %.4g]  %zu frames, %zu synced, "
            "%zu crc-ok  sync miss %.4g [%.4g, %.4g]  crc fail %.4g vs %.4g "
            "predicted over %zu bits\n",
            r->rx->name, r->point->name, r->fer.p_hat, r->fer.lo, r->fer.hi,
            r->frames, r->sync_detected, r->crc_passed, r->sync_miss.p_hat,
            r->sync_miss.lo, r->sync_miss.hi, r->crc_fail, r->crc_fail_pred,
            r->prot_bits);
  /* The headroom, printed rather than left to be discovered when it runs out.
     Requiring BOTH error targets is right -- an FER interval from a handful of
     frame errors is set by luck -- but it converts a headroom question into a
     red gate, so a point approaching DP_RX_MAX_BURSTS is visible here before
     it flakes in CI rather than after. */
  printf ("  %-12s %-11s   %u/%u burst(s), %d unsettled, %d unaligned\n",
          r->rx->name, r->point->name, r->bursts, (unsigned)DP_RX_MAX_BURSTS,
          r->unsettled, r->unaligned);
  /* The FER anchor's POWER, printed beside the anchor itself.
     `fer.lo <= fer_pred * DP_RX_FER_TOL` is one-sided and asserted on the
     interval's lower limit, which is right -- but at 120 frames around an FER
     of 0.65 the exact interval is nearly half the unit interval, so the lower
     limit sits far below the prediction and the test passes for almost any
     measurement. Measured: corrupting every other frame's CRC leaves this
     gate GREEN (doppler#796). The slack is therefore reported rather than
     left implicit, so nobody reads the anchor as load-bearing where it is
     not. Under 1.0 means the gate has no headroom to spend and is doing
     work; well over 1.0 means it cannot currently fail. */
  if (r->framed && r->fer_pred > 0.0)
    printf ("  %-12s %-11s   FER anchor slack %.2fx (lower limit %.4g vs "
            "%.4g allowed)\n",
            r->rx->name, r->point->name,
            (r->fer_pred * DP_RX_FER_TOL)
                / (r->fer.lo > 0.0 ? r->fer.lo : 1e-9),
            r->fer.lo, r->fer_pred * DP_RX_FER_TOL);
}

#endif /* DP_RX_TEST_H */
