/**
 * @file RateConverter_core.h
 * @brief Optimal-speed rate conversion cascade.
 *
 * Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase
 * Resampler stages for a given output/input sample rate ratio at creation
 * time.  All sub-stage C objects are owned by the state struct.
 *
 * Stage selection (D = 1/rate):
 *
 *   rate >= 1.0 or D < 2           `[Resampler(rate)]`
 *   D ~= 2^1                        `[HalfbandDecimator]`
 *   D ~= 2^2                        `[HalfbandDecimator, HalfbandDecimator]`
 *   D ~= 2^n, n>=3, D<=CIC_R_MAX    `[CIC(D)]`
 *   D ~= 2^n, D > CIC_R_MAX         `[CIC(CIC_R_MAX), Resampler(R/D)]`
 *   D >= 8, non-power-of-2          `[CIC(R*), Resampler correction]`
 *                                    R* = nearest power-of-2 to D, capped
 *   otherwise (2 <= D < 8, non-int) `[Resampler(rate)]`
 *
 * A single CIC stage is capped at `CIC_R_MAX` (2048) — see cic_core.h for the
 * accumulator budget that sets it.  **The cap costs no rate**: whatever the
 * capped CIC leaves is handed to a Resampler stage, so the cascade still
 * delivers the D it was asked for.  That was not always true — gating the
 * residual on the matched-terminal flag alone meant a capped plan silently
 * decimated by R and claimed D (see the CHANGELOG for the measurement).
 *
 * **INPUT AMPLITUDE IS BOUNDED whenever the plan contains a CIC stage** —
 * that is, any decimation by 8 or more: |Re| and |Im| <= 2.0, clipped beyond
 * that, before any filtering.  The bound is `CIC_PAPR_HEADROOM` (6 dB above
 * unity), which is there so a pulse-shaped waveform's PEAKS have somewhere to
 * sit above its unit average; see cic_core.h.  `stages` is how you tell: a
 * plan naming `CIC(...)` is not scale-free, every other plan is.  This is the
 * one property of this object a caller cannot infer from an output that is
 * finite and looks plausible — an overdriven RRC-BPSK waveform matched-filters
 * to -25 dB EVM where the same waveform well inside the bound reaches -50 dB.
 *
 * Lifecycle:
 * @code
 *   RateConverter_state_t *rc = RateConverter_create(0.1, 0);
 *   // rc->n_stages == 2: CIC(8) then Resampler(0.8)
 *   float _Complex out[512];
 *   size_t n = RateConverter_execute(rc, in, 4096, out, 512);
 *   RateConverter_destroy(rc);
 * @endcode
 */
#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "clib_common.h"
#include "dp_state.h"

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "resamp/resamp_core.h"
#include "fir/fir_core.h"
#include "agc/agc_core.h"
#include "dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Maximum number of visible cascade stages. */
#define RC_MAX_STAGES 3

/** Stage type tags. */
typedef enum
{
  RC_STAGE_HB     = 0, /**< HalfbandDecimator (2:1, CF32) */
  RC_STAGE_CIC    = 1, /**< CIC decimator (optionally with comp FIR) */
  RC_STAGE_RESAMP = 2, /**< Polyphase Resampler (any positive rate) */
} rc_stage_t;

/**
 * @brief Matched-filter pulse selection for the terminal stage.
 *
 * A cascade whose terminal stage carries a *pulse-shaped* bank instead of the
 * default Kaiser anti-alias bank IS the matched filter — the same dot product
 * does the rate conversion and the matched filtering, and its arm is the
 * fractional timing delay a downstream loop steers. Values 0/1 match
 * `MPSK_RX_PULSE_IANDD`/`_RRC` so one vocabulary covers the family.
 */
typedef enum
{
  RC_PULSE_IANDD = 0, /**< rectangular: integrate-and-dump boxcar.        */
  RC_PULSE_RRC   = 1, /**< root-raised cosine, roll-off `beta`.           */
  RC_PULSE_NONE  = 2, /**< default: Kaiser anti-alias (plain conversion). */
} rc_pulse_t;

/**
 * @brief Cascade state -- owns all sub-stage C objects.
 *
 * Do not initialise directly; use RateConverter_create().
 */
typedef struct
{
  double         rate;                        /**< current rate ratio        */
  int            compensate;                  /**< CIC droop-comp flag       */
  int            n_stages;                    /**< active stage count        */
  rc_stage_t     stage_types[RC_MAX_STAGES];  /**< stage type per slot       */
  void          *stage_ptrs[RC_MAX_STAGES];   /**< sub-object per slot       */
  /** Ping-pong intermediate buffers, grown lazily on first execute. */
  float _Complex *bufs[2];
  size_t          buf_cap;
  /* Matched-filter configuration (RC_PULSE_NONE = plain Kaiser terminal
     bank, i.e. everything RateConverter_create() builds).  Kept so
     RateConverter_set_rate() can re-plan without losing the pulse. */
  int    pulse;      /**< rc_pulse_t; RC_PULSE_NONE when not matched  */
  double beta;       /**< RRC roll-off                                */
  size_t span;       /**< one-sided RRC span, symbols                 */
  double pulse_sps;  /**< symbol period in OUTPUT samples             */
  size_t num_phases; /**< terminal-stage arms (power of two)          */
  /** Set when a rectangular pulse was selected with fewer than four output
      samples per symbol, where its matched filter degenerates to a 2-3 tap
      sum.  Read by the binding, which turns it into a UserWarning. */
  bool narrow_pulse;
  /* ── Pre-terminal AGC (NULL = off, which is the default and what every
     constructor builds).  See RateConverter_enable_agc(). ─────────────── */
  agc_state_t *agc;          /**< NULL when off — one branch per sample  */
  double       bank_sps;     /**< symbol period on the terminal's grid   */
  double       bank_e0;      /**< sum h(t)^2 on that grid; the bank's
                                  own normaliser, and the AGC's reference */
  double       agc_ref_db;   /**< derived: 10*log10(bank_e0 / bank_sps)  */
  double       agc_bn_sym;   /**< requested bandwidth, cycles/SYMBOL     */
  double       agc_alpha;    /**< detector EMA coefficient               */
  /** The telemetry attachment as REQUESTED, not as currently applied.
      Held here rather than only on the AGC because the AGC is destroyed and
      rebuilt whenever the plan changes (rc_agc_build), and may not exist yet
      when the attach arrives; keeping the request lets every rebuild re-apply
      it. Never packed into a state blob — telemetry is observation.
      See RateConverter_set_telemetry(). */
  struct
  {
    dp_tlm_t *ctx;                   /**< NULL = detached              */
    char      prefix[DP_TLM_NAME_MAX]; /**< as passed by the caller    */
    uint32_t  decim;                 /**< as passed by the caller      */
  } agc_tlm_req;
} RateConverter_state_t;

/**
 * @brief Create a rate converter for the given output/input rate ratio.
 * Selects the cheapest cascade of CIC, HalfbandDecimator, and/or
 * polyphase Resampler stages at construction time (see file header for
 * the selection table). Setting compensate=1 appends a closed-form
 * Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which
 * improves passband flatness at the cost of one extra FIR stage.
 *
 * @param rate       Output-to-input sample rate ratio. Any positive float.
 * @param compensate Non-zero to append a CIC passband-droop compensating
 *                   FIR after any CIC stage.
 * @return Non-NULL on success; NULL if rate <= 0 or OOM.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.rate
 * 0.5
 * @endcode
 */
RateConverter_state_t *RateConverter_create (double rate, int compensate);

/**
 * @brief Create a rate converter whose terminal stage IS a matched filter.
 *
 * Plans the same cheap cascade as RateConverter_create(), then puts a
 * pulse-shaped polyphase bank on the **terminal** stage instead of the default
 * Kaiser one. The cascade therefore does rate conversion and matched filtering
 * in a single dot product, and that stage's polyphase arm is the fractional
 * timing delay — which is what makes RateConverter_execute_ctrl() a timing
 * control port rather than just a Doppler knob.
 *
 * Three things this does that plain create() cannot:
 *
 * - **The terminal fractional stage always exists.** The ordinary planner drops
 *   it for an exact power-of-two decimation, and again when the correction
 *   lands within 1e-6 of 1.0 — so `rate = 2/64` plans a bare `CIC(32)` with
 *   nothing steerable at the end. Here the terminal stage is simultaneously the
 *   matched filter and the timing element, so it is appended (at rate 1.0 if
 *   there is no rate left to correct).
 * - **The bank is sized by the POST-decimation rate.** Matched-filtering at the
 *   input rate costs taps proportional to the input samples per symbol (4225
 *   taps/arm at 256 samples/symbol — 17 MB of bank); after the integer stages
 *   have done the bulk decimation it is ~`2*span*pulse_sps` taps, constant in
 *   the input rate.
 * - **CIC droop folds into the bank**, exactly rather than approximately: the
 *   Molnar-Vucic compensator (ciccompmf) runs at the decimated rate, which IS
 *   the terminal stage's tap grid, so the fold is a per-arm convolution and
 *   costs no extra stage. `compensate` therefore adds no FIR on this path.
 *
 * Measured on RRC-BPSK (beta 0.35, span 8, two outputs per symbol), best-case
 * timing phase, noiseless: a halfband cascade reaches -60 dB EVM; a CIC
 * cascade reaches **-50 dB with `compensate = 1` and only -22 dB without**, so
 * on this path compensation is not a refinement — it is 28 dB, for six extra
 * taps per arm and no extra pass over the data. Folded or appended agree to
 * within 0.6 dB, i.e. the fold gives up nothing to a separate comp FIR.
 *
 * @note Keep the INPUT inside +-1.0 whenever the plan contains a CIC stage —
 * see the file header.  The clip is silent, and it costs 25 dB of the EVM
 * quoted above for reasons that have nothing to do with the matched filter.
 *
 * @param rate       Output-to-input sample rate ratio (any positive float).
 *                   Rate-agnostic: this object never learns about symbols —
 *                   a caller wanting `m` samples per symbol asks for
 *                   `rate = m/sps`.
 * @param compensate Non-zero to correct CIC passband droop (folded into the
 *                   bank here, not appended as a stage).
 * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD. RC_PULSE_NONE is invalid
 *                   here — use RateConverter_create() for a plain conversion.
 * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle).
 * @param span       One-sided RRC span in symbols (ignored for the rectangle,
 *                   whose support is always exactly one symbol).
 * @param pulse_sps  The pulse's period measured in **output** samples (2 =
 *                   two samples per symbol out). This is a shape parameter,
 *                   not a rate-planning one: a matched filter has a symbol
 *                   duration, and the planner still knows nothing of symbols.
 * @param num_phases Terminal-stage arms; power of two. Sets the fractional
 *                   timing resolution to `1/num_phases` of an output period.
 * @return Non-NULL on success; NULL on a bad parameter or OOM.
 */
RateConverter_state_t *
RateConverter_create_matched (double rate, int compensate, int pulse,
                              double beta, size_t span, double pulse_sps,
                              size_t num_phases);

/**
 * @brief Has any planned CIC stage clipped its input since the last reset?
 *
 * The cascade inherits cic_core's input bound (|Re|, |Im| <= 1.0) whenever the
 * plan contains a CIC — any decimation by 8 or more — and the clip does not
 * show up in the samples: the output stays finite and plausible, merely
 * distorted.  This is the only reliable way to find out, and it is free (the
 * boundary comparisons run on every sample regardless).
 *
 * Sticky, cleared by RateConverter_reset().  Always 0 for a cascade with no
 * CIC stage, which is the honest answer: those plans are scale-free.
 *
 * @param s  Pointer to a valid RateConverter_state_t.
 * @return 1 if any CIC stage has clipped, else 0.
 */
bool RateConverter_get_clipped (const RateConverter_state_t *s);

/**
 * @brief Is this converter's rectangular matched filter degenerately narrow?
 *
 * True only for a matched cascade built with RC_PULSE_IANDD and
 * `pulse_sps < 4`: the rectangle is exactly one symbol wide, so its matched
 * filter is a 2-3 tap sum there.  It works — it just barely opens the eye
 * (measured on the timing loop this feeds, a lock statistic of -0.34 at two
 * samples per symbol against +0.95 at four).  The RRC spans many symbols and
 * is never affected.  Construction also raises a UserWarning.
 */
bool RateConverter_get_narrow_pulse (const RateConverter_state_t *s);

/** @brief Number of planned cascade stages (backs the `stages` property). */
size_t RateConverter_num_stages (const RateConverter_state_t *s);

/**
 * @brief The cascade's response to a constant input, from its stages' own
 *        coefficients — computed, never measured.
 *
 * Each stage answers for itself (hbdecim_dc_gain(), cic_dc_gain() times
 * fir_dc_gain() for a compensated CIC, resamp_dc_gain()) and this is their
 * product. So the number tracks whatever the stages actually hold: if a
 * filter's normalisation drifts, this moves with it, and a gate comparing it
 * against a measured DC probe catches the drift from either side.
 *
 * **A plain cascade is unity** — a rate conversion that adds gain of its own
 * is a defect, and `RateConverter_create()` returns 1.0 here at every rate.
 *
 * **A matched cascade is not, and should not be.** Its terminal stage is a
 * matched filter, which is deliberately not flat; the invariant that holds
 * there is at the SYMBOL level (a symbol of amplitude A in, amplitude A out),
 * not at DC. This function still reports that cascade's true DC gain, which
 * is the pulse's `sum(h)/sum(h^2)`.
 *
 * @param s State. Must be non-NULL.
 * @return The DC gain of the whole cascade.
 *
 * @code
 * RateConverter_state_t *rc = RateConverter_create (1.0 / 12.0, 1);
 * printf ("%.4f\n", RateConverter_gain (rc));   // 1.0000
 * RateConverter_destroy (rc);
 * @endcode
 */
double RateConverter_gain (const RateConverter_state_t *s);
/**
 * @brief Label of stage @p i, e.g. "CIC(8)+FIR" or "Resampler(0.923,rrc)".
 *
 * Points at a per-thread scratch buffer valid until this thread's next call —
 * the binding converts it to a Python string immediately.  NULL if @p i is
 * out of range.
 */
const char *RateConverter_stages_value (const RateConverter_state_t *s,
                                        size_t i);

/**
 * @brief Terminal polyphase bank shape (backs the `bank_shape` property).
 * @return 2 when the cascade ends in a Resampler stage, else 0.
 */
size_t RateConverter_num_bank_shape (const RateConverter_state_t *s);
/** @brief Element @p i of the bank shape: 0 -> num_phases, 1 -> num_taps. */
size_t RateConverter_bank_shape_value (const RateConverter_state_t *s,
                                       size_t i);

/**
 * @brief Level the stream feeding the terminal (matched) stage.
 *
 * Wedges an AGC into the cascade immediately BEFORE the terminal polyphase
 * stage — after every integer decimation, ahead of the matched filter and the
 * timing element. Off until this is called, and off is what both constructors
 * build, so a plain cascade is untouched and RateConverter_gain() still reads
 * exactly 1.0.
 *
 * @par Why here and not somewhere else
 * The consumer is a timing-error detector. A TED's raw output is the timing
 * error multiplied by three things it did not choose — the signal amplitude,
 * the transition density, and the detector's own slope — and only the last is
 * the detector's to divide out (symsync_ted_slope(), which is computed at
 * construct FOR A UNIT-AMPLITUDE SYMBOL STREAM). Amplitude enters as `A^2`
 * for Gardner and `A^1` for DTTL, so a 4x level error is a 16x loop-gain
 * error. Levelling it is this object's job because this object owns the bank
 * that sets what "unit amplitude" means.
 *
 * The tap is pre-terminal rather than post because the terminal stage's
 * OUTPUT rate is the one a timing loop is actively steering, and an AGC whose
 * bandwidth is quoted in cycles per sample of a stream another loop is
 * stretching is coupled to that loop. The pre-terminal rate is fixed.
 *
 * @par The reference level is derived, not chosen
 * The AGC sets average POWER; the TED wants unit symbol AMPLITUDE. The bridge
 * is the pulse's own energy on its own tap grid — `bank_e0 = sum h(t)^2`, the
 * quantity the bank is already normalised by — so for i.i.d. unit-power
 * symbols at `bank_sps` samples per symbol the pre-terminal average power is
 * `bank_e0 / bank_sps` and that is the reference. No caller supplies a level;
 * read it back with RateConverter_agc_ref_db().
 *
 * @note This levels signal PLUS noise, so at finite Es/N0 the symbols land
 * slightly low — about 0.95x amplitude at 10 dB Es/N0, i.e. 0.91x Gardner
 * loop gain. That is a fact of the measurement, not an error to estimate
 * away: an AGC that tried to exclude noise would be estimating the very
 * quantity the receiver is trying to measure.
 *
 * @par Bandwidth
 * @p bn_sym is in cycles per SYMBOL, matching every other loop bandwidth in
 * this family, and is converted to the AGC's own per-sample units with the
 * one number that describes its position (`bn_sym / bank_sps`). It must stay
 * well below the bandwidth of every loop downstream — an AGC divides out the
 * amplitude those loops' discriminators are built around, so one running near
 * a loop's bandwidth corrects the excursions that loop is itself producing.
 * See mpsk_rx_agc_bn() for the ratio a composing receiver uses.
 *
 * The loop starts at unity gain and walks to the level; there is no seed and
 * no sample is treated specially at the start. A seed is a STEP in gain, and
 * one taken off a signal that has not arrived is a shock the loops downstream
 * cannot absorb -- see rc_agc_tap() for the measurement that settled this. So
 * @p bn_sym also sets how fast a level error is corrected, and a very slow
 * AGC leaves the early symbols under- or over-driven for a loop time
 * constant.
 *
 * @param s        Must be non-NULL, and must be a MATCHED cascade
 *                 (RateConverter_create_matched()) — a plain one has no pulse
 *                 and therefore no reference to derive.
 * @param bn_sym   AGC loop noise bandwidth in cycles/symbol; > 0.
 * @param alpha    Power-detector EMA coefficient, in (0, 1].
 * @return DP_OK, or DP_ERR_INVALID for a plain cascade or a bad parameter
 *         (the converter is left exactly as it was, AGC still off).
 *
 * @code
 * RateConverter_state_t *rc =
 *     RateConverter_create_matched (2.0 / 8.0, 1, RC_PULSE_RRC, 0.35, 8,
 *                                   2.0, 1024);
 * RateConverter_enable_agc (rc, 1e-4, 0.01);
 * printf ("%.2f dB\n", RateConverter_agc_ref_db (rc));
 * RateConverter_destroy (rc);
 * @endcode
 */
int RateConverter_enable_agc (RateConverter_state_t *s, double bn_sym,
                              double alpha);

/**
 * @brief The pre-terminal AGC's reference level, in dB.
 *
 * `10*log10(bank_e0 / bank_sps)` — the average power a unit-amplitude symbol
 * stream has where the AGC sits, derived from the terminal bank's own pulse
 * energy. Defined for any MATCHED cascade whether or not the AGC is enabled,
 * because it describes the bank rather than the loop; 0.0 for a plain one.
 */
double RateConverter_agc_ref_db (const RateConverter_state_t *s);

/**
 * @brief Gain the pre-terminal AGC last applied, in dB; 0.0 when off.
 *
 * The cascade's time-varying gain, kept deliberately separate from
 * RateConverter_gain(): that function reports the response computed from the
 * stages' own COEFFICIENTS, and an AGC has none. A caller asking "what did
 * this cascade do to my amplitude" with the AGC on wants both, and they
 * multiply.
 */
double RateConverter_agc_gain_db (const RateConverter_state_t *s);

/**
 * @brief Attach (or detach) a telemetry context on the pre-terminal AGC.
 *
 * The cascade has no loop of its own to report — the stages are fixed
 * filters — so this forwards to the one child that does: the pre-terminal
 * AGC, under @p prefix verbatim. It registers that child's probes
 * ("<prefix>.gain_db" and "<prefix>.level_db"; see agc_set_telemetry()) and
 * nothing else, which is why the prefix is not extended with a component
 * name — there is no second thing here to disambiguate it from.
 *
 * A composing object forwards its own prefix down: an `mpsk_receiver`
 * attached as "rx" passes "rx.agc", and the receiver's gain trajectory joins
 * its carrier and timing probes on one context.
 *
 * With the AGC off (a plain cascade, or a matched one where
 * RateConverter_enable_agc() was never called) there is nothing to instrument
 * and this is a successful no-op — DP_OK with no probes registered. That is
 * deliberate: whether the AGC exists is the composing receiver's
 * construction-time choice (`agc = 0`), and a caller attaching telemetry
 * should not have to know which way that went to avoid an error.
 *
 * The attachment is remembered as a REQUEST, so it survives the AGC being
 * rebuilt by a rate change and is applied to an AGC enabled after the fact.
 * One consequence of that: an attach made before the AGC exists reports DP_OK
 * here, and if the probe table has filled by the time the AGC is built the
 * probes are dropped without failing the build — signal processing does not
 * fail because observation could not be set up. Attach after construction
 * (which is what every composing object here does) and the return value
 * covers it; otherwise check the context's probe names.
 *
 * Setup path, never hot: call before the producer thread starts; the context
 * is borrowed and must outlive the attachment (SPSC rules in
 * dp_tlm/dp_tlm_core.h). Passing NULL detaches.
 *
 * @param s      Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "agc" or "rx.agc".
 * @param decim  Emit every decim-th gain update; >= 1.
 * @return DP_OK — including when no AGC is enabled — or DP_ERR_INVALID when
 *         the probe table cannot take the AGC's probes (the attach fails
 *         whole; the AGC stays detached).
 *
 * @code
 * RateConverter_state_t *rc =
 *     RateConverter_create_matched (2.0 / 8.0, 1, RC_PULSE_RRC, 0.35, 8,
 *                                   2.0, 1024);
 * RateConverter_enable_agc (rc, 1e-4, 0.01);
 * dp_tlm_t *tlm = dp_tlm_create (1 << 12);
 * RateConverter_set_telemetry (rc, tlm, "agc", 1);
 * RateConverter_destroy (rc);
 * dp_tlm_destroy (tlm);
 * @endcode
 */
int RateConverter_set_telemetry (RateConverter_state_t *s, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

/** @brief Free all resources.  NULL is a no-op. */
void RateConverter_destroy (RateConverter_state_t *s);

/**
 * @brief Zero all sub-stage filter memories.
 * Rate, stage count, and stage types are preserved. Processing from a
 * reset state produces the same output as a freshly created converter
 * fed the same input. Use between signal bursts to suppress transient
 * artefacts from prior filter memory.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.reset()
 * >>> rc.rate
 * 0.5
 * @endcode
 */
void RateConverter_reset (RateConverter_state_t *s);

/* Serializable state (standard bytes interface; see dp_state.h): the standard
 * envelope followed by the concatenated mutable state of the active cascade
 * stages (HB / CIC[+comp FIR] / Resampler), in cascade order — each a
 * self-contained sub-blob with its own leaf envelope.  The stage plan is config
 * (rebuilt from rate), so a same-rate RateConverter round-trips exactly.
 * v2: an enabled pre-terminal AGC appends its seed scalars and its own
 * sub-blob after the stages. A converter with the AGC off writes exactly the
 * bytes v1 did — but the version still moves, because nothing in the blob
 * distinguishes an AGC-off v2 from a v1, and the size check alone cannot. */
#define RC_STATE_MAGIC DP_FOURCC ('R', 'C', 'V', 'T')
#define RC_STATE_VERSION 2u

/** @brief Bytes RateConverter_get_state() writes for @p s (envelope + stages). */
size_t RateConverter_state_bytes (const RateConverter_state_t *s);
/** @brief Serialize @p s's active-stage state into @p blob. */
void RateConverter_get_state (const RateConverter_state_t *s, void *blob);
/** @brief Restore active-stage state from @p blob (same rate).
 *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
int RateConverter_set_state (RateConverter_state_t *s, const void *blob);

/**
 * @brief Convert a block of CF32 samples through the cascade.
 * Passes input through each stage in order, ping-ponging between two
 * intermediate buffers. State persists between calls, so contiguous
 * calls on sequential blocks give the same result as one large call.
 * Output length is approximately n_in * rate.
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param in       CF32 input block.
 * @param n_in     Number of input samples.
 * @param out      Output buffer; must hold at least max_out samples.
 * @param max_out  Capacity of out in samples.
 * @return         CF32 output array; length is approximately n_in * rate.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> import numpy as np
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
 * >>> y.shape, y.dtype
 * ((512,), dtype('complex64'))
 * @endcode
 */
size_t RateConverter_execute (RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

/**
 * @brief Upper bound on execute output for a standard 65536-sample block.
 *
 * Returns (size_t)(65536 * max(rate, 1.0)) + 2.  The Python extension uses
 * this to pre-allocate the output buffer on the first execute call.
 */
size_t RateConverter_execute_max_out (RateConverter_state_t *s);

/** @brief As RateConverter_execute_max_out(), for the block control form. */
size_t RateConverter_execute_ctrl_max_out (RateConverter_state_t *s);
/** @brief Bound for ONE pushed input: `ceil(rate) + 1` output periods.
 *  Non-zero because the push form has no input block to size from. */
size_t RateConverter_execute_ctrl_push_max_out (RateConverter_state_t *s);

/**
 * @brief Convert a block, steering the cascade's fractional stage by @p ctrl.
 *
 * The control-port form of RateConverter_execute(): the fixed integer stages
 * (HalfbandDecimator / CIC) run unchanged, and the scalar rate deviation
 * @p ctrl is forwarded to the **terminal polyphase Resampler stage's**
 * accumulator (via resamp_execute_ctrl_push) — so its effective rate becomes
 * `stage_rate + ctrl` for this call. This exposes the fractional tail's control
 * port that RateConverter_execute() hides: a timing/rate-tracking loop can
 * decimate a high input rate cheaply through the HB/CIC stages and then
 * arbitrary-rate + strobe-align in the last stage, updating @p ctrl per block.
 *
 * `ctrl` is referenced to the terminal stage's (post-decimation) rate, not the
 * overall rate. It is meaningful only when the cascade actually ends in a
 * Resampler stage; a pure integer HB/CIC cascade has no fractional stage to
 * steer, so this **falls through to RateConverter_execute()** (ctrl ignored).
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param x        CF32 input block.
 * @param n_in     Number of input samples.
 * @param ctrl     Rate deviation added to the terminal Resampler stage's rate.
 * @param out      Output buffer; must hold at least max_out samples.
 * @param max_out  Capacity of out in samples.
 * @return CF32 output array; length tracks the accumulated effective rate.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> import numpy as np
 * >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
 * >>> x = np.ones(1000, dtype=np.complex64)
 * >>> rc.execute_ctrl(x, 0.0).shape[0]    # base rate: 1000 -> 800
 * 800
 * >>> rc2 = RateConverter(rate=0.8, compensate=0)
 * >>> rc2.execute_ctrl(x, 0.05).shape[0]  # +ctrl speeds the tail up
 * 851
 *
 * @endcode
 */
size_t RateConverter_execute_ctrl (RateConverter_state_t *s,
                                   const float _Complex *x, size_t n_in,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

/**
 * @brief Push ONE input sample; emit whatever outputs it completes.
 *
 * The per-input streaming form of RateConverter_execute_ctrl(), and the only
 * form a closed loop can use: a block call must know its whole `ctrl` history
 * up front, whereas a timing loop computes each correction *from* the outputs
 * already emitted. Feeding a stream one sample at a time through this
 * reproduces RateConverter_execute_ctrl() on the same block bit-for-bit when
 * @p ctrl is held constant (the cascade is block-boundary invariant), so the
 * cheap block form stays correct for open-loop use.
 *
 * The integer HB/CIC stages consume the sample and emit at most one
 * intermediate sample each; the terminal Resampler stage then emits 0 outputs
 * (a decimator between strobes — the common case), 1, or several (an
 * interpolator). A cascade with no terminal Resampler ignores @p ctrl.
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param x        One CF32 input sample.
 * @param ctrl     Rate deviation added to the terminal stage's rate for this
 *                 input (referenced to the terminal, post-decimation rate).
 * @param out      Output buffer for any emitted samples.
 * @param max_out  Capacity of @p out (emission stops at this bound).
 * @return CF32 array of the outputs completed by this input (0, 1, or more).
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> import numpy as np
 * >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
 * >>> x = (np.arange(10, dtype=np.float32) + 1).astype(np.complex64)
 * >>> # a decimator emits 0 between strobes, 1 on a strobe:
 * >>> [rc.execute_ctrl_push(complex(v), 0.0).shape[0] for v in x]
 * [1, 1, 1, 1, 0, 1, 1, 1, 1, 0]
 *
 * @endcode
 */
size_t RateConverter_execute_ctrl_push (RateConverter_state_t *s,
                                        float _Complex x, double ctrl,
                                        float _Complex *out, size_t max_out);

/**
 * @brief RateConverter_execute_ctrl_push(), also emitting the PRE-TERMINAL
 *        sample — the cascade's output after every integer stage and after
 *        the AGC, but before the terminal matched filter.
 *
 * This is the tap a non-data-aided carrier discriminator wants, and it is the
 * reason this variant exists (see docs/design/mpsk.md §3.3). It is already
 * band-limited by the cascade's own decimation filters and already levelled
 * by the AGC that sits on this exact node, yet it is ahead of the matched
 * filter — so reading it needs no symbol timing, and it carries none of the
 * matched filter's group delay or its between-symbol ISI.
 *
 * The rate is `bank_sps` samples per symbol, a planner outcome: read it with
 * RateConverter_get_bank_sps() rather than assuming it. A consumer wanting a
 * fixed clock decimates this stream itself.
 *
 * A non-terminal stage swallows inputs between its decimation strobes, so
 * @p n_pre is 0 on those calls — exactly as the return value is.
 *
 * @param s        Must be non-NULL.
 * @param x        One input sample.
 * @param ctrl     Fractional-rate control for the terminal stage.
 * @param out      Terminal outputs.
 * @param max_out  Capacity of @p out.
 * @param pre_out  Receives the pre-terminal sample; may be NULL.
 * @param n_pre    Receives 1 if @p pre_out was written, else 0; may be NULL.
 * @return Number of terminal outputs written, as the non-tap form.
 */
size_t RateConverter_execute_ctrl_push_tap (RateConverter_state_t *s,
                                            float _Complex x, double ctrl,
                                            float _Complex *out,
                                            size_t max_out,
                                            float _Complex *pre_out,
                                            int *n_pre);

/**
 * @brief Samples per symbol on the terminal stage's grid — the rate the
 *        pre-terminal tap runs at.
 *
 * A planner outcome, not a constant: `bank_sps = pulse_sps / resamp_rate` for
 * whatever integer decimation the plan chose, so it depends on the caller's
 * rate ratio. Reported for the same reason RateConverter::stages is — a
 * caller who can read back what was planned can check it.
 */
double RateConverter_get_bank_sps (const RateConverter_state_t *s);

/**
 * @brief Get / set the output-to-input sample rate ratio.
 * The setter rebuilds the entire cascade (new stage selection, new
 * sub-objects) and resets all filter memories — equivalent to
 * destroying and recreating with the new rate. Setting rate <= 0 is
 * silently ignored.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.rate
 * 0.5
 * >>> rc.rate = 2.0
 * >>> rc.rate
 * 2.0
 * @endcode
 */
double RateConverter_get_rate (const RateConverter_state_t *s);

/**
 * @brief Change the rate; rebuilds the cascade and resets all filter state.
 * Silently ignores rate <= 0.
 *
 * @param s     Pointer to a valid RateConverter_state_t.
 * @param rate  New output/input rate ratio.
 */
void RateConverter_set_rate (RateConverter_state_t *s, double rate);

/**
 * @brief Write a human-readable label for stage i into buf.
 *
 * Examples: "HalfbandDecimator", "CIC(8)", "CIC(8)+FIR", "Resampler(0.8)".
 *
 * @param s    Must be non-NULL.
 * @param i    Stage index in `[0, s->n_stages)`.
 * @param buf  Output buffer.
 * @param len  Capacity of buf in bytes.
 * @return 1 on success, 0 if i is out of range.
 */
int RateConverter_stage_label (RateConverter_state_t *s, int i,
                               char *buf, size_t len);

/**
 * @brief One-shot rate conversion — no persistent state required.
 *
 * Creates a temporary converter, converts n_in samples, destroys it.
 * Equivalent to:
 * @code
 * RateConverter_state_t *rc = RateConverter_create(rate, compensate);
 * size_t n = RateConverter_execute(rc, in, n_in, out, max_out);
 * RateConverter_destroy(rc);
 * @endcode
 *
 * Use RateConverter_create() directly when processing multiple blocks at
 * the same rate — the one-shot form resets filter memory on every call.
 *
 * @param rate       Output-to-input sample rate ratio.
 * @param compensate Non-zero to enable CIC droop compensation.
 * @param in         CF32 input samples.
 * @param n_in       Number of input samples.
 * @param out        Output buffer.
 * @param max_out    Output buffer capacity in samples.
 * @return Number of output samples written; 0 only if OOM or n_in == 0.
 */
size_t RateConverter_convert (double rate, int compensate,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* RATE_CONVERTER_CORE_H */
