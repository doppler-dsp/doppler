/**
 * @file acq_core.h
 * @brief Streaming DSSS burst-acquisition engine.
 *
 * Acquires a direct-sequence spread-spectrum burst — a run of repeated,
 * BPSK-modulated PN-code segments — arriving with an unknown integer code
 * phase and an unknown carrier-frequency (Doppler) offset, buried in AWGN.
 * It jointly estimates the (Doppler bin, code phase) of the burst and declares
 * a detection whenever the CFAR test statistic crosses an automatically
 * configured threshold.
 *
 * Pipeline (owned end to end, one object):
 *   push(raw cf32) -> ring buffer -> reframe to (doppler_bins, code_bins) ->
 *   slow-time Doppler FFT (FFT along the segment axis) -> 2-D code correlation
 *   against a single-row PN reference (corr2d) -> argmax + CFAR noise estimate
 *   -> threshold gate -> acq_result_t.
 *
 * The fast-time axis (code_bins = sf*spc columns) is the circular code matched
 * filter; the slow-time axis (doppler_bins rows, one row per code repetition)
 * is the Doppler search.  A carrier offset f (cycles/sample) lands the peak at
 * row = round(f*code_bins*doppler_bins) mod doppler_bins, column = code phase.
 *
 * Physics-only construction: the user gives the PN @p code, the front-end
 * geometry (@p reps, @p spc, @p chip_rate), the sensitivity (@p cn0_dbhz), and
 * the detection targets (@p pfa, @p pd, optional @p doppler_uncertainty).  The
 * engine converts C/N0 to a per-sample amplitude SNR
 * (snr = sqrt(10^(cn0_dbhz/10) / (chip_rate*spc))) and picks the *smallest*
 * coherent depth doppler_bins in `[1, reps]` whose doppler_bins*code_bins
 * coherent samples meet @p pd (det_threshold / det_pd) — minimum latency for a
 * strong signal.  A tighter @p doppler_uncertainty shrinks the searched cell
 * count, lowering the Bonferroni threshold (more sensitive).  Every reported
 * detection inverts this same relationship to report an estimated C/N0
 * (acq_result_t::cn0_dbhz_est) — a bandwidth/integration-time-independent
 * figure of merit directly comparable to @p cn0_dbhz, unlike a raw per-sample
 * or coherently-integrated ratio (both scale with @p spc/@p reps and so
 * aren't portable across configurations).
 *
 * **Wideband mode** (@p doppler_uncertainty > the native span
 * `chip_rate/(2*sf)`): a coherent slow-time Doppler FFT can only ever resolve
 * frequency *within* one native span — more coherent depth subdivides that
 * SAME fixed range more finely, it never widens it — and for a continuous
 * (async, data-modulated) signal, a multi-epoch coherent axis wide enough to
 * matter aliases the data's own bit transitions across the whole Doppler-bin
 * axis (a structural mislock, not a graceful SNR loss — see
 * docs/design/dsss-acquisition.md).  So once @p doppler_uncertainty exceeds
 * the native span, this engine forces `doppler_bins = 1` (no coherent
 * multi-epoch combining at all — sidesteps the aliasing entirely) and instead
 * tiles the requested uncertainty with `n_freq_bins =
 * ceil(doppler_uncertainty / (chip_rate/(2*sf)))` parallel frequency-window
 * hypotheses, each one native span wide, searched every epoch from a SINGLE
 * shared forward FFT of that epoch: hypothesis r's spectrum is the shared FFT
 * circularly rolled by r bins (exact — the window spacing IS this
 * code_bins-point FFT's own bin spacing) against one fixed precomputed
 * replica spectrum, then inverse-FFT'd — `n_freq_bins` inverse FFTs plus the
 * one shared forward FFT per epoch, not `n_freq_bins` independent
 * down-conversions. Empirically the cheaper of the two realizations
 * benchmarked for this (`prototypes/async_despreader/bench_freq_bank.py`):
 * ~1.2x-1.55x faster than an equivalent tuned-mixer bank, measured with real
 * doppler.spectral.FFT.  SNR margin in this mode comes entirely from
 * @p max_noncoh non-coherent looks (magnitude-squared accumulation, immune to
 * data-modulation sign flips) rather than coherent depth.  `doppler_bin` in
 * @ref acq_result_t reports the frequency-window index (0 … n_freq_bins-1,
 * native FFT-bin ordering) instead of a slow-time-FFT row; `doppler_res_hz`
 * still reports the per-window spacing (chip_rate/sf, unchanged formula at
 * doppler_bins=1).  @p doppler_resolution / @p doppler_rate are ignored in
 * this mode (doppler_bins is forced to 1 regardless — the finest resolution
 * this mode offers is one native window); combining wideband search WITH a
 * coherent depth > 1 per window is not supported (a possible future
 * extension, not needed by any current use case).
 *
 * @code
 * // 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
 * uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
 * acq_state_t *a = acq_create(code, 31, 16, 4, 1.0e6, 45.0,
 *                             0.0, 1e-3, 0.9, 0, 1, 0.0, 0.0);
 * acq_result_t hits[64];
 * size_t nh = acq_push(a, samples, n_samples, hits, 64);
 * for (size_t i = 0; i < nh; i++)
 *   printf("Doppler %zu, code phase %zu, C/N0 %.1f dB-Hz\n",
 *          hits[i].doppler_bin, hits[i].code_phase,
 *          hits[i].cn0_dbhz_est);
 * acq_destroy(a);
 * @endcode
 */
#ifndef ACQ_CORE_H
#define ACQ_CORE_H

#include "buffer/buffer.h"
#include "clib_common.h"
#include "corr2d/corr2d_core.h"
#include "detection/detection_core.h"
#include "dp_state.h"
#include "fft/fft_core.h"
#include "jm_perf.h"
/* detector2d_core.h supplies det_noise_mode_t (guarded typedef). */
#include "detector2d/detector2d_core.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief One acquisition detection event.
   */
  typedef struct
  {
    size_t doppler_bin; /**< Peak row: Doppler bin (0 … doppler_bins-1), or,
                             in wideband mode, the frequency-window index
                             (0 … n_freq_bins-1) — see acq_core.h's file
                             doc comment.                                   */
    size_t code_phase; /**< Peak col: code phase (0 … code_bins-1).          */
    float  peak_mag;   /**< max `|R[i,j]|` over the surface (linear).        */
    float  noise_est;  /**< CFAR noise estimate over `[noise_lo, noise_hi]`. */
    float  test_stat;  /**< The gating CFAR statistic: peak_mag / noise_est
                            under coherent-only detection (n_noncoh == 1);
                            scaled by sqrt(2*n_noncoh) once non-coherent
                            looks are combined. 0 if noise_est == 0.        */
    float cn0_dbhz_est;        /**< Estimated carrier-to-noise density (dB-Hz),
                                    backed out of test_stat via the same C/N0 <->
                                    per-sample-amplitude-SNR relationship used to
                                    size the engine (see acq_create()). Tracks the
                                    true C/N0 while receiver AWGN dominates the
                                    CFAR noise estimate; saturates at the code's
                                    own autocorrelation-sidelobe floor once the
                                    true C/N0 exceeds what this code/geometry can
                                    resolve — a real ceiling, not a fault.        */
    uint64_t samples_consumed; /**< st->samples_consumed at the exact
                                     moment this hit was appended — the
                                     raw sample offset (since this engine's
                                     own stream start) this detection's
                                     epoch ended at. The per-hit anchor a
                                     caller needs to derive a precise
                                     timestamp (dp_sample_clock_stamp_at(),
                                     timing/timing_core.h) instead of
                                     reusing one message-level timestamp
                                     for every hit — a single push() call
                                     spanning multiple epochs can emit
                                     several hits at different sample
                                     offsets. Acquisition itself stays
                                     wall-clock-agnostic; this is the raw
                                     material a caller with a real clock
                                     converts.                            */
  } acq_result_t;

  /**
   * @brief Streaming acquisition-engine state.
   *
   * Allocate with acq_create(); never stack-allocate.
   */
  typedef struct
  {
    corr2d_state_t *corr; /**< Single-row-ref correlator (dwell=1).          */
    fft_state_t *slow_fft; /**< Length-doppler_bins forward FFT (slow time). */
    dp_f32_t    *ring;  /**< Raw cf32 input ring (the only ring).          */
    float complex *ref; /**< Single-row reference (n), owned.              */
    float complex *yframe;  /**< Slow-time-FFT'd frame (n) fed to corr.  */
    float complex *colbuf;  /**< Gathered column scratch (doppler_bins).  */
    float complex *colout;  /**< FFT'd column scratch (doppler_bins).  */
    float complex *out_buf; /**< corr2d dump output (n) — also the wideband
                                 mode's (n_freq_bins, code_bins) grid.       */
    float *mag_buf;       /**< |out_buf| (n).                                */
    float *noise_scratch; /**< Scratch for the median sort (n).              */
    float *nc_surface;    /**< Non-coherent |·|² accumulator (n); NULL
                               unless n_noncoh > 1.                          */

    /* Wideband mode only (n_freq_bins > 1) — see the file doc comment.
     * Independent of corr/slow_fft/yframe/colbuf/colout above (unused, but
     * left allocated at their trivial doppler_bins=1 size, in this mode). */
    fft_state_t *wide_fwd; /**< Forward FFT, length code_bins.               */
    fft_state_t *wide_inv; /**< Inverse FFT, length code_bins.               */
    float complex
        *wide_ref_spec; /**< conj(FFT(replica row)), length code_bins.     */
    float complex
        *wide_spec; /**< FFT(raw epoch), length code_bins; once/epoch.    */
    float complex *wide_prod; /**< Rolled-spectrum * wide_ref_spec product,
                                    length code_bins; reused per hypothesis. */

    size_t
        doppler_bins;   /**< Coherent depth = slow-time FFT length (<= reps).
                              Forced to 1 in wideband mode (n_freq_bins > 1).  */
    size_t n_freq_bins; /**< Wideband frequency-window hypotheses (1 =
                              disabled/native — see the file doc comment).   */
    size_t code_bins; /**< One segment in samples = sf*spc.                 */
    size_t n; /**< Output grid size in samples: doppler_bins * n_freq_bins *
                   code_bins (one of doppler_bins/n_freq_bins is always 1).  */
    size_t frame_n; /**< Raw samples consumed from the ring per iteration:
                          == n natively; == code_bins in wideband mode (one
                          epoch's worth — n_freq_bins hypotheses come from
                          ONE shared epoch, not from consuming more input).  */
    size_t sf;      /**< Chips per PN segment (= len(code)).              */
    size_t spc;     /**< Samples per chip (chip-rate oversample factor).  */
    size_t reps;    /**< Max coherent code repetitions (the ceiling).     */
    size_t
        searched_bins; /**< Doppler bins scanned (<= doppler_bins; du prior).*/
    size_t n_noncoh;   /**< Non-coherent looks per detection (1 = coherent). */
    size_t max_noncoh; /**< Cap on the auto-split non-coherent look count.   */
    size_t nc_count; /**< Coherent dumps in the current look (0…n_noncoh-1).*/
    size_t ring_cap; /**< Ring capacity in complex samples.                */
    size_t noise_lo; /**< First CFAR reference bin (inclusive).            */
    size_t noise_hi; /**< Last CFAR reference bin (inclusive).             */
    det_noise_mode_t noise_mode; /**< CFAR aggregation mode. */

    double chip_rate; /**< Chip rate (Hz).                               */
    double fs;        /**< Sample rate (Hz) = chip_rate * spc.           */
    double cn0_dbhz;  /**< Sensitivity used to size the search (dB-Hz).  */
    double
        doppler_span_hz; /**< Native Doppler half-range = chip_rate/(2*sf). */
    double
        doppler_res_hz; /**< Doppler bin width = chip_rate/(sf*doppler_bins).*/
    double pfa;         /**< Target system false-alarm probability (stored for
                             configure_search_raw's threshold re-derivation).    */
    double doppler_uncertainty; /**< One-sided Doppler search half-range
                                      (Hz); 0 = full native span.         */
    double symbol_rate; /**< Continuous data-symbol rate (Hz); 0 = no known
                              data-modulation clock (legacy sizing).     */
    double epochs_per_symbol;  /**< (chip_rate/sf)/symbol_rate; 0 when
                                     symbol_rate <= 0.                    */
    double doppler_resolution; /**< Desired Doppler-bin resolution (Hz) on
                                     the data-modulation search; 0 = no
                                     floor (minimize total epochs outright,
                                     the legacy joint-search behavior).   */
    double doppler_rate; /**< Expected Doppler rate of change (Hz/s) on the
                               data-modulation search; 0 = static Doppler
                               (no ceiling beyond reps).                  */

    float  threshold; /**< CFAR gate on test_stat (theta); coherent path.   */
    float  eta;       /**< Raw per-cell Rayleigh amplitude threshold.       */
    float  eta_nc;    /**< Non-coherent CFAR threshold (order-N_nc Marcum).  */
    double pfa_cell;  /**< Bonferroni per-cell false-alarm probability.     */
    double pd;        /**< Target detection probability.                    */
    double pd_predicted;  /**< Predicted Pd at cn0_dbhz and the chosen
                               grid: the AVERAGE Pd over the straddle
                               priors (slow-time scalloping, intra-segment
                               rotation, code sample offset — quadrature
                               over uniform priors), not the on-grid best
                               case, and not Pd at the mean amplitude
                               (which Jensen makes optimistic). */
    double straddle_loss; /**< Mean AMPLITUDE derating from grid straddle —
                               a diagnostic summary (~20*log10 of it in dB);
                               sizing and pd_predicted average Pd itself
                               over the priors. Derived config, recomputed
                               by create(). */
    uint8_t underpowered; /**< 1 when pd_predicted < pd. */

    uint64_t
        samples_consumed; /**< Total framed samples (the state's offset).   */

    /* Last-dump bookkeeping (for inspection). */
    size_t peak_row;
    size_t peak_col;
    float  peak_mag;
    float  noise_est;
    float  test_stat;
  } acq_state_t;

  /**
   * @brief Per-object extra header for an engine's cross-call state.
   *
   * The state blob is the *only* thing a fresh engine needs to continue a
   * stream from `(descriptor, state, input)` — it makes the engine a pure
   * transducer for the elastic fan-out (thread / process / pod).  Standard
   * bytes interface (see dp_state.h); layout, contiguous and flat:
   *
   *   `[ dp_state_hdr_t ] [ acq_extra_t ]`
   *   `[ float complex unconsumed[n_unconsumed] ]`   (partial frame, < n
   * samples)
   *   `[ float          nc_surface[n] ]`             (only when n_noncoh > 1)
   *
   * Build the byte buffer with acq_state_bytes(); set_state validates the
   * envelope (magic/version/size) plus n / n_noncoh below, rejecting a
   * mismatch rather than reinterpreting it.
   */
  typedef struct
  {
    uint16_t has_nc; /**< 1 if `nc_surface[n]` follows the samples.     */
    uint16_t _pad;
    uint32_t n_noncoh;         /**< Non-coherent looks (consistency).     */
    uint64_t n;                /**< Frame size; must equal engine's n.    */
    uint64_t samples_consumed; /**< Stream offset framed so far.          */
    uint32_t nc_count;     /**< Looks accumulated in the current dump.    */
    uint32_t n_unconsumed; /**< Partial-frame samples that follow (< n).  */
  } acq_extra_t;

#define ACQ_STATE_MAGIC DP_FOURCC ('A', 'C', 'Q', 'R')
#define ACQ_STATE_VERSION 1u

  /**
   * @brief Create a streaming DSSS acquisition engine.
   *
   * Builds the single-row oversampled BPSK reference from @p code, infers
   * sf = @p code_len, converts @p cn0_dbhz to a per-sample amplitude SNR
   * (snr = sqrt(10^(cn0_dbhz/10) / (chip_rate*spc))), and auto-configures the
   * search grid.
   *
   * With @p symbol_rate <= 0 (default; no known continuous data-modulation
   * clock): picks the *smallest* coherent depth doppler_bins in `[1, reps]`
   * whose doppler_bins*code_bins coherent samples meet @p pd at the
   * Bonferroni threshold, plus non-coherent looks (up to @p max_noncoh) if
   * the coherent depth alone falls short.
   *
   * With @p symbol_rate > 0: a continuous data-modulated signal makes a
   * data-bit transition landing mid-coherent-epoch split the coherent sum
   * into two oppositely-signed partial segments, a self-cancellation loss
   * the Doppler/code-phase-only model above doesn't know about and can
   * silently under-size for (see docs/gallery/dsss-acq-async-data.md).  The
   * engine instead jointly searches doppler_bins in `[1, reps]` x
   * non-coherent looks in `[1, max_noncoh]`, pricing that loss honestly
   * (semi-analytical: quadrature over the window's phase relative to the
   * symbol clock, crossed with exact enumeration over the data signs the
   * window touches), and picks the grid meeting @p pd with the fewest total
   * epochs, breaking ties toward a smaller coherent depth (which also lowers
   * mislock risk) -- unless @p doppler_resolution floors the search (below).
   *
   * A tighter @p doppler_uncertainty narrows the scanned Doppler band,
   * lowering the per-cell threshold (more sensitive), on both paths.  Use
   * acq_configure_search_raw() to pin the grid directly instead of relying
   * on either search.
   *
   * @p doppler_resolution > 0 (only meaningful with @p symbol_rate > 0)
   * floors the coherent depth at `ceil(chip_rate / (sf * doppler_resolution))`
   * (clipped to `[1, reps]`) before the joint search runs, and the search
   * then takes the *first* `(doppler_bins, n_noncoh)` starting from that
   * floor that meets @p pd -- trading the fewest-total-epochs guarantee for a
   * guaranteed minimum resolution, and, critically, for search cost: the
   * unfloored joint search is a full `reps x max_noncoh` sweep of the
   * `O(doppler_bins^2)` data-modulation model (`_data_mod_pd`), which the
   * function's own inner comment already flags as assuming a coherent depth
   * "physically small (tens at most)" -- fine at the default @p reps, but
   * cubic in @p reps once a caller raises it to reach a fine
   * @p doppler_resolution.  Anchoring the sweep at the resolution floor
   * instead of 1 turns that into a handful of evaluations near the floor
   * (first success wins), independent of how large @p reps is.
   *
   * @p doppler_rate > 0 (only meaningful with @p symbol_rate > 0) caps the
   * coherent depth from the other direction: over a `doppler_bins`-epoch
   * coherent window, a nonzero Doppler rate of change shifts the true
   * frequency across the window, smearing the FFT peak once that drift
   * approaches a resolution bin.  The largest depth that keeps in-window
   * drift under one bin is `doppler_bins < chip_rate / (sf * sqrt
   * (doppler_rate))`; the joint search (both its floored and unfloored
   * modes) clips its coherent-depth ceiling to this in addition to @p reps,
   * so a caller-raised @p doppler_resolution can never push `doppler_bins`
   * past the point where the signal's own dynamics would invalidate the
   * coherent sum.
   *
   * @param code        PN chips (0/1), length @p code_len.
   * @param code_len    Number of chips supplied (= sf, the spreading factor).
   * @param reps        Max coherent code repetitions, the coherence ceiling
   * (>=1).
   * @param spc         Samples per chip (>= 1).
   * @param chip_rate   Chip rate in Hz (> 0).
   * @param cn0_dbhz    Carrier-to-noise density in dB-Hz (> 0).
   * @param doppler_uncertainty  One-sided Doppler search half-range in Hz; 0
   * uses the full native span +/- chip_rate/(2*sf).  A value greater than the
   * native span engages wideband mode (see the file doc comment above):
   * doppler_bins is forced to 1 and the uncertainty is tiled with parallel
   * frequency-window hypotheses instead.
   * @param pfa         Target system (max-of-N) false-alarm probability (0,1).
   * @param pd          Target detection probability (0,1).
   * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
   * @param max_noncoh  Cap on the auto-split non-coherent look count (>= 1;
   *                    default 1 keeps the engine purely coherent).
   * @param symbol_rate Continuous data-symbol rate in Hz; <= 0 (default)
   *                    disables the data-modulation-aware search above.
   * @param doppler_resolution  Desired Doppler-bin resolution in Hz; 0
   *                    (default) places no floor on the coherent depth --
   *                    see above.
   * @param doppler_rate  Expected Doppler rate of change in Hz/s; 0
   *                    (default) assumes a static Doppler -- see above.
   * @return Heap-allocated state, or NULL on bad arguments / allocation
   * failure.
   */
  acq_state_t *acq_create (const uint8_t *code, size_t code_len, size_t reps,
                           size_t spc, double chip_rate, double cn0_dbhz,
                           double doppler_uncertainty, double pfa, double pd,
                           int noise_mode, size_t max_noncoh,
                           double symbol_rate, double doppler_resolution,
                           double doppler_rate);

  /** @brief Destroy and free an engine.  @param state May be NULL. */
  void acq_destroy (acq_state_t *state);

  /**
   * @brief Drain the input ring and reset the coherent accumulator.
   * @param state Must be non-NULL.
   */
  void acq_reset (acq_state_t *state);

  /**
   * @brief Pin the search grid directly, bypassing both auto-sizing
   *        searches — the advanced escape hatch (mirrors Dll's/Costas's
   *        configure_lock_raw()).
   *
   * Resizes every buffer/plan that depends on the grid (the slow-time FFT,
   * the code correlator, the reference, and every per-frame scratch buffer),
   * re-derives the threshold ladder for the pinned grid from the same
   * physics acq_create() used, and clears in-flight accumulation (ring
   * contents, the non-coherent power accumulator, dwell bookkeeping) — call
   * between push() calls, never a substitute for one.
   *
   * @param state        Allocated engine (non-NULL).
   * @param doppler_bins Coherent depth to pin, in `[1, reps]`.
   * @param n_noncoh     Non-coherent look count to pin, in `[1, max_noncoh]`.
   * @return 0 on success, -1 if either argument is out of range or an
   *         allocation fails (the engine is left usable at its prior grid
   *         on failure).
   */
  int acq_configure_search_raw (acq_state_t *state, size_t doppler_bins,
                                size_t n_noncoh);

  /**
   * @brief Stream raw samples; emit one event per CFAR dump above threshold.
   *
   * Buffers @p in, then for every complete frame applies the slow-time Doppler
   * FFT, correlates against the PN reference, dumps the coherent surface (or,
   * when n_noncoh > 1, accumulates |·|² over n_noncoh looks first), gates the
   * peak on the auto-configured threshold, and appends an acq_result_t.
   *
   * @param state        Allocated engine (non-NULL).
   * @param in           Raw input, interleaved CF32, @p n_in complex samples.
   * @param n_in         Number of complex input samples.
   * @param result       Output array for detection events.
   * @param max_results  Capacity of @p result.
   * @return Number of events written (0 … max_results).
   */
  size_t acq_push (acq_state_t *state, const float complex *in, size_t n_in,
                   acq_result_t *result, size_t max_results);

  /* ── Serializable state — the elastic / pure-transducer face
   * ─────────────────
   *
   * The OO engine above is convenient but stateful.  These match the rest of
   * the library's serializable objects (lo/cic/fir/ddcr): serialize a
   * channel's cross-call state to a flat POD, ship (descriptor, state, input)
   * to any thread/process/pod, rebuild the engine from the descriptor
   * (acq_create), inject the state, and continue — bit-identical to an
   * uninterrupted run.
   */

  /** @brief Byte size of @p state's blob (header + unconsumed + nc). */
  size_t acq_state_bytes (const acq_state_t *state);

  /**
   * @brief Serialize @p state's cross-call state into @p blob (caller-owned,
   *        acq_state_bytes() long).  Call between pushes (no partial dump
   * pending).
   */
  void acq_get_state (const acq_state_t *state, void *blob);

  /**
   * @brief Restore cross-call state from @p blob into @p state (replacing it).
   * @return 0 on success, -1 if the blob's magic/version/n/n_noncoh disagree
   *         with @p state (rebuild the engine from the matching descriptor
   * first).
   */
  int acq_set_state (acq_state_t *state, const void *blob);

  /**
   * @brief Pure run: inject @p state_in, stream @p in, emit hits, export
   *        @p state_out — `(state_in, input) -> (state_out, output)` over an
   *        engine treated as immutable config + scratch.  @p state_in / @p
   *        state_out may alias.  Either may be NULL (NULL in = fresh;
   *        NULL out = discard).
   * @return Number of events written (0 … max_results).
   */
  size_t acq_run (acq_state_t *state, const void *state_in, void *state_out,
                  const float complex *in, size_t n_in, acq_result_t *result,
                  size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* ACQ_CORE_H */
