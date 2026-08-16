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
             double *err, double *nf_out, long *lt_out, int *clipped)
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
               cold start always finds. */
            err[nout] = fabs (rx->last_error (r));
            nout++;
          }
      }
    *clipped = rx->clipped (r);
    if (nf_out)
      *nf_out = rx->norm_freq (r);
    if (lt_out)
      *lt_out = rx->lock_time (r);
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
 * @brief Run one receiver at one operating point — stages 5-10 of §8.
 *
 * The two REFUSE paths are intact and they name themselves: "the loops never
 * locked" and "the marker never detected" call for different repairs, and one
 * counter would say neither.
 */
static inline dp_rx_result_t
dp_rx_run (const dp_rx_iface_t *rx, const dp_rx_point_t *pt)
{
  dp_rx_result_t     r;
  wfm_frame_t        f = dp_frame_named (pt->frame);
  wfm_frame_layout_t l;
  dp_ber_t           acc;
  size_t             nbits = wfm_frame_nbits (&f), nsym;
  size_t             bps   = (size_t)mpsk_bps (pt->m);
  uint8_t           *bits = NULL, *truth = NULL;
  float complex     *out = NULL;
  unsigned char     *lc = NULL, *tk = NULL;
  double            *err = NULL;
  size_t             lo = 0, hi = 0, settle = 0;
  int                settled = 0;

  memset (&r, 0, sizeof r);
  r.point    = pt;
  r.rx       = rx;
  r.acq_frac = r.acq_time_bl = -1.0;

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

  nsym         = DP_RX_NSYM;
  r.framed     = (l.crc_bits > 0);
  r.frame_bits = nbits;

  bits  = (uint8_t *)malloc (nbits);
  truth = (uint8_t *)malloc (nsym);
  out   = (float complex *)malloc (nsym * sizeof *out);
  lc    = (unsigned char *)malloc (nsym);
  tk    = (unsigned char *)malloc (nsym);
  err   = (double *)malloc (nsym * sizeof *err);
  dp_ber_init (&acc, pt->m, DP_BER_TARGET_ERRORS);
  if (!bits || !truth || !out || !lc || !tk || !err || !acc.meter
      || wfm_frame_bits (&f, bits, nbits) != nbits)
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
    for (burst = 0; burst < DP_RX_MAX_BURSTS && !dp_ber_enough (&acc); burst++)
      {
        dp_ber_marker_t mk;
        double          nf      = 0.0;
        long            lt      = -1;
        int             clipped = 0, ok = 0;
        size_t n = dp_rx_burst (rx, pt, bits, nbits, pt->seed + burst, nsym,
                                out, lc, tk, err, &nf, &lt, &clipped);
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
        settle = dp_ber_settle (pt->bn_timing, pt->bn_carrier, NULL, lc,
                                pt->acq_to_track ? tk : NULL, n, &ok);
        if (!ok
            || settle + (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS + 500 >= n)
          {
            r.unsettled++;
            continue;
          }
        settled = 1;

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

        /* The loop numbers, over the SAME settled window the trio used —
           which the report hands back rather than the caller recomputing. */
        {
          double s2 = 0.0, pk = 0.0;
          size_t k, cnt       = 0;
          for (k = r.rep.window_lo; k < r.rep.window_hi && k < n; k++)
            {
              if (err[k] > pk)
                pk = err[k];
              s2 += err[k] * err[k];
              cnt++;
            }
          if (pk > r.disturb_peak_rad)
            r.disturb_peak_rad = pk;
          r.disturb_rms_rad = cnt ? sqrt (s2 / (double)cnt) : -1.0;
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

done:
  dp_ber_free (&acc);
  free (bits);
  free (truth);
  free (out);
  free (lc);
  free (tk);
  free (err);
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
    { "anchor", RX_FRAME_CONT, 2,    8.0,   0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,      0.0,           0.01, 0.005, 0,     0,          6.79,
      0.0,      0.0,           0.0,  0.0,   -10.0, 7u },
    /* ACQUIRE — a static offset at half the design envelope B_l/M. Inside it
       the loop is linear and settles; beyond it pull-in is nonlinear and
       depends on initial conditions, which is why nothing asks for more. */
    { "acquire", RX_FRAME_CONT, 2,    8.0,   0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,       0.0025,        0.01, 0.005, 0,     0,          6.79,
      0.0,       0.0,           0.0,  0.0,   -10.0, 7u },
    /* DOPPLER — a RATE, not an offset: a type-2 loop nulls a step regardless
       of gain, so only a ramp leaves a constant lag with a closed form. It
       comes through doppler_channel, so the carrier and every clock move
       together as they physically must. */
    { "doppler", RX_FRAME_CONT, 2,    8.0,   0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,       0.0,           0.01, 0.005, 0,     0,          6.79,
      1.0e6,     2.4e9,         0.02, 0.02,  -10.0, 7u },
    /* RUNBURST — the timing loop coasts through a transition-starved stretch
       and then slews. The question is whether that reaches the CARRIER loop,
       which is the whole reason a pre-terminal tap exists. */
    { "runburst", RX_FRAME_BURST,
      2,          8.0,
      0,          DP_RX_BETA,
      DP_RX_SPAN, 0.0,
      0.0,        0.01,
      0.005,      0,
      0,          6.79,
      1.0e6,      2.4e9,
      0.02,       0.0,
      -10.0,      7u },
    /* OVERSAMPLED — where m_out and the bank rate stop being construction
       constants and become planner outcomes. */
    { "oversampled", RX_FRAME_CONT, 2,    64.0,  0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,           0.0,           0.01, 0.005, 0,     0,          6.79,
      0.0,           0.0,           0.0,  0.0,   -18.0, 7u },
    /* QPSK — the anchor at M = 4, at ITS OWN SER=1e-3 Es/N0. Holding one
       Es/N0 across M would compare constellations rather than receivers:
       the same 6.79 dB that anchors BPSK at 1e-3 leaves QPSK at ~4e-2, so
       every M is read at the Es/N0 where it means the same thing.
       ber_esn0_db_for_ser(4, 1e-3) = 10.3453. */
    { "qpsk", RX_FRAME_CONT, 4,    8.0,   0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,    0.0,           0.01, 0.005, 0,     0,          10.3453,
      0.0,    0.0,           0.0,  0.0,   -10.0, 7u },
    /* PSK8 — ber_esn0_db_for_ser(8, 1e-3) = 15.6782. The worst case for
       an NDA path: the M-th-power squaring loss grows with M while the
       decision margin shrinks to +-pi/8. m_out is left DERIVED, which the
       header says reaches 8 and calls non-optional at M = 8. */
    { "psk8", RX_FRAME_CONT, 8,    8.0,   0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,    0.0,           0.01, 0.005, 0,     0,          15.6782,
      0.0,    0.0,           0.0,  0.0,   -10.0, 7u },
    /* IRRATIONAL — the header's own 17.33389. Not a round rate and not a
       ratio of small integers, so the symbol boundary lands between
       samples on almost every symbol. */
    { "irrational",
      RX_FRAME_CONT,
      2,
      17.33389,
      0,
      DP_RX_BETA,
      DP_RX_SPAN,
      0.0,
      0.0,
      0.01,
      0.005,
      0,
      0,
      6.79,
      0.0,
      0.0,
      0.0,
      0.0,
      -10.0,
      7u },
    /* RATE_ODD — high and irrational at once. Kept as its own point
       because it is where a hand-rolled Python estimator reported a
       receiver defect while its OWN alignment had failed (align_ok = 0,
       margin -3.4 dB, a lag outside the window it searched). The
       instrument's four gates either defend a number here or refuse, and
       a refusal is a result rather than a failure. */
    { "rate_odd", RX_FRAME_CONT, 2,    31.7,  0,     DP_RX_BETA, DP_RX_SPAN,
      0.0,        0.0,           0.01, 0.005, 0,     0,          6.79,
      0.0,        0.0,           0.0,  0.0,   -14.0, 7u },
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
  return 0;
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
          "  |e| pk %.3f rms %.4f  acq %.3f t %.2f/Bl  %s\n",
          r->rx->name, r->point->name, r->rep.ser.p_hat, r->rep.evm_db,
          r->rep.m2m4_db, r->rep.loss_db, r->disturb_peak_rad,
          r->disturb_rms_rad, r->acq_frac, r->acq_time_bl,
          r->rep.ok ? "ok" : (r->rep.why ? r->rep.why : "not ok"));
}

#endif /* DP_RX_TEST_H */
