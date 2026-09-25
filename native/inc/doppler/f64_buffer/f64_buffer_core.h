/**
 * @file f64_buffer_core.h
 * @brief The complex128 ring as the component just-makeit binds.
 *
 * The ring itself is `dp_f64_*` in buffer/buffer.h, header-only and
 * macro-stamped. This header is what makes it a jm component without
 * changing it:
 *
 *   - `f64_buffer_state_t` IS `dp_f64_t`, so the binding holds the real
 *     ring and calls the real functions -- nothing is wrapped.
 *   - #DECLARE_DP_BUFFER_VIEW stamps the element-typed face (one element
 *     per SAMPLE), which is the face a numpy array has.
 *   - The Doxygen below sits on DECLARATIONS. The macros supply the
 *     definitions, but a doc extractor reads text, not the preprocessor's
 *     output, so the per-width documentation -- and the Python examples the
 *     stub and `help()` both render -- has to be written where it can be
 *     seen. The `<obj>_get_<prop>` accessors are the one thing defined
 *     here: they are jm's naming, not the ring's.
 *
 * The two siblings (f32 / f64 / i16) are the same file over a different
 * element; a manifest template (just-makeit#1310) will say so once.
 */
#ifndef F64_BUFFER_CORE_H
#define F64_BUFFER_CORE_H

#include "doppler/clib_common.h"

#include "doppler/buffer/buffer.h"
#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief The component's state IS the ring. */
typedef dp_f64_t f64_buffer_state_t;

/**
 * @brief Lock-free SPSC ring buffer for complex128 (CF64) samples.
 *
 * Identical in design to :class:`F32Buffer` but stores ``double``
 * complex (128-bit / 16 bytes per sample) instead of ``float``
 * complex.  The virtual-memory double-mapping and cache-line separated
 * head/tail layout are the same.  The GIL is released inside
 * :meth:`wait` so a producer thread can run concurrently.
 *
 * @param capacity How many samples the ring holds: any size from 1 up, and
 *                 :attr:`capacity` is exactly this number on every machine.
 *                 What is rounded is the MAPPING behind it -- up to a
 *                 power of two, because indexing is a mask, and up to a
 *                 whole page -- so a capacity that is not a power of two
 *                 costs some address space (under 2x) and nothing per
 *                 call.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(512)
 * >>> buf.capacity >= 512
 * True
 * >>> buf.write(np.ones(256, dtype=np.complex128))
 * True
 * @endcode
 */
static inline dp_f64_t *dp_f64_create (size_t capacity);

/**
 * @brief Write complex128 samples into the buffer without blocking.
 *
 * Copies the entire array in a single ``memcpy``.  Rejects the
 * write atomically if there is insufficient free space; the
 * call is refused whole -- nothing copied, ``x`` untouched -- and
 * :attr:`dropped` grows by ``len(x)``.
 * The array must be 1-D and C-contiguous.
 *
 * @param state The ring. Must be non-NULL.
 * @param x Samples to write.  Must be 1-D and C-contiguous.
 * @param x_len Length of ``x``, in samples.
 *
 * @return ``True`` if all samples were written; ``False`` if the ring was full
 *         and the call was refused (``x`` untouched).
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(512)
 * >>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex128))
 * True
 * >>> buf2 = F64Buffer(512)
 * >>> buf2.write(np.zeros(512, dtype=np.complex128))
 * True
 * >>> buf2.write(np.zeros(1, dtype=np.complex128))
 * False
 * @endcode
 */
static inline bool
dp_f64_write_view (dp_f64_t *state, const double _Complex *x, size_t x_len);

/**
 * @brief Write as much of ``x`` as fits and say how much that was.
 *
 * The partial-write twin of :meth:`write`. Where :meth:`write`
 * refuses a block that does not fit whole, this takes the leading
 * samples that do and returns their count -- ``0`` when the ring
 * is full. It never refuses, so it never touches
 * :attr:`dropped`. It is the only way to feed a chunk larger than
 * the ring: loop, advancing by the return value, draining in
 * between.
 *
 * @param state The ring. Must be non-NULL.
 * @param x Samples to write.  Must be 1-D and C-contiguous.
 * @param x_len Length of ``x``, in samples.
 *
 * @return Samples accepted, ``0 <= k <= len(x)``.  The caller still owns
 *         ``x[k:]``.
 *
 * @code
 * A chunk three times the size of the ring, fed by looping:
 *
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(1024)
 * >>> cap = buf.capacity
 * >>> chunk = np.ones(3 * cap, dtype=np.complex128)
 * >>> fed = 0
 * >>> while fed < len(chunk):
 * ...     fed += buf.write_some(chunk[fed:])
 * ...     _ = buf.peek(buf.available); buf.consume()
 * >>> fed == 3 * cap, buf.dropped
 * (True, 0)
 * @endcode
 */
static inline size_t
dp_f64_write_some_view (dp_f64_t *state, const double _Complex *x,
                        size_t x_len);

/**
 * @brief Block until ``n`` samples are available; return zero-copy view.
 *
 * Spins with the GIL released until the producer has written at
 * least ``n`` samples.  Returns a zero-copy 1-D complex128 view
 * directly into the ring buffer.  Caller must call
 * :meth:`consume` before the next ``wait``.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of complex samples to wait for.
 *
 * @return Zero-copy view into the ring buffer.
 *
 * @throws EOFError The producer called :meth:`close` and fewer than ``n``
 *                  samples remain.  The tail is drained and no more is coming,
 *                  so the wait ends rather than blocking forever.
 * @throws KeyboardInterrupt Somebody asked this process to stop, through a
 *         :class:`doppler.interrupt.Interrupt` guard -- from any module:
 *         the flag is process-wide. Without a guard the spin checks for
 *         no signals at all.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(512)
 * >>> buf.write(np.array([1+2j, 3+4j], dtype=np.complex128))
 * True
 * >>> view = buf.wait(2)
 * >>> view.dtype
 * dtype('complex128')
 * >>> view.shape
 * (2,)
 * >>> view.tolist()
 * [(1+2j), (3+4j)]
 * >>> buf.consume()
 * @endcode
 */
static inline double _Complex *dp_f64_wait_view (dp_f64_t *state, size_t n);

/**
 * @brief :meth:`wait` that never blocks: a view, or None for not yet.
 *
 * The single-threaded consumer's read. :meth:`wait` spins until a
 * producer on *another* thread delivers, so a caller that is its
 * own producer would deadlock in it; ``peek`` answers at once
 * instead. When ``n`` samples are buffered it returns the same
 * zero-copy, always-contiguous view :meth:`wait` would
 * (1-D complex128); otherwise it returns ``None``.
 *
 * ``None`` means **not yet** and nothing else. The two conditions
 * no amount of waiting can cure are raised, exactly as
 * :meth:`wait` raises them, so a poll loop cannot mistake either
 * for a slow producer.
 *
 * Peeking does not consume. Follow it with :meth:`consume`; a
 * ``consume(k)`` with ``k < n`` advances by a hop smaller than
 * the frame, which is how overlapped frames are read.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of samples wanted.  Must be positive and not larger than
 *          :attr:`capacity`.
 *
 * @return Zero-copy view of the next ``n`` samples, or ``None`` when fewer
 *         than ``n`` have been written so far.
 *
 * @throws EOFError The ring is closed and fewer than ``n`` samples remain: the
 *                  rest is never coming.
 * @throws ValueError ``n`` exceeds :attr:`capacity` (or is not positive), so
 *                    no producer could ever satisfy it.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(1024)
 * >>> buf.peek(4) is None
 * True
 * >>> buf.write_some(np.ones(8, dtype=np.complex128))
 * 8
 * >>> buf.peek(4).shape
 * (4,)
 * >>> buf.consume(2)
 * >>> buf.available
 * 6
 * >>> buf.close()
 * >>> buf.peek(8)
 * Traceback (most recent call last):
 *     ...
 * EOFError: end of stream: the producer closed the ring
 * @endcode
 */
static inline double _Complex *dp_f64_peek_view (dp_f64_t *state, size_t n);

/**
 * @brief Release ``n`` samples back to the producer.
 *
 * Advances the consumer tail pointer by ``n``, making that space
 * available for the producer to overwrite, and ends the loan: the view
 * a :meth:`wait` or :meth:`peek` lent must not be used afterwards.  If
 * ``n`` is omitted it is the count of that outstanding view, so the
 * number is written once.  ``n`` smaller than the view is how overlapped
 * frames are read: release a hop, keep the rest.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of samples to release.  Defaults to the count of the
 *          outstanding :meth:`wait` / :meth:`peek` view.
 *
 * @return DP_OK, or DP_ERR_INVALID when ``n`` exceeds what is readable.
 *
 * @throws ValueError ``n`` exceeds :attr:`available`. Nothing is released:
 *         past that point the ring's counts would stop describing it.
 * @throws RuntimeError ``n`` was omitted and nothing is outstanding -- no view
 *         was lent since the last release, so there is no count to default
 *         to.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(512)
 * >>> buf.write(np.ones(4, dtype=np.complex128))
 * True
 * >>> _ = buf.wait(4)
 * >>> buf.consume()
 * @endcode
 */
static inline int dp_f64_consume (dp_f64_t *state, size_t n);

/**
 * @brief Say that no more data is coming.
 *
 * The producer's half of end of stream.  Until this exists a
 * consumer cannot tell a slow producer from a finished one --
 * both look like an empty ring -- so :meth:`wait` had nothing to
 * do but spin.  Call it once, after the last write.
 *
 * Release ordering: every sample written before this is visible
 * to a consumer that observes the flag.  Closing does not discard
 * what was already written; :meth:`wait` keeps returning batches
 * until the ring is drained, and only then raises ``EOFError``.
 *
 * See ``docs/design/io-termination.md`` for the one termination
 * contract shared with the network and disk transports.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.buffer import F64Buffer
 * >>> buf = F64Buffer(1024)
 * >>> buf.close()
 * >>> buf.closed
 * True
 * >>> buf.wait(4)
 * Traceback (most recent call last):
 *     ...
 * EOFError: end of stream: the producer closed the ring
 * @endcode
 */
static inline void dp_f64_close (dp_f64_t *state);

/**
 * @brief Empty the ring and reopen it.
 *
 * Discards everything buffered, and clears :attr:`closed` so the
 * same ring can carry a second stream -- without it, reuse after
 * :meth:`close` means destroying and re-mapping.  :attr:`dropped`
 * is a lifetime count and is kept.
 *
 * Not safe against a concurrent producer or consumer: it moves
 * both ends of the ring.  Call it only when both sides are idle.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(1024)
 * >>> buf.write_some(np.ones(8, dtype=np.complex128))
 * 8
 * >>> buf.close()
 * >>> buf.reset()
 * >>> buf.available, buf.closed
 * (0, False)
 * @endcode
 */
static inline void dp_f64_reset (dp_f64_t *state);

/**
 * @brief Unmap the buffer and free the underlying struct.
 *
 * Releases both virtual-address views and frees the C struct.
 * Safe to call more than once; subsequent calls are no-ops.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> buf = F64Buffer(512)
 * >>> buf.destroy()
 * @endcode
 */
static inline void dp_f64_destroy (dp_f64_t *state);

/**
 * @brief Buffer capacity in complex samples.
 *
 * Read-only.  Exactly the number passed to the constructor, whatever
 * the machine's page size; the mapping behind it is larger when that
 * number is not a power of two or spans less than a page, and that
 * slack is never room.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> F64Buffer(512).capacity >= 512
 * True
 * @endcode
 */
static inline size_t
f64_buffer_get_capacity (const f64_buffer_state_t *state)
{
  return state->capacity;
}

/**
 * @brief Samples written but not yet consumed.
 *
 * The largest ``n`` for which :meth:`wait` is guaranteed to return
 * without spinning.  Read this rather than tracking the count
 * yourself: :meth:`wait` has no timeout and no short return, so
 * asking for more than has been written spins until the producer
 * catches up -- forever, if there is no producer.
 *
 * Read from the consumer side this is a *lower* bound.  A producer
 * on another thread can only increase it, so a block sized from it
 * is always safe; it may simply be smaller than what has landed by
 * the time :meth:`wait` runs.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(1024)
 * >>> buf.available
 * 0
 * >>> _ = buf.write(np.zeros(100, dtype=np.complex128))
 * >>> buf.available
 * 100
 * >>> _ = buf.wait(64); buf.consume(64)
 * >>> buf.available
 * 36
 * @endcode
 */
static inline size_t
f64_buffer_get_available (const f64_buffer_state_t *state)
{
  return dp_f64_available (state);
}

/**
 * @brief Free room in samples: the largest :meth:`write` sure to fit.
 *
 * ``capacity - available``, read in one place so callers stop
 * deriving it.  Read from the producer side it is a *lower*
 * bound: a consumer on another thread can only increase it, so a
 * block sized from it is always accepted.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(1024)
 * >>> buf.space == buf.capacity
 * True
 * >>> buf.write_some(np.ones(8, dtype=np.complex128))
 * 8
 * >>> buf.capacity - buf.space
 * 8
 * @endcode
 */
static inline size_t
f64_buffer_get_space (const f64_buffer_state_t *state)
{
  return dp_f64_space (state);
}

/**
 * @brief Cumulative samples in REFUSED writes -- not samples lost.
 *
 * **Not a count of lost data.** :meth:`write` is all-or-nothing:
 * with no room it copies nothing, leaves the caller's array
 * untouched and refuses the call -- and this counter is then
 * incremented by the length of that refused call, not by 1 and not
 * by anything actually lost.
 *
 * So a producer that spins on :meth:`write` until it succeeds, the
 * obvious way to apply backpressure, inflates this while losing
 * nothing: a 60,000-sample run written that way reported 5,960,438.
 * Samples are lost only when the caller *discards* them, which is
 * what ignoring the return value does. Wait for room if you want
 * this to mean what it sounds like.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> import numpy as np
 * >>> buf = F64Buffer(512)
 * >>> buf.dropped
 * 0
 * >>> buf.write(np.zeros(512, dtype=np.complex128))
 * True
 * >>> buf.write(np.zeros(1, dtype=np.complex128))
 * False
 * >>> buf.dropped
 * 1
 * @endcode
 */
static inline size_t
f64_buffer_get_dropped (const f64_buffer_state_t *state)
{
  return state->dropped;
}

/**
 * @brief ``True`` once the producer has called :meth:`close`.
 *
 * The consumer's half of end of stream: it distinguishes "the
 * producer is slow" from "the producer has finished", which an
 * empty ring alone cannot.
 *
 * @code
 * >>> from doppler.buffer import F64Buffer
 * >>> buf = F64Buffer(1024)
 * >>> buf.closed
 * False
 * >>> buf.close()
 * >>> buf.closed
 * True
 * @endcode
 */
static inline bool
f64_buffer_get_closed (const f64_buffer_state_t *state)
{
  return dp_f64_closed (state);
}

DECLARE_DP_BUFFER_VIEW (f64, double, double _Complex)

#ifdef __cplusplus
}
#endif

#endif /* F64_BUFFER_CORE_H */
