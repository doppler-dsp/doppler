/**
 * @file correlator_core.h
 * @brief The one FFT cross-correlator, with coherent integrate-and-dump.
 *
 * One object, built by flavor (docs/design/correlator.md section 4):
 *
 * | constructor                            | flavor                      |
 * | -------------------------------------- | --------------------------- |
 * | dp_correlator_create_1d()              | a 1-D reference, one row    |
 * | dp_correlator_create_2d()              | a (ny, nx) reference        |
 * | dp_correlator_create_delay_doppler()   | a code, mapped to (Doppler, delay) |
 * | dp_correlator_create_detector_2d()     | a streaming 2-D detector    |
 * | dp_correlator_create_detector_1d()     | a streaming 1-D detector    |
 * | dp_correlator_create_continuous()      | a continuous DSSS search    |
 * | dp_correlator_create_burst()           | a burst preamble search     |
 *
 * The methods are shared. The 1-D flavor is the 2-D one at ny = 1, which
 * the single-row path below serves, so a length-n correlation and a
 * one-row 2-D correlation are the same bits (test_corr_equiv), and so are
 * the two detectors' hits (test_detector_equiv).  The map flavors serve
 * execute and its batch family; the detector and search flavors serve
 * push, and each refuses the other's (SIZE_MAX).  The search flavors are
 * the acquisition engine (the search flavors' section, at the end).
 *
 * All buffers are ny×nx row-major flat arrays of length ny*nx.  The
 * correlation theorem extends naturally:
 *
 *   `R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`
 *
 * The reference spectrum is pre-computed at create time.  The int-dump
 * semantics are identical to the 1-D case: coherently sum ``dwell``
 * frames, then dump.
 *
 * **Single-row-reference fast path.**  When @p ref is nonzero only in row 0
 * (the DSSS acquisition/detection shape: a code replica with no Doppler
 * content of its own) and @p ny_out == @p ny (no row-axis interpolation),
 * `ref_spec[u,v]` is independent of `u` — the row axis of the 2-D transform
 * pair cancels to an exact identity (DFT orthogonality:
 * `sum_u exp(2*pi*i*u*(i-i')/ny) = ny` iff `i==i'`, else 0), for *any* row
 * content.  `dp_correlator_execute` then reduces exactly to
 * `R(i,j) = IFFT_nx(FFT_nx(row_i) · conj(FFT_nx(ref_row0)))(j) / nx`,
 * applied independently per row — so `dp_correlator_create_2d` detects this shape and
 * runs `ny` independent length-`nx` 1-D FFTs instead of a full `(ny,nx)`
 * 2-D FFT, skipping the row-axis work that would otherwise cancel to a
 * no-op.  Any other reference shape, or `ny_out > ny`, uses the general
 * 2-D path unchanged.
 *
 * Lifecycle:
 * @code
 * enum { NY = 4, NX = 8 };
 * float _Complex ref[NY * NX] = { 1.0f };   // row-major 2-D reference
 * float _Complex frame[NY * NX] = { 1.0f }; // one frame, fed four times
 * float _Complex out[NY * NX];
 * dp_correlator_state_t *c
 *     = dp_correlator_create_2d (ref, NY, NX, 4, 1, 0, 0, -1);
 * size_t dumps = 0;
 * for (int i = 0; i < 4; i++)
 *   dumps += dp_correlator_execute (c, frame, NY * NX, out, NY * NX, NULL)
 *            != 0;
 * printf ("dumps: %zu, peak %.1f\n", dumps, crealf (out[0])); // 1, on i == 3
 * dp_correlator_destroy (c);
 * @endcode
 */
#ifndef DP_CORRELATOR_CORE_H
#define DP_CORRELATOR_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/fft2d/fft2d_core.h"
#include "doppler/cvt/cvt_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Which constructor built a correlator.
 *
 * Fixed at create and the state blob's first field: a blob restores only
 * into a correlator of its own flavor. Python's Correlator.flavor reads it
 * by name, through the correlator_flavor enum table in just-makeit.toml.
 */
/* SSOT: enum=correlator_flavor */
typedef enum
{
  DP_CORRELATOR_1D            = 1, /**< dp_correlator_create_1d()      */
  DP_CORRELATOR_2D            = 2, /**< dp_correlator_create_2d()      */
  DP_CORRELATOR_DELAY_DOPPLER = 3, /**< dp_correlator_create_delay_doppler() */
  DP_CORRELATOR_DETECTOR_1D   = 4, /**< dp_correlator_create_detector_1d() */
  DP_CORRELATOR_DETECTOR_2D   = 5, /**< dp_correlator_create_detector_2d() */
  DP_CORRELATOR_SEARCH_CONTINUOUS = 6, /**< dp_correlator_create_continuous() */
  DP_CORRELATOR_SEARCH_BURST  = 7  /**< dp_correlator_create_burst()     */
} dp_correlator_flavor_t;

/**
 * @brief How a detector flavor aggregates the noise floor from the
 *        correlation magnitudes over the flat bins noise_lo to noise_hi,
 *        inclusive.
 *
 * The test statistic is peak_mag / noise_est.  A zero noise_est (e.g., when
 * all bins in the range are zero) yields test_stat = 0, and so does a NaN
 * one (a NaN anywhere in the range is a NaN mean, median, min or max).
 */
typedef enum
{
  DET_NOISE_MEAN   = 0, /**< Arithmetic mean of the magnitudes.          */
  DET_NOISE_MEDIAN = 1, /**< Median (upper middle for an even count).    */
  DET_NOISE_MIN    = 2, /**< Minimum magnitude.                          */
  DET_NOISE_MAX    = 3, /**< Maximum magnitude.                          */
} det_noise_mode_t;

/**
 * @brief One listed peak of a surface: a cell and its value, in the
 *        surface's own units (a magnitude on a coherent surface, a power on
 *        a non-coherent one). What det_peak_list() (correlator_private.h)
 *        returns, and what the acquisition engine keeps per dwell.
 */
typedef struct
{
  size_t row;   /**< surface row                                    */
  size_t col;   /**< surface column                                 */
  float  value; /**< the cell's value                               */
} det_peak_t;

/**
 * @brief One detection, from any detector or search flavor's
 *        dp_correlator_push().
 *
 * docs/design/correlator.md section 4: the one hit record.  The peak's
 * flat index in the correlation map, decomposed into (row, col) so that a
 * caller does not need the map's width.  At one row (the 1-D detector) row
 * is 0 and col is the lag.  A search flavor's row is the Doppler bin and
 * its col the code phase, and it fills every field; a detector flavor
 * fills row through test_stat and writes the rest as 0.  64 bytes, with no
 * unnamed padding.
 */
typedef struct
{
  size_t row; /**< Row of the peak: 0 at one row; a search flavor's Doppler
                   bin (0 … coherent_bins-1), or, in wideband mode, a bin
                   of the uniform grid of window_bins*coherent_bins bins
                   (window_bins tiles of coherent_bins each) — see the
                   search flavors' section. */
  size_t col; /**< Column of the peak: the lag, at one row; a search
                   flavor's code phase (0 … lag_bins-1), a lag of
                   col * code_bins / lag_bins samples. */
  float  peak_mag;  /**< max |R| (linear magnitude, not power).             */
  float  noise_est; /**< Noise estimate aggregated over noise_lo..noise_hi. */
  float  test_stat; /**< peak_mag / noise_est; 0 if noise_est is not > 0.
                         A search flavor's is scaled by sqrt(2*n_noncoh)
                         once non-coherent looks are combined.            */
  float cn0_dbhz_est;        /**< Estimated carrier-to-noise density (dB-Hz),
                                  backed out of test_stat via the same C/N0 <->
                                  per-sample-amplitude-SNR relationship used to
                                  size the engine (see dp_correlator_create_burst()/
                                  dp_correlator_create_continuous()). Tracks the
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
  double chip_phase; /**< The code's own instantaneous phase at the
                          hand-off instant, in chips, Dll's convention --
                          the seed a code loop wants for this hit. The
                          same value dp_correlator_build_handoff() reports as
                          dp_correlator_handoff_t::chip_phase: the inverted lag
                          advanced by the dwell's dilation when a carrier
                          is set (dp_correlator_set_carrier_freq_hz()). For an
                          engine built by dp_correlator_create_burst(), one chip
                          is one template sample, so chip_phase is in
                          template samples over code_bins: a caller at s
                          samples per chip divides by s. The burst
                          search refuses a carrier setter, so its hits
                          carry no dwell advance. Appended last so
                          positional initialisers do not shift. */
  double doppler_rate_hz_s; /**< The Doppler rate, Hz/s, of the rate
                          hypothesis that won this hit's dwell
                          (dp_correlator_create_burst()'s rate_hypotheses); 0
                          without hypotheses, and 0 when the dwell's block is one
                          or two epochs, which cannot see a rate (every
                          hypothesis is then the same surface). A
                          hypothesis, not a fit, and a reliable one only
                          from a block WHOLLY inside the preamble and well
                          above the design point. Then it names the
                          hypothesis nearest the carrier's rate -- within
                          half a spacing, `doppler_rate/(n - 1)` -- every
                          time (validate_acq_rate_pd's reported-rate rows,
                          N = 9, D = 10, design -13 dB, 23 dB over it:
                          119/119 at true rate 0, 121/121 at +R). Below
                          that it is not: at the design point only 30% (rate
                          0) and 66% (+R) of the full-block hits name it,
                          adjacent hypotheses differing by at most pi/4 of
                          phase at the block's edges so that noise picks
                          among neighbours; and about three hits in four
                          come from dwells STRADDLING the preamble's edges,
                          which often name +-R (39% of all hits at +10
                          dB-Hz name the nearest at true rate 0, 51% at
                          +R). DETECTION is unaffected -- the surface that
                          wins is a good one whichever neighbour it is.
                          Treat the value as a seed, not a measurement, at
                          the C/N0 the engine was sized for. With
                          `max_peaks > 1` one hypothesis's surface serves
                          the whole dwell's list, so every hit of a dwell
                          wears the winner's rate. The Doppler the hit
                          reports is the carrier's at the middle of the
                          dwell's block. Appended last so positional
                          initialisers do not shift. */
  float test_stat_peak; /**< test_stat as the emitter would read on its
                             own lag: the straddle a delay between two
                             lag columns costs, put back. Estimated from
                             the larger of the hit's two neighbouring
                             columns under the mainlobe the engine
                             models (a triangle `shape.zone` samples
                             wide, as for a chip code): a centre c and
                             neighbour n one column, s samples, apart
                             give (c + n) / (2 - s / zone), exact on
                             that triangle, clamped between c and
                             c / (1 - s / (2 zone)) -- never below what
                             was read,
                             never past the worst straddle. Noise lifts
                             the neighbour, so at low statistics it
                             reads high, within that clamp. For a bound
                             on what the emitter's sidelobes can reach
                             (a sidelobe does not lose its straddle with
                             the peak); detection reads test_stat.
                             Appended last, like the field above. */
  uint32_t _pad; /**< Alignment padding, named so the record has none
                      unnamed; written as 0 (every builder's designated
                      initializer zeroes it). A new field goes before it,
                      which moves it, and test_correlator_search.c's layout assert
                      goes red until the comparison names the field. */
} dp_correlator_hit_t;


/**
 * @brief Telemetry attachment: a borrowed context + this engine's probe
 *        ids (docs/design/async-dsss-receiver.md §2.4). NULL ctx (the
 *        default) means detached — the one probe site is then a single
 *        predicted-not-taken branch per
 *        decided dwell. Never in a state blob; preserved across
 *        dp_correlator_set_state() like the borrowed code.
 */
typedef struct
{
  dp_tlm_t *ctx;      /**< NULL = detached                                */
  int32_t id_stat;    /**< "<prefix>.stat"    — the dwell's test statistic */
  int32_t id_gate;    /**< "<prefix>.gate"    — the gate it was held to    */
  int32_t id_noise;   /**< "<prefix>.noise"   — the CFAR reference         */
  int32_t id_peak;    /**< "<prefix>.peak"    — the strongest cell's value */
  int32_t id_row;     /**< "<prefix>.row"     — its native Doppler row     */
  int32_t id_col;     /**< "<prefix>.col"     — its code-phase column      */
  int32_t id_n_peaks; /**< "<prefix>.n_peaks" — picks in the dwell         */
  int32_t id_n_held;  /**< "<prefix>.n_held"  — picks held as twins        */
  int32_t id_conc;    /**< "<prefix>.conc"    — the peak's concentration   */
  int32_t id_hit;     /**< "<prefix>.hit"     — 1 when the gate fired      */
} dp_correlator_tlm_t;

/**
 * @brief A surface sink: called once per decided dwell (every `decim`-th)
 *        with the dwell's surface in test-statistic units, row-major
 *        `rows` x `cols` (Doppler rows by code-phase columns), and the
 *        dwell's `samples_consumed`. The pointer is the engine's and is
 *        valid only for the call. See dp_correlator_set_surface_sink().
 */
typedef void (*dp_correlator_surface_sink_fn) (void *ctx, const float *surface,
                                     size_t rows, size_t cols,
                                     uint64_t samples_consumed);

/**
 * @brief One tile's share of a decided surface
 *        (docs/design/acq-surface.md §13): the surface is cut into
 *        `window_bins` chunks of whole rows, and the per-cell passes
 *        after the fan -- the magnitude, the CFAR reference, the mask
 *        copy, each scan of the peak list -- run per
 *        chunk into one of these, merged serially in tile order. The
 *        merge is bit-identical at any thread count because the chunks
 *        never move.
 */
typedef struct
{
  float  ref;  /**< det_noise_estimate() over the chunk (MEAN/MIN/MAX). */
  size_t best; /**< The chunk's first maximum among the candidate cells;
                    `n_surf` when it has none.                           */
} dp_correlator_part_t;

/** Quadrature nodes the Pd model averages the delay straddle over. */
#define DP_CORRELATOR_DELAY_LOSS_NODES 4

/**
 * @brief What the engine knows about the SHAPE of the repeated preamble,
 *        beyond its samples (doppler#1470).
 *
 * The engine's framing, slow-time transform and wideband tiling hold for
 * any periodic reference; only two things read the waveform's
 * correlation shape, and they read it from here rather than from the
 * chip count:
 *
 * - the width of the correlation mainlobe, which sets the peak list's
 *   exclusion zone and the twin rule's reach (@ref zone);
 * - the amplitude a peak keeps when the true delay falls between two
 *   samples, which the Pd model averages over a uniform half-sample prior
 *   (@ref delay_loss, @ref delay_loss_mean).
 *
 * The constructor that owns the waveform fills it once. A PN code of
 * `spc` samples per chip has a triangular autocorrelation one chip wide,
 * so its descriptor is analytic: `zone = spc`, and a peak `delta` samples
 * off the grid keeps `1 - delta/spc`.
 */
typedef struct
{
  size_t zone; /**< Correlation mainlobe half-width, samples: peaks
                    closer than this in delay are one emitter.       */
  double delay_loss_mean; /**< Mean amplitude kept over a uniform delay
                               straddle of 0 to 1/2 sample.          */
  double delay_loss[DP_CORRELATOR_DELAY_LOSS_NODES]; /**< Amplitude kept at the
                               midpoint nodes (k + 1/2)/nodes of that
                               half-sample range, k = 0 … nodes-1.  */
  double off_peak; /**< Correlation energy OFF the peak, relative to
                        it: sum over m != 0 of |R(m)|^2 / R(0)^2 for
                        the periodic autocorrelation R. It lands in the
                        cells the CFAR reference averages, so the Pd
                        model charges it (doppler#1501): ~0 for a
                        perfect sequence. Filled by the constructor from
                        the replica, whatever built the rest. */
  double off_peak_amp; /**< The same lags' AMPLITUDE: sum over m != 0
                            of |R(m)| / R(0). With off_peak it says how
                            concentrated that energy is, which sets how
                            much mean it adds to the reference. */
  double psl; /**< Peak sidelobe: the largest |R(m)| / R(0) OUTSIDE the
                   mainlobe, zone <= m <= n - zone -- the lags the peak
                   list's exclusion does not cover. 0 for a perfect
                   sequence (below 1e-6, rounding). dp_correlator_psl_db() is the
                   read-back (doppler#1470). */
  double dsl; /**< Doppler-offset sidelobe: the largest |chi(m, nu)| over
                   the same lags as psl, relative to the emitter's own
                   peak, over every residual and tile offset the grid can
                   put between an emitter and a tile (design
                   acq-surface.md §8). psl is its zero-offset case, so
                   dsl >= psl. A strong emitter lists peaks this far
                   below it, more than a mainlobe from its own lag.
                   dp_correlator_dsl_db() is the read-back, and computes
                   it on the first call: read it through that, never
                   directly. Defined on the construction grid; a grid
                   pinned later keeps it. */
} dp_correlator_shape_t;

/** dp_correlator_sidelobe_ratio()'s Doppler points: across the emitter's native
  row, its own bin's width. */
#define DP_CORRELATOR_SIDELOBE_DOPPLER_PTS 9
/** dp_correlator_sidelobe_ratio()'s lag points: across the emitter's own lag
  column, one column (code_bins / lag_bins samples) wide. */
#define DP_CORRELATOR_SIDELOBE_LAG_PTS 5
/** (hit, tile) rows dp_correlator_sidelobe_ratio() keeps every column of: room
  for a pick's few stronger emitters on the tiles their cells touch. */
#define DP_CORRELATOR_SIDELOBE_SLOTS 32

/**
 * @brief The search flavors' policy: everything the two search
 *        constructors add on top of the four layers
 *        (docs/design/correlator.md section 7, 3.4b). Held by
 *        dp_correlator_state_t::search, allocated once at create;
 *        NULL in every other flavor.
 */
typedef struct dp_correlator_search
{
  float _Complex *ref; /**< Single-row reference (n), owned.              */
  float _Complex *out_buf; /**< The decided coherent surface (n_surf):
                               the one delay-Doppler map's output,
                               rows of lag_bins, scaled 1/code_bins on
                               every path (docs/design/acq-surface.md
                               §18.1).                                 */
  float *nc_surface;    /**< Non-coherent |·|² accumulator (n); NULL
                             unless n_noncoh > 1.                          */

  float _Complex *ref_spec; /**< conj(FFT(replica)), code_bins: the
                                 template every tile's weights carry.  */
  float _Complex *map_spec; /**< The block's epoch spectra,
                                 coherent_bins x code_bins.           */
  float _Complex *map_work; /**< The map's scratch for its schedule
                                 (dp__dd_map_work_len).               */
  struct dp__dd_tile *map_tiles; /**< window_bins tiles: roll, ramp
                                      offsets, weights, rows.        */
  size_t  *map_rows; /**< window_bins x D*interp: each slow row's
                          surface row (acq_block_row).                */
  int32_t *map_off;  /**< window_bins x code_bins rounded-ramp offsets;
                          NULL when no tile ramps (map_f == 1).       */
  float _Complex *map_wt; /**< The tiles' weights: window_bins x
                               code_bins with a ramp, else one shared
                               code_bins (ref_spec / code_bins).      */
  size_t map_f;      /**< The ramp's fine grid: 4 when a carrier is
                          set, the engine is tiled and D > 1; else 1. */
  size_t coherent_bins; /**< Coherent depth = slow-time FFT length (<= reps
                           on a burst engine). In wideband mode the block
                           depth D inside every tile (file doc); 1 = one
                           epoch, the continuous engine's default.        */
  size_t window_bins; /**< Wideband frequency-window hypotheses (1 =
                            disabled/native — see the file doc comment).   */
  size_t code_bins; /**< One code period in samples, fs * sf / chip_rate:
                         the length of every forward fast-time transform
                         and of the raw epoch.                         */
  size_t lag_bins;  /**< Code-phase columns of the surface: the inverse
                         fast-time transform's length. code_bins, except
                         on a continuous engine whose code_bins is not
                         7-smooth (dp_fft_smooth_floor()): there the
                         cross-spectrum is zero-padded to the next power
                         of two before the inverse, which interpolates
                         the circular correlation exactly and costs a
                         fraction of a transform of awkward length.
                         Column c is lag c * code_bins / lag_bins
                         samples.                                      */
  size_t n; /**< NATIVE grid size in samples: coherent_bins * window_bins *
                 code_bins (both may exceed 1: D rows inside each tile).
                 This is the INPUT frame and the count of statistically
                 independent cells -- it is what the threshold ladder is
                 sized from, and it is what `row` is reported on.  */
  size_t n_surf; /**< Cells in the correlation SURFACE = `interp` * n. The
                      inverse transform is evaluated on a finer Doppler
                      grid, so the buffers, the peak search and the CFAR
                      reference span this and not `n`.                     */
  size_t interp; /**< Doppler-axis interpolation factor of the inverse (1 =
                      none). Zero-padding the product in frequency is exact
                      band-limited interpolation, so a peak landing between
                      two slow-time bins is no longer attenuated by the
                      scalloping loss that made a half-bin burst invisible
                      at any SNR (gh-1002). It adds resolution to the
                      SURFACE only: the reported bin is mapped back to the
                      native grid, so no consumer's arithmetic changes.    */
  size_t frame_n; /**< Raw samples consumed from the ring per iteration:
                        == n natively; == code_bins in wideband mode (one
                        epoch's worth — window_bins hypotheses come from
                        ONE shared epoch, not from consuming more input).  */
  size_t sf;      /**< Chips per PN segment (= len(code)).              */
  dp_correlator_shape_t shape; /**< The preamble's correlation shape (zone, delay
                          straddle); config, filled by the constructor. */
  size_t reps;    /**< Max coherent code repetitions (the ceiling); always
                       1 for an engine built via dp_correlator_create_continuous().*/
  size_t searched_bins; /**< Doppler bins scanned (<= coherent_bins; du prior).*/
  size_t n_noncoh;   /**< Non-coherent looks per detection (1 = coherent). */
  double chip_rate; /**< Chip rate (Hz).                               */
  double fs;        /**< Sample rate (Hz) = chip_rate * code_bins / sf. */
  double cn0_dbhz;  /**< Design C/N0 the search is sized for (dB-Hz);
                         NaN (DP_CORRELATOR_CN0_NONE) means none was given.     */
  double doppler_span_hz; /**< Native Doppler half-range = chip_rate/(2*sf). */
  double doppler_res_hz; /**< Doppler bin width = chip_rate/(sf*coherent_bins).*/
  double pfa;         /**< Target system false-alarm probability (stored for
                           configure_search_raw's threshold re-derivation).    */
  double doppler_uncertainty; /**< One-sided Doppler search half-range
                                    (Hz); 0 = full native span.         */
  size_t dsl_window_bins; /**< The tiles of the grid the engine was
                               BUILT with, which dp_correlator_shape_t::dsl is
                               defined on; a grid pinned later does not
                               move it. */
  double symbol_rate; /**< Continuous data-symbol rate (Hz); 0 = no known
                            data-modulation clock. Diagnostic only on an
                            engine built via dp_correlator_create_continuous() (which
                            always forces coherent_bins=1 regardless) —
                            informational, doesn't feed sizing.         */
  double epochs_per_symbol;  /**< (chip_rate/sf)/symbol_rate; 0 when
                                   symbol_rate <= 0.                    */
  size_t code_only_epochs; /**< Whole code-only epochs a waveform's
                                code-only window holds at any chip phase
                                (docs/design/async-dsss-receiver.md
                                §2.1); 1 = no window, D = 1.           */
  double doppler_rate; /**< Doppler rate the depth is bounded against
                            (Hz/s); 0 = no bound from the rate.       */
  size_t rate_hyps;    /**< Doppler-rate hypotheses searched (>= 1); 1 =
                            none: the engine as it ran without them.
                            dp_correlator_create_burst()'s rate_hypotheses.   */
  float _Complex *rate_chirp; /**< rate_hyps * rate_chirp_d slow-time
                                   de-chirp phasors; NULL while
                                   rate_hyps == 1. Rebuilt lazily when
                                   coherent_bins moves.                */
  size_t rate_chirp_d; /**< The coherent_bins rate_chirp was built for. */
  double carrier_freq_hz; /**< RF carrier the Doppler is physically
                               coupled to, Hz (dp_correlator_set_carrier_freq_hz());
                               0 = uncoupled: no code-rate hypothesis per
                               tile, no dwell advance in the hand-off. */
  float _Complex *blk_raw; /**< coherent_bins * code_bins: the block's raw
                                epochs as pushed, which the map reads at
                                the block's end (and the tile-edge test
                                of search_resolve_tile_alias()); NULL unless
                                the engine is tiled with D > 1.       */
  int              threads;   /**< workers the pool runs on, the caller
                                   included; 1 without a pool          */
  int threads_req; /**< what the caller asked for: set_threads()'s n,
                        0 = the online cores. `threads` is this capped
                        at one worker a tile; every regrid re-applies
                        it, so narrowing to one tile and widening again
                        comes back to min(threads_req, tiles), not to
                        the 1 the narrow grid ran on (doppler#2797). */
  int epoch_fan; /**< 1 (create's default): every tile stage fans on the
                      pool. 0: the stages that run once an epoch run
                      inline, and the per-block ones fan only when a
                      block is more than one epoch. The result is
                      bit-identical either way; only the cost moves.
                      async_dsss_pool clears it on its search
                      (doppler#2775): on bare metal the per-epoch fan
                      pays even at one tile a worker, while on a host
                      with a costly wake-up (WSL2) it costs that pool
                      1.9-3.5x at D = 1, so no tile count decides it.  */
  dp_correlator_part_t *parts;          /**< window_bins partial slots of the
                                   decided surface's per-tile passes  */

  float  eta;       /**< Raw per-cell Rayleigh amplitude threshold.       */
  float  eta_nc;    /**< Non-coherent CFAR threshold (order-N_nc Marcum).  */
  double pfa_cell;  /**< Sidak per-cell false-alarm probability, split
                         over the cells a dwell's maximum ranges over:
                         the in-band rows of the interpolated surface,
                         times code_bins, times the rate hypotheses.  */
  double pd;        /**< Target detection probability.                    */
  double pd_predicted;  /**< Predicted Pd at cn0_dbhz and the chosen
                             grid of ONE dwell lying wholly inside the
                             signal (a burst engine's is judged on
                             pd_burst instead): the AVERAGE Pd over the straddle
                             priors (slow-time scalloping over the
                             INTERPOLATED bin the peak search samples,
                             intra-segment rotation, code sample offset
                             — quadrature over uniform priors), not the
                             on-grid best case, and not Pd at the mean
                             amplitude (which Jensen makes optimistic).
                             Conservative by construction: the engine
                             takes the maximum over an interpolated
                             surface, which the Marcum form does not
                             credit (doppler#1183, #1064). NAN when no
                             design C/N0 was given. */
  double pd_burst; /**< Predicted Pd of one BURST of `reps` repetitions
                        at cn0_dbhz: the dwells are aligned to the stream,
                        so the preamble lands at a uniform offset and
                        spans about reps/D of them, whole or partial; the
                        burst is detected when any one is. Averaged over
                        that alignment and the same straddle nodes as
                        pd_predicted (one node for every dwell of a
                        burst: they share its Doppler and delay). What a
                        burst engine sizes on and sets `underpowered`
                        from (doppler#1498). Assumes the preamble is
                        exactly `reps` periods. NAN on a continuous
                        engine, with n_noncoh > 1, or with no design
                        C/N0. */
  double straddle_loss; /**< Mean AMPLITUDE derating from grid straddle —
                             a diagnostic summary (~20*log10 of it in dB);
                             sizing and pd_predicted average Pd itself
                             over the priors. Derived config, recomputed
                             by create(). */
  uint8_t underpowered; /**< 1 when the Pd the engine is judged on falls
                             short of pd: pd_burst on a burst engine,
                             pd_predicted where pd_burst is NAN. Never
                             without a design C/N0 -- there is no target
                             to be under. */
  uint8_t burst; /**< 1 for an engine built by dp_correlator_create_burst(), 0 for
                      dp_correlator_create_continuous(). Config.               */

  /* Observability (docs/design/async-dsss-receiver.md §2.4):
     attach-on-demand, nothing in blobs. */
  dp_correlator_tlm_t tlm;       /**< telemetry attachment; ctx NULL = detached      */
  int       keep_surface; /**< 1 = the caller asked to keep surfaces; set
                               by a caller or by
                               dp_correlator_set_surface_sink(). A
                               surface is kept while this OR a sink is
                               set (dp_correlator_surface())               */
  float *stat_surface; /**< n_surf: the last decided dwell in the gate's
                            units; allocated on the first dwell decided
                            while a surface is kept                     */
  dp_correlator_surface_sink_fn sink; /**< NULL = none                              */
  void    *sink_ctx;   /**< caller's opaque pointer, passed to sink      */
  uint32_t sink_decim; /**< the sink sees every sink_decim-th dwell       */
  /* Everything above is written by create, regrid and the setters;
   * the threaded tile stages read it. The fields below are what a
   * push writes, so they sit behind a full cache line, as the
   * state's hot fields do (dp_correlator_state_t's _hot_pad). */
  unsigned char _hot_pad[64]; /**< Keeps the push-written fields off
                                   the read fields' cache lines. */
  float _Complex *prompt_scratch; /**< dp_correlator_block_prompt()'s: an
                                       epoch's spectrum, its weights
                                       and a row (2 code_bins +
                                       lag_bins); made on first use. */

  size_t nc_count; /**< Coherent dumps in the current look (0…n_noncoh-1).*/
  int dsl_ready; /**< 1 once dp_correlator_shape_t::dsl holds its value: it is
                      computed on its first read, dp_correlator_dsl_db(), not
                      by the constructor -- a sweep of the ambiguity
                      that only a peak-list consumer reads. */
  float _Complex *rate_best; /**< n_surf cells: the best hypothesis' surface
                                  while the others are searched.       */
  size_t rate_best_cap;      /**< Cells rate_best holds.                  */
  size_t rate_best_h;  /**< Winning hypothesis of the last dwell.       */
  size_t blk_epoch;    /**< Epochs gathered in the current block
                            (0 … coherent_bins-1).                    */
  struct dp__dd_map *sl_map; /**< dp_correlator_sidelobe_ratio()'s transforms:
                                  the one delay-Doppler map at one
                                  epoch, code_bins in and lag_bins
                                  out, made on its first call      */
  struct dp__dd_map *sl_map_cb; /**< ... and one at code_bins out, for
                                     the delayed replica           */
  float _Complex *sl_scratch; /**< its buffers: the replica's spectrum,
                                   a mixed signal, its spectrum, the
                                   delayed replica, the spectrum's
                                   conjugate and ones (code_bins
                                   each), then the row (lag_bins) */
  double *sl_cmax; /**< ... its cache: DP_CORRELATOR_SIDELOBE_SLOTS rows of
                        lag_bins, each one hit's worst ratio at every
                        column as read in one tile. Derived, never in a
                        blob. */
  size_t sl_crow[DP_CORRELATOR_SIDELOBE_SLOTS]; /**< each slot's hit: its row,   */
  size_t sl_ccol[DP_CORRELATOR_SIDELOBE_SLOTS]; /**< its column,                 */
  double sl_ctile[DP_CORRELATOR_SIDELOBE_SLOTS]; /**< and the tile centre, Hz    */
  size_t sl_cn;    /**< slots filled                                  */
  size_t sl_cnext; /**< the slot a full cache overwrites next        */
  uint64_t samples_consumed; /**< Total framed samples (the state's offset).   */

  uint64_t surface_at; /**< samples_consumed of that dwell; 0 = none      */
  uint64_t dwells;     /**< dwells decided since create/reset             */
  size_t   n_held;     /**< the last dwell's picks held as twins (§7.1)  */
  float    peak_conc;  /**< the strongest pick's column concentration     */
} dp_correlator_search_t;

/**
 * @brief Correlator state, every flavor.
 *
 * Allocate with a dp_correlator_create_*() constructor; never
 * stack-allocate.  All heap buffers are ``ny * nx`` complex floats stored in
 * row-major order.
 */
typedef struct {
  dp_correlator_flavor_t flavor; /**< The constructor that built it.      */
  dp_fft2d_state_t *fwd;       /**< Forward 2-D plan (sign = -1) at (ny, nx).
                                 NULL when @ref fast_path.                  */
  dp_fft2d_state_t *inv;       /**< Inverse 2-D plan (sign = +1) at (ny_out,…).
                                 NULL when @ref fast_path.                  */
  float _Complex *ref_spec;  /**< conj(FFT2(ref)), pre-computed.  (ny, nx).
                                 NULL when @ref fast_path (see row_ref_spec). */
  float _Complex *work_fft;  /**< Scratch: FFT(in)·ref_spec product.  (ny,nx)
                                 either path — fast path reinterprets this
                                 as ny independent length-nx row spectra.   */
  float _Complex *accum;     /**< Coherent product-spectrum accumulator, same
                                 (ny,nx)/reinterpretation rule as work_fft.  */
  /* Decoupled-inverse scratch — allocated only when (ny_out,nx_out) differ
   * from (ny,nx); NULL on the native path.  General path: zeropad goes
   * accum -> ztmp (rows) -> work_pad (cols), then inverse(work_pad).  Fast
   * path (nx_out != nx only, ny_out == ny is required for fast_path at all):
   * zeropad goes accum -> work_pad directly, one row at a time, via
   * dp_fft_pad_spectrum(); ztmp/zcol/zcolout are unused (2-axis-pad only). */
  float _Complex *work_pad;  /**< Zero-padded product, (ny_out, nx_out) or,
                                 fast path, (ny, nx_out).                   */
  float _Complex *ztmp;      /**< Row-padded intermediate, (ny, nx_out).
                                 General path only.                        */
  float _Complex *zcol;      /**< Column gather scratch, (ny). General path
                                 only.                                      */
  float _Complex *zcolout;   /**< Column-padded scratch, (ny_out). General
                                 path only.                                 */
  /* Single-row-reference fast path (see the file doc comment for the
   * identity this relies on).  fast_path is decided once at create() and
   * fixed for the object's lifetime; set_ref() may only refresh within the
   * same mode (see dp_correlator_set_ref doc comment). */
  int             fast_path;    /**< 1 if using the 1-D-per-row fast path.   */
  dp_fft_state_t    *fwd1d;         /**< Forward 1-D plan, length nx.  Fast only.*/
  dp_fft_state_t    *inv1d;         /**< Inverse 1-D plan, length nx_out.  Fast
                                      only.                                 */
  float _Complex  *row_ref_spec;  /**< conj(FFT_nx(ref row 0)), length nx.
                                      Fast-path replacement for ref_spec.   */
  size_t ny;                /**< Row count.                               */
  size_t nx;                /**< Column count.                            */
  size_t n;                 /**< ny * nx — total element count.           */
  size_t ny_out;            /**< Output rows (== ny unless decoupled).    */
  size_t nx_out;            /**< Output columns (== nx unless decoupled). */
  size_t n_out;             /**< ny_out * nx_out — output element count.  */
  size_t dwell;             /**< Integration depth.                       */
  /* Known-column output (see dp_correlator_create_2d's @p col_out).  A caller that
   * already knows the correlation lag it wants does not need the other
   * nx_out-1 columns, and evaluating the inverse at one bin is a dot
   * product against the conjugated reference, with NO transform in either
   * direction -- O(nx) per row against the full map's O(nx log nx), which
   * is what makes the kernel affordable for a caller that would otherwise
   * hand-roll the lag sum beside it. */
  int             col_out;   /**< Output column, or < 0 for the full map. */
  float _Complex *col_ref;   /**< conj(ref row 0) in the TIME domain, length
                                  nx; NULL when col_out < 0.              */
  /** Bounce buffer for a dump into an under-sized `out` (jm gh-138).
   *  Both execute paths -- the 2-D inverse and the per-row fast path --
   *  write the full n_out surface, so a short `out` is served by writing
   *  here and copying the prefix. Allocated lazily; the sized path never
   *  touches it. */
  float _Complex *work_trunc;
  /* Worker fan-out (#2666).  The fast path's per-row stage and each stage
   * of the delay-Doppler flavor's map are each a set of independent
   * transforms, run over `nworkers` contiguous chunks on `pool`.  A pocketfft
   * plan keeps scratch of its own, so it cannot be shared between threads:
   * every worker owns one plan per stage, and chunk t always runs on plan t,
   * which is what keeps the output bit-identical across thread counts. */
  int              nworkers;  /**< Chunks per stage (1 = serial).          */
  dp_pool_t       *pool;      /**< Persistent workers; NULL when serial.  */
  dp_fft_state_t **row_fwd;   /**< Per-worker forward row plans, length
                                   nx; worker 0's is @ref fwd1d.  Fast
                                   path only.                             */
  dp_fft_state_t **row_inv;   /**< Per-worker inverse row plans, length
                                   nx_out; worker 0's is @ref inv1d.  Fast
                                   path only.                             */
  /* Delay-Doppler flavor (dp_correlator_create_delay_doppler; `flavor` is
   * DP_CORRELATOR_DELAY_DOPPLER, the one record of it, #2901). The flavor
   * is the fast path's accumulation plus the library's one delay-Doppler
   * map at dump time (docs/design/acq-surface.md §18); it is fixed at
   * create() and changes what the state blob means, so the blob carries
   * its own magic. */
  double           doppler_step;  /**< Doppler spacing between adjacent
                                       output rows, cycles/sample:
                                       1 / (ny_out * nx).  0 for the 2-D map.   */
  struct dp__dd_map *dd_map;  /**< The one delay-Doppler block map
                                   (docs/design/acq-surface.md §18.1),
                                   one slot per worker.  Delay-Doppler
                                   only; internal.                        */
  float _Complex  *dd_fine;   /**< The map's scratch when it runs whole
                                   (slow first, or one slow row);
                                   NULL when it runs rows first, fused
                                   into the row stage.  Delay-Doppler.    */
  size_t          *dd_rows;   /**< Output row of slow row i: the
                                   centered (i + ny_out/2) mod ny_out.    */
  int              dd_fused;  /**< 1 when the map runs rows first (its
                                   rule at one tile, one rate): the dump
                                   frame's rows are inverted as they are
                                   accumulated.                           */
  /* Batch scratch (dp_correlator_execute_fold and the typed batch calls),
   * allocated on the first batch, one slot per worker.  A batch makes its
   * maps here and never touches accum or count, so it is not state and is
   * not in the blob. */
  float _Complex  *bat_acc;   /**< Per-worker accumulators, nworkers x n;
                                   zero between maps.                    */
  float _Complex  *bat_work;  /**< Per-worker forward spectra, nworkers x n. */
  float _Complex  *bat_pad;   /**< Per-worker row zero-pad, nworkers x ny x
                                   nx_out; fast path with nx_out > nx only. */
  float _Complex  *bat_map;   /**< Per-worker maps, nworkers x n_out.    */
  float _Complex  *bat_fine;  /**< Per-worker @ref dd_fine for batches;
                                   NULL when the map runs rows first.     */
  /* Detection layer (the detector flavors, docs/design/correlator.md
   * section 4): what create sets and push only reads.  Zero / NULL in the
   * map flavors.  Appended here, after every field the map reads, so that
   * no field the threaded map was measured at moved (#2752 step 3.3). */
  dp_f32_t       *ring;          /**< The carry's storage: bound to
                                      @ref framer, freed by destroy.       */
  size_t          ring_cap;      /**< Ring capacity, complex samples.     */
  float _Complex *det_out;       /**< The last dump's correlation map,
                                      n complex samples.                  */
  float          *mag_buf;       /**< |det_out|, n floats.                */
  float          *noise_scratch; /**< Scratch for the median's sort.      */
  size_t          noise_lo;      /**< Noise bin range, first (inclusive). */
  size_t          noise_hi;      /**< Noise bin range, last (inclusive).  */
  det_noise_mode_t noise_mode;   /**< Noise aggregation.                  */
  /* The search flavors' (#2752 3.4b): NULL / 0 in every other flavor. */
  size_t      max_peaks; /**< Peak-list capacity per dwell (1 = the maximum
                              only); dp_correlator_set_max_peaks().      */
  det_peak_t *peaks;     /**< max_peaks: the last dwell's picks.         */
  uint8_t    *band_mask; /**< n_surf; 1 = outside the searched band.     */
  uint8_t    *peak_mask; /**< n_surf; the working mask.                  */
  uint32_t   *twin_row;  /**< max_peaks: last dwell's picks, native rows. */
  uint32_t   *twin_col;  /**< max_peaks: last dwell's picks, code phases. */
  dp_correlator_search_t *search; /**< The search policy: @ref search_rec
                                       in the search flavors, NULL in the
                                       other five.                      */
  /* Everything above is written by create (and set_ref) only, and the
   * threaded map's workers read it on every call. `count` is the one field
   * execute() writes every frame, so it lives behind a full cache line of
   * padding: sharing a line with `n` (as it did once `flavor` was added
   * first) made every frame invalidate that line in each worker's cache
   * (#2886). A detector flavor's push writes the fields after it every
   * call, and a caller may set the threshold every push (carrier_acq
   * does), so they sit with `count` (#2752 step 3.3). A new field goes
   * ABOVE this comment unless execute() or push() writes it, in which case
   * it goes below. */
  unsigned char _hot_pad[64]; /**< Keeps @ref count off the read fields'
                                   cache lines.                            */
  size_t count;             /**< Frames accumulated (0 … dwell-1).        */
  /* Detection layer, written by every push (detector flavors only). */
  dp_f32_framer_t framer;    /**< Any chunk in, n-sample frames out.      */
  float  threshold;          /**< 0 = always fire; > 0 gates test_stat.  */
  size_t peak_row;           /**< Last dump: peak row (0 before one).    */
  size_t peak_col;           /**< Last dump: peak column (the lag at one
                                  row).                                   */
  float  peak_mag;           /**< Last dump: peak |R|.                   */
  float  noise_est;          /**< Last dump: the noise floor.            */
  float  test_stat;          /**< Last dump: peak_mag / noise_est, or 0. */
  int    _last_corr_valid;   /**< 1 once a dump has filled det_out; 0
                                  after create, reset and set_state.     */
  size_t consumed;           /**< Input samples the last push took.      */
  size_t n_peaks;            /**< Search flavors: picks in the last dwell,
                                  held ones too.                         */
  size_t n_twins;            /**< Search flavors: picks carried over from
                                  the last dwell.                        */
  /* The search flavors' policy record, embedded rather than allocated
     (#2752 step 3.4b, docs/design/correlator.md section 7): behind a
     pointer to its own allocation, a small burst push ran 3% slower than
     acq's (bench-ab twice, then aligned with heap placement pinned), and
     here it ties. Last, so no field 3.2 or 3.3 measured moves; every
     flavor carries its 1.5 KB, unused outside the search. */
  dp_correlator_search_t search_rec; /**< @ref search points here in a
                                          search flavor.                */
} dp_correlator_state_t;

/**
 * @brief Which constructor built @p state: what dp_correlator_create()
 *        chose from its knobs, so a caller can ask before reading a member
 *        only some flavors serve.
 * @param state  Must be non-NULL.
 * @return The flavor, fixed at create.
 */
dp_correlator_flavor_t
dp_correlator_flavor (const dp_correlator_state_t *state);

/**
 * @brief Allocate a 1-D FFT correlator with coherent integrate-and-dump.
 * Pre-computes conj(FFT(ref)) once at construction so each execute()
 * call costs only two FFTs and n complex multiplies.  @p ref may be freed
 * after this returns.  With @p dwell == 1 every call produces output; with
 * larger values the accumulator absorbs @p dwell frames before dumping.
 *
 * It is dp_correlator_create_2d() at ny = 1, nx = @p ref_len: the
 * single-row path, so the same bits (test_corr_equiv).  Its frame and its
 * map are both 1-D: execute() takes @p ref_len samples and a dump writes
 * n_out.
 *
 * @param ref       Reference signal, CF32, length @p ref_len.
 * @param ref_len   Reference / FFT length in samples.
 * @param dwell     Integration depth; must be >= 1 (0 is refused).  Pass 1
 *                  for immediate output on every call.
 * @param nthreads  Accepted for API compatibility; ignored.
 * @param n_out     Inverse/output length; 0 => native (n).  Must be >= n.  A
 *                  larger value zero-pads the cross-spectrum before the inverse,
 *                  returning the band-limited (Dirichlet) interpolation of the
 *                  correlation on a finer length-n_out grid — same peak, sub-bin
 *                  lag resolution.  Native is bit-exact and allocates no extra
 *                  buffer.
 * @return Heap-allocated state (free with dp_correlator_destroy()), or NULL
 *         on a dwell of 0, an @p n_out below @p ref_len, or allocation
 *         failure.
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
 * >>> corr = Correlator(ref=ref, dwell=1, nthreads=1)
 * >>> corr.n, corr.dwell, corr.count
 * (4, 1, 0)
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_1d (const float _Complex *ref,
                                                size_t ref_len, size_t dwell,
                                                int nthreads, size_t n_out);

/**
 * @brief Allocate a 2-D FFT correlator with coherent integrate-and-dump.
 * Two-dimensional extension of dp_correlator_create_1d().  The reference is a flat
 * row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once
 * so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies.
 * The Python wrapper requires @p ref to be a 2-D ndarray with shape
 * (ny, nx); it passes a flat view to C.
 *
 * @param ref       Reference image, 2-D (ny, nx) CF32 ndarray in Python.
 * @param ny        Number of rows in the reference and input frames.
 * @param nx        Number of columns in the reference and input frames.
 * @param dwell     Integration depth; must be >= 1.
 * @param nthreads  Workers for the single-row-reference fast path's per-row
 *                  stage: 1 runs serially (no threads), N runs N workers
 *                  (the caller included), <= 0 uses every online core.  The
 *                  count is capped at @p ny, one row per worker at most.
 *                  The output is bit-identical whatever the count.  The
 *                  general 2-D path and the @p col_out path run serially and
 *                  ignore it.
 * @param ny_out    Inverse/output rows; 0 => native (ny).  Must be >= ny.  A
 *                  larger output zero-pads the cross-spectrum before the
 *                  inverse, returning the band-limited (Dirichlet) interpolation
 *                  of the correlation on a finer (ny_out, nx_out) grid — same
 *                  peak, sub-bin resolution.  Native is bit-exact and allocates
 *                  no extra buffers.
 * @param nx_out    Inverse/output columns; 0 => native (nx).  Must be >= nx.
 * @param col_out   Emit ONLY this correlation lag, or < 0 for the whole map.
 *                  When set, a dump writes @p ny values -- one per row --
 *                  instead of ny*nx_out, computed as the time-domain sum
 *                  over p of `conj(ref(p)) * row((p + col_out) mod nx)`,
 *                  which is what R(i, col_out) expands to once the 1/nx
 *                  cancels.
 *                  No transform runs in either direction, so the cost per
 *                  row is O(nx) rather than O(nx log nx).  Requires the
 *                  single-row-reference fast path (a caller that knows its
 *                  lag is by construction correlating against a code
 *                  replica) and the NATIVE output grid: an interpolated
 *                  column is a fractional lag, which no time-domain sum
 *                  produces.  create() returns NULL if @p col_out >= 0 with
 *                  a reference the fast path rejects, with @p ny_out or
 *                  @p nx_out decoupled, or with a lag outside 0 .. nx-1.
 * @return Heap-allocated state, or NULL on failure.
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Correlator(ref=ref, dwell=1, nthreads=1)
 * >>> c.ny, c.nx, c.dwell, c.count
 * (4, 4, 1, 0)
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_2d(const float _Complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out, int col_out);

/**
 * @brief Allocate the delay-Doppler flavor: a code correlator that maps a raw
 * stream of @p ny code periods onto (Doppler, delay).
 *
 * Input to dp_correlator_execute() is the raw stream -- @p ny periods of
 * @p code_len samples, back to back, no reshaping or reference padding by the
 * caller.  Each period (row) is correlated against @p code exactly as
 * the 2-D map's single-row fast path does (FFT, multiply by `conj(FFT(code))`,
 * coherent @p dwell accumulation of the product spectrum, inverse scaled by
 * `1/nx`), and on the dump the slow-time FFT runs down every delay column.
 * The library's one delay-Doppler map computes it (docs/design/acq-surface.md
 * §18) in the order its rule picks: here the rows first, each row inverted
 * as it is accumulated, which is what makes the row stage fused.
 * In numpy terms, for one frame `x` (dwell 1):
 *
 *     C   = np.conj(np.fft.fft(code))
 *     R   = np.fft.ifft(np.fft.fft(x.reshape(ny, nx), axis=1) * C, axis=1)
 *     map = np.fft.fftshift(np.fft.fft(R, n=ny_out, axis=0), axes=0)
 *
 * The output is `ny_out x nx` row-major: rows are Doppler bins, centered
 * (zero Doppler on row `ny_out / 2`), columns are delay lags.  A signal of
 * amplitude `a` that is the code delayed by `d` samples and rotated by `f`
 * cycles/sample peaks at column `d` and row `ny_out/2 + f / doppler_step`,
 * where @ref dp_correlator_state_t::doppler_step is `1 / (ny_out * nx)`
 * cycles/sample (times the sample rate for Hz).  Its peak is
 * `a * dwell * ny * sum(abs(code)**2)`: the 1/nx of the row inverse
 * cancels the row correlation's own gain of nx, and the slow-time FFT is
 * unnormalized, as numpy's is, so it sums the ny rows coherently.
 *
 * `ny_out > ny` zero-pads the slow-time samples after the ny rows: the
 * ny_out-point DFT then samples the same Doppler response on a grid ny_out/ny
 * times finer (band-limited interpolation).  The pad adds no energy, so the
 * peak at a bin center is unchanged and a Doppler between native bins is
 * found at its fractional row.
 *
 * Both stages fan out over @p nthreads workers -- the rows, then the
 * columns -- each worker on its own FFT plans, and the result is
 * bit-identical for every thread count.
 *
 * @param code      The code replica, @p code_len samples (one period).
 * @param code_len  Samples per period, nx; must be >= 1.
 * @param ny        Periods per frame, the slow-time length; must be >= 1.
 * @param dwell     Frames coherently accumulated per dump; must be >= 1.
 * @param nthreads  Workers: 1 serial, N for N (the caller included), <= 0
 *                  every online core; capped at the most chunks a stage
 *                  can use, `max(ny, ceil(code_len / 16))`.
 * @param ny_out    Doppler bins in the map; 0 means @p ny.  Must be >= @p ny.
 * @return Heap-allocated state (free with dp_correlator_destroy()), or NULL if
 *         @p code is NULL, @p code_len or @p ny is 0, @p dwell is 0, or
 *         @p ny_out is below @p ny.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d, k = 16, 64, 5, 3       # delay 5, Doppler 3 bins
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> n = np.arange(ny * nx)
 * >>> f = k / (ny * nx)                 # cycles/sample
 * >>> x = (np.tile(np.roll(code, d), ny)
 * ...      * np.exp(2j * np.pi * f * n)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny, dwell=2)
 * >>> out = np.empty(dd.execute_max_out(), np.complex64)
 * >>> for _ in range(2):                # one preallocated map per dump
 * ...     m = dd.execute(x, out=out)
 * >>> m is not None and m.base is out
 * True
 * >>> row, col = np.unravel_index(np.argmax(np.abs(out)), (ny, nx))
 * >>> int(row) - ny // 2, int(col)      # (Doppler bins, delay)
 * (3, 5)
 * >>> dd.doppler_step == 1 / (ny * nx)
 * True
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_delay_doppler (const float _Complex *code,
                                                   size_t code_len, size_t ny,
                                                   size_t dwell, int nthreads,
                                                   size_t ny_out);

/** @brief Destroy and free a correlator, any flavor.  @param state May be NULL. */
void dp_correlator_destroy(dp_correlator_state_t *state);

/**
 * @brief Restart the correlator's running state, keeping how it was built.
 *
 * What each flavor drops; the reference, its spectrum, the FFT plans and
 * every knob stay:
 *
 * - a map: zeroes the accumulator and the integration counter, a fresh
 *   dwell cycle;
 * - a detector: that, plus the carry (the partial frame held between
 *   pushes) and the last dump's fields, so it matches a fresh detector
 *   built with the same arguments; last_corr reads None until the next
 *   dump;
 * - a search: drains the input ring and clears the non-coherent power
 *   accumulator and the dwell bookkeeping, so the next push() begins a
 *   fresh search; the grid and thresholds are untouched.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Correlator(ref=ref, dwell=3)
 * >>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
 * >>> c.count
 * 1
 * >>> c.reset()
 * >>> c.count
 * 0
 * @endcode
 */
void dp_correlator_reset(dp_correlator_state_t *state);

/**
 * @brief Replace the reference and recompute its spectrum.
 *
 * Also resets accumulator and counter on success.  The object's fast-path
 * mode (see the file doc comment) is fixed at create() time and never
 * changes here: on a fast-path object, @p ref must still be nonzero only in
 * row 0, or the call is rejected and the object's existing reference and
 * spectrum are left completely untouched (never a silent partial update) —
 * a caller that needs a genuinely different reference shape must build a
 * new dp_correlator_state_t.  A general-path object accepts any (ny,nx) @p ref and
 * always succeeds.  A detector flavor resets first (see
 * dp_correlator_reset()), so it is reset even when the reference is then
 * refused.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 * Served by the 1-D and 2-D maps and both detectors (docs/design/
 * correlator-python.md section 18); the delay-Doppler map and the searches
 * refuse it, touching nothing. A detector is reset first, so it is reset
 * even when its new reference is then refused (DP_ERR_INVALID).
 *
 * @return 0 on success; DP_ERR_UNSUPPORTED on the delay-Doppler map or a
 *         search; DP_ERR_INVALID if @p state is fast-path and @p ref is no
 *         longer single-row.
 */
int dp_correlator_set_ref(dp_correlator_state_t *state, const float _Complex *ref);

/** @brief Maximum output samples per execute call (@ref dp_correlator_state_t::n_out
 *         -- ny_out*nx_out normally, or ny when a single @ref
 *         dp_correlator_state_t::col_out was selected).
 *  @param state  Must be non-NULL.
 *
 * 0 on a detector or search flavor, which execute() refuses.
 *  @return n_out, fixed at create(). */
size_t dp_correlator_execute_max_out(dp_correlator_state_t *state);

/**
 * @brief Correlate one 2-D frame and optionally dump the coherent accumulator.
 * Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → accumulate
 * the cross-spectrum; on dump, IFFT2 → normalise (÷ ny*nx).  Accumulating in
 * the frequency domain and inverting once is exactly the per-frame inverse
 * summed, by linearity of the IFFT — valid because the dwell is **coherent**
 * (a complex sum); a non-coherent (magnitude) integration could not defer the
 * inverse.
 *
 * **Layout.**  A frame is ny*nx complex samples in row-major order: row i is
 * samples i*nx .. (i+1)*nx - 1.  For the 2-D map that is a (ny, nx) image;
 * for the delay-Doppler map it is the raw stream, ny code periods of nx = code_len samples
 * back to back.  The map a dump produces is flat, n_out = ny_out * nx_out
 * values (ny values with @p col_out), also row-major: reshape it to
 * (ny_out, nx_out).  On the delay-Doppler map, axis 0 is Doppler (centered, zero
 * Doppler on row ny_out / 2) and axis 1 is delay (the lag, in samples).
 *
 * **Ownership.**  In Python, a call that dumps returns a new array when no
 * `out=` is given, and a view of `out=` when one is (a 1-D array holding at
 * least n_out values; it is written on a dump only, and the next dump
 * overwrites it).  A call that does not dump returns None and touches
 * neither.
 *
 * @param state  Allocated 2-D correlator (non-NULL).
 * @param in     Input frame, flat row-major CF32, length ny*nx.
 * @param n_in   Number of input samples; must equal ny*nx; else REFUSED.
 * @param out    Output buffer for the correlation map (CF32, length n_out:
 *               ny_out*nx_out, or ny with @p col_out); written only on a
 *               dump call.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the map on a dump, None otherwise; a frame that is not
 *         ny*nx samples is REFUSED with nothing read, written or counted, as a
 *         ValueError (#1925).  C: n_out on a dump (or @p max_out if smaller),
 *         0 otherwise, and SIZE_MAX for the refused frame.
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Correlator(ref=ref, dwell=2)
 * >>> x = np.ones((2, 2), dtype=np.complex64)
 * >>> c.execute(x) is None   # frame 1 — no dump
 * True
 * >>> c.execute(x).tolist()  # frame 2 — dump
 * [(2+0j), (2+0j), (2+0j), (2+0j)]
 * @endcode
 */
size_t dp_correlator_execute(dp_correlator_state_t *state, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out, const char **why);

/* ── Batches: many frames in one call (#2683) ──────────────────────────────
 *
 * A batch is H frames back to back, n_in = H * ny * nx: frame h is samples
 * h*ny*nx .. (h+1)*ny*nx - 1, each laid out as execute() takes it.  What a
 * hypothesis or a frame IS, is the caller's: the batch only knows H.  Each
 * map is n_out values (reshape to (ny_out, nx_out); Doppler is axis 0 and
 * delay axis 1 on the delay-Doppler map), and map h is row h of the (H, n_out)
 * result.  Map h is exactly
 * what a fresh object with dwell 1 returns for frame h -- the same
 * arithmetic, bit for bit, on every path (general 2-D, single-row fast
 * path, col_out, delay-Doppler) and at every thread count.  A batch never
 * reads or writes the dwell: accum, count and the state blob are the same
 * after it as before, so a batch may run in the middle of a dwell.
 *
 * Each typed call folds the H maps into a caller-owned result, in h order.
 * Every one refuses, with nothing written and SIZE_MAX returned, an empty
 * batch (n_in == 0), a length that is not a whole number of frames, or an
 * output with room for less than the whole result: a truncated reduction
 * has no meaning, so a batch never truncates the way execute() does.
 *
 * Threads: with two frames or more, each worker makes whole maps on its own
 * plans and scratch, and the maps are folded in h order, because splitting
 * one short frame across a pool costs more than it saves
 * (corr2d-hypotheses.md section 5).  A batch of one frame is split as
 * execute() splits it.  The coherent sum gives each worker a fixed range of
 * rows through every frame.  The result is the same at every thread
 * count. */

/**
 * @brief A C reducer: called once per map, in h order, by
 *        dp_correlator_execute_fold().
 *
 * @p map holds @p n_out values laid out as execute() writes them, and is
 * valid only for the duration of the call.
 */
typedef struct
{
  /** Fold map @p h into the result @p ctx names. */
  void (*fold) (void *ctx, size_t h, const float _Complex *map, size_t n_out);
  void *ctx; /**< Passed to every call of @ref fold. */
} dp_correlator_fold_t;

/**
 * @brief Make the map of every frame in a batch and hand each to a reducer.
 *
 * The one loop every typed batch call runs on.  Maps are made as described
 * above and passed to `fold->fold(fold->ctx, h, map, n_out)` for h = 0 ..
 * H-1, in that order, from the calling thread.
 *
 * @param state  Must be non-NULL.
 * @param in     H frames, each ny * nx samples (CF32), back to back.
 * @param n_in   Samples in @p in: a nonzero multiple of ny * nx.
 * @param fold   The reducer; must be non-NULL.
 * @return H, or SIZE_MAX (nothing folded) when @p n_in is 0 or not a
 *         multiple of the frame.
 * @code
 * #include "doppler/correlator/correlator_core.h"
 * #include <stdio.h>
 *
 * // A reducer: the energy of each map, one value per frame.
 * static void
 * energy (void *ctx, size_t h, const float _Complex *m, size_t n_out)
 * {
 *   double e = 0.0;
 *   for (size_t k = 0; k < n_out; k++)
 *     e += crealf (m[k]) * crealf (m[k]) + cimagf (m[k]) * cimagf (m[k]);
 *   ((double *)ctx)[h] = e;
 * }
 *
 * int
 * main (void)
 * {
 *   const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 *   dp_correlator_state_t   *dd
 *       = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1, 0);
 *   float _Complex frames[3 * 2 * 4]; // 3 frames of ny x nx = 2 x 4
 *   for (size_t k = 0; k < 3 * 2 * 4; k++)
 *     frames[k] = code[k % 4] * (float)(1 + k / 8); // frame h is (h+1) x
 *   double           e[3];
 *   dp_correlator_fold_t f = { energy, e };
 *   size_t           H = dp_correlator_execute_fold (dd, frames, 3 * 2 * 4, &f);
 *   printf ("%zu maps, energy ratios %.0f %.0f\n", H, e[1] / e[0],
 *           e[2] / e[0]); // 3 maps, energy ratios 4 9
 *   dp_correlator_destroy (dd);
 *   return !(H == 3 && e[1] > 3.99 * e[0] && e[1] < 4.01 * e[0]);
 * }
 * @endcode
 */
size_t dp_correlator_execute_fold (dp_correlator_state_t *state,
                               const float _Complex *in, size_t n_in,
                               const dp_correlator_fold_t *fold);

/**
 * @brief Samples dp_correlator_execute_all() writes for @p n_in input samples:
 *        H * n_out, H = n_in / (ny * nx).
 * @param state  Must be non-NULL.
 * @param n_in   Input samples about to be passed.
 * 0 on a detector or search flavor, which execute_all() refuses.
 *
 * @return The capacity `execute_all` needs for @p n_in samples: H * n_out
 *         values (C: SIZE_MAX if that overflows).
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32
 * 512
 * @endcode
 */
size_t dp_correlator_execute_all_max_out (const dp_correlator_state_t *state,
                                      size_t n_in);

/**
 * @brief Every map of a batch, unreduced: row h of the (H, n_out) result is
 *        the map of frame h (in C, map h is at `out + h * n_out`).
 *
 * Python: `x` is the H frames back to back, a flat array of H * ny * nx
 * samples (H >= 1): frame h is the samples from h * ny * nx up to, not
 * including, (h + 1) * ny * nx.  With no `out=` each call returns a new
 * array; with `out=` it returns a view of it, and the next call into the same
 * `out=` overwrites that view.  `out=` is a 2-D complex64 array of shape
 * (rows, n_out) with at least H rows (a shorter one is refused); the result is
 * its first H rows.  The object's dwell is untouched.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      H * n_out values (CF32).
 * @param max_out  Capacity of @p out; at least
 *                 dp_correlator_execute_all_max_out().
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the (H, n_out) array; a refused batch (empty, not a whole
 *         number of frames, or an `out=` with fewer than H rows) is a
 *         ValueError with nothing written.  C: H * n_out, or SIZE_MAX for the
 *         refused batch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> maps = dd.execute_all(frames)
 * >>> maps.shape
 * (2, 256)
 * >>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay
 * [3, 11]
 * >>> dd.count                    # the dwell never moved
 * 0
 * @endcode
 */
size_t dp_correlator_execute_all (dp_correlator_state_t *state,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out, const char **why);

/**
 * @brief Samples dp_correlator_execute_coherent() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_coherent_max_out()
 * 256
 * @endcode
 */
size_t dp_correlator_execute_coherent_max_out (const dp_correlator_state_t *state);

/**
 * @brief The coherent sum of a batch's maps, inverted once.
 *
 * Python: `x` is the H frames back to back, a flat array of H * ny * nx
 * samples (H >= 1).
 *
 * The frames' product spectra are summed and the sum is inverted, as a
 * dwell does: the result is bit-identical to what a fresh object with
 * dwell = H dumps for the same frames, and equal to the sum of the H maps
 * up to rounding.  The object's own dwell is untouched.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out values (CF32).
 * @param max_out  Capacity of @p out; at least n_out.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the one map, a new (n_out,) array; a refused batch is a
 *         ValueError with nothing written.  C: n_out, or SIZE_MAX for the
 *         refused batch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> x = np.tile(code, ny)
 * >>> total = dd.execute_coherent(np.concatenate([x, -x]))
 * >>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel
 * True
 * @endcode
 */
size_t dp_correlator_execute_coherent (dp_correlator_state_t *state,
                                   const float _Complex *in, size_t n_in,
                                   float _Complex *out, size_t max_out, const char **why);

/**
 * @brief Powers dp_correlator_execute_power_sum() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_power_sum_max_out()
 * 256
 * @endcode
 */
size_t dp_correlator_execute_power_sum_max_out (const dp_correlator_state_t *state);

/**
 * @brief The power of a batch's maps, summed cell by cell: each output
 *        cell is the sum over h of that cell's `|map_h|^2`, summed in h
 *        order in float.
 *
 * Python: `x` is the H frames back to back, a flat array of H * ny * nx
 * samples (H >= 1).
 *
 * Unlike the coherent sum, phase does not cancel: frames x and -x sum to
 * twice one map's power, where their coherent sum is zero.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out powers (float).
 * @param max_out  Capacity of @p out; at least n_out.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the (n_out,) float32 powers, a new array; a refused batch
 *         is a ValueError with nothing written.  C: n_out, or SIZE_MAX for the
 *         refused batch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> x = np.tile(code, ny)
 * >>> power = dd.execute_power_sum(np.concatenate([x, -x]))
 * >>> one = np.abs(dd.execute_all(x)[0]) ** 2
 * >>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel
 * True
 * @endcode
 */
size_t dp_correlator_execute_power_sum (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, size_t max_out, const char **why);

/**
 * @brief Values per output dp_correlator_execute_power_max() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 * dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
 *                                                          0);
 * size_t n = dp_correlator_execute_power_max_max_out (dd); // 2 x 4 = 8
 * dp_correlator_destroy (dd);
 * return n != 8;
 * @endcode
 */
size_t dp_correlator_execute_power_max_max_out (const dp_correlator_state_t *state);

/**
 * @brief The largest power over a batch's maps, cell by cell, and the map
 *        it came from.
 *
 * Python: `x` is the H frames back to back, a flat array of H * ny * nx
 * samples (H >= 1).
 *
 * Each cell of @p out is the largest `|map_h|^2` that cell takes over the
 * maps, and the same cell of @p out1 is the h it came from.  A tie goes to
 * the lowest h: a later map replaces the running maximum only when it is
 * strictly greater.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32); H at most 2^32.
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out powers (float).
 * @param out1     n_out map indices (uint32).
 * @param max_out  Capacity of each of @p out and @p out1; at least n_out.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the tuple (power, index), each (n_out,): float32 powers and
 *         uint32 map indices, always new arrays (this call takes no `out=`);
 *         a refused batch, or H above 2**32, is a ValueError.  C: n_out, or
 *         SIZE_MAX with nothing written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> power, index = dd.execute_power_max(frames)
 * >>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks
 * (0, 1)
 * @endcode
 */
size_t dp_correlator_execute_power_max (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, uint32_t *out1,
                                    size_t max_out, const char **why);

/**
 * @brief Records dp_correlator_execute_peaks() writes for @p n_in samples: H.
 * @param state  Must be non-NULL.
 * @param n_in   Input samples about to be passed.
 * 0 on a detector or search flavor, which execute_peaks() refuses.
 *
 * @return n_in / (ny * nx).
 * @code
 * const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 * dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 4, 2, 1, 1,
 *                                                          0);
 * size_t h = dp_correlator_execute_peaks_max_out (dd, 3 * 2 * 4); // 3 frames
 * dp_correlator_destroy (dd);
 * return h != 3;
 * @endcode
 */
size_t dp_correlator_execute_peaks_max_out (const dp_correlator_state_t *state,
                                        size_t n_in);

/**
 * @brief One record per map of a batch: its peak power, where the peak is,
 *        and its mean power.
 *
 * Python: `x` is the H frames back to back, a flat array of H * ny * nx
 * samples (H >= 1).
 *
 * Entry h of each output describes map h: @p out its largest `|map|^2`,
 * @p out1 and @p out2 the row and column of that cell (the first in
 * row-major order on a tie), and @p out3 the mean `|map|^2` over its n_out
 * cells.  A map is ny_out x nx_out; with col_out it is ny x 1, and the
 * column reported is col_out.  The peak-to-mean of each map is @p out over
 * @p out3, entry by entry: a statistic of each map, which the same
 * statistic of a pointwise maximum over maps does not give.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      H peak powers (float).
 * @param out1     H peak rows (uint32).
 * @param out2     H peak columns (uint32).
 * @param out3     H mean powers (float).
 * @param max_out  Capacity of each output; at least H.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the tuple (peak, row, col, mean), each (H,): float32 peak
 *         power, uint32 row and column, float32 mean power, always new arrays
 *         (this call takes no `out=`); a refused batch is a ValueError.  C: H,
 *         or SIZE_MAX with nothing written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> peak, row, col, mean = dd.execute_peaks(frames)
 * >>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2
 * ([4, 4], [3, 11])
 * >>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean
 * True
 * @endcode
 */
size_t dp_correlator_execute_peaks (dp_correlator_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float *out, uint32_t *out1, uint32_t *out2,
                                float *out3, size_t max_out, const char **why);

/* ── The hypothesis bank (#2683 phase 3) ───────────────────────────────── */

/**
 * @brief Run one frame through a bank of carrier and rate hypotheses and hand
 *        each map to a reducer.
 *
 * A bank is what a frame batch of corrected copies would compute, without
 * the copies: carrier g corrects an offset of `g / (L * nx)` cycles/sample,
 * for every g from @p g_min to @p g_max, and each rate r weights the
 * slow-time samples by the r-th ny-long vector of @p rates.  Map h, for
 * `h = (g - g_min) * R + r` (R = `rates_len / ny`, or 1 with no rates), is
 * what a fresh dwell-1 object's execute() returns for the frame with period
 * i multiplied by element i of rate vector r and by
 * `exp(-j 2 pi g n / (L nx))`, where n
 * counts samples WITHIN period i (0 .. nx-1), up to float rounding.
 *
 * The correction restarts at every period on purpose: the phase a carrier
 * adds between periods is slow-time Doppler, which the slow-time FFT
 * measures, so a signal stays on its own absolute Doppler row under every
 * carrier hypothesis (docs/design/corr2d-hypotheses.md, rule 4).
 *
 * Every path runs a bank, and only the single-row fast path and the delay-Doppler map
 * share work: whole bins (g a multiple of L) are a roll of the row spectrum,
 * each fraction `g mod L` mixes and transforms the rows once for every whole
 * bin that shares it.  On the delay-Doppler map the one map then runs
 * in the order its rule picks for the bank's carriers per fraction and its
 * rates (docs/design/acq-surface.md §18.1):
 * rows first inverts each carrier's rows once for all its rates and pays a
 * slow-time FFT per rate; slow first shares each fraction's slow time across
 * its whole-bin carriers and pays their rows per rate.  On the 2-D map, whose
 * rows are its periods, a rate scales the carrier's rows.  The general 2-D
 * path and a known column run each hypothesis's corrected frame through
 * their own frame code, which is the definition with nothing shared.
 *
 * Maps reach @p fold in h order, so a reducer's tie rule is the batch's.  The
 * stages split across the object's workers exactly as execute() does, and
 * every map is bit-identical for any worker count.  The dwell accumulator,
 * `count` and the state blob are never read or written.
 *
 * @param state      A map flavor (1-D, 2-D or delay-Doppler).
 * @param x          One frame, @p x_len samples.
 * @param x_len      Must be exactly `ny * nx`.
 * @param L          Fractions per whole bin; at least 1.
 * @param g_min      Lowest carrier, in units of `1 / (L * nx)` cycles/sample.
 * @param g_max      Highest carrier; at least @p g_min.
 * @param rates      Per-period weights, R vectors of ny, or NULL with
 *                   @p rates_len 0 (one unit vector).
 * @param rates_len  A whole multiple of ny, 0 included.
 * @param fold       The reducer, called once per map.
 * @return The number of maps, `(g_max - g_min + 1) * R`; or SIZE_MAX, with
 *         nothing folded, for a frame of another length, `L == 0`,
 *         `g_min > g_max`, a @p rates_len that is not a whole number of
 *         ny-long vectors, a count past 2^32, or scratch that cannot be
 *         allocated.
 * @code
 * #include "doppler/correlator/correlator_core.h"
 * #include <stdio.h>
 * static void count_maps (void *ctx, size_t h, const float _Complex *map,
 *                         size_t n_out)
 * {
 *   (void)h, (void)map, (void)n_out;
 *   ++*(size_t *)ctx;
 * }
 * int main (void)
 * {
 *   float _Complex code[8] = { 1, -1, 1, 1, -1, 1, -1, -1 }, x[4 * 8] = { 0 };
 *   dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (code, 8, 4, 1, 1, 0);
 *   size_t maps = 0;
 *   dp_correlator_fold_t f = { count_maps, &maps };
 *   size_t h = dp_correlator_bank_fold (dd, x, 4 * 8, 4, -4, 4, NULL, 0, &f);
 *   printf ("%zu %zu\n", h, maps); // 9 carriers, one rate: 9 maps
 *   dp_correlator_destroy (dd);
 *   return !(h == 9 && maps == 9);
 * }
 * @endcode
 */
size_t dp_correlator_bank_fold (const dp_correlator_state_t *state, const float _Complex *x,
                            size_t x_len, size_t L, int g_min, int g_max,
                            const float _Complex *rates, size_t rates_len,
                            const dp_correlator_fold_t *fold);

/**
 * @brief Capacity dp_correlator_bank_power_max() needs: one map.
 * @param state  The correlator.
 * @param x_len  The frame's length (one map whatever it is).
 * @return `n_out`.
 */
size_t dp_correlator_bank_power_max_max_out (dp_correlator_state_t *state,
                                         size_t x_len);

/**
 * @brief The strongest power at every cell over a bank, and which
 *        hypothesis it came from.
 *
 * dp_correlator_bank_fold() with the `POWER_MAX` reducer: at each cell, @p out
 * holds the largest |map_h|^2 over every hypothesis h, and @p out1 that h,
 * `(g - g_min) * R + r`, the lowest h on a tie.  Every argument and refusal
 * is dp_correlator_bank_fold()'s; in addition @p max_out below `n_out` is
 * refused with nothing written.
 *
 * @param state      The map; a detector is refused.
 * @param x          One frame, @p x_len samples.
 * @param x_len      Must be exactly `ny * nx`.
 * @param L          Fractions per whole bin; at least 1.
 * @param g_min      Lowest carrier, in units of `1 / (L * nx)` cycles/sample.
 * @param g_max      Highest carrier; at least @p g_min.
 * @param rates      Per-period weights, R vectors of ny, or empty (NULL with
 *                   @p rates_len 0 in C) for one unit vector.
 * @param rates_len  A whole multiple of ny, 0 included.
 * @param out       `n_out` powers.
 * @param out1      `n_out` hypothesis indices.
 * @param max_out   Capacity of each output; at least `n_out`.
 * @param status  Where the refusal's cause goes (just-makeit#2584); may be
 *              NULL. `DP_ERR_MEMORY` when memory ran out (Python:
 *              MemoryError), `DP_ERR_UNSUPPORTED` off the flavors that
 *              serve the call, `DP_ERR_INVALID` for a refused argument.
 *              Untouched on success.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the tuple (power, index), each (n_out,): float32 powers
 *         and uint32 hypothesis indices, always new arrays (this call takes
 *         no `out=`); out of memory is a MemoryError, any other refusal
 *         a ValueError.  C: `n_out`, or SIZE_MAX with nothing written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples
 * >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
 * >>> m = np.arange(ny * nx)
 * >>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off
 * ...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> no_rates = np.empty(0, np.complex64)
 * >>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), -4..4
 * >>> cell = int(np.argmax(pw))
 * >>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)
 * (5, 3)
 * @endcode
 */
size_t dp_correlator_bank_power_max (const dp_correlator_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 size_t L, int g_min, int g_max,
                                 const float _Complex *rates,
                                 size_t rates_len, float *out, uint32_t *out1,
                                 size_t max_out, int *status,
                                 const char **why);

/**
 * @brief Capacity dp_correlator_bank_detect() needs: one record per map cell.
 * @param state  The correlator.
 * @param x_len  The frame's length (the bound does not depend on it).
 * @return `n_out`.
 */
size_t dp_correlator_bank_detect_max_out (dp_correlator_state_t *state,
                                      size_t x_len);

/**
 * @brief Detect on a coarse carrier grid, refine each candidate at its own
 *        cell on a fine one, and keep what confirms.
 *
 * The bank searches on @p L_d and resolves on @p L_r
 * (docs/design/corr2d-hypotheses.md section 6):
 *
 * 1. **Detect.** dp_correlator_bank_power_max() over carriers @p g_min ..
 *    @p g_max on the `1 / (L_d nx)` grid, with every rate.
 * 2. **Candidates.** Every cell whose power exceeds @p t_candidate and is
 *    at least each of its 8 neighbors, both axes circular (a delay lag and a
 *    Doppler bin both wrap).
 * 3. **Refine.** For a candidate found by coarse carrier g_d and rate r, the
 *    map value at its cell under every fine carrier g_r from
 *    `m g_d - (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers
 *    between its coarse neighbors, on the `1 / (L_r nx)` grid.  On the fast
 *    path and the delay-Doppler map with a native delay axis each is a time-domain
 *    correlation at the cell's lag and a slow-time DFT at its Doppler row, in
 *    double, with no FFT: `2 ny nx` multiply-adds a carrier.  Any other
 *    object makes that hypothesis's map and reads the cell.  The best g_r
 *    wins, the lowest on a tie.
 * 4. **Confirm.** A candidate whose refined power exceeds @p t_confirm is a
 *    record.
 *
 * Records come out in cell order (row-major), one entry of each output per
 * record: @p out the refined power, @p out1 g_r on the fine grid, @p out2
 * the rate r, @p out3 the row and @p out4 the column, the cell as execute()
 * lays out the map.  Measured at 64 x 512 against an oracle
 * that corrects the exact offset, @p L_d = 2 with @p L_r = 4 holds L = 4's
 * sensitivity within 0.16 dB, for about half the bank (the design page).
 * @p L_d = @p L_r is the one-stage bank.  Thresholds are powers in the
 * map's own units; the caller sets them for its noise, as for any detector.
 *
 * @param state, x, x_len, g_min, g_max, rates, rates_len
 *                     As dp_correlator_bank_fold(); @p g_min and @p g_max are on
 *                     the coarse grid.
 * @param L_d          The search grid; at least 1.
 * @param L_r          The resolution grid; a multiple of @p L_d.
 * @param t_candidate  Candidate threshold, on the coarse power.
 * @param t_confirm    Confirm threshold, on the refined power.
 * @param out          Refined powers.
 * @param out1         Refined carriers, on the `1 / (L_r nx)` grid.
 * @param out2         Rates.
 * @param out3         Rows.
 * @param out4         Columns.
 * @param max_out      Capacity of each output; at least `n_out`.
 * @param status  Where the refusal's cause goes (just-makeit#2584); may be
 *              NULL. `DP_ERR_MEMORY` when memory ran out (Python:
 *              MemoryError), `DP_ERR_UNSUPPORTED` off the flavors that
 *              serve the call, `DP_ERR_INVALID` for a refused argument.
 *              Untouched on success.
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return Python: the tuple (power, g, rate, row, col), one entry per
 *         confirmed record (possibly none): float32 refined power, int32
 *         refined carrier, uint32 rate, row and column, always new arrays
 *         (this call takes no `out=`); out of memory is a MemoryError, and
 *         anything else below a ValueError.  C: the record count (0
 *         included), or SIZE_MAX with nothing written for anything
 *         dp_correlator_bank_fold() refuses, `L_d == 0`, an @p L_r that
 *         @p L_d does not divide, a refined carrier past int32, @p max_out
 *         below `n_out`, or memory for the detector's work or the refine's
 *         scratch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d = 8, 32, 5
 * >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
 * >>> m = np.arange(ny * nx)
 * >>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2
 * ...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)
 * >>> dd = Correlator(code, doppler_rows=ny)
 * >>> no_rates = np.empty(0, np.complex64)
 * >>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)
 * >>> t = 0.5 * coarse.max()
 * >>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,
 * ...                                           no_rates)
 * >>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid
 * (1, 5, 5)
 * >>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered
 * 1.233
 * @endcode
 */
size_t dp_correlator_bank_detect (const dp_correlator_state_t *state,
                              const float _Complex *x, size_t x_len,
                              size_t L_d, size_t L_r, int g_min, int g_max,
                              float t_candidate, float t_confirm,
                              const float _Complex *rates, size_t rates_len,
                              float *out, int32_t *out1, uint32_t *out2,
                              uint32_t *out3, uint32_t *out4, size_t max_out,
                              int *status, const char **why);

/* ── Detector flavors ──────────────────────────────────────────────────────
 * The correlator with the stream and detection layers on top
 * (docs/design/correlator.md section 4): any chunk in, the ring's framer
 * cutting n-sample frames (n = ny*nx), each frame correlated and
 * integrated over dwell frames, and on every dump a test statistic
 *
 *   test_stat = peak_mag / noise_est
 *
 * where peak_mag is the largest |R| (a NaN cell is never the peak) and
 * noise_est an aggregate of |R| over the flat bins noise_lo..noise_hi.  A
 * hit is emitted when test_stat > threshold, or on every dump when
 * threshold is 0.  The detections are a function of the input stream, not
 * of how it was split into calls.  One object is single-threaded: never
 * push it from two threads at once.
 *
 * A detector takes its frames through dp_correlator_push() alone: every
 * execute, batch and bank call refuses it with SIZE_MAX and nothing
 * changed, since a frame from outside would put the dwell off the framer's
 * grid.  dp_correlator_reset() also drops its carry (the partial frame held
 * between pushes), and its last dump's fields (the peak, noise_est,
 * test_stat) read 0 until the next dump, so the blob after a reset matches
 * a fresh object's (doppler#2642); dp_correlator_consumed() reads 0.
 *
 * @code
 * // header-code: skip=a stream-loop fragment: recv() and ref are the caller's
 * dp_correlator_state_t *det = dp_correlator_create_detector_1d (
 *     ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
 * dp_correlator_hit_t hits[64];
 * // a full hits[] stops a push, and the input it did not take is offered
 * // again: dp_correlator_consumed(det) says where it stopped
 * while (recv (chunk, CHUNK_SZ)) {
 *     for (size_t off = 0; off < CHUNK_SZ; off += dp_correlator_consumed (det)) {
 *         size_t n = dp_correlator_push (det, chunk + off, CHUNK_SZ - off,
 *                                        hits, 64);
 *         for (size_t i = 0; i < n; i++)
 *             printf ("lag=%zu stat=%.2f\n", hits[i].col, hits[i].test_stat);
 *     }
 * }
 * dp_correlator_destroy (det);
 * @endcode
 */

/**
 * @brief Allocate a 2-D streaming detector: the 2-D correlator with the
 *        stream and detection layers.
 *
 * Input frames are flat row-major CF32 arrays of length ny*nx, cut from
 * the stream by the ring's framer.  On every dump the peak's flat index is
 * decomposed into (row, col), and a dp_correlator_hit_t is emitted when
 * test_stat > threshold.  The Python class accepts a (ny, nx) CF32 ndarray
 * for both @p ref and the push input.  The ring capacity is
 * next_pow_two(max(ny*nx, 512)) complex samples.
 *
 * @param ref        2-D reference image, (ny, nx) CF32 ndarray in Python.
 * @param ny         Number of rows in the reference and input frames.
 * @param nx         Number of columns in the reference and input frames.
 * @param dwell      Int-dump depth; must be >= 1.
 * @param noise_lo   Lower flat-index noise bin (inclusive, 0-based).
 * @param noise_hi   Upper flat-index noise bin (inclusive, < ny*nx).  A
 *                  value at or beyond the window clamps to ny*nx - 1, so
 *                  the default sentinel selects the full window.
 * @param noise_mode Noise aggregation: "mean", "median", "min", or "max".
 * @param threshold  Test-stat gate; 0.0 = always emit.
 * @param nthreads   Workers for the single-row-reference fast path (1
 *                   serial, <= 0 every online core); the general 2-D path
 *                   ignores it.
 * @return Heap-allocated state, or NULL when the arguments are refused
 *         (an empty axis, @p dwell 0) or the stream's ring cannot be made.
 *         Out of memory for the detection buffers aborts, by the library's
 *         dp_x* convention (clib_common.h), as the map's plans do.
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> det = Correlator(ref=ref, detect=1, dwell=1, noise_lo=1, noise_hi=15,
 * ...                  noise_mode="mean", threshold=0.0)
 * >>> det.ny, det.nx, det.n, det.dwell
 * (4, 4, 16, 1)
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_detector_2d (
    const float _Complex *ref, size_t ny, size_t nx, size_t dwell,
    size_t noise_lo, size_t noise_hi, det_noise_mode_t noise_mode,
    float threshold, int nthreads);

/**
 * @brief Allocate a 1-D streaming detector: the 2-D detector at one row
 *        (docs/design/correlator.md section 2.1), so its hits are the 2-D
 *        one's at ny = 1, each with row 0 and the lag in col.
 *
 * After every dump the peak-to-noise test statistic is compared against
 * @p threshold, and a hit is emitted when it passes; 0.0 fires on every
 * dump.  The ring capacity is next_pow_two(max(n, 512)) complex samples.
 *
 * @param ref        Reference signal, CF32 ndarray of length ref_len.
 * @param ref_len    Reference / FFT length in complex samples.
 * @param dwell      Int-dump depth; must be >= 1.
 * @param noise_lo   Lower noise bin index (inclusive, 0-based).
 * @param noise_hi   Upper noise bin index (inclusive, < n).  A value at or
 *                  beyond the window clamps to n - 1, so the
 *                  default sentinel selects the full window.
 * @param noise_mode Noise aggregation: "mean", "median", "min", or "max".
 * @param threshold  Test-stat gate; 0.0 = always emit.
 * @param nthreads   Accepted for API compatibility; ignored.
 * @return Heap-allocated state, or NULL when the arguments are refused
 *         (an empty reference, @p dwell 0) or the stream's ring cannot be
 *         made.  Out of memory for the detection buffers aborts, by the
 *         library's dp_x* convention (clib_common.h).
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
 * >>> det = Correlator(ref=ref, detect=1, dwell=1, noise_lo=1, noise_hi=7,
 * ...                noise_mode="mean", threshold=0.0)
 * >>> det.n, det.dwell, det.ring_cap
 * (8, 1, 512)
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_detector_1d (
    const float _Complex *ref, size_t ref_len, size_t dwell,
    size_t noise_lo, size_t noise_hi, det_noise_mode_t noise_mode,
    float threshold, int nthreads);

/**
 * @brief Stream an arbitrary-length CF32 chunk through a detector or a
 *        search flavor; return its detections.
 *
 * Takes the input in order and frames it.  A detector runs each complete
 * n-sample frame (n = ny*nx) through the correlator and, on every dump,
 * computes the test statistic peak_mag / noise_est: each dump that passes
 * the threshold is one hit, row and col its cell (on one row, col is the
 * lag).  A search gates each decided dwell on its CFAR threshold and lists
 * up to max_peaks hits a dwell, each with its Doppler bin (row), code
 * phase (col), C/N0 estimate and hand-off fields (dp_correlator_hit_t).
 * A chunk that ends mid-frame keeps the rest as the carry, so the hits do
 * not depend on how the stream is split into calls.
 *
 * In Python the hits are one structured array of dp_correlator_hit_t
 * records, a field per member, sized from dp_correlator_push_max_out():
 * a push never loses input to a full list.  A map flavor refuses it.
 *
 * @param state        A detector or search flavor (non-NULL).
 * @param in           CF32 input chunk of arbitrary length.
 * @param n_in         Number of input samples in @p in.
 * @param result       Caller-supplied array of at least @p max_results
 *                     hits; filled on return.
 * @param max_results  Capacity of @p result (maximum detections to emit).
 *                     A full @p result never loses input: a detector's
 *                     frame yields at most one detection (a search's at
 *                     most max_peaks), and once @p result is full the
 *                     push takes the rest of its input only if the rest
 *                     completes no frame (it is then the carry); otherwise
 *                     it takes nothing more, stopping on the boundary of
 *                     its last frame.  dp_correlator_consumed() says how
 *                     many samples it took, and the caller offers the rest
 *                     again.  Input that runs out mid-frame is the carry,
 *                     held inside (fewer than n samples).  With room 0 the
 *                     push is full from the start, so a resume loop needs
 *                     room for at least one; with that, a push of any
 *                     input takes at least one sample.
 * @return Python: the hits, a structured array, empty when no dump or
 *         dwell passed the gate.  C: the number of hits written to
 *         @p result, or SIZE_MAX (nothing taken, nothing changed) when
 *         @p state is a map flavor.
 * @code
 * >>> from doppler.correlation import Correlator
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> det = Correlator(ref=ref, detect=1, dwell=1, noise_lo=1, noise_hi=15,
 * ...                  noise_mode="mean", threshold=0.0)
 * >>> h = det.push(np.ones((4, 4), dtype=np.complex64))[0]
 * >>> row, col = int(h["row"]), int(h["col"])
 * >>> peak, noise, stat = (float(h[f]) for f in ("peak_mag", "noise_est",
 * ...                                            "test_stat"))
 * >>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
 * (0, 0, 1.0, 1.0, 1.0)
 * >>> det1 = Correlator(ref=ref[0], detect=1, dwell=1, noise_lo=1, noise_hi=3,
 * ...                 noise_mode="mean", threshold=0.0)
 * >>> h = det1.push(np.ones(4, dtype=np.complex64))[0]
 * >>> lag = int(h["col"])               # one row: the lag is the column
 * >>> peak, noise, stat = (float(h[f]) for f in ("peak_mag", "noise_est",
 * ...                                            "test_stat"))
 * >>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
 * (0, 1.0, 1.0, 1.0)
 *
 * A burst search over a PN preamble, 4 samples a chip, finds the burst's
 * delay; any repeated preamble works, a Zadoff-Chu sequence included:
 *
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(31),
 * ...                   np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(np.complex64)
 * >>> b = Correlator(s0, framing="burst", reps=8, sample_rate=4e6,
 * ...                cn0_dbhz=60.0)
 * >>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]
 * >>> int(h["row"]), int(h["col"])     # (Doppler bin, delay in samples)
 * (0, 17)
 * >>> k = np.arange(127)
 * >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)
 * >>> z = Correlator(zc, framing="burst", reps=8, sample_rate=1.0)
 * >>> z.code_bins                      # samples a repetition
 * 127
 * >>> h = z.push(np.tile(np.roll(zc, 40), 10))[0]
 * >>> int(h["row"]), int(h["col"])
 * (0, 40)
 * @endcode
 */
size_t dp_correlator_push (dp_correlator_state_t *state,
                           const float _Complex *in, size_t n_in,
                           dp_correlator_hit_t *result, size_t max_results);

/**
 * @brief The most hits dp_correlator_push() can return for @p n_in more
 *        samples: a result array this long is never short.
 *
 * The frames @p n_in completes, counting the carry the last push left,
 * times what one frame can make: one hit for a detector (one per dump),
 * `max_peaks` for a search (a frame that ends a dwell lists up to that
 * many). A bound, not a count: most frames end no dump. 0 on a map
 * flavor, which push() refuses. SIZE_MAX if the product would overflow.
 *
 * @param state  Must be non-NULL.
 * @param n_in   Samples the next push() will be given.
 * @return The bound, in hits.
 */
size_t dp_correlator_push_max_out (const dp_correlator_state_t *state,
                                   size_t n_in);

/**
 * @brief Input samples the last dp_correlator_push() took.
 *
 * Equal to its @p n_in unless @p result filled up; then the caller resumes
 * at in + consumed.  0 after create, reset, set_ref and set_state, and
 * always 0 for a map flavor.
 *
 * @param state  Must be non-NULL.
 * @return Samples of the last push's input consumed (including any kept as
 *         the carry).
 */
size_t dp_correlator_consumed (const dp_correlator_state_t *state);

/**
 * @brief Change a detector flavor's threshold without rebuilding it.
 *
 * The value is part of the state blob, so set_state restores it.  A
 * search flavor refuses it: its gate is sized from Pfa and Pd at create,
 * together with the raw and non-coherent thresholds it implies.
 *
 * @param state     Must be non-NULL.
 * @param threshold New threshold; 0.0 = always fire.
 * @return DP_OK, or DP_ERR_UNSUPPORTED (nothing changed) for a map or
 *         search flavor.
 */
int dp_correlator_set_threshold (dp_correlator_state_t *state,
                                 float threshold);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * One blob for every flavor (docs/design/correlator.md section 4): the
 * flavor, the grid, the frame count and the product-spectrum accumulator.
 * The flavor, ny, ny_out, fast_path, col_out and nx_out are reject keys:
 * each changes what the accumulator holds, so a blob that differs in any
 * of them is refused rather than reinterpreted.
 * A detector flavor's blob goes on: noise_lo, noise_hi and noise_mode
 * (u64 each; reject keys, since they decide what the stored noise_est and
 * test_stat mean), the threshold (f32; a mutator's value, restored), the
 * framer's carry as its own child blob (fewer than n samples), then the
 * last dump's peak_row and peak_col (u64) and peak_mag, noise_est and
 * test_stat (f32).  The last correlation map (det_out) is not in the blob,
 * so last_corr is None after set_state, and dp_correlator_consumed() reads
 * 0.  The retired detector magics 'DET1' and 'DET2' are not reused.
 * The FFT plans and the reference spectrum are config, rebuilt by create.
 * The retired magics 'CORR', 'CR2D' and 'DDMP' are not reused, so a blob
 * from a release before the one correlator fails dp_state_validate.
 * A search flavor's blob is the header, its flavor (u64; a reject key),
 * then the search payload: a fixed block of the counts that are reject
 * keys (the grid size, the non-coherent looks, the peak-list capacity,
 * the rate hypotheses, the carrier) and the stream position, the framer's
 * carry as its own child blob, the non-coherent power accumulator when
 * n_noncoh > 1, the last dwell's picks (the twins), and a tiled block's
 * raw epochs.  The carry, the position, the block's epoch and the look
 * count must name one stream point, or the blob is refused.  acq's
 * retired magic 'ACQR' is not reused. */
/** @brief State-blob magic ('CORL'); set_state() rejects any other. */
#define CORRELATOR_STATE_MAGIC DP_FOURCC ('C','O','R','L')
/** @brief State-blob layout version; set_state() rejects any other. 2: the
 *  search's extra block carries keep_surface (#2022). */
#define CORRELATOR_STATE_VERSION 2u
/**
 * @brief Size in bytes of this correlator's state blob.
 * @param state  Must be non-NULL.
 * @return Header + flavor, ny, ny_out, fast_path, col_out, nx_out and
 *         frame count (u64 each) + the
 *         ny*nx complex-float accumulator; a detector flavor adds 5 u64,
 *         4 f32 and the framer's child blob.
 */
size_t dp_correlator_state_bytes (const dp_correlator_state_t *state);
/**
 * @brief Serialize the frame count and spectrum accumulator into @p blob.
 * @param state  Must be non-NULL.
 * @param blob   Destination; at least dp_correlator_state_bytes() bytes.
 */
void dp_correlator_get_state (const dp_correlator_state_t *state, void *blob);
/**
 * @brief Restore a mid-dwell correlator from dp_correlator_get_state()'s blob.
 * @param state  Must be non-NULL, created with the same (ny, nx) and
 *               reference (the reference is not in the blob).
 * @param blob   Source blob of dp_correlator_state_bytes() bytes.
 * @return `DP_OK`, or `DP_ERR_INVALID` (state untouched) when the header's
 *         magic, version, endianness or size does not match, when the blob's
 *         flavor, ny, ny_out, fast_path, col_out or nx_out differs from
 *         @p state's (a blob of one flavor never restores into another), when
 *         its frame count is not below @p state's dwell, or, for a detector
 *         flavor, when noise_lo, noise_hi or noise_mode differs, the
 *         framer's child blob is refused, the frames that blob has handed
 *         out are not the dwell's position (frames mod dwell must equal the
 *         count, as every push, reset and set_ref keeps it), or the last
 *         peak is not a cell of the map.  Everything is checked before
 *         anything is written.
 */
int dp_correlator_set_state (dp_correlator_state_t *state, const void *blob);
/**
 * @brief Whether dp_correlator_set_state() would accept @p blob, without
 *        writing anything.
 *
 * DP_OK exactly when set_state would accept it, and DP_ERR_INVALID exactly
 * when it would refuse it: both read the blob through one check.  What
 * lets a composition check this child before it restores any other, so a
 * refusal of either changes nothing (carrier_acq's 'CAQR' does).
 *
 * @param state  Must be non-NULL.
 * @param blob   Candidate blob of dp_correlator_state_bytes() bytes.
 * @return `DP_OK` or `DP_ERR_INVALID`; @p state is never written.
 */
int dp_correlator_state_check (const dp_correlator_state_t *state,
                               const void *blob);

/* ── Search flavors ──────────────────────────────────────────────────────────
 * #2752 3.4b: the acquisition engine, as the correlator's sixth and
 * seventh flavors: a streaming DSSS acquisition engine, burst and
 * continuous, acq until that step.
 *
 *
 * Acquires a signal built from a repeated sequence -- a spread-spectrum
 * code (the continuous door) or any repeated complex preamble (the burst
 * door) -- arriving with an unknown integer code phase and an unknown
 * carrier-frequency (Doppler) offset, buried in AWGN.  It
 * jointly estimates the (Doppler bin, code phase) and declares a detection
 * whenever the CFAR test statistic crosses an automatically configured
 * threshold.
 *
 * Pipeline (owned end to end, one object):
 *   push(raw cf32) -> ring buffer -> reframe to (coherent_bins, code_bins) ->
 *   slow-time Doppler FFT (FFT along the segment axis) -> 2-D code correlation
 *   against a single-row reference (corr2d) -> argmax + CFAR noise estimate
 *   -> threshold gate -> dp_correlator_hit_t.
 *
 * The fast-time axis (code_bins samples, one code period) is the circular
 * code matched filter, read on lag_bins columns: code_bins itself, or, on a
 * continuous engine whose period is not 7-smooth, the next power of two --
 * the cross-spectrum zero-padded before the inverse, an exact interpolation
 * of the same circular correlation at a fraction of the cost of inverses of
 * an awkward length (lag_bins' doc). The slow-time axis (coherent_bins
 * rows, one row per code repetition) is the coherent Doppler search.  A
 * carrier offset f (cycles/sample) lands the peak at
 * row = round(f*code_bins*coherent_bins) mod coherent_bins, column = code
 * phase.
 *
 * **Two mode-fixed public constructors, one shared engine.** A coherent
 * slow-time Doppler FFT can only ever resolve frequency *within* one native
 * span `chip_rate/(2*sf)` — more coherent depth subdivides that SAME fixed
 * range more finely, it never widens it — and for a continuous (async,
 * data-modulated) signal, a multi-epoch coherent axis wide enough to matter
 * aliases the data's own bit transitions across the whole Doppler-bin axis (a
 * structural mislock, not a graceful SNR loss — see
 * docs/design/acq-surface.md §9).  So the two constructors fix a mode each,
 * never a per-call knob:
 *
 * - dp_correlator_create_burst() — the smallest coherent depth `coherent_bins` in
 *   `[1, reps]` whose BURST Pd meets @p pd: the preamble lands at any
 *   offset against stream-aligned dwells and is detected when any one of
 *   them is (see dp_correlator_create_burst()); never a non-coherent look. Minimum
 *   latency for a strong signal, unmodulated bursts/preambles only.  A tighter
 *   @p doppler_uncertainty shrinks the searched cell count, lowering the
 *   Sidak threshold (more sensitive).  When @p doppler_uncertainty
 *   exceeds the native span, the wideband window-tiling mechanism below
 *   tiles it, and the coherent depth is chosen INSIDE every tile -- the
 *   smallest that meets @p pd at a stated design C/N0, and 1 with none (a
 *   default-built tiled search has never carried a block).
 * - dp_correlator_create_continuous() — for a continuous, data-modulated signal:
 *   ALWAYS uses the wideband window-tiling mechanism below, unconditionally
 *   (never attempts coherent multi-epoch combining, even when
 *   @p doppler_uncertainty is narrower than one native span) — closes the
 *   aliasing footgun structurally rather than pricing it as a tunable loss.
 *   Sensitivity margin comes entirely from auto-selected non-coherent looks.
 *
 * Both convert C/N0 to a per-sample amplitude SNR
 * (snr = sqrt(10^(cn0_dbhz/10) / fs)), where fs is the sample rate: the
 * burst door's @p fs, the continuous door's @p sample_rate.  Every reported
 * detection inverts this same relationship to report an estimated C/N0
 * (dp_correlator_hit_t::cn0_dbhz_est) — a bandwidth/integration-time-independent
 * figure of merit directly comparable to @p cn0_dbhz, unlike a raw per-sample
 * or coherently-integrated ratio (both scale with the sample rate and the
 * coherent depth, and so aren't portable across configurations).
 *
 * **Wideband window-tiling mode**: instead of coherent combining, tiles the
 * requested uncertainty with `window_bins = 2*ceil((du - span)/(2*span)) + 1`
 * (`du = doppler_uncertainty`, `span = chip_rate/(2*sf)`; always odd, so the
 * coverage is symmetric) parallel frequency-window hypotheses, each one native
 * span wide, searched every epoch from a SINGLE shared forward FFT of that
 * epoch: hypothesis r's spectrum is the shared FFT circularly rolled by r
 * bins (exact — the window spacing IS this code_bins-point FFT's own bin
 * spacing) against one fixed precomputed replica spectrum, then
 * inverse-FFT'd — `window_bins` inverse FFTs plus the one shared forward FFT
 * per epoch, not `window_bins` independent down-conversions. Empirically the
 * cheaper of the two realizations benchmarked for this
 * (a frequency-bank benchmark): ~1.2x-1.55x faster than
 * an equivalent tuned-mixer bank, measured with real doppler.spectral.FFT.
 * SNR margin in this mode comes entirely from auto-selected non-coherent
 * looks (magnitude-squared accumulation, immune to data-modulation sign
 * flips) rather than coherent depth, sized against an internal safety-valve
 * ceiling rather than a caller-supplied cap (the semi-analytical Pd model
 * this engine sizes against grows unreliable past a few hundred looks — not
 * a public knob to tune around that). `row` in @ref dp_correlator_hit_t
 * reports the frequency-window index (0 … window_bins-1, native FFT-bin
 * ordering) instead of a slow-time-FFT row when this mode is active at
 * coherent_bins=1, where `doppler_res_hz` is the per-window spacing
 * (chip_rate/sf). A burst engine sized to a coherent depth D > 1 inside the
 * tiles (a stated design C/N0) indexes the combined grid instead:
 * `window_bins*D` bins of `chip_rate/(sf*D)`, folded over dp_correlator_grid_bins().
 *
 * **Block-coherent depth inside the tiles** (docs/design/async-dsss-receiver.md
 * §2.3): the engine allows a coherent depth, to accommodate waveforms with
 * code-only windows. Given `code_only_epochs > 1` -- the whole code-only
 * epochs such a window holds at any chip phase -- it runs a coherent depth
 * `D` inside every tile: the per-tile epoch
 * correlations are gathered for `D` epochs, then a zero-padded slow-time
 * FFT per code-phase column turns each tile into `D` Doppler rows
 * `chip_rate/(sf*D)` apart, detected per block. Blocks are non-overlapping
 * and the engine does not know any emitter's window phase, so `D` is at
 * most `(code_only_epochs+1)/2` (a whole block always lands inside the
 * window) and, when `doppler_rate` is given, at most
 * `f_epoch/sqrt(2*doppler_rate)` (the drift over a block stays inside half a
 * row), rounded down to a 7-smooth length (dp_fft_smooth_floor()): `D` is a
 * slow-time FFT length, and a prime factor of 13 or more costs pocketfft its
 * slow path (doppler#2457). The Doppler axis is then ONE uniform grid of
 * `window_bins*coherent_bins` bins of `doppler_res_hz = chip_rate/(sf*D)`
 * over the tiled span, in native FFT-bin order (0 = DC, ascending, then
 * wrapping negative): `row` indexes it, and dp_correlator_build_handoff()
 * folds it with dp_fftfreq_index() over that count. A block that straddles
 * data spreads that emitter over its rows, `10*log10(D)` below an aligned
 * block, at its own code phase -- the `conc` probe
 * (docs/design/async-dsss-receiver.md §2.4) reads it.
 * `code_only_epochs = 1` (the default) is `D = 1` and the engine exactly as
 * described above. A tile de-rotates by its own centre, so an emitter on
 * the edge between two tiles reads the same in both (within 0.03 dB) and
 * the slow-time transform folds it to the same row index of each: every
 * listed peak is therefore asked at its ROW's frequency before it is
 * reported -- the block's raw epochs correlated with the replica at the
 * pick's code phase, mixed by the row's frequency and by that one span
 * down and up, the winner reported (docs/design/acq-surface.md §6,
 * docs/design/async-dsss-receiver-measurements.md §12.18,
 * doppler#1270).
 *
 * **A roll per thread** (docs/design/acq-surface.md §13): the tiles are
 * independent after the one forward transform, so the per-epoch tile loop
 * and, at `D > 1`, the
 * block-end column loop run across a persistent pool of workers
 * (dp_parallel.h's `dp_pool_*`), created once with the engine and parked
 * between pushes. Each tile owns its inverse plan and scratch, so the
 * result is bit-identical at any thread count. The per-cell passes that
 * decide a surface -- the magnitude, the CFAR reference, the working mask
 * and every scan of the peak list -- run per tile too, each into a slot of
 * its own, and are merged serially in tile order (a mean of the tiles'
 * means over equal cells, the first of the tiles' first maxima), so the
 * serial remainder is per tile, not per cell. dp_correlator_set_threads() sets the
 * count.
 *
 * @code
 * // header-code: skip=a fragment: samples is the caller's stream
 * // 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
 * uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
 * float nrz[31];
 * bin_to_nrz(code, 31, nrz, 31);           // chip 0 -> +1, chip 1 -> -1
 * float _Complex pre[124];
 * for (size_t i = 0; i < 124; i++)
 *   pre[i] = nrz[i / 4];                   // each chip held 4 samples
 * dp_correlator_state_t *a = dp_correlator_create_burst(pre, 124, 16, 4.0e6, 45.0,
 *                                   0.0, 1e-3, 0.9, 0, 0.0, 1);
 * dp_correlator_hit_t hits[64];
 * size_t nh = dp_correlator_push(a, samples, n_samples, hits, 64);
 * for (size_t i = 0; i < nh; i++)
 *   printf("Doppler %zu, code phase %zu, C/N0 %.1f dB-Hz\n",
 *          hits[i].row, hits[i].col,
 *          hits[i].cn0_dbhz_est);
 * dp_correlator_destroy(a);
 * @endcode
 *
 * The functions every flavor shares, on a search flavor (kept here rather
 * than in their own doc blocks, which every Python class of the
 * correlator renders):
 *
 * - dp_correlator_push() frames the search's own grid.  For every complete
 *   frame it applies the slow-time Doppler FFT, correlates against the
 *   reference, dumps the coherent surface (or, when n_noncoh > 1,
 *   accumulates |.|^2 over n_noncoh looks first), lists up to max_peaks
 *   picks above the auto-configured gate and writes one hit each, with
 *   every field filled: the Doppler bin and code phase as row and col, the
 *   statistic, the C/N0 estimate, the stream position, the hand-off chip
 *   phase and the rate hypothesis.  A frame that ends a dwell may write up
 *   to max_peaks hits, so it is taken only while at least
 *   min(max_peaks, max_results) slots are left (at least one); a frame
 *   that ends none needs no room while any slot is left.  A max_results
 *   under max_peaks still takes the dwell and keeps its strongest
 *   max_results picks: that loses results, never input.
 * - dp_correlator_consumed() is not a hit's samples_consumed, which is
 *   CUMULATIVE and counts only framed samples (the stream position a hit's
 *   epoch ended at): resuming from that would re-feed the carry.  It reads
 *   0 after a regrid too.
 * - dp_correlator_reset() drains the ring and clears the non-coherent
 *   power accumulator, the block in progress and the dwell bookkeeping, so
 *   the next push begins a fresh search from an empty ring; the grid, the
 *   thresholds and the reference are untouched.
 * - dp_correlator_set_threshold() refuses it: the gate is sized from Pfa
 *   and Pd at create, with the raw and non-coherent thresholds it implies.
 * - The state triplet: the blob is described with the others, under
 *   "Serializable state".
 */


/** The largest `max_peaks` dp_correlator_set_max_peaks() accepts: the bindings'
 *  push result arrays (1024) are at least this many, so one dwell can
 *  always be reported whole. */
#define DP_CORRELATOR_MAX_PEAKS 64u


/** Most Doppler-rate hypotheses an engine searches. Each one costs a full
 *  slow-time pass per dwell, so this bounds the cost of a typo, not a
 *  design: the residual rate falls as 1/(R-1), and a few dozen hypotheses
 *  already take a rate span out to depths no preamble repeats to. */
#define DP_CORRELATOR_RATE_HYPS_MAX 64u

/**
 * @brief Internal safety-valve ceiling on auto-selected non-coherent
 *        looks -- not a public knob (no caller-facing equivalent of the
 *        retired @c max_noncoh parameter).
 *
 * The semi-analytical Pd model both auto-sizers ascend against turns
 * non-monotonic and unreliable past a few hundred looks (this project's
 * own geometry found ~256 empirically -- see
 * docs/design/async-dsss-receiver.md).  Hitting this ceiling without
 * meeting @p pd leaves dp_correlator_search_t::underpowered set, same as any other
 * infeasible operating point -- no separate bookkeeping needed.
 */
#define DP_CORRELATOR_N_NONCOH_SAFETY_CEILING 256u

/**
 * @brief The design C/N0 that means "none given" (doppler#1484): a burst
 *        engine then sizes for the whole preamble, its `pd_predicted` is NaN
 *        and it is never `underpowered`.
 *
 * NaN, because every finite value is a real design point -- including a
 * negative one: at `fs = 1` (normalized units) the C/N0 IS the per-sample
 * SNR in dB, which is negative wherever acquisition is hard. It used to be
 * 0, which made exactly 0 dB unreachable and a negative per-sample SNR
 * unstatable. Test with isnan(), never ==.
 */
#define DP_CORRELATOR_CN0_NONE NAN

/**
 * @brief Create a burst-mode acquisition engine for a repeated preamble:
 *        coherent multi-repetition combining, up to @p reps deep.
 *
 * Takes the preamble as its SAMPLES: one period of @p n complex samples at
 * @p fs, repeated up to @p reps times. A PN code is one such preamble --
 * pass its chips mapped by dp_bin_to_nrz() and held `spc` samples each, at
 * `fs = chip_rate * spc`. The engine sees one chip = one sample: `sf = n`,
 * `spc = 1`, `chip_rate = fs`, the native Doppler span is `+/- fs/(2n)`,
 * and `col` is the delay into the repetition, in samples -- for a
 * preamble whose ambiguity function is a ridge (Zadoff-Chu: one native
 * bin of Doppler moves the peak `u^-1 mod n` samples), the delay plus
 * that shift whenever the Doppler is not resolved before correlation. It
 * converts @p cn0_dbhz to a per-sample amplitude SNR
 * (snr = sqrt(10^(cn0_dbhz/10) / fs)), and picks the
 * *smallest* coherent depth `coherent_bins` in `[1, reps]` whose burst Pd
 * `pd_burst` meets @p pd at the Sidak threshold (minimum latency for
 * a strong signal). `pd_burst` treats the preamble as exactly @p reps
 * periods at a uniform offset against the stream-aligned dwells, and
 * credits every dwell it spans (doppler#1498). If no depth meets @p pd the
 * engine takes the one with the most burst Pd and is `underpowered`; it
 * does NOT add non-coherent looks.  A burst has one frame of preamble, so looks beyond
 * it add noise to the statistic and move the hit -- `samples_consumed` is
 * stamped at the end of the LAST accumulated look, so a consumer resolving
 * the preamble's position sees an anchor up to n_noncoh*coherent_bins
 * periods late (doppler#1181).  Intended for an unmodulated burst or
 * preamble window -- a continuous, data-modulated signal should use
 * dp_correlator_create_continuous() instead (coherent combining under continuous
 * data is a structural aliasing mislock, not a tunable SNR trade-off --
 * see the file doc comment).
 *
 * @p cn0_dbhz is the DESIGN (minimum) C/N0 and is optional: NaN
 * (@ref DP_CORRELATOR_CN0_NONE) means none was given, and the engine then integrates the whole preamble
 * (`coherent_bins = reps`; 1 in a tiled search, where a depth multiplies
 * the buffers and the latency and only a stated C/N0 asks for one) with the
 * threshold set by @p pfa alone.  @p pd
 * is a sizing target only when a design C/N0 is given; without one
 * `pd_predicted` and `pd_burst` are NAN and `underpowered` is never set.
 *
 * A tighter @p doppler_uncertainty narrows the scanned Doppler band,
 * lowering the per-cell threshold (more sensitive).  When
 * @p doppler_uncertainty exceeds the native span `fs/(2n)`,
 * engages the wideband window-tiling mechanism (see the file doc
 * comment): `2*ceil((du - span)/(2*span)) + 1` windows, with a coherent
 * depth chosen inside each (1 with no design C/N0).  Use dp_correlator_configure_search_raw()
 * to pin the grid directly instead of relying on this auto-sizer.
 *
 * What a code once knew analytically, the preamble's own correlation
 * gives numerically, from one FFT at construction (dp_correlator_shape_t):
 *
 * - the peak zone (the twin rule's reach and the exclusion around each
 *   listed peak) is the autocorrelation's first null, the lag of the
 *   first local minimum of |R(k)| -- one chip of samples for a code;
 * - the delay straddle the Pd model averages is the band-limited
 *   autocorrelation over a half-sample offset.
 *
 * For a PN code the numeric model is the more accurate one: against a
 * rectangular-chip signal it stays conservative (never optimistic) and
 * sits 0.01-0.08 below the measured Pd at spc 2-4, where the analytic
 * triangle sat 0.03-0.21 below (doppler#1470).
 *
 * The within-repetition rotation loss keeps the engine's `sinc` model:
 * it is the zero-delay cut of the ambiguity function, which a periodic
 * preamble's envelope does not move -- measured within 0.007 dB of sinc
 * for RRC-shaped QPSK at 4.7 dB peak-to-average.
 *
 * @p tmpl is scaled to unit RMS (a copy -- the caller's buffer is only
 * read), so `peak_mag` and `noise_est` come out in the signal's units
 * whatever the preamble's amplitude. C/N0 is the preamble's mean power.
 *
 * **What hypotheses change**, when @p rate_hypotheses is `n > 1`:
 * - **Depth.** The drift bound is applied to the RESIDUAL rate, the farthest
 *   a true rate can sit from a hypothesis, `R/(n - 1)`, so `coherent_bins`
 *   can grow by up to `sqrt(n - 1)` at a fixed span.
 * - **False alarms.** A dwell now offers `n` looks at each cell: the
 *   per-cell Pfa is split over `n` times the cells the dwell's maximum
 *   ranges over (every searched row of the Doppler-interpolated surface,
 *   times `code_bins`), and the gate is the larger count's. Each hypothesis is CFAR-tested against its
 *   own surface and the dwell takes the strongest; dp_correlator_hit_t::
 *   doppler_rate_hz_s names the winner.
 * - **Cost.** A dwell does `n` slow-time passes (and a recompute of the
 *   winner when it was not the last): about `n` times the per-dwell cost.
 * - **Pd.** The model prices a rate ON a hypothesis. Between two, the
 *   coherent gain falls with the residual; the depth bound limits that to
 *   half a slow-time row, and validate_acq_rate_pd measures it (a rate half
 *   a spacing from every hypothesis, the worst case, against a rate drawn
 *   uniformly).
 *
 * The count is config, not running state, and it is a reject key of the
 * state blob: a blob from an engine with another count is refused, not
 * reinterpreted.
 *
 * @param tmpl        One period of the preamble, @p n samples; not all
 *                    zero, every sample finite.
 * @param n           Samples per repetition (>= 1).
 * @param reps        Max coherent repetitions, the coherence ceiling
 *                    (>= 1).
 * @param fs          Sample rate in Hz (> 0); 1 for normalized units.
 * @param cn0_dbhz    Design carrier-to-noise density in dB-Hz, of the
 *                    preamble's mean power: any finite value (at fs = 1
 *                    the per-sample SNR, usually negative), or
 *                    @ref DP_CORRELATOR_CN0_NONE (NaN) for no design point -- size
 *                    for the whole preamble.
 * @param doppler_uncertainty  One-sided Doppler search half-range in Hz; 0
 *                    uses the full native span +/- fs/(2n). A value greater
 *                    than the native span engages wideband mode (see the
 *                    file doc comment above): the uncertainty is tiled
 *                    with parallel frequency-window hypotheses and a
 *                    coherent depth is chosen inside every tile (1 with no
 *                    design C/N0); the surface is
 *                    `window_bins * coherent_bins` rows.
 * @param pfa         Target system (max-of-N) false-alarm probability (0,1).
 * @param pd          Target detection probability (0,1); a sizing target
 *                    only when @p cn0_dbhz is given.
 * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
 * @param doppler_rate  Doppler rate in Hz/s (>= 0) the coherent depth is
 *                    bounded against: at most `f_epoch/sqrt(2*doppler_rate)`
 *                    repetitions (`f_epoch = fs/n`, the repetition rate), so
 *                    the carrier's drift over one block stays inside half a
 *                    slow-time row (doppler#1482). 0 is no bound: the depth
 *                    is sized up to @p reps. With @p rate_hypotheses > 1 it
 *                    is the SPAN of the rates searched, below.
 * @param rate_hypotheses  Doppler-rate hypotheses to search (0, or >= 1 and
 *                    at most
 *                    @ref DP_CORRELATOR_RATE_HYPS_MAX); 1 searches none and
 *                    @p doppler_rate only caps the depth. Above 1 the
 *                    engine COMPENSATES the rate: `doppler_rate` is the span
 *                    `[-R, +R]` and the coherent block is searched at
 *                    @p rate_hypotheses rates evenly spaced across it,
 *                    `a_h = -R + 2 R h / (n - 1)`. For each the slow-time
 *                    axis is multiplied by `exp(-j pi a_h t^2)`, `t` the
 *                    epoch's time from the block's middle, before the
 *                    slow-time transform -- so the Doppler an engine
 *                    reports is the carrier's at the block's middle, where a
 *                    hand-off wants it, and a signal whose rate sits within
 *                    half a spacing of a hypothesis integrates as a still
 *                    one (see below). A block of one or two epochs
 *                    cannot see a rate -- every hypothesis is then the
 *                    same surface -- so an engine sized to D <= 2 searches
 *                    ONE look (the gate of an engine without hypotheses),
 *                    runs once, and reports a rate of 0. Needs a non-zero @p doppler_rate: it
 *                    is the span.
 *                    **0 is AUTO**: the engine picks the smallest `n` whose
 *                    residual `R/(n - 1)` no longer limits the depth the
 *                    sizer would choose with no drift bound at all (priced
 *                    at that `n`), or @ref DP_CORRELATOR_RATE_HYPS_MAX if none does;
 *                    the choice is the engine's `rate_hyps`. Burst only,
 *                    and needs a non-zero @p doppler_rate like any
 *                    `n > 1`.
 *
 * @return Heap-allocated state, or NULL on bad arguments (including a
 *         @p rate_hypotheses above DP_CORRELATOR_RATE_HYPS_MAX, or 0 or above 1
 *         with `doppler_rate == 0`) / allocation failure.
 * @code
 * // 127-sample Zadoff-Chu preamble at 1 MS/s, up to 8 repetitions
 * float _Complex zc[127];
 * for (int k = 0; k < 127; k++)
 *   zc[k] = cexpf (-I * (float)(M_PI * 5.0 * k * (k + 1) / 127.0));
 * dp_correlator_state_t *a = dp_correlator_create_burst (zc, 127, 8, 1.0e6, 50.0, 0.0, 1e-3,
 *                                    0.9, 0, 0.0, 1);
 * dp_correlator_destroy (a);
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_burst (const float _Complex *tmpl, size_t n,
                               size_t reps, double fs, double cn0_dbhz,
                               double doppler_uncertainty, double pfa,
                               double pd, int noise_mode,
                               double doppler_rate,
                               size_t rate_hypotheses);

/**
 * @brief The samples one code period should be searched at: the smallest
 *        power of two that holds at least two samples a chip.
 *
 * The search's fast-time transform is one code period long, so its
 * length decides its speed: 2 * 1023 = 2,046 = 2 * 3 * 11 * 31 runs 5x
 * slower than 2,048 (docs/design/async-dsss-receiver.md). An object that
 * resamples its input for the search resamples to
 * `chip_rate * dp_correlator_period_samples(code_len) / code_len`, and builds
 * the engine at that rate. Two samples a chip is the least a code loop's
 * early and late taps need.
 *
 * @param code_len Chips in one code period (>= 1).
 * @return The period in samples, a power of two: 2048 for 1023 or 1024
 *         chips, 64 for 31; 0 for @p code_len 0 or one whose period would
 *         not fit a size_t.
 */
size_t dp_correlator_period_samples (size_t code_len);

/**
 * @brief The samples one code period is searched at, for a caller feeding
 *        @p sample_rate: its own period when that is already fast, else
 *        dp_correlator_period_samples().
 *
 * The caller's period, `sample_rate * code_len / chip_rate`, is kept
 * when it is a whole number of samples (to a part in 1e9) and at least
 * @p code_len: searching the samples as they come needs no resampler.
 * Its length need not be fast for the FFT: one with a larger prime
 * (2 * 1,023 = 2 * 3 * 11 * 31) is read on a padded lag grid, `lag_bins`
 * columns, the next power of two. Otherwise -- a fractional period
 * (11 MSa/s over 5 Mcps for 1,023 chips is 2,250.6) -- it is the power
 * of two dp_correlator_period_samples() derives, and the caller resamples to
 * `chip_rate * period / code_len`. 4 samples a chip of a 7-chip code
 * keeps 28; 2 a chip of 1,023 chips keeps 2,046 (read on 2,048 lag
 * columns); 11 MSa/s over 5 Mcps takes 2,048.
 *
 * @param code_len    Chips in one code period (>= 1).
 * @param sample_rate The caller's sample rate, Hz (> 0).
 * @param chip_rate   Chip rate, Hz (> 0).
 * @return The period in samples; 0 for an argument outside the domain.
 */
size_t dp_correlator_search_period (size_t code_len, double sample_rate,
                             double chip_rate);

/**
 * @brief Create a continuous-mode acquisition engine: always wideband
 *        window-tiling, allowing a block-coherent depth inside the
 *        tiles to accommodate waveforms with code-only windows.
 *
 * Builds the single-row oversampled BPSK reference from @p code, infers
 * sf = @p code_len, converts @p cn0_dbhz to a per-sample amplitude SNR,
 * and ALWAYS tiles `window_bins = 2*ceil((doppler_uncertainty - span) /
 * (2*span)) + 1` parallel frequency-window hypotheses, with `span =
 * chip_rate/(2*sf)` one native span (see the file doc comment's "Wideband
 * window-tiling mode") -- unconditionally, so `window_bins = 1` when
 * @p doppler_uncertainty is no wider than one native span.
 * A continuous, data-modulated signal's own bit transitions make coherent
 * multi-epoch combining across DATA a structural aliasing mislock (see
 * docs/design/acq-surface.md §9), so the depth is bounded by what a
 * waveform's code-only window holds: `coherent_bins = D =
 * min((code_only_epochs+1)/2, f_epoch/sqrt(2*doppler_rate))`, at least 1,
 * rounded down to a 7-smooth length (dp_fft_smooth_floor(): 94 runs as
 * 90), run in non-overlapping D-epoch blocks inside every tile (file doc,
 * docs/design/acq-surface.md §4). With @p code_only_epochs = 1 it is 1
 * and the engine is exactly the epoch-by-epoch search. Sensitivity
 * margin beyond the depth comes from auto-selected non-coherent looks
 * over blocks (up to the
 * internal @ref DP_CORRELATOR_N_NONCOH_SAFETY_CEILING).
 *
 * @param code        PN chips (0/1), length @p code_len.
 * @param code_len    Number of chips supplied (= sf, the spreading factor).
 * @param sample_rate The rate of the samples pushed, Hz. One code period
 *                    is then `n = sample_rate * code_len / chip_rate`
 *                    samples, which must be a whole number of at least
 *                    @p code_len: the search's transforms are n points
 *                    long, so a smooth n (a power of two,
 *                    dp_correlator_period_samples()) is what makes them fast.
 *                    When n is a multiple of @p code_len each chip is held
 *                    n / code_len samples; otherwise each sample takes the
 *                    chip its centre falls in, and the correlation shape
 *                    is measured from that replica.
 * @param chip_rate   Chip rate in Hz (> 0).
 * @param symbol_rate Continuous data-symbol rate in Hz; <= 0 means no
 *                    known clock. Diagnostic only (exposed via
 *                    dp_correlator_search_t::epochs_per_symbol), doesn't feed
 *                    sizing: this engine never coherently combines
 *                    regardless of the data-modulation clock.
 * @param cn0_dbhz    Carrier-to-noise density in dB-Hz: any finite value.
 *                    A continuous engine needs one -- its non-coherent
 *                    looks cannot be chosen without a target.
 * @param doppler_uncertainty  One-sided Doppler search half-range in Hz; 0
 * uses the full native span +/- chip_rate/(2*sf) (still window-tiled, at
 * window_bins=1).
 * @param pfa         Target system (max-of-N) false-alarm probability (0,1).
 * @param pd          Target detection probability (0,1).
 * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
 * @param code_only_epochs  Whole code-only epochs a waveform's code-only
 *                    window holds at any chip phase
 *                    (docs/design/async-dsss-receiver.md §2.1:
 *                    `floor(W_symbols * chips_per_symbol / sf) - 1`); 1
 *                    (>= 1) means no window and a depth of 1.
 * @param doppler_rate  Doppler rate in Hz/s the depth is bounded against
 *                    (>= 0); 0 leaves the window as the only bound.
 * @return Heap-allocated state, or NULL on bad arguments / allocation
 * failure.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
 * ...                      length=5).generate(31)).astype(np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
 * ...     np.complex64)
 * >>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0)
 * >>> h = a.push(burst)
 * >>> int(h["row"][0]), int(h["col"][0])    # detects (Doppler-window bin, code phase)
 * (0, 18)
 * >>> (a.code_bins, a.lag_bins)  # 124 samples, read on 128 columns
 * (124, 128)
 * >>> round(18 * a.code_bins / a.lag_bins, 2)  # column 18, in samples
 * 17.44
 * >>> a.coherent_bins            # no window given: one epoch
 * 1
 * >>> b = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0,
 * ...                 code_only_epochs=7)
 * >>> b.coherent_bins            # (7 + 1) // 2: a whole block fits
 * 4
 *
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create_continuous (const uint8_t *code, size_t code_len,
                                    double sample_rate, double chip_rate,
                                    double symbol_rate, double cn0_dbhz,
                                    double doppler_uncertainty, double pfa,
                                    double pd, int noise_mode,
                                    size_t code_only_epochs,
                                    double doppler_rate);

/**
 * @brief How dp_correlator_create() frames its input: maps or detections
 *        from whole frames, or a search over a stream.
 */
typedef enum
{
  DP_CORRELATOR_FRAMING_NONE       = 0, /**< a map or detector flavor   */
  DP_CORRELATOR_FRAMING_CONTINUOUS = 1, /**< the continuous search      */
  DP_CORRELATOR_FRAMING_BURST      = 2  /**< the burst search           */
} dp_correlator_framing_t;

/**
 * @brief Build the flavor the knobs name: the one constructor the Python
 *        class `Correlator` calls.
 *
 * A C caller names the flavor it wants and calls that flavor's own
 * constructor; this one is the binding's entry point
 * (docs/design/correlator-python.md section 3). It reads the knobs, picks
 * the flavor, and calls that constructor with them:
 *
 * | framing      | ref rows | doppler_rows | detect | flavor                                 |
 * | ------------ | -------- | ------------ | ------ | -------------------------------------- |
 * | `NONE`       | 1        | 0            | 0      | dp_correlator_create_1d()              |
 * | `NONE`       | 1        | 0            | 1      | dp_correlator_create_detector_1d()     |
 * | `NONE`       | > 1      | 0            | 0      | dp_correlator_create_2d()              |
 * | `NONE`       | > 1      | 0            | 1      | dp_correlator_create_detector_2d()     |
 * | `NONE`       | 1        | > 0          | 0      | dp_correlator_create_delay_doppler()   |
 * | `CONTINUOUS` | 1        | 0            | any    | dp_correlator_create_continuous()      |
 * | `BURST`      | 1        | 0            | any    | dp_correlator_create_burst()           |
 *
 * A one-row reference is the 1-D flavor: dp_correlator_create_1d() is
 * dp_correlator_create_2d() at one row, and @p nx_out is its padded
 * length.
 *
 * **A value the flavor would not honor is refused**, never ignored: a knob
 * the flavor does not read must hold its default, which is the value named
 * with it below. A search honors @p detect = 1, so it may be either. The
 * refusal returns NULL and points @p why at a sentence that names the knob.
 *
 * The continuous search's reference is NRZ chips as samples: exactly +1
 * (chip 0) or -1 (chip 1), with no imaginary part, the mapping of
 * dp_bin_to_nrz(); any other sample is refused, so a shaped or noisy
 * reference is never quantized silently.
 *
 * The class `Correlator` calls it with the knobs as keywords, and the
 * flavor follows from them; a refusal is a ValueError carrying the
 * sentence @p why points at. The C call, `why` included, is the guide's
 * (docs/guide/correlator.md, "From C").
 *
 * @param ref         Row-major @p ny x @p nx reference, CF32; borrowed.
 * @param ny          Rows of @p ref (1 for a 1-D reference).
 * @param nx          Samples a row.
 * @param framing     Maps and detections, or a search; default NONE.
 * @param doppler_rows Doppler rows of the delay-Doppler map, the code periods
 *                    one frame holds; 0 (default) for no map.
 * @param detect      1 for detections from push(); default 0 (maps).
 * @param dwell       Frames integrated a dump; default 1.
 * @param ny_out      Padded Doppler rows; 0 (default) for native.
 * @param nx_out      Padded lags; 0 (default) for native.
 * @param col_out     The one lag to emit; -1 (default) for every lag.
 * @param threshold   Detection threshold on peak / noise; default 0.
 * @param noise_lo    First noise bin; default 0.
 * @param noise_hi    Last noise bin; default SIZE_MAX, the last bin.
 * @param noise_mode  The noise statistic, a ::det_noise_mode_t; default
 *                    DET_NOISE_MEAN.
 * @param sample_rate Samples a second; 0 (default) is not given, which a
 *                    search refuses.
 * @param chip_rate   Chips a second; 0 (default) is not given, which the
 *                    continuous search refuses.
 * @param symbol_rate Data symbols a second, read by the continuous search
 *                    for a diagnostic only; 0 (default) is no known data
 *                    clock.
 * @param reps        Preamble repetitions a burst; default 1.
 * @param cn0_dbhz    Design-point C/N0, dB-Hz; default
 *                    DP_CORRELATOR_CN0_NONE (NaN, none). The continuous
 *                    search needs a finite one.
 * @param doppler_uncertainty Half-width of the Doppler search, Hz; default 0.
 * @param pfa         False-alarm probability a search; default 1e-3.
 * @param pd          Detection probability at the design point; default 0.9.
 * @param doppler_rate Doppler rate bound, Hz/s; default 0.
 * @param rate_hypotheses Rate hypotheses of the burst search; default 1.
 * @param code_only_epochs Code-only epochs of the continuous search;
 *                    default 1.
 * @param nthreads    Worker threads of a map or detector; default 1. A
 *                    search sizes its own pool and refuses any other
 *                    value here: its count is set after create with
 *                    dp_correlator_set_threads().
 * @param why         Where a refusal's sentence goes; may be NULL. A
 *                    static string, never freed.
 * @return The correlator, or NULL: refused (@p why says which knob) or
 *         out of memory.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> ref = np.zeros(8, np.complex64); ref[0] = 1.0
 * >>> Correlator(ref).flavor                      # a 1-D map
 * '1d'
 * >>> Correlator(ref, detect=1).flavor            # a streaming detector
 * 'detector_1d'
 * >>> Correlator(np.ones((4, 8), np.complex64)).flavor
 * '2d'
 * >>> Correlator(ref, doppler_rows=4).flavor      # a delay-Doppler map
 * 'delay_doppler'
 * >>> Correlator(ref).execute(ref).shape          # one dump of nx lags
 * (8,)
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(31),
 * ...                   np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(np.complex64)
 * >>> b = Correlator(s0, framing="burst", reps=8, sample_rate=4e6,
 * ...                cn0_dbhz=60.0)       # a burst search: 8 repetitions
 * >>> b.flavor
 * 'burst'
 * >>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]
 * >>> int(h["row"]), int(h["col"])     # (Doppler bin, delay in samples)
 * (0, 17)
 * @endcode
 */
dp_correlator_state_t *dp_correlator_create (
    const float _Complex *ref, size_t ny, size_t nx,
    dp_correlator_framing_t framing, size_t doppler_rows, int detect,
    size_t dwell, size_t ny_out, size_t nx_out, int col_out,
    float threshold, size_t noise_lo, size_t noise_hi, int noise_mode,
    double sample_rate, double chip_rate, double symbol_rate, size_t reps,
    double cn0_dbhz, double doppler_uncertainty, double pfa, double pd,
    double doppler_rate, size_t rate_hypotheses, size_t code_only_epochs,
    int nthreads, const char **why);

/**
 * @brief Rate hypothesis @p h of @p n over `[-span, +span]`, Hz/s:
 *        `-span + 2*span*h/(n - 1)`; 0 for `n <= 1`.
 *
 * The engine's grid, exported so a composing object searches the SAME
 * rates and not a restatement of them (dp_burst_capture's refine).
 *
 * @param span  Half-width of the rate search, Hz/s.
 * @param n     Number of hypotheses on the grid.
 * @param h     Hypothesis index, 0 .. n - 1 (not range-checked).
 * @return The rate of hypothesis @p h, Hz/s.
 */
double dp_correlator_rate_hypothesis (double span, size_t n, size_t h);

/**
 * @brief The de-chirp phasor `exp(-j*pi*rate*t^2)` for a rate in Hz/s at
 *        time @p t seconds from the block's middle.
 *
 * What the engine multiplies a block's slow-time axis by, and what refine
 * multiplies its per-period correlations by: one definition of the centring
 * and the sign.
 *
 * @param rate  Doppler rate hypothesis, Hz/s.
 * @param t     Time from the block's middle, seconds.
 * @return `exp(-j*pi*rate*t^2)`, unit magnitude.
 */
double _Complex dp_correlator_rate_phasor (double rate, double t);

/**
 * @brief The fewest rate hypotheses over `[-span, +span]` that leave the
 *        residual `span/(n - 1)` within what a block of @p depth epochs
 *        tolerates, `f_epoch^2/(2*depth^2)` -- the drift rule behind the
 *        depth cap, inverted. At least 2 for a positive span; 1 for none;
 *        at most @ref DP_CORRELATOR_RATE_HYPS_MAX.
 *
 * @param f_epoch  Epoch rate, Hz (`fs/n` for a burst engine).
 * @param span     Rate span, Hz/s (>= 0).
 * @param depth    Block depth in epochs (>= 1).
 * @return The hypothesis count; 1 when @p span is not positive and
 *         finite, @p f_epoch is not positive and finite, or @p depth
 *         is 0.
 */
size_t dp_correlator_rate_hypotheses_for (double f_epoch, double span,
                                   size_t depth);



/**
 * @brief Pin the search grid directly, bypassing both auto-sizing
 *        searches — the advanced escape hatch (mirrors Dll's/Costas's
 *        configure_lock_raw()).
 *
 * Resizes every buffer/plan that depends on the grid (the slow-time FFT,
 * the code correlator, the reference, and every per-frame scratch buffer),
 * re-derives the threshold ladder for the pinned grid from the same
 * physics dp_correlator_create_burst()/dp_correlator_create_continuous() used, and clears
 * in-flight accumulation (ring contents, the non-coherent power
 * accumulator, dwell bookkeeping) — call between push() calls, never a
 * substitute for one.
 *
 * @param state        Allocated engine (non-NULL).
 * @param doppler_bins Coherent depth to pin, in `[1, reps]`.
 * @param n_noncoh     Non-coherent look count to pin, in
 *                     `[1, DP_CORRELATOR_N_NONCOH_SAFETY_CEILING]`.
 *
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return 0 on success; DP_ERR_UNSUPPORTED on a map or detector flavor
 *         (no search record); DP_ERR_INVALID if either argument is out
 *         of range (or @p n_noncoh > 1 on an engine with rate hypotheses,
 *         which do not compose with non-coherent looks); DP_ERR_MEMORY if
 *         an allocation fails (the engine is left usable at its prior
 *         grid).
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
 * ...                      length=5).generate(31)).astype(np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
 * ...     np.complex64)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0)
 * >>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
 * >>> a.doppler_bins, a.n_noncoh
 * (1, 4)
 * >>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
 * >>> h = a.push(burst)
 * >>> int(h["row"][0]), int(h["col"][0])      # detects at the pinned grid
 * (0, 18)
 *
 * @endcode
 */
int dp_correlator_configure_search_raw (dp_correlator_state_t *state, size_t doppler_bins,
                              size_t n_noncoh, const char **why);

/**
 * @brief How many peaks a dwell may report: the peak list's capacity.
 *
 * One (the default) is the classic detector -- the maximum of the surface,
 * gated. More is the list of docs/design/async-dsss-receiver.md §7.1:
 * every peak above the same gate, strongest first, each with an exclusion
 * zone of one Doppler bin by the reference's first autocorrelation null
 * around it (one chip for a PN code; one emitter's main lobe,
 * so its own shoulders are not the next peak), and the two-epoch rule
 * for a peak at an already-listed code phase -- a data transition inside
 * the epoch splits one emitter into twins at its own code phase on other
 * tiles, so such a peak is held for one dwell and listed only if it was
 * there, at the same tile, on the previous one. Each listed peak is one
 * dp_correlator_hit_t from dp_correlator_push(), all of a dwell's sharing its
 * `samples_consumed` and `noise_est`. A held twin takes a slot of the
 * `n` for that dwell but is not reported. The threshold does not change:
 * a second peak is another draw from the same cells against the same
 * union bound. Clears the held candidates.
 *
 * @param state  Must be non-NULL.
 * @param n      1 … DP_CORRELATOR_MAX_PEAKS.
 *
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return 0; DP_ERR_UNSUPPORTED off a search (a map or detector lists
 *         no peaks); DP_ERR_INVALID (state untouched) when @p n is out of
 *         range.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, symbol_rate=1e3,
 * ...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
 * >>> a.max_peaks
 * 1
 * >>> a.set_max_peaks(8)
 * >>> a.max_peaks
 * 8
 * @endcode
 */
int dp_correlator_set_max_peaks (dp_correlator_state_t *state, size_t n, const char **why);

/**
 * @brief Couple the code clock to the carrier: the chip rate dilates by
 *        `doppler_hz / carrier_freq_hz`, and the engine accounts for it.
 *
 * A physically-coupled Doppler moves the code as well as the carrier
 * -- 100 chips/s at 20 ppm of 5 Mcps -- and the engine's two long
 * integrations both smear over it (doppler#1256, #1254):
 * - **Inside a coherent block** of D epochs every tile's epoch
 *   correlations are shifted along the code axis by the drift the
 *   tile's own frequency implies, `f_tile / carrier` chips per chip,
 *   aligned to the block's middle (a linear phase on each epoch's
 *   product). The one delay-Doppler map applies it at the block's end,
 *   read on a slow-time grid 4 times finer and rounded. The bound is
 *   0.09 dB, the worst over 33 tones and three dilations
 *   (docs/design/acq-surface.md §18.1); the engine measures 0.020-0.026
 *   dB on its equivalence test's carrier scenes.
 *   dp_correlator_block_prompt() applies it exactly. A change of carrier
 *   mid-block applies to the whole block. Measured at the specification's
 *   20 ppm with D = 154 (3.1 chips of
 *   drift across the block): without it the block's peak is 13 dB
 *   down and 3 chips wide and the depth detects nothing at 34 dB-Hz;
 *   with it the block reads as a still one.
 * - **The hand-off** (dp_correlator_build_handoff()) advances the hit's code
 *   phase by the drift over half the dwell -- the non-coherent sum's
 *   peak is the phase at the dwell's middle, the seed is wanted at its
 *   end: 0.9 chip at the 40 dB-Hz floor, past a refine loop's pull-in.
 *
 * A mutator's value (#2022): it rides in the state blob (v8), and a
 * restore takes it through this setter's rebuild, so a resumed engine
 * carries it on. Setting it rebuilds the map's fine grid and every
 * tile's ramp tables when they move (the regrid that re-applies the
 * thread request). Default 0.0 (uncoupled) is the engine exactly as it
 * ran without it.
 *
 * @param state           Must be non-NULL.
 * @param carrier_freq_hz RF carrier, Hz; 0.0 = uncoupled.
 * Refused (DP_ERR_UNSUPPORTED) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18).
 *
 * @return `DP_OK`, `DP_ERR_INVALID` for a carrier outside
 *         dp_correlator_carrier_freq_ok() at this engine's sample rate, or
 *         `DP_ERR_MEMORY` when the rebuild cannot allocate (the engine
 *         is left as it was).
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=1e7, chip_rate=5e6, symbol_rate=2700.0,
 * ...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)
 * >>> a.carrier_freq_hz
 * 0.0
 * >>> a.set_carrier_freq_hz(2.5e9)
 * >>> a.carrier_freq_hz
 * 2500000000.0
 *
 * @endcode
 */
int dp_correlator_set_carrier_freq_hz (dp_correlator_state_t *state, double carrier_freq_hz);

/**
 * @brief Whether @p carrier_freq_hz is a carrier the code can be coupled
 *        to at @p sample_rate_hz: 0 (uncoupled), or finite and above half
 *        the sample rate.
 *
 * Every coupled stage divides a Doppler by the carrier: the hand-off's
 * dilation, a coherent block's epoch alignment, a code loop's rate aid.
 * A seed a receiver accepts has a Doppler below half its sample rate
 * (dp_async_dsss_receiver_seed_ok()), so a carrier above that keeps
 * |doppler / carrier| < 1, the physical v/c, when the hand-off is taken
 * from a seed. The acquisition search itself does not yet bound the
 * Doppler it reports to that range (doppler#2112 item 7); a hit past it
 * is refused as a seed rather than used, so the bound holds at the
 * receiver even where the search does not enforce it.
 * A tiny carrier sent doppler / carrier to inf, so the seed
 * phase came out NaN, and the receiver aborted building its code loop
 * from it (doppler#2103). The one rule for every site that takes the
 * divisor: dp_correlator_set_carrier_freq_hz() and the DSSS receivers' creates.
 *
 * @param carrier_freq_hz  Candidate carrier, Hz; 0 means uncoupled.
 * @param sample_rate_hz   Sample rate of the stream, Hz.
 * @return 1 if the carrier is taken, 0 otherwise (a NaN is not).
 */
int dp_correlator_carrier_freq_ok (double carrier_freq_hz, double sample_rate_hz);

/**
 * @brief Set how many threads the searcher fans its tiles across
 *        (docs/design/acq-surface.md §13: a roll per thread on
 *        persistent workers).
 *
 * A continuous engine is created with a pool of the machine's online
 * cores when it has more than one tile; a burst engine, and a
 * single-tile one, run serially. This sets the count: 0 auto-selects the
 * online core count, 1 runs everything on the calling thread, n runs on
 * n workers (the caller included) -- but never more than `window_bins`:
 * every fan hands out one tile a task, so a worker past that only wakes
 * and waits. The request is kept, and every regrid re-applies it, so
 * an engine narrowed to one tile and widened again runs on
 * min(n, tiles), not on the 1 the narrow grid ran on. The workers are
 * created here, once, and parked between pushes; nothing is created per
 * push. The surface
 * is bit-identical at every count -- the tiles are independent after the
 * one forward transform and each writes its own rows -- so this changes
 * the cost of a push and nothing about its result. Setup path, never
 * hot; not while another thread is inside push().
 *
 * @param state Must be non-NULL.
 * @param n     Thread count; 0 = online cores, 1 = serial.
 * Refused (DP_ERR_UNSUPPORTED) on a map or detector flavor (no search record).
 *
 * @return DP_OK. The count actually running is `threads`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(
 * ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
 * ...                 doppler_uncertainty=4000.0)
 * >>> a.threads >= 1               # at most one worker a tile
 * True
 * >>> a.set_threads(1)
 * >>> a.threads
 * 1
 *
 * @endcode
 */
int dp_correlator_set_threads (dp_correlator_state_t *state, int n);
/**
 * @brief Attach (or detach) a telemetry context and register the
 *        engine's probes on it (docs/design/async-dsss-receiver.md
 *        §2.4).
 *
 * Registers ten probes, emitted once per DECIDED dwell (a coherent dump,
 * or the dwell that completes `n_noncoh` looks) and further thinned by
 * @p decim: "<prefix>.stat" (the dwell's test statistic — the strongest
 * cell against the CFAR reference, in the units the gate is set in),
 * "<prefix>.gate" (that gate: `threshold` on the coherent path, `eta_nc`
 * on the non-coherent one — plotted together they show exactly where a
 * hit fired), "<prefix>.noise" (the CFAR reference `noise_est`),
 * "<prefix>.peak" (the strongest cell's raw value), "<prefix>.row" and
 * "<prefix>.col" (its native Doppler row and code-phase column — a
 * surface coordinate, not a physical unit; dp_correlator_surface_doppler_hz() and
 * dp_correlator_surface_chip_phase() convert), "<prefix>.n_peaks" (picks in the
 * dwell, held twins included), "<prefix>.n_held" (picks held as
 * same-code-phase twins rather than listed, §7.1), "<prefix>.conc" (the
 * strongest pick's concentration — see `peak_conc`: its main lobe's
 * power over its whole column's, near 1 for one clean emitter even
 * when it straddles two tiles, about 0.5 when a data transition splits
 * it into twins two or more tiles away, lower still when a coherent
 * block straddles data — the discriminator between one emitter's
 * splatter and a second emitter) and
 * "<prefix>.hit" (1 when the gate fired). Passing NULL detaches. Setup
 * path, never hot; the context is borrowed and must outlive the
 * attachment (SPSC rules in dp_tlm/dp_tlm_core.h).
 *
 * @param state  Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "acq" or "ch0.acq".
 * @param decim  Emit every decim-th decided dwell; >= 1.
 * Refused (DP_ERR_UNSUPPORTED) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18).
 *
 * @param why   Where the refusal's sentence goes, naming the argument it
 *              refused; may be NULL. A static string, never freed. Off the
 *              flavors that serve the call it stays NULL (the binding then
 *              raises the declared "only" sentence; just-makeit#2574).
 * @return DP_OK; DP_ERR_INVALID for an overlong @p prefix, a @p decim
 *         of 0, or a probe table without room for all ten probes;
 *         DP_ERR_MEMORY when the dry run's scratch context cannot be
 *         allocated. A refusal registers nothing on @p tlm and leaves the
 *         previous attachment (or detachment) exactly as it was.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.telemetry import Telemetry
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(
 * ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
 * >>> tlm = Telemetry(1 << 12)
 * >>> a.set_telemetry(tlm, "acq")
 * >>> sorted(tlm.probe_names)[:3]
 * ['acq.col', 'acq.conc', 'acq.gate']
 * >>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)
 * >>> _ = a.push(x)
 * >>> len(tlm.read()) % 10      # ten records per decided dwell
 * 0
 *
 * @endcode
 */
int dp_correlator_set_telemetry (dp_correlator_state_t *state, dp_tlm_t *tlm,
                       const char *prefix, uint32_t decim, const char **why);

/**
 * @brief The last decided dwell's surface, in the gate's own units.
 *
 * Copies the surface the last dwell was decided on into @p out, row-major
 * `surface_rows` (Doppler: tiles, or interpolated slow-time rows) by
 * `lag_bins` (code phase), every cell divided by the same CFAR reference
 * the gate used — so a cell reads as its own test statistic, to a float
 * rounding (the SIMD build's fast-math may take a reciprocal in this
 * loop and a divide in the gate's), and the gate (`threshold`, or
 * `eta_nc` on the non-coherent path) is a flat plane on a plot.
 *
 * **A surface is kept while `keep_surface` is set OR a sink is attached**
 * (dp_correlator_set_surface_sink()); this is the one statement of that
 * rule. Set either, push, then read. `surface_at` says which dwell it is;
 * a time-decimated record is the caller reading every k-th dwell, or a
 * sink with `decim`.
 *
 * @param state Must be non-NULL.
 * @param out   At least `surface_rows * lag_bins` floats.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18); 0 is "nothing yet".
 *
 * @return Cells written (`surface_rows * lag_bins`), or 0 when no dwell
 *         has been decided while a surface was kept, or @p out is too
 *         small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(
 * ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
 * >>> a.keep_surface = 1
 * >>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)
 * >>> _ = a.push(x)
 * >>> s = np.empty(a.surface_rows * a.lag_bins, dtype=np.float32)
 * >>> a.surface(s) == s.size
 * True
 * >>> a.code_bins, a.lag_bins  # 1022 is 2 x 7 x 73: read on 1024 lags
 * (1022, 1024)
 * >>> s.reshape(a.surface_rows, a.lag_bins).shape == (a.surface_rows, 1024)
 * True
 *
 * @endcode
 */
size_t dp_correlator_surface (dp_correlator_state_t *state, float *out, size_t n_out);

/**
 * @brief The surface's Doppler axis: the frequency of each row, in Hz.
 *
 * One value per surface row, the fold and scale a hit's `doppler_hz_est`
 * uses (dp_fftfreq_index() times `doppler_res_hz`, on the interpolated
 * grid where the slow-time axis is interpolated), so a plot of
 * dp_correlator_surface() carries the same axis a DetectionEvent reports on.
 *
 * @param state Must be non-NULL.
 * @param out   At least `surface_rows` doubles.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18); 0 is "nothing yet".
 *
 * @return Values written (`surface_rows`), or 0 if @p out is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(
 * ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
 * ...                 doppler_uncertainty=4000.0)
 * >>> f = np.empty(a.surface_rows, dtype=np.float64)
 * >>> a.surface_doppler_hz(f) == a.surface_rows
 * True
 * >>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())
 * True
 *
 * @endcode
 */
size_t dp_correlator_surface_doppler_hz (dp_correlator_state_t *state, double *out,
                               size_t n_out);

/**
 * @brief The surface's code-phase axis: the chip phase of each column.
 *
 * One value per surface column, in chips, the same mapping
 * dp_correlator_build_handoff() applies to a hit's `col` — so a plotted
 * peak sits at the chip phase the DetectionEvent would carry.
 *
 * @param state Must be non-NULL.
 * @param out   At least `lag_bins` doubles.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18); 0 is "nothing yet".
 *
 * @return Values written (`lag_bins`), or 0 if @p out is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(
 * ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
 * >>> c = np.empty(a.lag_bins, dtype=np.float64)
 * >>> a.surface_chip_phase(c) == a.lag_bins
 * True
 * >>> bool(c[0] == 0.0 and c[1] == 511 - 511 / 1024)
 * True
 *
 * @endcode
 */
size_t dp_correlator_surface_chip_phase (dp_correlator_state_t *state, double *out,
                               size_t n_out);

/**
 * @brief The last decided dwell's surface, complex: amplitude and
 *        carrier phase per cell, before the magnitude the gate reads.
 *
 * Copies the coherent sum the last dwell was decided on into @p out,
 * row-major `surface_rows` x `lag_bins` like dp_correlator_surface(), in the
 * correlation's own units rather than the gate's. A cell's phase is
 * the carrier at the block's middle, relative to its tile's centre; its
 * neighbours along the code axis are complex early and late arms, so a
 * tracker can form the coherent discriminator
 * `Re(conj(P) (L - E)) / |P|^2`, which the magnitude surface cannot
 * (docs/design/async-dsss-receiver-measurements.md
 * §12.21). Coherent path only: a non-coherent dwell
 * (`n_noncoh > 1`) is a power sum with no phase, and reads 0.
 *
 * @param state Must be non-NULL.
 * @param out   At least `surface_rows * lag_bins` complex floats.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on a map or detector flavor (no search record); 0 is
 * "nothing yet".
 *
 * @return Cells written (`surface_rows * lag_bins`), or 0 when no dwell
 *         has been decided, the path is non-coherent, or @p out is too
 *         small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
 * ...                      length=5).generate(31)).astype(np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
 * ...     np.complex64)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> a = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=70.0)
 * >>> a.n_noncoh                 # one look: the dwell is the coherent dump
 * 1
 * >>> _ = a.push(np.roll(s0, 17).astype(np.complex64))
 * >>> sc = np.empty(a.surface_rows * a.lag_bins, dtype=np.complex64)
 * >>> a.surface_complex(sc) == sc.size
 * True
 * >>> int(np.argmax(np.abs(sc)) % a.lag_bins)   # the peak's lag column
 * 18
 *
 * @endcode
 */
size_t dp_correlator_surface_complex (dp_correlator_state_t *state, float _Complex *out,
                            size_t n_out);

/**
 * @brief One epoch's correlation against the replica at ONE code phase
 *        and ONE frequency: the engine's surface, evaluated at a single
 *        cell, on raw samples.
 *
 * `sum_m x[m] * conj(ref[(m - col) mod nx]) * exp(-j 2 pi f (t0 + m)/fs)`
 * over the `code_bins` samples of @p x. The mixer phase is referenced to
 * @p t0, so epochs evaluated at consecutive `t0` combine COHERENTLY: sum
 * the returned values across epochs and the result is a depth-`len` cell
 * of the slow-time surface at @p f_hz, with no intra-epoch rotation loss.
 *
 * It is what a caller does once acquisition has SETTLED the code phase and
 * the Doppler: evaluate the same statistic elsewhere without searching the
 * grid again. The engine's tile-alias check uses it; burst_capture's
 * refine uses it to score each candidate repetition (doppler#1502).
 *
 * @param state  The engine whose replica, `code_bins` and `fs` are used.
 * @param x      One epoch, `code_bins` samples.
 * @param col    Code phase, samples, in [0, code_bins).
 * @param f_hz   Mixer frequency, Hz.
 * @param t0     Time of the first sample of @p x, in samples, on the
 *                caller's reference.
 * @return The complex correlation.
 */
double _Complex dp_correlator_cell_corr (const dp_correlator_state_t *state,
                               const float _Complex *x, size_t col,
                               double f_hz, double t0);

/**
 * @brief dp_correlator_cell_corr() at many frequencies over consecutive epochs, in
 *        one pass: the cells a caller scores once the code phase is
 *        settled and only the Doppler is left to search.
 *
 * Element `e * n_f + j` of @p out is dp_correlator_cell_corr() of epoch `e`
 * (`x + e*code_bins`, at `t0 + e*code_bins`) at the j-th frequency, to
 * within 1e-5 of the epoch's `sum |x * ref|`, the most either can reach
 * (measured 2.9e-6 at a 510-sample epoch): a coherent peak 100 dB above
 * the difference.
 *
 * How: evaluated one cell at a time, every frequency repeats the whole
 * despread-and-mix of the epoch. Here the replica is multiplied once per
 * call by a mixer at the frequencies' centre, so each sample costs one
 * multiply, summed over B equal blocks per epoch with the first and second
 * moments of the time within each block. What is left of a frequency is a
 * residual of at most `r` cycles per epoch either side of the centre;
 * B = ceil(48 r) blocks keep it turning under 1/96 of a cycle either side
 * of a block's centre, and it is applied per block from the moments as a
 * second-order expansion, whose remainder is the error above. So the cost
 * per epoch is `code_bins + 3 * B * n_f` against `n_f * code_bins`, and
 * burst_capture's refine, which scores every Doppler cell of every
 * candidate period, is the reason it exists (doppler#1538).
 *
 * Frequencies spread more than one cycle per epoch either side of their
 * centre fall back to dp_correlator_cell_corr() per cell.
 *
 * @param state    The engine whose replica, `code_bins` and `fs` are used.
 * @param x        `n_epochs * code_bins` contiguous samples.
 * @param n_epochs Epochs to evaluate.
 * @param col      Code phase, samples, in [0, code_bins).
 * @param f_hz     `n_f` mixer frequencies, Hz, in any order.
 * @param n_f      Number of frequencies.
 * @param t0       Time of the first sample of @p x, in samples, on the
 *                 caller's reference.
 * @param out      Written with `n_epochs * n_f` correlations, epoch-major.
 */
void dp_correlator_cell_corr_grid (const dp_correlator_state_t *state, const float _Complex *x,
                         size_t n_epochs, size_t col, const double *f_hz,
                         size_t n_f, double t0, double _Complex *out);

/**
 * @brief One cell's column of the last whole block: the per-epoch
 *        complex correlations at a code phase, the despread stream at
 *        epoch rate.
 *
 * The block-coherent engine gathers every tile's correlation row for
 * `coherent_bins` epochs before its slow-time transform (file doc,
 * docs/design/acq-surface.md §4). This copies the `coherent_bins` values
 * at column @p col of tile @p tile into @p out, in epoch order: each
 * epoch's complex prompt at that code phase, rolled to the tile's centre
 * frequency and shifted to the block's middle by the tile's code-rate
 * hypothesis, so the phase is continuous along the column for an
 * emitter at the tile's centre and rotates at its offset from it. At
 * an emitter's cell this is what a despreader produces, one value per
 * epoch, phase included (docs/design/async-dsss-receiver-measurements.md
 * §12.21). Valid once a block is whole,
 * until the next epoch is pushed.
 *
 * @param state Must be non-NULL.
 * @param tile  Tile index, `0 … window_bins-1` (native FFT order, the
 *              order the surface's rows are cut in).
 * @param col   Code-phase column, `0 … lag_bins-1`.
 * @param out   At least `coherent_bins` complex floats.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18); 0 is "nothing yet".
 *
 * @return Values written (`coherent_bins`), or 0 at `coherent_bins == 1`
 *         (no block is gathered), while a block is partial, for an index
 *         out of range, or when @p out is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
 * ...                      length=5).generate(31)).astype(np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
 * ...     np.complex64)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> b = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
 * ...                 doppler_uncertainty=40e3, code_only_epochs=7)
 * >>> b.coherent_bins                    # (7 + 1) // 2
 * 4
 * >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
 * >>> _ = b.push(blk)
 * >>> p = np.empty(b.coherent_bins, dtype=np.complex64)
 * >>> b.block_prompt(0, 17, p) == b.coherent_bins
 * True
 * >>> bool(np.abs(p).min() > 0.99 * np.abs(p).max())  # every epoch's prompt
 * True
 * >>> b.block_prompt(0, 17, np.empty(1, dtype=np.complex64))  # too small
 * 0
 *
 * @endcode
 */
size_t dp_correlator_block_prompt (dp_correlator_state_t *state, size_t tile, size_t col,
                         float _Complex *out, size_t n_out);

/**
 * @brief The last whole block's raw samples, as pushed.
 *
 * Copies the `coherent_bins * code_bins` samples the block-coherent
 * engine gathered for its last whole block into @p out, epoch by epoch
 * in stream order — the samples dp_correlator_push() consumed, untouched. Kept
 * for the tile-edge re-ask (search_resolve_tile_alias()); exposed so a
 * tracker can re-correlate them at any code phase, rate or symbol
 * boundary the engine's own grid does not have — a symbol-rate
 * despreader at the tracked timing runs on exactly this
 * (docs/design/async-dsss-receiver-measurements.md
 * §12.21). Valid once a block is whole, until the next epoch is
 * pushed.
 *
 * @param state Must be non-NULL.
 * @param out   At least `coherent_bins * code_bins` complex floats.
 * @param n_out Capacity of @p out.
 * SIZE_MAX (refused) on every flavor but the continuous search
 * (docs/design/correlator-python.md section 18); 0 is "nothing yet".
 *
 * @return Samples written (`coherent_bins * code_bins`), or 0 at
 *         `coherent_bins == 1`, while a block is partial, or when @p out
 *         is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.wfm import PN, mls_poly
 * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
 * ...                      length=5).generate(31)).astype(np.uint8)
 * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
 * ...     np.complex64)
 * >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
 * >>> b = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
 * ...                 doppler_uncertainty=40e3, code_only_epochs=7)
 * >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
 * >>> _ = b.push(blk)
 * >>> raw = np.empty(b.coherent_bins * b.code_bins, dtype=np.complex64)
 * >>> b.block_raw(raw) == raw.size
 * True
 * >>> bool(np.array_equal(raw, blk))      # the samples as pushed
 * True
 *
 * @endcode
 */
size_t dp_correlator_block_raw (dp_correlator_state_t *state, float _Complex *out,
                      size_t n_out);

/**
 * @brief Attach (or detach) a C surface sink: every @p decim-th decided
 *        dwell's surface, in test-statistic units, handed to @p fn on the
 *        pushing thread (docs/design/async-dsss-receiver.md §2.4).
 *
 * Sets `keep_surface`, so dp_correlator_surface() reads the same dwell afterwards.
 * While a sink is attached every decided dwell's surface is computed and
 * handed over whatever `keep_surface` reads: a dp_correlator_set_state()
 * (whose blob carries `keep_surface`, not the sink, which is the caller's
 * and stays attached) or dp_correlator_set_keep_surface(0) leaves it
 * firing. Detach with @p fn NULL.
 * The pointer handed to @p fn is the engine's and is valid only for the
 * call: copy or write it out there. This is how a long run records the
 * surface decimated in time without a copy per dwell it does not keep.
 *
 * @param state Must be non-NULL.
 * @param fn    The sink, or NULL to detach.
 * @param ctx   Passed through to @p fn.
 * @param decim Hand over every decim-th dwell; 0 reads as 1.
 * @return DP_OK; DP_ERR_UNSUPPORTED, with nothing attached, off a search
 *         (a map or a detector decides no dwell).
 */
int dp_correlator_set_surface_sink (dp_correlator_state_t *state, dp_correlator_surface_sink_fn fn,
                           void *ctx, uint32_t decim);

/**
 * @brief Ask to keep, or stop keeping, every decided dwell's surface for
 *        dp_correlator_surface(): 1 normalises each one into the gate's
 *        units at its decision; 0 (the default) keeps none unless a sink
 *        is attached (dp_correlator_surface() states the rule).
 *
 * Python's writable Correlator.keep_surface; dp_correlator_set_surface_sink()
 * sets it too, and a search flavor's other setters leave it as it is.  The
 * continuous search only (docs/design/correlator-python.md section 18):
 * any other flavor refuses it and keeps nothing.
 *
 * @param state  Must be non-NULL.
 * @param keep   Non-zero to keep (stored as 1), 0 not to.
 * @return DP_OK, or DP_ERR_UNSUPPORTED on any flavor but the continuous
 *         search.
 */
int dp_correlator_set_keep_surface (dp_correlator_state_t *state, int keep);



/**
 * @brief The engine's stream position: every sample its pushes have
 *        taken, framed ones and the carry, since create, reset, a regrid
 *        or set_state's restore.
 *
 * The timebase dp_correlator_hit_t::samples_consumed is counted in. Read at the
 * start of a push, it is where that push's input begins, so a hit from
 * that push ended at x + (hit.samples_consumed - position): the hand-off
 * a tracker resumes from. A caller's own count of what it has fed is a
 * different timebase -- configure_search_raw() and set_state() reset or
 * restore the engine and not the caller's count -- and subtracting the
 * two reads outside x.
 *
 * @code
 * float _Complex pre[124];               // a 31-chip code at 4x
 * for (size_t i = 0; i < 124; i++)
 *   pre[i] = (i / 4) % 3 ? 1.0f : -1.0f;
 * dp_correlator_state_t *a = dp_correlator_create_burst (pre, 124, 16, 4.0e6, 45.0,
 *                                          0.0, 1e-3, 0.9, 0, 0.0, 1);
 * float _Complex x[300] = { 0 };
 * uint64_t       before = dp_correlator_position (a);           // 0
 * dp_correlator_hit_t   hit;
 * if (dp_correlator_push (a, x, 300, &hit, 1) == 1)
 *   printf ("hit ended at x + %llu\n",
 *           (unsigned long long)(hit.samples_consumed - before));
 * uint64_t after = dp_correlator_position (a); // 300: framed and carried
 * dp_correlator_destroy (a);
 * if (before != 0 || after != 300)
 *   return 1;
 * @endcode
 *
 * @param state  Must be non-NULL.
 * 0 on a map or detector flavor (no search record).
 *
 * @return Samples taken since create, reset, a regrid or a restore:
 *         those framed plus the carry still pending.
 */
uint64_t dp_correlator_position (const dp_correlator_state_t *state);

/**
 * @brief Wire-ready hand-off record built from one dp_correlator_hit_t hit.
 *
 * Every field is in a physical unit and the record is flat and
 * pointer-free, so it can cross a thread or process boundary. It carries
 * no timestamp: whatever feeds the engine samples owns the clock and
 * stamps the record at its `samples_consumed`. The fields are tabulated
 * in docs/design/async-dsss-receiver.md §2.2.
 */
typedef struct
{
  uint64_t samples_consumed; /**< Raw-sample offset this hit ended at. */
  double
      chip_phase; /**< Chips, Dll's own instantaneous-phase convention
                       (the mirror image of dp_correlator_hit_t::col's
                       correlation-lag convention -- see
                       dp_correlator_build_handoff()'s doc comment). */
  double doppler_hz_est;  /**< Folded/signed Doppler estimate, Hz. */
  double doppler_res_hz;  /**< Width of the estimate (+/- half this). */
  double cn0_dbhz_est;    /**< Estimated carrier-to-noise density, dB-Hz. */
  float  peak_mag;        /**< Raw CFAR peak magnitude, diagnostic. */
  float  noise_est;       /**< Raw CFAR noise-floor estimate, diagnostic. */
  float  test_stat;       /**< Raw CFAR gating statistic, diagnostic. */
} dp_correlator_handoff_t;

/* The FFT-bin convention this engine reports in -- `0 = DC`, ascending
 * positive, then wrapping negative -- is `dp_fftfreq_index()` in
 * clib_common.h, and its doc comment there is the one definition. It was
 * declared here, and four call sites outside C restated the fold in three
 * mutually inconsistent ways; the engine's wideband search and its own
 * hand-off were two of them, which surfaced as a receiver reporting
 * `tracking == 1` while decoding noise. Every consumer -- this engine's
 * search, its hand-off, and any composing receiver -- now includes the
 * SAME inline rather than restating the formula. */

/**
 * @brief The Doppler grid a hit's `row` indexes: `window_bins *
 *        coherent_bins` bins.
 *
 * One bin in a native search; the window-tile count once a
 * @p doppler_uncertainty wider than the native span is tiled; tiles times
 * the block depth on a continuous engine with code-only epochs. Every
 * consumer that folds a `row` must fold it over THIS, not over
 * `coherent_bins`: a capture that did read every tiled hit as 0 Hz
 * (doppler#1512). 0 on a map or detector flavor, which has no Doppler grid.
 */
static inline size_t
dp_correlator_grid_bins (const dp_correlator_state_t *state)
{
  /* 0 off a search flavor, which has no Doppler grid (§18). */
  return state->search
             ? state->search->window_bins * state->search->coherent_bins
             : 0;
}

/**
 * @brief A hit's signed Doppler, Hz: its `row` folded over
 *        dp_correlator_grid_bins() by numpy's fftfreq convention (dp_fftfreq_index)
 *        and scaled by the grid's resolution.
 *
 * The one conversion from a bin to Hz; dp_correlator_build_handoff() and every
 * composing object call it rather than restating the fold.
 */
static inline double
dp_correlator_bin_doppler_hz (const dp_correlator_state_t *state, size_t doppler_bin)
{
  return (double)dp_fftfreq_index (doppler_bin, dp_correlator_grid_bins (state))
         * state->search->doppler_res_hz;
}

/**
 * @brief The preamble's peak sidelobe level, dB: 20*log10 of the largest
 *        periodic-autocorrelation lag outside the mainlobe, relative to
 *        the peak (dp_correlator_shape_t::psl).
 *
 * What it predicts: a detection's sidelobes sit this far below it, at
 * delays outside the peak zone -- where the peak list's exclusion does
 * not reach. So a burst that clears the threshold by more than
 * `-psl_db` also lists its own sidelobe as a second peak (with
 * `max_peaks > 1`), and a strong burst's sidelobe can mask a weak one
 * there. A code's is its periodic floor, 20*log10(1/31) = -29.8 dB for
 * a 31-chip m-sequence; a perfect sequence (Zadoff-Chu, a Frank-type
 * chirp) has none and reads -INFINITY. Configuration, not state: it is
 * fixed at construction and not serialized.
 *
 * @param state  Must be non-NULL.
 * NaN on a map or detector flavor (no search record).
 *
 * @return Peak sidelobe level in dB (<= 0), or -INFINITY.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.correlation import Correlator
 * >>> from doppler.cvt import bin_to_nrz
 * >>> from doppler.wfm import PN
 * >>> chips = (np.asarray(PN(poly=0, seed=1, length=5).generate(31)) & 1
 * ...          ).astype(np.uint8)
 * >>> nrz = np.zeros(31, dtype=np.float32)
 * >>> _ = bin_to_nrz(chips, nrz)
 * >>> pre = np.repeat(nrz, 4).astype(np.complex64)    # 4 samples a chip
 * >>> a = Correlator(pre, framing="burst", reps=4, sample_rate=4.0e6, cn0_dbhz=61.0)
 * >>> round(a.psl_db, 2)          # an m-sequence: 1/31
 * -29.83
 * >>> k = np.arange(127)
 * >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)
 * >>> Correlator(zc, framing="burst", reps=8, sample_rate=1.0e6, cn0_dbhz=53.0).psl_db
 * -inf
 *
 * @endcode
 */
double dp_correlator_psl_db (const dp_correlator_state_t *state);

/**
 * @brief The search's Doppler-offset sidelobe level, dB: 20*log10 of the
 *        largest lag of the code's periodic ambiguity outside the
 *        mainlobe, at any Doppler offset the grid's tiles can put between
 *        an emitter and a tile, relative to the emitter's own peak
 *        (dp_correlator_shape_t::dsl).
 *
 * What it predicts: one emitter's peak list carries peaks up to this far
 * below its own, more than a mainlobe from its own lag, in the tiles it
 * is not in -- and they stay there dwell after dwell, being the code's,
 * not the noise's. A list consumer that skips the strongest peak (a
 * rank, an exclusion) must not take one for a second emitter. It is at
 * least dp_correlator_psl_db(), which is its zero-offset case, and equals it on
 * a single-tile grid searched at one Doppler. -16.95 dB for the
 * receivers' Gold-1023 search at two samples a chip over +-5 kHz (five
 * 3 kHz tiles), against a -23.9 dB psl. Configuration, not state: fixed
 * at construction, kept by a grid pinned later, and not serialized.
 * Computed on the first call and cached in @p state (hence non-const).
 *
 * @param state  Must be non-NULL.
 * NaN on a map or detector flavor (no search record).
 *
 * @return Doppler-offset sidelobe level in dB, or -INFINITY when there
 *         is none.
 *
 * @code
 * #include "doppler/correlator/correlator_core.h"
 * #include "doppler/gold/gold_core.h"
 * #include <stdio.h>
 *
 * int main (void)
 * {
 *   uint8_t          code[1023];
 *   dp_gold_state_t *g = dp_gold_create (934, 350, 567, 73, 10);
 *   dp_gold_generate (g, 1023, code, 1023); // the specification Gold-1023 code
 *   dp_gold_destroy (g);
 *   dp_correlator_state_t *a = dp_correlator_create_continuous (
 *       code, 1023, 2 * 3.069e6, 3.069e6, 2700.0, 47.0, 5000.0, 1e-3, 0.9,
 *       0, 1, 0.0);
 *   printf ("%.1f %.1f\n", dp_correlator_psl_db (a), dp_correlator_dsl_db (a));
 *   // -23.9 -16.9
 *   dp_correlator_destroy (a);
 *   return 0;
 * }
 * @endcode
 */
double dp_correlator_dsl_db (dp_correlator_state_t *state);

/**
 * @brief The gate a dwell's statistic must clear to be a hit, in the
 *        units of dp_correlator_hit_t::test_stat: `threshold` on the coherent
 *        path (one look), `eta_nc` on the non-coherent one. A dwell hits
 *        when its strongest cell's statistic exceeds this, and every
 *        listed peak's statistic does too. Both statistics are linear in
 *        the cell's amplitude, so a list consumer can compare a peak's
 *        excess over another's against it in the same units.
 *        Configuration: fixed by the thresholds the grid was sized with.
 *
 * @param state  Must be non-NULL.
 * NaN on a map or detector flavor (no search record).
 *
 * @return The gate, in dp_correlator_hit_t::test_stat units.
 */
double dp_correlator_gate_stat (const dp_correlator_state_t *state);

/**
 * @brief Convert one dp_correlator_push() hit into a wire-ready hand-off record.
 *
 * Two convention inversions live here, ported verbatim from
 * `dsss_receiver_core.c`'s own (pre-existing, now-shared) handoff logic:
 *
 * - **Chip phase**: @p hit's `col` is a correlation LAG (0 …
 *   lag_bins-1); a code-tracking loop's `init_chip` wants the code's
 *   own instantaneous phase instead — the mirror-image inversion
 *   `phase = fmod(code_len - col * code_len / lag_bins,
 *   code_len)`, folded non-negative.
 * - **Doppler**: @p state is assumed built via dp_correlator_create_continuous()
 *   (`window_bins` tiles of `coherent_bins` each, the only mode this
 *   function supports), so `hit`'s `row` indexes the uniform grid
 *   of `window_bins * coherent_bins` bins, mapped to a signed bin by
 *   `dp_fftfreq_index()` — the SAME helper the search uses — and scaled
 *   by `state->search->doppler_res_hz`.
 *
 * - **The dwell's dilation** (doppler#1254): a hit is decided on a
 *   non-coherent sum over `n_noncoh` looks, and the code phase it reports
 *   is that sum's peak -- the phase at the MIDDLE of the dwell, not at
 *   its end, when the chip clock is dilated by the same Doppler the hit
 *   reports (a physically-coupled carrier, `doppler_hz / carrier_freq_hz`
 *   chips per chip). The seed a code loop wants is the phase at the next
 *   sample, so with the carrier set (dp_correlator_set_carrier_freq_hz()) the phase
 *   is advanced by the drift over HALF the dwell, `doppler_hz_est /
 *   carrier_freq_hz * n_noncoh * coherent_bins * code_len / 2` chips
 *   (a coherent block's epochs are aligned to its middle by the same
 *   setting, so the block's peak is its middle too). At the specification's 20 ppm the
 *   continuous engine's dwell at 45 dB-Hz is 15 epochs (0.15 chip, inside
 *   any code loop's pull-in) and at the 40 dB-Hz floor 88 epochs -- 0.9
 *   chip, measured directly, past the refine Dll's; without this the
 *   floor's hand-offs never refined. Uncoupled (0.0): no advance.
 * @param state    The engine @p hit came from (non-NULL, built via
 *                 dp_correlator_create_continuous()).
 * @param hit      One hit from dp_correlator_push() (non-NULL).
 * @param code_len Spreading-code length (chips): a hit's lag, in columns
 *                 of the engine's `lag_bins`-column period, becomes a
 *                 phase in these units (`code_len / lag_bins` chips a
 *                 column). For a code engine it is the code's length; a
 *                 burst engine built from a code held k samples a chip
 *                 passes the code's length too.
 * @param out      Written on return (non-NULL).
 */
void dp_correlator_build_handoff (const dp_correlator_state_t *state, const dp_correlator_hit_t *hit,
                        size_t code_len, dp_correlator_handoff_t *out);

/**
 * @brief How far up the sidelobes of the emitter behind one hit can reach
 *        at another hit's cell, relative to that emitter's own peak.
 *
 * One emitter puts its code's cross-ambiguity on every cell of the
 * surface. This computes that row the way the surface itself is made:
 * one period of the replica, carrying @p from's Doppler less the cell's
 * tile centre and delayed by @p from's lag (a phase ramp, so a lag
 * between two samples is exact), its spectrum times the replica's
 * conjugate, zero-padded to lag_bins and inverted (dp_fft_pad_spectrum()),
 * read at @p to's column. The emitter's own peak is the same product's
 * zero lag at its residual from its own tile's centre. The ratio is the
 * most it can be over what the hits do not pin down:
 *
 * - @p from's lag is known to half a lag column either side of its
 *   column: five points across that are read;
 * - @p from's Doppler is known to half a native row either side: nine
 *   points across that row are read, its own residual moving with each;
 * - a cell on a tile edge, or on the combined axis's wrap, is read in
 *   every tile it touches.
 *
 * The cell's fast-time mixing frequency is its tile's centre (a
 * multiple of the epoch rate, chip_rate / sf; DC on a single-tile
 * engine): within a tile the slow-time transform only lowers a sidelobe
 * that sits off the emitter's own row, and no credit is taken for that.
 * The value is a SAMPLED maximum, not a proven bound: the
 * cross-ambiguity varies on a one-sample scale, and between the five lag
 * and nine Doppler points it reads, it can sit higher than any of them.
 * What holds it is measurement: native/validation/acq_emitter_floor.c
 * holds every cell of a clean emitter's tile and the two beside it under
 * it, on the scale rule 2 uses (the emitter's statistic on its own lag):
 * least margin +0.61 dB over 6,129 cells, on the Gold-1023 floor. With a
 * data transition inside the epoch it does not bound the surface (a cell
 * reads 20.4 dB above it: the model is one clean period, doppler#2734),
 * and async_dsss_near_far.c takes no sidelobe over the operating range,
 * where the gate covers that. It is the code's worst sidelobe
 * (dp_correlator_dsl_db()) only at the worst cells:
 * most cells sit well below it, which is the room a weak emitter beside
 * a strong one needs. Times @p from's statistic on its own lag
 * (dp_correlator_hit_t::test_stat_peak), it is the statistic @p from's
 * sidelobe can read at @p to.
 *
 * Cost: the first read against a hit, in a tile, sweeps that hit's whole
 * row there: 5 inverse transforms of code_bins (the lag points), then per
 * lag and Doppler point one forward of code_bins and one inverse of
 * lag_bins (45), every complex exponential a resynced recurrence. Every
 * column is kept, so later reads against the same hit and tile, any
 * cell, are lookups: DP_CORRELATOR_SIDELOBE_SLOTS (hit, tile) rows, keyed on the
 * hit's cell and the tile, round-robin when full, and emptied by every
 * regrid (each slot was read on the grid it was filled on). A pick
 * reads many cells against its few stronger hits, dwell after dwell, so
 * nearly every read is a lookup. The plans, buffers and cache are made
 * on the first call and kept on @p state.
 *
 * @param state  The engine both hits came from (non-NULL).
 * @param from   The hit whose emitter's sidelobes are bounded.
 * @param to     The hit whose cell they are bounded at.
 * @return The ratio, from 0 to 1.
 */
double dp_correlator_sidelobe_ratio (dp_correlator_state_t     *state,
                              const dp_correlator_hit_t *from,
                              const dp_correlator_hit_t *to);

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




/**
 * @brief The stream position a blob records -- dp_correlator_position() of the
 *        engine it would restore -- read without restoring it.
 *
 * What lets a composition check its child's blob against its own count
 * before it writes anything, so a refusal never has to be undone (an
 * undo is a second set_state, and set_state can fail). Accepts exactly
 * the blobs dp_correlator_set_state() checks and accepts before its carrier
 * rebuild, through the same check.
 *
 * @code
 * float _Complex pre[124];               // a 31-chip code at 4x
 * for (size_t i = 0; i < 124; i++)
 *   pre[i] = (i / 4) % 3 ? 1.0f : -1.0f;
 * dp_correlator_state_t *a = dp_correlator_create_burst (pre, 124, 16, 4.0e6, 45.0,
 *                                          0.0, 1e-3, 0.9, 0, 0.0, 1);
 * float _Complex x[300] = { 0 };
 * dp_correlator_hit_t   hit;
 * (void)dp_correlator_push (a, x, 300, &hit, 1);
 * void    *blob = dp_xmalloc (dp_correlator_state_bytes (a));
 * uint64_t at   = 0;
 * dp_correlator_get_state (a, blob);
 * int ok = dp_correlator_state_position (a, blob, &at); // DP_OK, 300
 * int same = at == dp_correlator_position (a);
 * free (blob);
 * dp_correlator_destroy (a);
 * if (ok != DP_OK || at != 300 || !same)
 *   return 1;
 * @endcode
 *
 * @param state     The engine the blob would be restored into.
 * @param blob      A blob from dp_correlator_get_state().
 * @param position  Out: the blob's position. Unchanged on a refusal.
 * @return DP_OK, or DP_ERR_INVALID where dp_correlator_set_state() would refuse
 *         the blob as invalid.
 */
int dp_correlator_state_position (const dp_correlator_state_t *state, const void *blob,
                           uint64_t *position);

/**
 * @brief Pure run: inject @p state_in, stream @p in, emit hits, export
 *        @p state_out — `(state_in, input) -> (state_out, output)` over an
 *        engine treated as immutable config + scratch.  @p state_in / @p
 *        state_out may alias.  Either may be NULL (NULL in = fresh;
 *        NULL out = discard).
 *
 * The run is one dp_correlator_push(), so it stops where a push stops: once
 * @p result is full, or before a frame that ends a dwell when fewer than
 * min(max_peaks, @p max_results) slots are left.  It may then leave part
 * of @p in untaken.  dp_correlator_consumed(@p state) says how much it took;
 * the caller calls again with @p state_out as the next @p state_in and
 * the rest, in + consumed, and the two runs together equal one over the
 * whole input. A refused @p state_in runs nothing and returns 0, with
 * dp_correlator_consumed() reading 0.
 *
 * @param state        Engine (config + scratch); its running state is
 *                     replaced by @p state_in, or reset when that is NULL.
 * @param state_in     Blob from dp_correlator_get_state(), or NULL for fresh.
 * @param state_out    Destination blob (dp_correlator_state_bytes() bytes), or
 *                     NULL to discard.
 * @param in           Input cf32 samples.
 * @param n_in         Number of input samples.
 * @param result       Output hits.
 * @param max_results  Capacity of @p result.
 * @return Number of events written (0 … max_results).
 */
size_t dp_correlator_run (dp_correlator_state_t *state, const void *state_in, void *state_out,
                const float _Complex *in, size_t n_in, dp_correlator_hit_t *result,
                size_t max_results);

/**
 * @brief Whether every decided dwell's surface is kept for
 * dp_correlator_surface(): 1 or 0.
 *
 * The continuous search only (docs/design/correlator-python.md section
 * 18): any other flavor keeps no surface and refuses the read, and
 * Python's read raises AttributeError.
 *
 * @param state  Must be non-NULL.
 * @param out    Receives 1 if kept, else 0; untouched on a refusal.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the continuous search.
 */
int dp_correlator_get_keep_surface(const dp_correlator_state_t *state, int *out);

/**
 * @brief Whether this flavor has a last correlation vector to read:
 * Python's Correlator.last_corr asks it before it builds the view.
 *
 * Both detectors serve it (section 18); there, the view reads None until a
 * push() has produced a result. Any other flavor refuses, and Python's read
 * raises AttributeError.
 *
 * @param state  Must be non-NULL.
 * @return DP_OK on a detector, DP_ERR_UNSUPPORTED on any other flavor.
 */
int dp_correlator_last_corr_status(const dp_correlator_state_t *state);
/**
 * @brief Rows of the reference: 1 for a 1-D one; the delay-Doppler map's
 * Doppler rows (doppler_rows).
 *
 * Served by a map or a detector (framing='none'); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_ny(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Samples a reference row: the correlation's native lag count.
 *
 * Served by a map or a detector (framing='none'); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_nx(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Rows a dump writes: ny unless padded (ny_out), which interpolates
 * the Doppler or row axis.
 *
 * Served by a map (framing='none', detect=False); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_ny_out(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Lags a dump writes per row: nx unless padded (nx_out), which
 * interpolates the lag axis. The one padding name at any row count.
 *
 * Served by the 1-D or 2-D map (framing='none', detect=False,
 * doppler_rows=0); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_nx_out(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief The one lag a dump emits per row, or -1 for every lag.
 *
 * Served by the 1-D or 2-D map (framing='none', detect=False,
 * doppler_rows=0); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_col_out(const dp_correlator_state_t *state, int *out);
/**
 * @brief Doppler bin width of the delay-Doppler map, cycles per sample.
 *
 * Served by the delay-Doppler map (doppler_rows > 0); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_doppler_step(const dp_correlator_state_t *state, double *out);
/**
 * @brief Frame length in samples: the reference's length, ny * nx.
 *
 * Served by the 1-D map or a detector (detect=True); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_n(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Integration depth: a dump every `dwell` frames.
 *
 * Served by a map or a detector (framing='none'); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_dwell(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Frames accumulated toward the next dump (0 ... dwell - 1).
 *
 * Served by a map or a detector (framing='none'); any other flavor refuses
 * it, and Python's read raises AttributeError (docs/design/correlator-
 * python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_count(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Input ring capacity in complex samples.
 *
 * Served by a detector (detect=True) or a search (framing='continuous' or
 * 'burst'); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_ring_cap(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief First noise (CFAR reference) bin, inclusive.
 *
 * Served by a detector (detect=True) or a search (framing='continuous' or
 * 'burst'); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_noise_lo(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Last noise (CFAR reference) bin, inclusive.
 *
 * Served by a detector (detect=True) or a search (framing='continuous' or
 * 'burst'); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_noise_hi(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief The gate on the test statistic: a detector's as given (0 fires on
 * every dump); a search's derived from pfa (the coherent path's gate).
 *
 * Served by a detector (detect=True) or a search (framing='continuous' or
 * 'burst'); any other flavor refuses it, and Python's read raises
 * AttributeError (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_threshold(const dp_correlator_state_t *state, float *out);
/**
 * @brief The peak list's capacity per dwell (1 = the classic gated maximum);
 * set with set_max_peaks().
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_max_peaks(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief One period of the reference in samples: the length of every forward
 * transform and of a raw epoch.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_code_bins(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Doppler hypotheses searched, window tiles times the coherent depth:
 * the count to fold a hit's row over (bin_to_signed(row, doppler_bins) *
 * doppler_res_hz is the signed Doppler).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_doppler_bins(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Non-coherent looks per detection (1 = pure coherent).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_n_noncoh(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Raw per-cell Rayleigh amplitude threshold.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_eta(const dp_correlator_state_t *state, float *out);
/**
 * @brief Non-coherent CFAR threshold (order-N_nc Marcum).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_eta_nc(const dp_correlator_state_t *state, float *out);
/**
 * @brief Sidak per-cell false-alarm probability over the searched cells.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_pfa_cell(const dp_correlator_state_t *state, double *out);
/**
 * @brief Predicted Pd at cn0_dbhz and the chosen grid, averaged over the
 * straddle priors (slow-time scalloping, intra-segment rotation, code-phase
 * sample offset): what the Monte-Carlo characterization measures, not the on-
 * grid best case.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_pd_predicted(const dp_correlator_state_t *state, double *out);
/**
 * @brief Mean amplitude derating of the correlation peak from grid straddle,
 * a diagnostic; 20*log10(straddle_loss) is the loss in dB.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_straddle_loss(const dp_correlator_state_t *state, double *out);
/**
 * @brief Sample rate the search runs at, Hz: the constructor's sample_rate.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_sample_rate(const dp_correlator_state_t *state, double *out);
/**
 * @brief Carrier-to-noise density the search was sized for, dB-Hz; NaN for no
 * design point (the burst search only).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_cn0_dbhz(const dp_correlator_state_t *state, double *out);
/**
 * @brief Native unambiguous Doppler half-range, Hz: sample_rate / (2 *
 * code_bins).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_doppler_span_hz(const dp_correlator_state_t *state, double *out);
/**
 * @brief Doppler bin width, Hz.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_doppler_res_hz(const dp_correlator_state_t *state, double *out);
/**
 * @brief Target detection probability.
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_pd(const dp_correlator_state_t *state, double *out);
/**
 * @brief True when the search cannot meet pd at cn0_dbhz and its geometry:
 * pd_predicted < pd for the continuous search, pd_burst < pd for the burst
 * search. It still builds a best-effort grid, and construction warns
 * (UserWarning).
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_underpowered(const dp_correlator_state_t *state, bool *out);
/**
 * @brief Workers the search fans its tiles across, the calling thread
 * included; 1 = serial. Set with set_threads().
 *
 * Served by a search (framing='continuous' or 'burst'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_threads(const dp_correlator_state_t *state, int *out);
/**
 * @brief Code-phase columns of the surface: code_bins unless code_bins is not
 * 7-smooth, then the next power of two (an exact interpolation).
 * surface_chip_phase() gives each column in chips.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_lag_bins(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief The block-coherent depth D inside every window tile: epochs per
 * block and Doppler rows per tile. 1 without a code-only window.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_coherent_bins(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Chips per code period: len(ref).
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_sf(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Chip rate, Hz.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_chip_rate(const dp_correlator_state_t *state, double *out);
/**
 * @brief Data-symbol rate the search was built with, Hz (0 = no known data
 * clock): a diagnostic, never sizing.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_symbol_rate(const dp_correlator_state_t *state, double *out);
/**
 * @brief (chip_rate / sf) / symbol_rate: code epochs per data symbol; 0 when
 * symbol_rate is 0.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_epochs_per_symbol(const dp_correlator_state_t *state, double *out);
/**
 * @brief Picks in the last decided dwell, held twins included.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_n_peaks(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Picks of the last decided dwell held as same-code-phase twins rather
 * than listed (docs/design/async-dsss-receiver.md section 7.1).
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_n_held(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Concentration of the last dwell's strongest peak: its main lobe's
 * power over its code-phase column's, across every Doppler row and tile. Near
 * 1 for a clean single emitter; about 0.5 when a data transition splits it
 * into twins (docs/design/async-dsss-receiver.md section 2.4).
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_peak_conc(const dp_correlator_state_t *state, float *out);
/**
 * @brief RF carrier the Doppler is coupled to, Hz (0.0 = uncoupled); set with
 * set_carrier_freq_hz().
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_carrier_freq_hz(const dp_correlator_state_t *state, double *out);
/**
 * @brief Rows of the surface surface() returns: the Doppler axis in surface
 * units; its columns are lag_bins.
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_surface_rows(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief samples_consumed of the dwell whose surface surface() returns (0
 * until one has been captured).
 *
 * Served by the continuous search (framing='continuous'); any other flavor
 * refuses it, and Python's read raises AttributeError
 * (docs/design/correlator-python.md section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_surface_at(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Preamble repetitions a burst holds: the coherence ceiling.
 *
 * Served by the burst search (framing='burst'); any other flavor refuses it,
 * and Python's read raises AttributeError (docs/design/correlator-python.md
 * section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_reps(const dp_correlator_state_t *state, size_t *out);
/**
 * @brief Predicted Pd of one burst of reps repetitions at cn0_dbhz: the
 * number the sizer meets pd with. NaN with no design cn0_dbhz or with
 * n_noncoh > 1.
 *
 * Served by the burst search (framing='burst'); any other flavor refuses it,
 * and Python's read raises AttributeError (docs/design/correlator-python.md
 * section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_pd_burst(const dp_correlator_state_t *state, double *out);
/**
 * @brief The preamble's peak sidelobe level, dB: its periodic
 * autocorrelation's largest lag outside the mainlobe, relative to the peak
 * (-inf for a perfect sequence such as Zadoff-Chu).
 *
 * Served by the burst search (framing='burst'); any other flavor refuses it,
 * and Python's read raises AttributeError (docs/design/correlator-python.md
 * section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_psl_db(const dp_correlator_state_t *state, double *out);
/**
 * @brief Doppler rate the coherent depth is bounded against, Hz/s (0 = no
 * bound); with rate_hypotheses > 1, the span of the hypotheses.
 *
 * Served by the burst search (framing='burst'); any other flavor refuses it,
 * and Python's read raises AttributeError (docs/design/correlator-python.md
 * section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_doppler_rate(const dp_correlator_state_t *state, double *out);
/**
 * @brief Doppler-rate hypotheses searched: as given, or the engine's choice
 * when built with 0; 1 is none.
 *
 * Served by the burst search (framing='burst'); any other flavor refuses it,
 * and Python's read raises AttributeError (docs/design/correlator-python.md
 * section 18).
 *
 * @param state  Must be non-NULL.
 * @param out    Receives the value when served.
 * @return DP_OK, or DP_ERR_UNSUPPORTED off the flavors that serve it.
 */
int dp_correlator_get_rate_hypotheses(const dp_correlator_state_t *state, size_t *out);
#ifdef __cplusplus
}
#endif

#endif /* DP_CORRELATOR_CORE_H */
