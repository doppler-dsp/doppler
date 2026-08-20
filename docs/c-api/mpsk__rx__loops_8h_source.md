

# File mpsk\_rx\_loops.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md)

[Go to the documentation of this file](mpsk__rx__loops_8h.md)


```C++

#ifndef MPSK_RX_LOOPS_H
#define MPSK_RX_LOOPS_H

#include "agc/agc_core.h"
#include "viterbi/viterbi_core.h"
#include "carrier_nda/carrier_nda_core.h" /* carrier_nda_disc            */
#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include "ratesync/ratesync_core.h" /* ratesync_loop_t — the timing half */
#include "dp_tlm/dp_tlm_core.h"
#include <complex.h>
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  enum
  {
    MPSK_RX_PULSE_IANDD = RC_PULSE_IANDD, 
    MPSK_RX_PULSE_RRC   = RC_PULSE_RRC    
  };

/* Numerical guard on the symbol magnitude in the decision discriminator. */
#define MPSK_RX_EPS 1e-12

/* THE RECEIVER HAS EXACTLY ONE AGC, and it is the front-end cascade's
 * (RateConverter_enable_agc). One, not none and not one per detector: it
 * levels the SIGNAL PATH, and both loops run on that path, so it sits inside
 * BOTH of them. There is no SECOND AGC in front of either detector, because
 * each divides out its own contribution -- carrier_nda_disc() by its own
 * amplitude law |z|^M, the timing detector by its own slope. A detector that
 * divides out what it contributed does not need a loop upstream manufacturing
 * the condition it assumed, and one AGC per detector is how you end up with
 * level loops in series, correcting each other's excursions.
 *
 * The two detectors depend on the level differently, and only one of them
 * depends on it at all: the TED's slope is a construct-time constant for a
 * unit-amplitude stream, so a level error is a loop-gain error outright,
 * while the carrier discriminator is scale-invariant and sees only the AGC's
 * dynamics. Serving both is why the bandwidth is sized against both.
 *
 * `bn_agc_ratio` sets that one AGC's bandwidth as a fraction of the SLOWEST
 * loop it feeds -- the minimum of bn_carrier and bn_timing, not either one
 * alone, because the AGC is a shared element of both control loops.
 * MPSK_RX_AGC_BW_RATIO is the default. The RATIO, not the number,
 * is the part that is not negotiable: let an AGC approach the bandwidth of a
 * loop it feeds and it starts correcting the excursions that loop is itself
 * producing, and the two integrate against each other. The level is a slow
 * property of the channel, not a disturbance to reject at loop speed.
 *
 * It is a construction parameter rather than a constant because the right
 * separation depends on how fast the channel's LEVEL moves against how fast
 * its phase and timing do -- a property of the link, not of this code. It is
 * validated to (0, 1) at construction: at 1 the AGC is exactly as fast as the
 * loop it feeds and past that it is faster, so construction refuses rather
 * than warns. mpsk_rx_agc_bn() is the one place that computes it, and
 * test_agc_is_slower_than_both_loops pins it.
 *
 * The ratio was abandoned here once, on an argument neither half of which
 * survives. "Never converges over a realistic burst" is a cold-START
 * complaint about a loop that now starts at unity and walks -- a ramp the
 * loops downstream can follow, where a seeded STEP taken off a signal that
 * has not arrived is a shock they cannot (see rc_agc_tap() in
 * RateConverter_core.c for that measurement). And it sized a CONTINUOUS
 * receiver's loop against a burst budget; this object is continuous by
 * design, and a receiver built for bursts would take feed-forward estimates
 * rather than lean on loops at all. */
/* Default `bn_agc_ratio`: 20x slower than the slowest loop the AGC feeds.
 *
 * Not carrier_nda's 0.01, and the difference is measured rather than
 * stylistic. That AGC sat directly on a discriminator input and only ever had
 * to track drift; this one has to CORRECT a cold level, and at 0.01 it cannot
 * do so inside a realistic burst -- a 40 dB correction takes ~1/(4*bn_agc)
 * = 2500 symbols, so a 4000-symbol record still shows EVM tracking input
 * level. Measured across two decades of input level (EVM spread, complex /
 * real): 4.70 / 4.39 dB at 0.01, 0.17 / 0.06 dB at 0.05, 0.13 / 0.03 at 0.2.
 * 0.05 is the first value that is flat, and it is still 20x below the loops
 * it feeds, which is what the separation is actually for.
 *
 * Raising it further buys nothing measurable and spends stability margin. */
#define MPSK_RX_AGC_BW_RATIO 0.05

/* Power-detector EMA coefficient for that one AGC (agc_core's `alpha`): how
 * hard |y|^2 is smoothed before the dB loop filter sees it. Distinct from the
 * loop bandwidth above -- alpha sets what the AGC BELIEVES the level is,
 * bn_agc sets how fast it acts on that belief.
 *
 * This lived in carrier_nda as CARRIER_NDA_AGC_ALPHA until gh-657 retired
 * that object's arm AGC; both receivers were reaching across to a carrier
 * loop's private constant for a cascade AGC's detector, which outlived the
 * reason. The value is unchanged, so the receivers are bit-identical across
 * that move. */
#define MPSK_RX_AGC_ALPHA 0.01

  JM_FORCEINLINE double
  mpsk_rx_agc_bn (double bn_carrier, double bn_timing, double ratio)
  {
    double slowest = bn_carrier < bn_timing ? bn_carrier : bn_timing;
    return ratio * slowest;
  }

/* Default matched-filter bank arms — the timing resolution is 1/this of an
 * output period. What a composing C caller (the DSSS receivers) passes when it
 * has no reason to want anything else; the Python default matches. */
#define MPSK_RX_NUM_PHASES 1024u

/* Default terminal outputs per symbol — where an I&D matched filter reaches
 * the coherent bound (see mpsk_receiver_create's @p m_out for the
 * measurements). Same role as MPSK_RX_NUM_PHASES above: what a composing C
 * caller passes when it has no reason to want anything else, so the composed
 * receivers do not each carry their own copy of the number.
 * MUST match `default` on m_out in objects/mpsk_receiver{,_r}.toml, which is
 * what the Python binding uses. */
#define MPSK_RX_M_OUT_DEFAULT 8

/* ── Derived construction parameters (doppler#644, design/mpsk.md §8) ──────
 *
 * §8's principle is that **the caller states the link, not the loops**. Four
 * of the constructor's parameters are not design axes at all — they are a
 * constant, a false-alarm probability and two rates the object already knows —
 * so the receiver derives them and REPORTS them back, on the same argument as
 * `RateConverter.stages`: a caller who can read what was chosen can check it.
 *
 * **Zero means derive.** Each parameter keeps its place in the signature, so
 * a caller who wants to pin one still can; passing 0 — which every one of
 * these validators previously REJECTED, so no working call site can be
 * relying on it — asks the object for its own answer. That is what makes this
 * additive rather than a break.
 *
 * These live here, beside the loop bandwidths, because BOTH twins need them
 * and a rule that exists twice is a rule free to drift.
 */

JM_FORCEINLINE size_t
mpsk_rx_derive_m_out (double cap, int strict)
{
  double lim = strict ? nextafter (cap, 0.0) : cap;
  double h   = floor (lim / 2.0); /* m_out = 2*h */
  if (!(h >= 1.0))
    return 0u;
  return (h >= 4.0) ? 8u : (size_t)(2.0 * h);
}

#define MPSK_RX_ZETA_DEFAULT 0.70710678118654752

#define MPSK_RX_NUM_PHASES_DEFAULT 64u

#define MPSK_RX_AGC_RATIO_DEFAULT MPSK_RX_AGC_BW_RATIO

#define MPSK_RX_LOCK_THRESH_DEFAULT 0.4999

/* Carrier lock rule (see mpsk_rx_loops_init's lock_thresh doc). Declare fast,
 * drop reluctantly: 8 straight above-threshold symbols declare lock, and 32
 * straight below the 0.8x drop threshold withdraw it. The asymmetry reflects
 * what the indicator is FOR — a caller sizing a measurement window wants the
 * declaration promptly, and wants a momentary dip in a noisy statistic not to
 * retract it. Nothing inside the receiver reads it: it steers no loop and
 * gates no output, so a wrong reading costs a caller its window and costs the
 * demodulator nothing. */
#define MPSK_RX_LOCK_DOWN 0.8
#define MPSK_RX_LOCK_N_UP 8u
#define MPSK_RX_LOCK_N_DOWN 32u



  typedef struct
  {
    dp_tlm_t *ctx;         
    int32_t   id_lock;     
    int32_t   id_e;        
    int32_t   id_freq;     
    int32_t   id_nco;      
    int32_t   id_locked;   
    /* The recovered symbol itself, as two scalars. A telemetry record
       carries one `float`, so a complex value cannot be one probe; pairing
       them by the sample index the format already stamps is the only way to
       put the OUTPUT in a capture beside the loop state that produced it.
       Without these a filed capture shows every internal and not the thing
       they exist to produce -- no constellation, and no EVM or error rate
       recomputable from the evidence (doppler#846). */
    int32_t id_sym_i; 
    int32_t id_sym_q; 
  } mpsk_rx_tlm_t;

  typedef struct
  {
    ratesync_loop_t timing; 
    /* ── carrier loop ────────────────────────────────────────────────── */
    loop_filter_state_t car_lf;  
    double freq_ctrl;   
    double freq_scale;  
    double car_error;   
    double lock;        
    lockdet_state_t car_lock;   
    /* ── config (restored by the owner's create(), never packed) ─────── */
    int    m;          
    double sps;        
    double lo_sps;     
    size_t m_out;      
    double bn_carrier; 
    double bn_agc_ratio; 
    double zeta;       
    /* ── carrier lock indicator ──────────────────────────────────────── */
    size_t  sym_count; 
    int64_t lock_time; 
    /* ── demapper ────────────────────────────────────────────────────── */
    int           differential;  
    int           have_prev_idx; 
    unsigned      prev_idx;      
    float complex sym_rot;       
    mpsk_rx_tlm_t tlm; 
  } mpsk_rx_loops_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  void mpsk_rx_loops_init (mpsk_rx_loops_t *l, int m, double sps,
                           double lo_sps, size_t m_out, double bn_carrier,
                           double zeta, double bn_timing, double bn_agc_ratio,
                           int ted, double lock_thresh, int differential);

  JM_FORCEINLINE double
  mpsk_rx_updates_per_symbol (const mpsk_rx_loops_t *l)
  {
    (void)l;
    /* The discriminator reads the on-time strobe, which is one output per
       symbol, so the carrier loop updates once per symbol. Kept as a function
       rather than folded into the caller because it is the quantity
       mpsk_rx_config_carrier() sizes the loop filter against, and naming it
       is what makes that sizing legible. */
    return 1.0;
  }

  void mpsk_rx_config_carrier (mpsk_rx_loops_t *l);

  void mpsk_rx_loops_reset (mpsk_rx_loops_t *l);

  double mpsk_rx_freq_est (const mpsk_rx_loops_t *l);

  void mpsk_rx_set_freq_est (mpsk_rx_loops_t *l, double val);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  JM_FORCEINLINE JM_HOT void
  mpsk_rx_steer (mpsk_rx_loops_t *l, double pe)
  {
    l->car_error = pe;
    l->freq_ctrl = -loop_filter_step (&l->car_lf, pe) * l->freq_scale;
  }

  JM_FORCEINLINE JM_HOT void
  mpsk_rx_disc (mpsk_rx_loops_t *l, float complex z)
  {
    /* No AGC here, and none is wanted: carrier_nda_disc() normalises by its
       own |z|^M, so the discriminator is amplitude-blind on both outputs.
       The receiver's one AGC lives in the front-end cascade, where it serves
       the signal path rather than a detector. */
    double pe, lk;
    carrier_nda_disc (z, l->m, &pe, &lk);
    l->lock += CARRIER_NDA_LOCK_ALPHA * (lk - l->lock);
    /* First declaration dates `lock_time`, and only the first: a drop and
       re-acquire does not restamp it, because the question a caller is
       asking is "how long did this receiver take to lock", not "when did it
       last hold". Reset clears it back to -1. */
    if (lockdet_step (&l->car_lock, l->lock) && l->lock_time < 0)
      l->lock_time = (int64_t)l->sym_count;
    mpsk_rx_steer (l, pe);
  }


  JM_FORCEINLINE JM_HOT int
  mpsk_rx_take_output (mpsk_rx_loops_t *l, float complex y, float complex *sym,
                       int ted)
  {
    float complex on;
    if (!ratesync_loop_take_output (&l->timing, y, &on, ted))
      return 0;

    /* The carrier discriminator sees the ON-TIME STROBE and nothing else.
       The OTHER terminal outputs sit between symbols, where the matched
       filter is averaging two different ones, so they are not constellation
       points at all; their M-th power is ISI, not carrier phase. Feeding them
       in costs nothing visible at QPSK (the loop averages through it) and is
       fatal at 8PSK, whose decision margin is +-pi/8: measured 0.85 symbol
       error rate, i.e. chance, against 0 on the strobe alone. */

    /* The discriminator reads the ON-TIME STROBE: the cleanest input there
       is, and the only node already matched to the signal. It acts on every
       strobe from the first, locked or not.

       It depends on symbol timing, and that costs nothing where it would
       matter most. A carrier with its modulation off is SAMPLING-PHASE
       INVARIANT -- every sample is the same constellation point -- so the
       M-th-power discriminator does not care which phase the timing loop
       nominated, which is why NDA is the implicit answer when there is no
       data to be aided by (docs/design/mpsk.md §3.3).

       An earlier revision gated the steer and the AGC seed on the timing
       loop's lock detector, on the grounds that a pre-lock strobe
       is an arbitrary phase of the pulse and its M-th power is nothing in
       particular. The reasoning is sound and the transient is real (measured
       QPSK, sps = 8, Es/N0 20 dB, 5 seeds: the timing loop declares at symbol
       185 on average, range 132-265, and across that window the carrier lock
       statistic swings between -0.947 and +0.888 where a settled lock reads
       +0.906 — a statistic ranging over nearly its whole span, sign included,
       is proof the input is not a constellation). But the gate was
       measured and it does not buy what it was supposed to: across a 24-cell
       sweep it changed exactly one cell, and the reason it looked helpful was
       that it made carrier acquisition easier to MEASURE, not easier to
       achieve — with the steer frozen until timing declares, the carrier's
       transient starts from a known instant.

       Gating the steer on the timing loop hid that behind a coupling the
       caller could neither see nor override, for one cell of a 24-cell
       sweep. */
    mpsk_rx_disc (l, on);

    /* The NDA loop's stable points are the 0-grid (z^m = +1), but the QPSK
       constellation sits on the pi/4-offset grid, so a raw strobe would land
       every symbol on a decision boundary. Rotating the SYMBOL rather than the
       matched filter's input is both correct and cheaper: the matched filter's
       taps are real, so a rotation commutes through it, and this way it costs
       one multiply per symbol instead of one per input sample — while the
       discriminator above still sees the unrotated stream it locks. */
    float complex y_rot = on * l->sym_rot;

    l->sym_count++;
    *sym = y_rot;
    return 1;
  }

  JM_FORCEINLINE JM_HOT int
  mpsk_rx_fold (mpsk_rx_loops_t *l, const float complex *ys, size_t n,
                float complex *y_out, int ted)
  {
    int emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= mpsk_rx_take_output (l, ys[oi], y_out, ted);
    return emitted;
  }

  int mpsk_rx_symbol_to_bits (mpsk_rx_loops_t *l, float complex y,
                              uint8_t *bits);

  /* ------------------------------------------------------------------
   * Telemetry
   * ------------------------------------------------------------------ */

  void mpsk_rx_tlm_flush (const mpsk_rx_loops_t *l, float complex y);

  int mpsk_rx_set_telemetry (mpsk_rx_loops_t *l, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim);

/* ── Serializable state — the loops alone (nested by every owner) ──────────
 * Envelope, the carrier/demapper running scalars, then the timing
 * loop's and the carrier loop filter's self-validating sub-blobs. The arm AGC
 * is running state too. Config is restored by the owner's create(). */
#define MPSK_RX_LOOPS_STATE_MAGIC DP_FOURCC ('M', 'R', 'X', 'L')
/* v3: `agc_seeded` became a COUNTER (the AGC seed averages
 * MPSK_RX_AGC_SEED_SAMPS samples instead of taking one), so the flags word
 * carries it in bits 8..15 where v2 held a single "seeded" bit. The blob's
 * SIZE is unchanged — the counter reuses spare bits of an existing u64 — so
 * only the version distinguishes them, and it must, since a v2 blob's bit 2
 * read as a count would resume a seeded receiver mid-seeding and corrupt its
 * gain. Rejected rather than reinterpreted, per the envelope rule.
 * v4: the seed's running mean (`agc_seed_pwr`) is packed. v3 accumulated it
 * in `car_agc.p_avg`, which is the DETECTOR's average of OUTPUT power — so
 * the loop filter was handed an error equal to the whole seeded gain and
 * integrated it (measured: 13.4 dB of further excursion on a 4x-hot input).
 * The mean now has its own field, p_avg is seeded to the reference, and the
 * blob grows by one double.
 * v5: the carrier AGC is GONE -- carrier_nda_disc() normalises by its own
 * |z|^M, so the receiver has exactly one AGC and it lives in the front-end
 * cascade. The blob loses that AGC's sub-blob and both seed scalars.
 * v6: the Costas ARM is gone (gh-768) -- there is no free-running boxcar to
 * pack, so the blob loses that child entirely. It also gains `lock_time`, the
 * symbol at which carrier lock was first declared, which is running state and
 * so has to survive a hand-off. Two shape changes at once, one version.
 * v7: the acquisition/tracking handover is gone (doppler#877) -- the blob
 * loses that lock detector's `cnt`/`locked` pair and the `tracking` bit of
 * the flags word, so `have_prev_idx` moves down to bit 0. The surviving
 * `car_lock` detector is an indicator, not a discriminator switch. */
#define MPSK_RX_LOOPS_STATE_VERSION 7u

  size_t mpsk_rx_loops_state_bytes (const mpsk_rx_loops_t *l);
  void mpsk_rx_loops_get_state (const mpsk_rx_loops_t *l, void *blob);
  int mpsk_rx_loops_set_state (mpsk_rx_loops_t *l, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RX_LOOPS_H */
```


