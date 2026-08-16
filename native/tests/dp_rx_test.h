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
#include "wfm/wfm_frame.h"

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

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

  double bn_timing;    /**< timing loop noise bandwidth, per symbol       */
  double bn_carrier;   /**< carrier loop noise bandwidth, per symbol      */
  int    acq_to_track; /**< NDA -> decision-directed handover             */
  int    nda_tap;      /**< MPSK_RX_NDA_TAP_*                             */

  double esn0_db; /**< matched-filter-output Es/N0 the stimulus carries   */

  /* Impairment — `doppler_channel`, so the carrier and every clock move
     TOGETHER. `doppler_ppm` alone is a static offset plus a clock error;
     `doppler_rate_ppm_s` adds the ramp. Leave both 0 for an unimpaired run. */
  double fs_hz;              /**< receive sample rate, Hz                 */
  double carrier_hz;         /**< RF carrier — converts ppm to Hz         */
  double doppler_ppm;        /**< d0, ppm of nominal                      */
  double doppler_rate_ppm_s; /**< d-dot, ppm/s                            */

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
  size_t         frames, sync_detected, crc_passed;
  ber_interval_t fer, sync_miss;

  /* Loop behaviour, from the SAME record as the trio. */
  double acq_frac;     /**< fraction of `foff` removed; 1.0 is acquired    */
  double acq_time_bl;  /**< symbols to lock, in units of 1/bn_carrier —
                            the loop's own clock, so the number is
                            comparable across every point in the set      */
  double ramp_lag_rad; /**< settled phase lag under a Doppler RATE       */
  double ramp_law_rad; /**< what `2*pi*r/wn^2` says it should be         */
  double disturb_peak_rad; /**< peak |discriminator| excursion           */
  double disturb_rms_rad;  /**< and its RMS                              */

  int         clipped; /**< the front end clipped: nothing here is real  */
  const char *refused; /**< non-NULL: not measurable, and WHY            */
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
  DP_RX_POINT_COUNT
} dp_rx_point_name_t;

/** @brief The named point, or NULL if @p name is out of range. */
const dp_rx_point_t *dp_rx_point (dp_rx_point_name_t name);

/* ── 5. The methods ─────────────────────────────────────────────────────── */

/**
 * @brief Run one receiver at one operating point — the whole §8 sequence.
 *
 * describe -> generate -> impair -> run -> settle -> align -> score -> enough
 * -> anchor -> report, with the two REFUSE paths intact. Returns a record
 * whose `refused` is non-NULL rather than a number it cannot defend.
 */
dp_rx_result_t dp_rx_run (const dp_rx_iface_t *rx, const dp_rx_point_t *pt);

/**
 * @brief Apply the gates to a record.
 * @return 0 when every gate the point claims is met; non-zero otherwise, with
 *         the reason already printed.
 */
int dp_rx_check (const dp_rx_result_t *r);

/** @brief Print the standard record — one point, every number, one block. */
void dp_rx_print (const dp_rx_result_t *r);

#endif /* DP_RX_TEST_H */
