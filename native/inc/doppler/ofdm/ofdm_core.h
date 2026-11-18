/**
 * @file ofdm_core.h
 * @brief A general OFDM modulator: constellation points and a layout in,
 *        baseband samples out.
 *
 * The modulator names no standard and includes nothing of wfmgen. What it
 * renders is described by a **layout**, a list of **classes** (one kind of
 * OFDM symbol: a transform length, a subcarrier map, a fixed pattern of pilots
 * or training values, a scale, a cyclic prefix and a window) and a list of
 * **runs** (a count of OFDM symbols of one class). A standard's preset is a
 * filled-in layout; the design page, docs/design/ofdm.md, owns the shape and
 * what forced each field.
 *
 * ## One OFDM symbol
 *
 * Per OFDM symbol the modulator takes the data count of the symbol's pattern
 * phase from the points, places them on the data subcarriers that no fixed
 * entry displaces (in increasing subcarrier order), sets the fixed entries of
 * the phase times the class's polarity element, multiplies the grid by the
 * subcarrier gains, inverse transforms, multiplies by `scale`, reads `useful`
 * samples cyclically from the transform output, prefixes the last
 * `cyclic_prefix` of them, and overlap-adds the window.
 *
 * With `m` the sample index into the transform output and `k` the
 * subcarrier, the unnormalized inverse transform is
 * `x[m] = scale * sum_k grid[k] * exp(+2j*pi*k*m/n_fft)`. The body is
 * `x[m mod n_fft]` for `m < useful`; the symbol is the last `cyclic_prefix`
 * body samples followed by the body.
 *
 * ## The window
 *
 * The boundary between two OFDM symbols is a raised-cosine cross-fade of
 * `window` samples, `rise[j] = sin^2 (pi (j + 1/2) / (2 window))` and
 * `fall[j] = 1 - rise[j]`. The later symbol's first `window` samples are
 * weighted by `rise`; the earlier symbol is continued past its end by `window`
 * samples of the transform output (`x[(useful + j) mod n_fft]`), weighted by
 * `fall`, and overlap-added. The length used at a boundary is the later
 * symbol's class `window`; the first symbol rises from silence over its own,
 * and the last continues over its own. A window of 0 is no window: the output
 * is exactly the symbols, back to back. The continued tail is what crosses a
 * call, so it is state; @ref dp_ofdm_finish returns the last one.
 *
 * ## The calls
 *
 * Build the layout with @ref dp_ofdm_add_class, @ref dp_ofdm_add_fixed and
 * @ref dp_ofdm_add_run; the first @ref dp_ofdm_modulate (or
 * @ref dp_ofdm_check) checks it whole. Adding to a checked layout re-opens it
 * (the position and the held window tail stay), so a signal can be extended
 * while it is rendered. Then @ref dp_ofdm_modulate
 * renders whole OFDM symbols from the points it is given.
 *
 * @code
 * dp_ofdm_state_t *o = dp_ofdm_create ();
 * const uint8_t map[8] = { 0, 1, 1, 1, 0, 1, 1, 1 };   // DC and edge null
 * dp_ofdm_add_class (o, 8u, map, 8u, 0u, 0u, 0u, 1.0, 1u);
 * dp_ofdm_add_run (o, 0u, 2u, 0xFFFFFFFFu, 0u, 0u);
 * float _Complex pts[12], out[16];       // 6 data subcarriers x 2 symbols
 * for (int i = 0; i < 12; i++)
 *   pts[i] = 1.0f;
 * size_t n = dp_ofdm_modulate (o, pts, 12, out, 16);   // n == 16
 * dp_ofdm_destroy (o);
 * @endcode
 */
#ifndef DP_OFDM_CORE_H
#define DP_OFDM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/fft/fft_core.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief State blob magic ('O','F','D','M'). */
#define OFDM_STATE_MAGIC DP_FOURCC ('O', 'F', 'D', 'M')
  /** @brief State blob layout version. */
#define OFDM_STATE_VERSION 1u

  /** @brief A subcarrier map value: the subcarrier carries a data point. */
#define OFDM_DATA 1u
  /** @brief A subcarrier map value: the subcarrier carries nothing. */
#define OFDM_NULL 0u
  /** @brief A fixed entry that applies in every pattern phase. */
#define OFDM_ALL_PHASES 0xFFFFFFFFu
  /** @brief A run's cyclic prefix that keeps its class's. */
#define OFDM_CP_DEFAULT 0xFFFFFFFFu

  /** @brief Largest transform length of a class. */
#define OFDM_N_FFT_MAX 65536u
  /** @brief Largest pattern period of a class, in OFDM symbols. */
#define OFDM_N_PHASE_MAX 4096u
  /** @brief Most classes, and most fixed entries per class, a layout holds. */
#define OFDM_COUNT_MAX 65536u

  /** @brief One entry of a class's fixed pattern. */
  typedef struct
  {
    uint32_t       phase; /**< Pattern phase, or OFDM_ALL_PHASES.          */
    int32_t        k;     /**< Subcarrier, -n_fft/2 .. n_fft/2 - 1.        */
    float _Complex value; /**< A pilot, a training value, a boosted pilot. */
  } ofdm_fixed_t;

  /**
   * @brief One kind of OFDM symbol. The arrays are owned by the modulator,
   *        copied in by dp_ofdm_add_class() and dp_ofdm_add_fixed().
   */
  typedef struct
  {
    uint32_t n_fft;         /**< Transform length of this class.           */
    uint32_t useful;        /**< Body samples read cyclically; 0 = n_fft.   */
    uint32_t cyclic_prefix; /**< Default cyclic prefix, samples.            */
    uint32_t window;        /**< Window transition, samples.                */
    double   scale;         /**< Multiplies the transform output.           */
    uint8_t *subcarrier_map;        /**< n_fft: OFDM_DATA or OFDM_NULL,
                                         index k + n_fft/2.                 */
    float _Complex *subcarrier_gain; /**< n_fft multipliers, or NULL.      */
    uint32_t        n_phase;         /**< Fixed-pattern period, symbols.    */
    ofdm_fixed_t   *fixed;           /**< Sorted by (phase, k).             */
    uint32_t        n_fixed;  /**< Entries in `fixed`.                  */
    uint32_t        fixed_cap;      /**< Bookkeeping: allocated entries.    */
    float _Complex *polarity;       /**< Per-OFDM-symbol scalar, or NULL.   */
    uint32_t        n_polarity; /**< Length of `polarity`.              */
    uint32_t       *n_data;         /**< Derived at seal: points per phase. */
    float          *rise;           /**< Derived at seal: `window` weights. */
  } ofdm_class_t;

  /** @brief A run of OFDM symbols of one class. */
  typedef struct
  {
    uint32_t class_index;     /**< Index into the layout's classes.         */
    uint32_t count;           /**< OFDM symbols in the run.                  */
    uint32_t cyclic_prefix;   /**< Override, or OFDM_CP_DEFAULT.             */
    uint32_t polarity_offset; /**< Polarity element of the first symbol.     */
    uint32_t first_phase;     /**< Pattern phase of the first symbol.        */
  } ofdm_run_t;

  /**
   * @brief Ofdm state: the layout (configuration) and the position in it
   *        (state).
   *
   * Allocate with dp_ofdm_create().
   */
  typedef struct
  {
    ofdm_class_t *cls;       /**< Classes, `n_cls` of them.                 */
    uint32_t      n_cls;     /**< Classes held.                              */
    uint32_t      cap_cls;   /**< Allocated class slots.                     */
    ofdm_run_t   *runs;      /**< Runs, `n_runs` of them.                   */
    uint32_t      n_runs;    /**< Runs held.                                 */
    uint32_t      cap_runs;  /**< Allocated run slots.                       */
    uint32_t      max_window; /**< Largest class window; sizes the blob.    */

    int sealed;              /**< Checked; adding to the layout clears it.  */

    /* derived at seal */
    dp_fft_state_t **plans;  /**< One inverse plan per distinct n_fft.      */
    uint32_t         n_plans; /**< Distinct inverse plans held.              */
    uint32_t        *plan_of; /**< Class -> its plan.                       */
    float _Complex  *grid;   /**< Scratch: one grid, n_fft_max.             */
    float _Complex  *xform;  /**< Scratch: one transform, n_fft_max.        */
    uint8_t         *taken;  /**< Scratch: subcarriers a fixed entry owns.  */

    /* running state */
    uint32_t       run_index;    /**< The run the next symbol belongs to.   */
    uint32_t       sym_in_run;   /**< Symbols already rendered of that run. */
    uint32_t       tail_len;     /**< Samples of window tail held.          */
    float _Complex *tail;        /**< The held tail, `tail_cap` slots.      */
    uint32_t        tail_cap;    /**< Allocated tail slots, >= max_window.  */
  } dp_ofdm_state_t;

  /**
   * @brief Create an empty modulator (no classes, no runs).
   *
   * @return Heap-allocated state, never NULL: running out of memory aborts.
   * @note Caller must call dp_ofdm_destroy() when done.
   */
  dp_ofdm_state_t *dp_ofdm_create(void);

  /**
   * @brief Destroy a modulator and release all memory.
   * @param state  May be NULL.
   */
  void dp_ofdm_destroy(dp_ofdm_state_t *state);

  /**
   * @brief Return to the first OFDM symbol, keeping the layout.
   *
   * The next render starts from the post-seal state: the first run, no held
   * tail. The layout, sealed or not, is unchanged.
   *
   * @param state  Must be non-NULL.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 2)
   * 0
   * >>> x = o.modulate(np.ones(12, np.complex64))
   * >>> o.reset()
   * >>> o.points_remaining()
   * 12
   * @endcode
   */
  void dp_ofdm_reset(dp_ofdm_state_t *state);

  /**
   * @brief Bytes in the blob dp_ofdm_get_state() writes.
   *
   * A function of the layout alone (the envelope, the position and one
   * window-tail slot per sample of the largest window), so it does not change
   * as the render advances.
   *
   * @param state  The modulator.
   * @return Blob size in bytes.
   */
  size_t dp_ofdm_state_bytes(const dp_ofdm_state_t *state);

  /**
   * @brief Write the state into @p blob.
   *
   * The blob holds the position (run index, OFDM symbols rendered of the run,
   * the held window tail) and a hash of the layout, so it never restores into
   * a different one.
   *
   * @param state  The modulator.
   * @param blob   Receives dp_ofdm_state_bytes() bytes.
   */
  void dp_ofdm_get_state(const dp_ofdm_state_t *state, void *blob);

  /**
   * @brief Restore the state; nonzero rejects @p blob.
   *
   * Rejects a blob whose layout hash differs from this modulator's, whose
   * position names no OFDM symbol of the layout, or whose held tail is not
   * the length the position implies. The modulator is untouched on rejection.
   *
   * @param state  A modulator built with the same layout.
   * @param blob   A blob from dp_ofdm_get_state(), dp_ofdm_state_bytes() long.
   * @return DP_OK, or DP_ERR_INVALID if rejected.
   */
  int dp_ofdm_set_state(dp_ofdm_state_t *state, const void *blob);

  /**
   * @brief Append one kind of OFDM symbol and return its index.
   *
   * Copies @p subcarrier_map. The optional per-subcarrier gains and the
   * polarity sequence are added to the class by dp_ofdm_add_subcarrier_gain()
   * and dp_ofdm_add_polarity(). Refuses (a negative
   * return, nothing changed) what cannot be honoured; a sealed layout is
   * re-opened.
   *
   * @param state               The modulator.
   * @param n_fft               Transform length, 2 to OFDM_N_FFT_MAX.
   * @param subcarrier_map      `n_fft` entries, index `k + n_fft/2`, each
   *                            OFDM_DATA or OFDM_NULL.
   * @param subcarrier_map_len  Must equal @p n_fft.
   * @param useful              Body samples read cyclically from the
   *                            transform; 0 reads as @p n_fft.
   * @param cyclic_prefix       Default prefix, samples; at most `useful`.
   * @param window              Window transition, samples; at most the
   *                            symbol length.
   * @param scale               Multiplies the transform output.
   * @param n_phase             Period of the fixed pattern, at least 1.
   * @return The class index, or a negative value if refused.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> o = Ofdm()
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_class(8, m, cyclic_prefix=2, window=1)
   * 1
   * @endcode
   */
  int dp_ofdm_add_class(dp_ofdm_state_t *state, uint32_t n_fft, const uint8_t *subcarrier_map, size_t subcarrier_map_len, uint32_t useful, uint32_t cyclic_prefix, uint32_t window, double scale, uint32_t n_phase);

  /**
   * @brief Give a class a complex multiplier per subcarrier.
   *
   * Applied to the whole grid (data and fixed entries alike): a per-subband
   * phase rotation, or any per-subcarrier amplitude. Without it the gain is
   * 1 everywhere.
   *
   * @param state        The modulator.
   * @param class_index  An existing class without gains yet.
   * @param gain         `n_fft` multipliers, index `k + n_fft/2`.
   * @param gain_len     Must equal the class's `n_fft`.
   * @return 0, or a negative value if refused.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 1)
   * 0
   * >>> pts = np.ones(6, np.complex64)
   * >>> one = o.modulate(pts)
   * >>> o.reset()
   * >>> o.add_subcarrier_gain(0, 2 * np.ones(8, np.complex64))
   * >>> bool(np.allclose(o.modulate(pts), 2 * one))
   * True
   * @endcode
   */
  int dp_ofdm_add_subcarrier_gain(dp_ofdm_state_t *state, uint32_t class_index, const float _Complex *gain, size_t gain_len);

  /**
   * @brief Give a class a polarity sequence.
   *
   * OFDM symbol `n` of a run multiplies its fixed entries (never its data) by
   * element `(polarity_offset + n) mod polarity_len`. Without it the scalar is
   * 1.
   *
   * @param state         The modulator.
   * @param class_index   An existing class without a sequence yet.
   * @param polarity      The sequence.
   * @param polarity_len  At least 1.
   * @return 0, or a negative value if refused.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> z = np.zeros(8, np.uint8)               # no data subcarriers at all
   * >>> p = Ofdm(); p.add_class(8, z, cyclic_prefix=2)
   * 0
   * >>> p.add_fixed(0, 2**32 - 1, 2, 1.0)      # one pilot, every phase
   * >>> p.add_polarity(0, np.array([1, -1], np.complex64))
   * >>> p.add_run(0, 2)
   * 0
   * >>> x = p.modulate(np.empty(0, np.complex64))
   * >>> bool(np.allclose(x[2:10], -x[12:20]))   # the pilot flips sign
   * True
   * @endcode
   */
  int dp_ofdm_add_polarity(dp_ofdm_state_t *state, uint32_t class_index, const float _Complex *polarity, size_t polarity_len);

  /**
   * @brief Append one fixed entry to a class: a pilot or a training value.
   *
   * A fixed entry displaces data on its subcarrier in its phase (when the map
   * has data there) and is multiplied by the class's polarity element.
   *
   * @param state        The modulator.
   * @param class_index  An existing class.
   * @param phase        The pattern phase it belongs to, below the class's
   *                     `n_phase`, or OFDM_ALL_PHASES.
   * @param k            Subcarrier, `-n_fft/2 .. n_fft/2 - 1`.
   * @param value        The value placed.
   * @return 0, or a negative value if refused.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 1)
   * 0
   * >>> o.points_remaining()      # six data subcarriers
   * 6
   * >>> o.add_fixed(0, 2**32 - 1, 3, 1j)   # a pilot on a data subcarrier
   * >>> o.points_remaining()      # it displaces one
   * 5
   * @endcode
   */
  int dp_ofdm_add_fixed(dp_ofdm_state_t *state, uint32_t class_index, uint32_t phase, int32_t k, double _Complex value);

  /**
   * @brief Append a run of OFDM symbols of one class and return its index.
   *
   * @param state            The modulator.
   * @param class_index      An existing class.
   * @param count            OFDM symbols, at least 1.
   * @param cyclic_prefix    Override of the class's prefix, or
   *                         OFDM_CP_DEFAULT; at most the class's `useful`.
   * @param polarity_offset  Polarity element of the run's first symbol.
   * @param first_phase      Pattern phase of the run's first symbol.
   * @return The run index, or a negative value if refused.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 2)
   * 0
   * >>> o.add_run(0, 3)
   * 1
   * @endcode
   */
  int dp_ofdm_add_run(dp_ofdm_state_t *state, uint32_t class_index, uint32_t count, uint32_t cyclic_prefix, uint32_t polarity_offset, uint32_t first_phase);

  /**
   * @brief Check the whole layout and derive what the render uses; say why
   *        when it is not usable. This is the binding's `check`.
   *
   * Refuses a layout with no run, a window longer than an OFDM symbol of its
   * class, or two fixed entries on one subcarrier in one phase. Derives what
   * the render uses (the data count per phase, the window weights, one
   * inverse transform per distinct length). Idempotent once it has passed.
   *
   * @param state  The modulator.
   * @param why    Where the refusal's sentence goes: a static string naming
   *               the condition that failed. Left as the caller set it when
   *               the layout is usable. May be NULL.
   * @return 0 if the layout is usable, a negative value if not.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> o = Ofdm()
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3]] = 1
   * >>> o.add_class(8, m)
   * 0
   * >>> o.check()                # no run yet
   * Traceback (most recent call last):
   *     ...
   * ValueError: the layout has no run
   * >>> o.add_run(0, 1)
   * 0
   * >>> o.check()                # usable: returns None
   * @endcode
   */
  int dp_ofdm_check_why(dp_ofdm_state_t *state, const char **why);

  /**
   * @brief The plain form of dp_ofdm_check_why(): 0 if the layout is usable,
   *        a negative value if not, with no sentence. What a C caller that
   *        does not print the reason calls.
   *
   * @param state  The modulator.
   * @return 0 if the layout is usable, a negative value if not.
   */
  int dp_ofdm_check(dp_ofdm_state_t *state);

  /**
   * @brief Constellation points the rest of the layout consumes.
   *
   * Zero for a layout that does not seal.
   *
   * @param state  The modulator.
   * @return Points from the current position to the end of the layout.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 3)
   * 0
   * >>> o.points_remaining()
   * 18
   * @endcode
   */
  size_t dp_ofdm_points_remaining(dp_ofdm_state_t *state);

  /**
   * @brief The plain form of dp_ofdm_modulate_why_max_out().
   * @param state  The modulator.
   * @param n_in   Constellation points.
   * @return Samples a call with @p n_in points writes, 0 if it is refused.
   */
  size_t dp_ofdm_modulate_max_out(dp_ofdm_state_t *state, size_t n_in);

  /**
   * @brief Render OFDM symbols from constellation points, or say why not.
   *
   * @param state    The modulator; sealed by this call if it is not.
   * @param in       Constellation points, consumed in layout order.
   * @param n_in     Number of points; must end on an OFDM symbol boundary.
   * @param out      Receives the samples.
   * @param max_out  Capacity of @p out. Short is a refusal, not a truncation.
   * @param why      NULL, or receives a static sentence for a refusal and
   *                 is left as the caller set it otherwise: an unusable layout (the sentence
   *                 dp_ofdm_check_why() gives), points that end inside an
   *                 OFDM symbol, more points than the layout has left, or
   *                 an output buffer shorter than dp_ofdm_modulate_max_out().
   * @return Samples written, which may be 0: no points at the end of the
   *         layout, or zero points mid-layout when the next OFDM symbol
   *         needs some. A refused call changes nothing and reports it by
   *         face: C returns a negative value and sets @p why; the Python
   *         binding raises ValueError with that sentence.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> o = Ofdm()
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 3)
   * 0
   * >>> o.points_remaining()
   * 18
   * >>> x = o.modulate(np.ones(12, np.complex64))   # two of the three
   * >>> x.size
   * 20
   * >>> o.points_remaining()
   * 6
   * >>> o.modulate(np.ones(5, np.complex64))        # inside a symbol
   * Traceback (most recent call last):
   *     ...
   * ValueError: the points end inside an OFDM symbol: pass a count that ends where one does (see points_remaining and modulate_max_out)
   * @endcode
   */
  int64_t dp_ofdm_modulate_why(dp_ofdm_state_t *state, const float _Complex *in, size_t n_in, float _Complex *out, size_t max_out, const char **why);

  /* The binding derives this name from dp_ofdm_modulate_why, and has no key
     to name dp_ofdm_modulate_max_out instead: just-makeit#2680. Retire this
     alias, and the plain function's pointer doc, when that key exists. */
  /**
   * @brief Samples a render of @p n_in points writes.
   *
   * Simulates the render from the current position: OFDM symbols are taken
   * in layout order until the next one needs more points than remain. The
   * result is zero if @p n_in ends inside an OFDM symbol or runs past the
   * layout (that call is refused), for a layout that does not seal, and for
   * no points when the next OFDM symbol needs some. Use it to size the
   * output before the call.
   *
   * @param state  The modulator.
   * @param n_in   Constellation points.
   * @return Samples that call will write.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> o = Ofdm()
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o.add_class(8, m, cyclic_prefix=2)
   * 0
   * >>> o.add_run(0, 3)
   * 0
   * >>> o.modulate_max_out(12)   # two OFDM symbols of 10 samples
   * 20
   * >>> o.modulate_max_out(5)    # ends inside a symbol
   * 0
   * @endcode
   */
  size_t dp_ofdm_modulate_why_max_out(dp_ofdm_state_t *state, size_t n_in);

  /**
   * @brief The plain form of dp_ofdm_modulate_why().
   * @param state    The modulator.
   * @param in       Constellation points.
   * @param n_in     Number of points.
   * @param out      Receives the samples.
   * @param max_out  Capacity of @p out.
   * @return Samples written, or 0 if the call is refused or writes none; a
   *         caller who must tell the two apart uses dp_ofdm_modulate_why().
   */
  size_t dp_ofdm_modulate(dp_ofdm_state_t *state, const float _Complex *in, size_t n_in, float _Complex *out, size_t max_out);

  /**
   * @brief Capacity dp_ofdm_finish() needs for @p n samples.
   * @param state  The modulator.
   * @param n      Samples asked for.
   * @return The held tail's length, at most @p n.
   */
  size_t dp_ofdm_finish_max_out(dp_ofdm_state_t *state, size_t n);

  /**
   * @brief Return the window tail still held and clear it.
   *
   * After the last OFDM symbol this is the final fall of the window; with a
   * window of 0, or before anything is rendered, there is none.
   *
   * @param state    The modulator.
   * @param n        Samples asked for; the held tail is returned up to this.
   * @param out      Receives the tail.
   * @param max_out  Capacity of @p out.
   * @return Samples written.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.ofdm import Ofdm
   * >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
   * >>> o = Ofdm()
   * >>> o.add_class(8, m, cyclic_prefix=2, window=2)
   * 0
   * >>> o.add_run(0, 2)
   * 0
   * >>> x = o.modulate(np.ones(12, np.complex64))
   * >>> o.finish().size          # the window's last fall
   * 2
   * >>> o.finish().size          # and it is cleared
   * 0
   * @endcode
   */
  size_t dp_ofdm_finish(dp_ofdm_state_t *state, size_t n, float _Complex *out, size_t max_out);
#ifdef __cplusplus
}
#endif

#endif /* DP_OFDM_CORE_H */
