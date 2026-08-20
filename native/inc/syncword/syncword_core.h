/**
 * @file syncword_core.h
 * @brief Frame synchronisation: find a known marker in a bit stream, and
 * choose the threshold that decides what counts as finding it.
 *
 * `dp_syncword.h` owns the kernel — correlate a known pattern against every
 * bit offset, in both polarities, and report the first offset close enough.
 * This owns the DETECTOR built over one: a caller names the marker and gets
 * a searcher for it, plus the arithmetic for setting its tolerance. Nothing
 * here knows about CCSDS, which is a configuration of the same kernel (see
 * `ccsds_tm`); reach it with `doppler.wfm.ccsds_asm_bits()`.
 *
 * ## The threshold is not a property of the marker
 *
 * `max_errors` is the whole of the trade, and the number a caller needs is a
 * function of **how much stream they search**, not of how long the marker
 * is. A 32-bit marker invites "half of 32 is 16, so 8 sounds safe", and 8
 * finds the marker at its true offset only 58 % of the time on a stream with
 * no channel errors at all — because the search reports the FIRST acceptable
 * offset, and each of the offsets ahead of the real one is an independent
 * chance to false-hit first (doppler#897).
 *
 * So `pfa` and `max_errors_for` sit beside the search, answering FOR the
 * marker being searched — the same pairing `det_threshold` has with `det_pd`
 * in this module.
 *
 * Bit convention: **unpacked** bits, one per byte in the LSB, which is what
 * `wfm_frame_bits`, `dp_crc16_ccitt` and `ccsds_tm_randomise` already pass
 * around.
 *
 * Lifecycle: `create -> [find / pfa / max_errors_for]* -> destroy`.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.detection import SyncFinder
 * >>> from doppler.wfm import ccsds_asm_bits
 * >>> asm = ccsds_asm_bits()
 * >>> f = SyncFinder(asm)
 * >>> rx = np.concatenate([np.zeros(96, np.uint8), asm])
 * >>> hit = f.find(rx, max_errors=4)
 * >>> hit.found, hit.offset, hit.inverted, hit.errors
 * (1, 96, 0, 0)
 * @endcode
 */
#ifndef SYNCWORD_CORE_H
#define SYNCWORD_CORE_H

#include "clib_common.h"
#include "dp_syncword.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief What @ref syncword_find found.
 *
 * A record rather than an out-parameter and a status, because offset,
 * polarity and distance are ONE answer: a receiver that took the offset
 * without the polarity would hand its frame decoder bits it will silently
 * misread. @p found is what the other three mean nothing without, which is
 * why it is a field rather than a sentinel offset — the same choice
 * `frame_check_t` makes with its `checked`.
 *
 * Distinct from @ref dp_syncword_hit_t, which the kernel fills through a
 * pointer and leaves untouched on a miss: this one is a total answer,
 * returned by value, and so has somewhere to put "no".
 */
typedef struct
{
  int      found;    /**< A marker was found: 1 yes, 0 no          */
  size_t   offset;   /**< Bit index where the marker starts        */
  int      inverted; /**< The stream is complemented               */
  uint32_t errors;   /**< Hamming distance to the marker there     */
} syncword_hit_t;

/**
 * @brief A searcher for one marker.
 *
 * Opaque and heap-allocated: it owns a copy of the marker, so a caller may
 * free or reuse the array it constructed from.
 *
 * Allocate with syncword_create().
 */
typedef struct
{
  uint8_t *marker; /**< the pattern, unpacked, one bit per byte    */
  /*<<property_struct_fields>>*/
  size_t nbits;
} syncword_state_t;

/**
 * @brief Create a searcher for @p marker.
 *
 * The marker is COPIED. A searcher outlives the array it was built from,
 * which is what lets a caller construct one from a temporary — the CCSDS
 * marker arrives from `ccsds_asm_bits()` as exactly that.
 *
 * @param marker      Unpacked bits, one per byte; only the LSB is used.
 * @param marker_len  Marker length in bits; must be non-zero.
 * @return Heap-allocated state, or NULL for an empty marker or on
 *         allocation failure.
 * @note Caller must call syncword_destroy() when done.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.detection import SyncFinder
 * >>> from doppler.wfm import ccsds_asm_bits
 >>> asm = ccsds_asm_bits()   # 0x1ACFFC1D, no transcription
 * >>> f = SyncFinder(asm)
 * >>> f.nbits
 * 32
 * >>> rx = np.concatenate([np.zeros(96, np.uint8), asm])
 * >>> hit = f.find(rx, max_errors=f.max_errors_for(96, pfa=1e-3))
 * >>> hit.found, hit.offset, hit.inverted
 * (1, 96, 0)
 * @endcode
 */
syncword_state_t *syncword_create (const uint8_t *marker, size_t marker_len);

/**
 * @brief Destroy a searcher and release all memory.
 * @param state  May be NULL.
 */
void syncword_destroy (syncword_state_t *state);

/**
 * @brief Find the first marker in @p bits, either polarity.
 *
 * The FIRST offset whose Hamming distance to the marker, or to its
 * complement, is at most @p max_errors. First rather than best, because a
 * best-match search has to see the whole stream before it can answer and a
 * synchroniser reading a live capture cannot wait for that.
 *
 * Choose @p max_errors with `max_errors_for`, against the window this caller
 * actually searches — the marker length is the wrong thing to halve.
 *
 * @param state       The searcher.
 * @param bits        Unpacked bits, one per byte.
 * @param bits_len    Number of bits.
 * @param max_errors  Largest tolerated Hamming distance, in bits.
 * @return A record whose @c found says whether the rest of it means
 *         anything; a miss returns it zeroed.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.detection import SyncFinder
 * >>> m = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
 * >>> rx = np.concatenate([np.zeros(20, np.uint8), 1 - m])
 * >>> hit = SyncFinder(m).find(rx, max_errors=1)
 * >>> hit.found, hit.offset, hit.inverted
 * (1, 20, 1)
 * @endcode
 */
syncword_hit_t syncword_find (syncword_state_t *state, const uint8_t *bits,
                              size_t bits_len, uint32_t max_errors);

/**
 * @brief Probability that ONE random offset false-hits this marker at a
 * tolerance of @p max_errors.
 *
 * `2 * sum_{i <= max_errors} C(n, i) / 2^n`, the factor of two because
 * `find` searches the complement too. Measured against the 32-bit CCSDS
 * marker, this tracks the observed false-alarm rate to within 20 % at every
 * threshold where the count supports a rate
 * (`src/doppler/tests/validation/ccsds_tm/results.md` §2.2).
 *
 * This is the PER-OFFSET number. What a synchroniser cares about is its
 * whole window; `max_errors_for` is this inverted through it.
 *
 * @param state       The searcher.
 * @param max_errors  Tolerance in bits.
 * @return Probability in &#91;0, 1&#93;.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.detection import SyncFinder
 * >>> from doppler.wfm import ccsds_asm_bits
 * >>> f = SyncFinder(ccsds_asm_bits())
 * >>> # the marker and its complement, out of 2**32 windows
 * >>> round(f.pfa(0) * 2**32)
 * 2
 * >>> # ...plus each one's 32 one-bit neighbours
 * >>> round(f.pfa(1) * 2**32)
 * 66
 * @endcode
 */
double syncword_pfa (syncword_state_t *state, uint32_t max_errors);

/**
 * @brief The largest tolerance whose false-frame rate over a search window
 * still meets @p pfa.
 *
 * The question `find`'s signature cannot ask. Every offset ahead of the true
 * marker is an independent chance to win the race, so the probability the
 * window produces a false frame is `1 - (1 - pfa(t))^window_bits`, which
 * rises with `t`. The largest
 * `t` that still holds is the most tolerant threshold a caller can afford —
 * and it falls as they search further, which is the whole of doppler#897.
 *
 * @param state        The searcher.
 * @param window_bits  Offsets tried AHEAD of the marker: the length of
 *                     stream searched, not the length of the frame.
 * @param pfa          Tolerated probability of a false frame over that
 *                     window.
 * @return Tolerance in bits, or -1 when even an exact match exceeds @p pfa
 *         over that window.
 *
 * @code
 * >>> from doppler.detection import SyncFinder
 * >>> from doppler.wfm import ccsds_asm_bits
 * >>> f = SyncFinder(ccsds_asm_bits())
 * >>> f.max_errors_for(window_bits=96, pfa=1e-3)
 * 3
 * >>> f.max_errors_for(window_bits=100000, pfa=1e-3)   # search further
 * 0
 * @endcode
 */
int syncword_max_errors_for (syncword_state_t *state, size_t window_bits,
                             double pfa);
#ifdef __cplusplus
}
#endif

#endif /* SYNCWORD_CORE_H */
